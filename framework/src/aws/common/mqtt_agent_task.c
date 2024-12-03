/*
 * FreeRTOS STM32 Reference Integration
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023 Morse Micro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 * Derived from Lab-Project-coreMQTT-Agent 201215
 *
 */

/**
 * @file mqtt_agent_task.c
 * This file implements the MQTT Agent task loop.
 * This is responsible for maintaining the connection state to the MQTT broker
 * and attempting re-connection with a random back-off interval in the event
 * of a connection loss.  This task also contains the main message loop handler
 * which reads and executes commands from a message queue.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* MQTT library includes. */
#include "core_mqtt.h"
#include "core_mqtt_serializer.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"
#include "core_mqtt_agent_message_interface.h"
#include "command_pool.h"
#include "mqtt_agent_task.h"

/* Exponential backoff retry include. */
#include "backoff_algorithm.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

#include "transport_interface.h"

/* Morse Includes */
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"

/*-----------------------------------------------------------*/

/**
 * Timeout for receiving @c CONNACK after sending an MQTT CONNECT packet.
 * Defined in milliseconds.
 */
#define CONNACK_RECV_TIMEOUT_MS     (2000U)

/**
 * The minimum back-off delay to use for network operation retry
 * attempts.
 */
#define RETRY_BACKOFF_BASE          (10U)

/**
 * The maximum back-off delay for retrying failed operation
 *  with server.
 */
#define RETRY_MAX_BACKOFF_DELAY     (5U * 60U)

/**
 * Multiplier to apply to the back off delay to convert arbitrary units to milliseconds.
 */
#define RETRY_BACKOFF_MULTIPLIER    (100U)

static_assert(RETRY_BACKOFF_BASE < UINT16_MAX);
static_assert(RETRY_MAX_BACKOFF_DELAY < UINT16_MAX);
static_assert(((uint64_t) RETRY_BACKOFF_MULTIPLIER * \
               (uint64_t) RETRY_MAX_BACKOFF_DELAY) < UINT32_MAX);

/**
 * The maximum time interval in seconds which is allowed to elapse
 *  between two Control Packets.
 *
 *  It is the responsibility of the Client to ensure that the interval between
 *  Control Packets being sent does not exceed the this Keep Alive value. In the
 *  absence of sending any other Control Packets, the Client MUST send a
 *  @c PINGREQ Packet.
 */
#define KEEP_ALIVE_INTERVAL_S                 (60U)

/**
 * Socket send and receive timeouts to use.
 */
#define SEND_TIMEOUT_MS                       (2000U)

/**
 * This is the maximum size of any placeholder files, we determine the file is a dummy
 * if it is less than this size.
 */
#define DUMMY_FILE_SIZE                       100

/**
 * Specifies the maximum size of a generated Client ID.
 */
#define MAX_CLIENT_ID_LEN                     64

/** Macro to check if mutex is owned by current task */
#define MUTEX_IS_OWNED(xHandle)               mmosal_mutex_is_held_by_active_task(xHandle)

/**
 * Structure containing the message context for Agent commands
 */
struct MQTTAgentMessageContext
{
    struct mmosal_queue * xQueue;           /**< The queue to post the commands to */
};

/**
 * Subscription manager context
 */
typedef struct MQTTAgentSubscriptionManagerCtx
{
    MQTTSubscribeInfo_t pxSubscriptions[ MQTT_AGENT_MAX_SUBSCRIPTIONS ];/**< Subscriptions */
    MQTTSubAckStatus_t pxSubAckStatus[ MQTT_AGENT_MAX_SUBSCRIPTIONS ];  /**< Acknowledgment */
    uint32_t pulSubCbCount[ MQTT_AGENT_MAX_SUBSCRIPTIONS ];             /**< No of callbacks */
    SubCallbackElement_t pxCallbacks[ MQTT_AGENT_MAX_CALLBACKS ];       /**< Callbacks */

    size_t uxSubscriptionCount;                         /**< Total active subscriptions */
    size_t uxCallbackCount;                             /**< Total registered callbacks */
    MQTTAgentSubscribeArgs_t xInitialSubscribeArgs;     /**< Initial arguments */

    struct mmosal_mutex * xMutex;                           /**< Mutex to access this structure */
} SubMgrCtx_t;

/**
 * MQTT Agent Task context
 */
typedef struct MQTTAgentTaskCtx
{
    MQTTAgentContext_t xAgentContext;               /**< Context for MQTT Agent */

    MQTTFixedBuffer_t xNetworkFixedBuffer;          /**< Allocated buffer for this task */
    TransportInterface_t xTransport;                /**< Transport interface to use */

    MQTTAgentMessageInterface_t xMessageInterface;  /**< The message interface to use*/
    MQTTAgentMessageContext_t xAgentMessageCtx;     /**< Context for message */

    SubMgrCtx_t xSubMgrCtx;                         /**< Subscription manager context */

    MQTTConnectInfo_t xConnectInfo;                 /**< Connection status */
    char * pcMqttEndpoint;                          /**< MQTT endpoint to connect to */
    size_t uxMqttEndpointLen;                       /**< Length of above string */
    uint32_t ulMqttPort;                            /**< TCP Port to connect on */
} MQTTAgentTaskCtx_t;

/* ALPN protocols must be a NULL-terminated list of strings. */
static const char * pcAlpnProtocols[] = { "x-amzn-mqtt-ca", NULL };

/** Global event flags */
static volatile bool bConnectState = false;
static volatile bool bMqttTaskReady = false;

static MQTTAgentHandle_t xDefaultInstanceHandle = NULL;

/**
 * Returns the user agent string for AWS connection
 * @returns A static user agent string
 */
static char * prvGetUserAgentString(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version;
    static char userAgent[128];

    status = mmwlan_get_version(&version);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
    snprintf(userAgent, sizeof(userAgent),
             "?SDK=MorseLib&Version=%s&Platform=MorseMicro&MQTTLib=%s",
             version.morselib_version, MQTT_LIBRARY_VERSION);
    return userAgent;
}

/**
 * Fan out the incoming publishes to the callbacks registered by different
 * tasks. If there are no callbacks registered for the incoming publish, it will be
 * passed to the unsolicited publish handler.
 *
 * @param[in] pMqttAgentContext Agent context.
 * @param[in] packetId Packet ID of publish.
 * @param[in] pxPublishInfo Info of incoming publish.
 */
static void prvIncomingPublishCallback(MQTTAgentContext_t * pMqttAgentContext,
                                       uint16_t packetId,
                                       MQTTPublishInfo_t * pxPublishInfo);

/**
 * Function to attempt to resubscribe to the topics already present in the
 * subscription list.
 *
 * This function will be invoked when this demo requests the broker to
 * reestablish the session and the broker cannot do so. This function will
 * enqueue commands to the MQTT Agent queue and will be processed once the
 * command loop starts.
 *
 * @return @c MQTTSuccess if adding subscribes to the command queue succeeds, else
 * appropriate error code from @c MQTTAgent_Subscribe.
 * */
static MQTTStatus_t prvHandleResubscribe(MQTTAgentContext_t * pxMqttAgentCtx,
                                         SubMgrCtx_t * pxCtx);


static inline bool bLockSubCtx(SubMgrCtx_t * pxSubCtx)
{
    bool bResult = false;

    MMOSAL_ASSERT(pxSubCtx);
    MMOSAL_ASSERT(pxSubCtx->xMutex);

    MMOSAL_ASSERT(!MUTEX_IS_OWNED(pxSubCtx->xMutex));

    bResult = mmosal_mutex_get(pxSubCtx->xMutex, UINT32_MAX);

    if (!bResult)
    {
        printf("ERR:**** Mutex request failed, xResult=%d.\n", (int) bResult);
    }

    return bResult;
}


static inline bool bUnlockSubCtx(SubMgrCtx_t * pxSubCtx)
{
    bool bResult = false;

    MMOSAL_ASSERT(pxSubCtx);
    MMOSAL_ASSERT(pxSubCtx->xMutex);
    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxSubCtx->xMutex));

    bResult = mmosal_mutex_release(pxSubCtx->xMutex);

    if (!bResult)
    {
        printf("ERR:**** Mutex Give request failed, xResult=%d.\n", (int) bResult);
    }

    return bResult;
}

/**
 * Block until MQTT Agent is ready.
 */
void vSleepUntilMQTTAgentReady(void)
{
    while (!bMqttTaskReady)
    {
        mmosal_task_sleep(1000);
    }
}


/**
 * Block until MQTT Agent establishes a connection with the broker.
 */
void vSleepUntilMQTTAgentConnected(void)
{
    while (!bConnectState)
    {
        mmosal_task_sleep(1000);
    }
}


/**
 * Returns true if we are connected to the broker.
 * @return true if connected to the broker.
 */
bool bIsMqttAgentConnected(void)
{
    return bConnectState;
}


static inline void prvUpdateCallbackRefs(SubCallbackElement_t * pxCallbacksList,
                                         MQTTSubscribeInfo_t * pxSubList,
                                         size_t uxOldIdx,
                                         size_t uxNewIdx)
{
    MMOSAL_ASSERT(pxCallbacksList);
    MMOSAL_ASSERT(pxSubList);
    MMOSAL_ASSERT(uxOldIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS);
    MMOSAL_ASSERT(uxNewIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS);

    for (size_t uxIdx = 0; uxIdx < MQTT_AGENT_MAX_CALLBACKS; uxIdx++)
    {
        if (pxCallbacksList[ uxIdx ].pxSubInfo == &(pxSubList[ uxOldIdx ]))
        {
            pxCallbacksList[ uxIdx ].pxSubInfo = &(pxSubList[ uxNewIdx ]);
        }
    }
}


static inline void prvCompressSubscriptionList(MQTTSubscribeInfo_t * pxSubList,
                                               SubCallbackElement_t * pxCallbacksList,
                                               size_t * puxSubCount)
{
    size_t uxLastOccupiedIndex = 0;
    size_t uxSubCount = 0;

    MMOSAL_ASSERT(pxSubList);
    MMOSAL_ASSERT(pxCallbacksList);

    for (size_t uxIdx = 0U; uxIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS; uxIdx++)
    {
        if ((pxSubList[ uxIdx ].pTopicFilter == NULL) &&
            (pxSubList[ uxIdx ].topicFilterLength == 0) &&
            (uxLastOccupiedIndex < MQTT_AGENT_MAX_SUBSCRIPTIONS))
        {
            if (uxLastOccupiedIndex <= uxIdx)
            {
                uxLastOccupiedIndex = uxIdx + 1;
            }

            /* Iterate over remainder of list for occupied spots */
            for (; uxLastOccupiedIndex < MQTT_AGENT_MAX_CALLBACKS; uxLastOccupiedIndex++)
            {
                if (pxSubList[ uxLastOccupiedIndex ].topicFilterLength != 0)
                {
                    /* Move new item into place */
                    pxSubList[ uxIdx ] = pxSubList[ uxLastOccupiedIndex ];

                    /* Clear old location */
                    memset(&(pxSubList[ uxLastOccupiedIndex ]), 0, sizeof(MQTTSubscribeInfo_t));

                    prvUpdateCallbackRefs(pxCallbacksList, pxSubList, uxLastOccupiedIndex, uxIdx);

                    /* Increment count of active subscriptions */
                    uxSubCount++;

                    /* Increment counter so we don't visit this location again */
                    uxLastOccupiedIndex++;

                    break;
                }
            }
        }
        else if ((pxSubList[ uxIdx ].pTopicFilter != NULL) &&
                 (pxSubList[ uxIdx ].topicFilterLength != 0))
        {
            /* Increment count of active subscriptions */
            uxSubCount++;
        }
        else /*  pxSubList[ uxIdx ].topicFilterLength == 0 */
        {
            /* Continue iterating */
        }
    }

    *puxSubCount = uxSubCount;
}


static inline void prvCompressCallbackList(SubCallbackElement_t * pxCallbackList,
                                           size_t * puxCallbackCount)
{
    size_t uxLastOccupiedIndex = 0;
    size_t uxCallbackCount = 0;

    MMOSAL_ASSERT(pxCallbackList);
    MMOSAL_ASSERT(puxCallbackCount);

    for (size_t uxIdx = 0U; uxIdx < MQTT_AGENT_MAX_CALLBACKS; uxIdx++)
    {
        if ((pxCallbackList[ uxIdx ].pxSubInfo == NULL) &&    /* Current slot at uxIdx is Empty */
            (uxLastOccupiedIndex < MQTT_AGENT_MAX_CALLBACKS))
        {
            /* uxLastOccupiedIndex is always > uxIdx */
            if (uxLastOccupiedIndex <= uxIdx)
            {
                uxLastOccupiedIndex = uxIdx + 1;
            }

            /* Iterate over remainder of list for occupied spots */
            for (; uxLastOccupiedIndex < MQTT_AGENT_MAX_CALLBACKS; uxLastOccupiedIndex++)
            {
                if (pxCallbackList[ uxLastOccupiedIndex ].pxSubInfo != NULL)
                {
                    MMOSAL_ASSERT(uxLastOccupiedIndex > uxIdx);

                    /* Move context from uxLastOccupiedIndex to uxIdx */
                    pxCallbackList[ uxIdx ] = pxCallbackList[ uxLastOccupiedIndex ];

                    /* Clear old context location */
                    memset(&(pxCallbackList[ uxLastOccupiedIndex ]), 0,
                           sizeof(SubCallbackElement_t));

                    pxCallbackList[ uxLastOccupiedIndex ].pxIncomingPublishCallback = NULL;
                    pxCallbackList[ uxLastOccupiedIndex ].pvIncomingPublishCallbackContext = NULL;
                    pxCallbackList[ uxLastOccupiedIndex ].pxSubInfo = NULL;

                    /* Increment count of active callbacks */
                    uxCallbackCount++;

                    /* Increment counter so we don't visit this empty location again */
                    uxLastOccupiedIndex++;
                    break;
                }
            }
        }
        else if (pxCallbackList[ uxIdx ].pxSubInfo != NULL)
        {
            /* Increment count of valid contexts */
            uxCallbackCount++;
        }
        else /* pxCallbackList[ uxIdx ].pxSubInfo == NULL */
        {
            /* Continue iterating, ignore empty slot. */
        }
    }

    *puxCallbackCount = uxCallbackCount;
}


static bool prvAgentMessageSend(MQTTAgentMessageContext_t * pxMsgCtx,
                                MQTTAgentCommand_t * const * pxCommandToSend,
                                uint32_t blockTimeMs)
{
    bool bQueueStatus = false;

    if (pxMsgCtx && pxCommandToSend)
    {
        bQueueStatus = mmosal_queue_push(pxMsgCtx->xQueue, pxCommandToSend, blockTimeMs);
    }

    return bQueueStatus;
}


static bool prvAgentMessageReceive(MQTTAgentMessageContext_t * pxMsgCtx,
                                   MQTTAgentCommand_t ** ppxReceivedCommand,
                                   uint32_t blockTimeMs)
{
    bool bQueueStatus = false;
    if (pxMsgCtx && ppxReceivedCommand)
    {
        bQueueStatus = mmosal_queue_pop(pxMsgCtx->xQueue, ppxReceivedCommand, blockTimeMs);
    }
    if (!bQueueStatus && ppxReceivedCommand)
    {
        *ppxReceivedCommand = NULL;
    }

    return bQueueStatus;
}


static void prvResubscribeCommandCallback(MQTTAgentCommandContext_t * pxCommandContext,
                                          MQTTAgentReturnInfo_t * pxReturnInfo)
{
    SubMgrCtx_t * pxCtx = (SubMgrCtx_t *) pxCommandContext;

    MMOSAL_ASSERT(pxCommandContext != NULL);
    MMOSAL_ASSERT(pxReturnInfo != NULL);
    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xMutex));

    /* Ignore pxReturnInfo->returnCode */

    for (uint32_t ulSubIdx = 0; ulSubIdx < pxCtx->uxSubscriptionCount; ulSubIdx++)
    {
        /* Update cached SubAck status */
        pxCtx->pxSubAckStatus[ulSubIdx] = (MQTTSubAckStatus_t)pxReturnInfo->pSubackCodes[ulSubIdx];

        if (pxReturnInfo->pSubackCodes[ ulSubIdx ] == MQTTSubAckFailure)
        {
            MQTTSubscribeInfo_t * const pxSubInfo = &(pxCtx->pxSubscriptions[ ulSubIdx ]);

            printf("ERR:Failed to re-subscribe to topic filter!\n");

            for (uint32_t ulCbIdx = 0; ulCbIdx < MQTT_AGENT_MAX_CALLBACKS; ulCbIdx++)
            {
                SubCallbackElement_t * const pxCbInfo = &(pxCtx->pxCallbacks[ ulCbIdx ]);

                if (pxCbInfo->pxSubInfo == pxSubInfo)
                {
                    printf("WRN:Detected orphaned callback due to failed re-subscribe.\n");
                }
            }
        }
    }

    (void) bUnlockSubCtx(pxCtx);
}


static MQTTStatus_t prvHandleResubscribe(MQTTAgentContext_t * pxMqttAgentCtx,
                                         SubMgrCtx_t * pxCtx)
{
    MQTTStatus_t xStatus = MQTTSuccess;

    MMOSAL_ASSERT(pxCtx);
    MMOSAL_ASSERT(pxCtx->xMutex);
    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xMutex));

    prvCompressSubscriptionList(pxCtx->pxSubscriptions,
                                pxCtx->pxCallbacks,
                                &(pxCtx->uxSubscriptionCount));

    if ((xStatus == MQTTSuccess) && (pxCtx->uxSubscriptionCount > 0U))
    {
        MQTTAgentCommandInfo_t xCommandParams =
        {
            .blockTimeMs                 = 0U,
            .cmdCompleteCallback         = prvResubscribeCommandCallback,
            .pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) pxCtx,
        };

        pxCtx->xInitialSubscribeArgs.pSubscribeInfo = pxCtx->pxSubscriptions;
        pxCtx->xInitialSubscribeArgs.numSubscriptions = pxCtx->uxSubscriptionCount;

        /* Enqueue the subscribe command */
        xStatus = MQTTAgent_Subscribe(pxMqttAgentCtx,
                                      &(pxCtx->xInitialSubscribeArgs),
                                      &xCommandParams);

        /* prvResubscribeCommandCallback handles giving the mutex */

        if (xStatus != MQTTSuccess)
        {
            printf("ERR:Failed to enqueue the MQTT subscribe command. xStatus=%s.\n",
                   MQTT_Status_strerror(xStatus));
        }
    }
    else
    {
        (void) bUnlockSubCtx(pxCtx);

        /* Mark the resubscribe as success if there is nothing to be subscribed to. */
        xStatus = MQTTSuccess;
    }

    return xStatus;
}


static inline bool prvMatchTopic(MQTTSubscribeInfo_t * pxSubscribeInfo,
                                 const char * pcTopicName,
                                 const uint16_t usTopicLen)
{
    MQTTStatus_t xStatus = MQTTSuccess;
    bool isMatched = false;

    xStatus = MQTT_MatchTopic(pcTopicName,
                              usTopicLen,
                              pxSubscribeInfo->pTopicFilter,
                              pxSubscribeInfo->topicFilterLength,
                              &isMatched);
    return (xStatus == MQTTSuccess) && isMatched;
}


static inline bool prvMatchCbCtx(SubCallbackElement_t * pxCbCtx,
                                 MQTTSubscribeInfo_t * pxSubInfo,
                                 IncomingPubCallback_t pxCallback,
                                 void * pvCallbackCtx)
{
    return(pxCbCtx->pxSubInfo == pxSubInfo &&
           pxCbCtx->pvIncomingPublishCallbackContext == pvCallbackCtx &&
           pxCbCtx->pxIncomingPublishCallback == pxCallback);
}


static void prvIncomingPublishCallback(MQTTAgentContext_t * pMqttAgentContext,
                                       uint16_t packetId,
                                       MQTTPublishInfo_t * pxPublishInfo)
{
    SubMgrCtx_t * pxCtx = NULL;
    bool bPublishHandled = false;

    (void) packetId;

    MMOSAL_ASSERT(pMqttAgentContext);
    MMOSAL_ASSERT(pMqttAgentContext->pIncomingCallbackContext);
    MMOSAL_ASSERT(pxPublishInfo);

    pxCtx = (SubMgrCtx_t *) pMqttAgentContext->pIncomingCallbackContext;

    if (bLockSubCtx(pxCtx))
    {
        /* Iterate over pxCtx->pxCallbacks list */
        for (uint32_t ulCbIdx = 0; ulCbIdx < MQTT_AGENT_MAX_CALLBACKS; ulCbIdx++)
        {
            SubCallbackElement_t * const pxCallback = &(pxCtx->pxCallbacks[ ulCbIdx ]);
            MQTTSubscribeInfo_t * const pxSubInfo = pxCallback->pxSubInfo;

            if ((pxSubInfo != NULL) &&
                prvMatchTopic(pxSubInfo,
                              pxPublishInfo->pTopicName,
                              pxPublishInfo->topicNameLength))
            {
                pxCallback->pxIncomingPublishCallback(pxCallback->pvIncomingPublishCallbackContext,
                                                      pxPublishInfo);
                bPublishHandled = true;
            }
        }

        (void) bUnlockSubCtx(pxCtx);
    }

    if (!bPublishHandled)
    {
        printf("WRN:Incoming publish does not match any callback functions.\n");
    }
}


static void prvSubscriptionManagerCtxFree(SubMgrCtx_t * pxSubMgrCtx)
{
    MMOSAL_ASSERT(pxSubMgrCtx);

    if (pxSubMgrCtx->xMutex)
    {
        MMOSAL_ASSERT(MUTEX_IS_OWNED(pxSubMgrCtx->xMutex));
        mmosal_mutex_delete(pxSubMgrCtx->xMutex);
    }
}


static void prvSubscriptionManagerCtxReset(SubMgrCtx_t * pxSubMgrCtx)
{
    MMOSAL_ASSERT(pxSubMgrCtx);
    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxSubMgrCtx->xMutex));

    pxSubMgrCtx->uxSubscriptionCount = 0;
    pxSubMgrCtx->uxCallbackCount = 0;

    for (size_t uxIdx = 0; uxIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS; uxIdx++)
    {
        pxSubMgrCtx->pxSubAckStatus[ uxIdx ] = MQTTSubAckFailure;
        pxSubMgrCtx->pulSubCbCount[ uxIdx ] = 0;

        pxSubMgrCtx->pxSubscriptions[ uxIdx ].pTopicFilter = NULL;
        pxSubMgrCtx->pxSubscriptions[ uxIdx ].qos = (MQTTQoS_t) 0;
        pxSubMgrCtx->pxSubscriptions[ uxIdx ].topicFilterLength = 0;
    }

    for (size_t uxIdx = 0; uxIdx < MQTT_AGENT_MAX_CALLBACKS; uxIdx++)
    {
        pxSubMgrCtx->pxCallbacks[ uxIdx ].pvIncomingPublishCallbackContext = NULL;
        pxSubMgrCtx->pxCallbacks[ uxIdx ].pxIncomingPublishCallback = NULL;
        pxSubMgrCtx->pxCallbacks[ uxIdx ].pxSubInfo = NULL;
    }

    pxSubMgrCtx->xInitialSubscribeArgs.numSubscriptions = 0;
    pxSubMgrCtx->xInitialSubscribeArgs.pSubscribeInfo = NULL;
}


static MQTTStatus_t prvSubscriptionManagerCtxInit(SubMgrCtx_t * pxSubMgrCtx)
{
    MQTTStatus_t xStatus = MQTTSuccess;

    MMOSAL_ASSERT(pxSubMgrCtx);

    pxSubMgrCtx->xMutex = mmosal_mutex_create("MQTT_SUB_MGR");

    if (pxSubMgrCtx->xMutex)
    {
        (void) bLockSubCtx(pxSubMgrCtx);
        prvSubscriptionManagerCtxReset(pxSubMgrCtx);
    }
    else
    {
        xStatus = MQTTNoMemory;
    }

    return xStatus;
}


static void prvFreeAgentTaskCtx(MQTTAgentTaskCtx_t * pxCtx)
{
    if (pxCtx)
    {
        if (pxCtx->xAgentMessageCtx.xQueue != NULL)
        {
            mmosal_queue_delete(pxCtx->xAgentMessageCtx.xQueue);
        }

        if (pxCtx->xConnectInfo.pClientIdentifier != NULL)
        {
            mmosal_free((void *) pxCtx->xConnectInfo.pClientIdentifier);
        }

        if (pxCtx->pcMqttEndpoint != NULL)
        {
            mmosal_free(pxCtx->pcMqttEndpoint);
        }

        prvSubscriptionManagerCtxFree(&(pxCtx->xSubMgrCtx));

        mmosal_free((void *) pxCtx);
    }
}


static MQTTStatus_t prvConfigureAgentTaskCtx(MQTTAgentTaskCtx_t * pxCtx,
                                              NetworkContext_t * pxNetworkContext,
                                              uint8_t * pucNetworkBuffer,
                                              size_t uxNetworkBufferLen)
{
    MQTTStatus_t xStatus = MQTTSuccess;

    if (pxCtx == NULL)
    {
        xStatus = MQTTBadParameter;
        printf("ERR:Invalid pxCtx parameter.\n");
    }
    else if (pxNetworkContext == NULL)
    {
        xStatus = MQTTBadParameter;
        printf("ERR:Invalid pxNetworkContext parameter.\n");
    }
    else if (pucNetworkBuffer == NULL)
    {
        xStatus = MQTTBadParameter;
        printf("ERR:Invalid pucNetworkBuffer parameter.\n");
    }
    else
    {
        /* Zero Initialize */
        memset(pxCtx, 0, sizeof(MQTTAgentTaskCtx_t));
    }

    if (xStatus == MQTTSuccess)
    {
        /* Setup network buffer */
        pxCtx->xNetworkFixedBuffer.pBuffer = pucNetworkBuffer;
        pxCtx->xNetworkFixedBuffer.size = uxNetworkBufferLen;

        /* Setup transport interface */
        pxCtx->xTransport.pNetworkContext = pxNetworkContext;
        pxCtx->xTransport.send = transport_send;
        pxCtx->xTransport.recv = transport_recv;

        /* MQTTConnectInfo_t */
        /* Always start the initial connection with a clean session */
        memset((void *) &(pxCtx->xConnectInfo), 0x00, sizeof(pxCtx->xConnectInfo));
        pxCtx->xConnectInfo.cleanSession = true;
        pxCtx->xConnectInfo.keepAliveSeconds = KEEP_ALIVE_INTERVAL_S;
        pxCtx->xConnectInfo.pUserName = prvGetUserAgentString();
        pxCtx->xConnectInfo.userNameLength = strlen(pxCtx->xConnectInfo.pUserName);
        pxCtx->xConnectInfo.pPassword = NULL;
        pxCtx->xConnectInfo.passwordLength = 0U;

        /* Read Thing name */
        pxCtx->xConnectInfo.clientIdentifierLength = mmconfig_alloc_and_load(AWS_KEY_THING_NAME,
                (void**) &pxCtx->xConnectInfo.pClientIdentifier) - 1;
        if (pxCtx->xConnectInfo.pClientIdentifier == NULL)
        {
            /* If thingname is not found generate one using the MAC address */
            uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
            char *client_id = (char*) mmosal_malloc(MAX_CLIENT_ID_LEN);
            MMOSAL_ASSERT(mmwlan_get_mac_addr(mac_addr) == MMWLAN_SUCCESS);
            snprintf(client_id, MAX_CLIENT_ID_LEN,
                     "MM_Client_ID_%02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            pxCtx->xConnectInfo.pClientIdentifier = client_id;
            pxCtx->xConnectInfo.clientIdentifierLength =
                strlen(pxCtx->xConnectInfo.pClientIdentifier);
        }
        else if (pxCtx->xConnectInfo.pClientIdentifier[0] == '<')
        {
            printf("Please specify a valid [aws.thingname] in config store.\n");
            printf("Please refer to the documentation in aws_iot.c for "
                   "instructions on configuring the device for this application.\n");

            /* Free the memory as we failed */
            mmosal_free((void*) pxCtx->xConnectInfo.pClientIdentifier);
            pxCtx->xConnectInfo.pClientIdentifier = NULL;
            pxCtx->xConnectInfo.clientIdentifierLength = 0;
            xStatus = MQTTBadParameter;
        }
    }

    if (xStatus == MQTTSuccess)
    {
        pxCtx->xAgentMessageCtx.xQueue = mmosal_queue_create(MQTT_AGENT_COMMAND_QUEUE_LENGTH,
                                                             sizeof(MQTTAgentCommand_t *),
                                                             "MQTT_AGENT_TASK");

        if (pxCtx->xAgentMessageCtx.xQueue == NULL)
        {
            xStatus = MQTTNoMemory;
            printf("ERR:Failed to allocate MQTT Agent message queue.\n");
        }
    }

    if (xStatus == MQTTSuccess)
    {
        /* Setup message interface */
        pxCtx->xMessageInterface.pMsgCtx = &(pxCtx->xAgentMessageCtx);
        pxCtx->xMessageInterface.send = prvAgentMessageSend;
        pxCtx->xMessageInterface.recv = prvAgentMessageReceive;
        pxCtx->xMessageInterface.getCommand = Agent_GetCommand;
        pxCtx->xMessageInterface.releaseCommand = Agent_ReleaseCommand;

        /* Read endpoint from config store */
        if (mmconfig_alloc_and_load(AWS_KEY_ENDPOINT, (void**) &pxCtx->pcMqttEndpoint) <= 0)
        {
            printf("Could not find key [aws.endpoint] in config store.\n");
            printf("Please refer to the documentation in aws_iot.c for "
                   "instructions on configuring the device for this application.\n");
            xStatus = MQTTBadParameter;
        }
        else if (pxCtx->pcMqttEndpoint[0] == '<')
        {
            printf("Please specify a valid [aws.endpoint] in config store.\n");
            printf("Please refer to the documentation in aws_iot.c for "
                   "instructions on configuring the device for this application.\n");
            /* Free the memory as we failed */
            mmosal_free((void*)pxCtx->pcMqttEndpoint);
            pxCtx->pcMqttEndpoint = NULL;
            xStatus = MQTTBadParameter;
        }

        /* Read port from config store */
        char port_str[8];
        if (mmconfig_read_string(AWS_KEY_PORT, port_str, sizeof(port_str)) > 0)
        {
            pxCtx->ulMqttPort = atoi(port_str);
            if (pxCtx->ulMqttPort == 0)
            {
                printf("Invalid value specified for key [aws.port] in config store.\n");
                printf("Please refer to the documentation in aws_iot.c for "
                       "instructions on configuring the device for this application.\n");
                xStatus = MQTTBadParameter;
            }
        }
        else
        {
            /* If nothing specified assume default 443 */
            pxCtx->ulMqttPort = 443;
        }
    }

    if (xStatus == MQTTSuccess)
    {
        xStatus = prvSubscriptionManagerCtxInit(&(pxCtx->xSubMgrCtx));

        if (xStatus != MQTTSuccess)
        {
            printf("ERR:Failed to initialize Subscription Manager Context.\n");
        }
    }

    return xStatus;
}


/**
 * Returns the MQTT Agent handle.
 * @return The MQTT Agent handle.
 */
MQTTAgentHandle_t xGetMqttAgentHandle(void)
{
    return xDefaultInstanceHandle;
}


/**
 * The MQTT Agent task loop.
 * @param pvParameters Not used.
 */
void vMQTTAgentTask(void * pvParameters)
{
    MQTTStatus_t xMQTTStatus = MQTTSuccess;
    TransportStatus_t xTransportStatus = TRANSPORT_CONNECT_FAILURE;
    bool bExitFlag = false;

    MQTTAgentTaskCtx_t * pxCtx = NULL;
    uint8_t * pucNetworkBuffer = NULL;
    NetworkContext_t xNetworkContext = { 0 };
    NetworkCredentials_t xCredentials = { 0 };
    uint16_t usNextRetryBackOff = 0U;

    (void) pvParameters;

    /* Setup security */
    xCredentials.pAlpnProtos = pcAlpnProtocols;

    /* Load Root CA, first determine size to allocate */
    int len;
    uint8_t *allocptr;
    len =  mmconfig_read_bytes(AWS_KEY_ROOT_CA, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_ROOT_CA, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            xCredentials.pRootCa = allocptr;
            xCredentials.rootCaSize = (size_t) len + 1;
        }
        else
        {
            xMQTTStatus = MQTTNoMemory;
        }
    }
    else
    {
        printf("Could not find a valid key [aws.rootca] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        xMQTTStatus = MQTTBadParameter;
    }

    /* Load device certificate */
    len =  mmconfig_read_bytes(AWS_KEY_DEVICE_CERTIFICATE, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_DEVICE_CERTIFICATE, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            xCredentials.pClientCert = allocptr;
            xCredentials.clientCertSize = (size_t) len + 1;
        }
        else
        {
            xMQTTStatus = MQTTNoMemory;
        }
    }
    else
    {
        printf("Could not find a valid  key [aws.devicecert] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        xMQTTStatus = MQTTBadParameter;
    }

    /* Load device private keys */
    len =  mmconfig_read_bytes(AWS_KEY_DEVICE_KEYS, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid key */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_DEVICE_KEYS, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            xCredentials.pPrivateKey = allocptr;
            xCredentials.privateKeySize = (size_t) len + 1;
        }
        else
        {
            xMQTTStatus = MQTTNoMemory;
        }
    }
    else
    {
        printf("Could not find a valid  key [aws.devicekeys] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        xMQTTStatus = MQTTBadParameter;
    }

    /* Memory Allocation */
    pucNetworkBuffer = (uint8_t *) mmosal_malloc(MQTT_AGENT_NETWORK_BUFFER_SIZE);
    if (pucNetworkBuffer == NULL)
    {
        printf("ERR:Failed to allocate %d bytes for pucNetworkBuffer.\n",
               MQTT_AGENT_NETWORK_BUFFER_SIZE);
        xMQTTStatus = MQTTNoMemory;
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        pxCtx = (MQTTAgentTaskCtx_t *) mmosal_malloc(sizeof(MQTTAgentTaskCtx_t));

        if (pxCtx != NULL)
        {
            xMQTTStatus = prvConfigureAgentTaskCtx(pxCtx, &xNetworkContext,
                                                   pucNetworkBuffer,
                                                   MQTT_AGENT_NETWORK_BUFFER_SIZE);
        }
        else
        {
            printf("ERR:Failed to allocate %d bytes for MQTTAgentTaskCtx_t.\n",
                   sizeof(MQTTAgentTaskCtx_t));
            xMQTTStatus = MQTTNoMemory;
        }
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        Agent_InitializePool();
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        /* Initialize the MQTT context with the buffer and transport interface. */
        xMQTTStatus = MQTTAgent_Init(&(pxCtx->xAgentContext),
                                     &(pxCtx->xMessageInterface),
                                     &(pxCtx->xNetworkFixedBuffer),
                                     &(pxCtx->xTransport),
                                     mmosal_get_time_ms,
                                     prvIncomingPublishCallback,
                                     (void *) &(pxCtx->xSubMgrCtx));

        if (xMQTTStatus != MQTTSuccess)
        {
            printf("ERR:MQTTAgent_Init failed.\n");
        }
        else
        {
            bMqttTaskReady = true;
            xDefaultInstanceHandle = &(pxCtx->xAgentContext);
        }
    }

    /* Do not enter the main loop if we failed any of the steps above */
    if (xMQTTStatus == MQTTSuccess)
    {
        /* Outer Reconnect loop */
        while (bExitFlag != true)
        {
            BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
            BackoffAlgorithmContext_t xReconnectParams = { 0 };

            /* Initialize backoff algorithm with jitter */
            BackoffAlgorithm_InitializeParams(&xReconnectParams,
                                              RETRY_BACKOFF_BASE,
                                              RETRY_MAX_BACKOFF_DELAY,
                                              BACKOFF_ALGORITHM_RETRY_FOREVER);

            xTransportStatus = TRANSPORT_INTERNAL_ERROR;

            /* Connect a socket to the broker with retries */
            while (xTransportStatus != TRANSPORT_SUCCESS &&
                xBackoffAlgStatus == BackoffAlgorithmSuccess)
            {
                printf("INF:Attempting a TLS connection to %s:%u.\n",
                       pxCtx->pcMqttEndpoint, (unsigned int)pxCtx->ulMqttPort);

                xTransportStatus = transport_connect(&xNetworkContext,
                                                     pxCtx->pcMqttEndpoint,
                                                     (uint16_t) pxCtx->ulMqttPort,
                                                     &xCredentials);

                if (xTransportStatus != TRANSPORT_SUCCESS)
                {
                    /* Get back-off value (in seconds) for the next connection retry. */
                    xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff(&xReconnectParams,
                                                                        mmhal_random_u32(0,
                                                                        UINT32_MAX),
                                                                        &usNextRetryBackOff);

                    if (xBackoffAlgStatus == BackoffAlgorithmSuccess)
                    {
                        if (xTransportStatus == TRANSPORT_AUTHENTICATION_FAILED)
                        {
                            printf("WRN:Authentication failed connecting to the mqtt broker.\n"
                                   "Retrying connection in %u ms.\n",
                                   RETRY_BACKOFF_MULTIPLIER * usNextRetryBackOff);
                        }
                        else
                        {
                            printf("WRN:Connecting to the mqtt broker failed.\n"
                                   "Retrying connection in %u ms.\n",
                                   RETRY_BACKOFF_MULTIPLIER * usNextRetryBackOff);
                        }
                        mmosal_task_sleep(RETRY_BACKOFF_MULTIPLIER * usNextRetryBackOff);
                    }
                    else
                    {
                        printf("ERR:Connection to the broker failed, all attempts exhausted.\n");
                        bExitFlag = true;
                    }
                }
            }

            if (xTransportStatus == TRANSPORT_SUCCESS)
            {
                bool bSessionPresent = false;

                MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xSubMgrCtx.xMutex));

                (void) MQTTAgent_CancelAll(&(pxCtx->xAgentContext));

                xMQTTStatus = MQTT_Connect(&(pxCtx->xAgentContext.mqttContext),
                                           &(pxCtx->xConnectInfo),
                                           NULL,
                                           CONNACK_RECV_TIMEOUT_MS,
                                           &bSessionPresent);

                MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xSubMgrCtx.xMutex));

                /* Resume a session if desired. */
                if ((xMQTTStatus == MQTTSuccess) && (!pxCtx->xConnectInfo.cleanSession))
                {
                    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xSubMgrCtx.xMutex));
                    printf("INF:Resuming persistent MQTT Session.\n");

                    xMQTTStatus = MQTTAgent_ResumeSession(&(pxCtx->xAgentContext), bSessionPresent);

                    /* Re-subscribe to all the previously subscribed topics */
                    if ((xMQTTStatus == MQTTSuccess) && !bSessionPresent)
                    {
                        xMQTTStatus = prvHandleResubscribe(&(pxCtx->xAgentContext),
                                                           &(pxCtx->xSubMgrCtx));
                    }
                }
                else if (xMQTTStatus == MQTTSuccess)
                {
                    MMOSAL_ASSERT(MUTEX_IS_OWNED(pxCtx->xSubMgrCtx.xMutex));

                    printf("INF:Starting a clean MQTT Session.\n");

                    prvSubscriptionManagerCtxReset(&(pxCtx->xSubMgrCtx));

                    (void) bUnlockSubCtx(&(pxCtx->xSubMgrCtx));
                }
                else
                {
                    printf("ERR:Failed to connect to mqtt broker.\n");
                }

                /* Further reconnects will include a session resume operation */
                if (xMQTTStatus == MQTTSuccess)
                {
                    pxCtx->xConnectInfo.cleanSession = false;
                }
            }
            else
            {
                xMQTTStatus = MQTTKeepAliveTimeout;
            }

            if (xMQTTStatus == MQTTSuccess)
            {
                bConnectState = true;

                /* Reset backoff timer */
                BackoffAlgorithm_InitializeParams(&xReconnectParams,
                                                RETRY_BACKOFF_BASE,
                                                RETRY_MAX_BACKOFF_DELAY,
                                                BACKOFF_ALGORITHM_RETRY_FOREVER);

                /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
                * will manage the MQTT protocol until such time that an error occurs,
                * which could be a disconnect.  If an error occurs the MQTT context on
                * which the error happened is returned so there can be an attempt to
                * clean up and reconnect however the application writer prefers. */
                MQTTAgent_CommandLoop(&(pxCtx->xAgentContext));
            }

            (void) MQTTAgent_CancelAll(&(pxCtx->xAgentContext));

            transport_disconnect(&xNetworkContext);

            /* Flag disconnected state */
            bConnectState = false;

            /* Wait for any subscription related calls to complete */
            if (!MUTEX_IS_OWNED(pxCtx->xSubMgrCtx.xMutex))
            {
                (void) bLockSubCtx(&(pxCtx->xSubMgrCtx));
            }

            /* Reset subscription status */
            memset(pxCtx->xSubMgrCtx.pxSubAckStatus,
                   MQTTSubAckFailure,
                   sizeof(pxCtx->xSubMgrCtx.pxSubAckStatus));

            if (!bExitFlag)
            {
                /* Get back-off value (in seconds) for the next connection retry. */
                xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff(&xReconnectParams,
                                                                    mmhal_random_u32(0, UINT32_MAX),
                                                                    &usNextRetryBackOff);

                if (xBackoffAlgStatus == BackoffAlgorithmSuccess)
                {
                    printf("WRN:Disconnected from the MQTT Broker. Retrying in %u ms.\n",
                           RETRY_BACKOFF_MULTIPLIER * usNextRetryBackOff);

                    mmosal_task_sleep(RETRY_BACKOFF_MULTIPLIER * usNextRetryBackOff);
                }
                else
                {
                    printf("ERR:Reconnect limit reached. Will exit...\n");
                    bExitFlag = true;
                }
            }
        }
    }

    if (pxCtx != NULL)
    {
        prvFreeAgentTaskCtx(pxCtx);
        pxCtx = NULL;
    }

    /* Close transport incliding any TLS context */
    transport_disconnect(&xNetworkContext);

    if (pucNetworkBuffer)
    {
        mmosal_free(pucNetworkBuffer);
    }

    bMqttTaskReady = false;
    bConnectState = false;

    printf("ERR:Terminating MqttAgentTask.\n");
}


static inline MQTTQoS_t prvGetNewQoS(MQTTQoS_t xCurrentQoS,
                                     MQTTQoS_t xRequestedQoS)
{
    MQTTQoS_t xNewQoS;

    if (xCurrentQoS == xRequestedQoS)
    {
        xNewQoS = xCurrentQoS;
    }
    /* Otherwise, upgrade to QoS1 */
    else
    {
        xNewQoS = MQTTQoS1;
    }

    return xNewQoS;
}


static inline bool prvValidateQoS(MQTTQoS_t xQoS)
{
    return(xQoS == MQTTQoS0 || xQoS == MQTTQoS1 || xQoS == MQTTQoS2);
}


static void prvSubscribeRqCallback(MQTTAgentCommandContext_t * pxCommandContext,
                                   MQTTAgentReturnInfo_t * pxReturnInfo)
{
    struct mmosal_queue * pxSubQueue = (struct mmosal_queue *) pxCommandContext;

    MMOSAL_ASSERT(pxReturnInfo);

    uint32_t ulNotifyValue = (pxReturnInfo->returnCode & 0xFFFFFF);

    if (pxReturnInfo->pSubackCodes)
    {
        ulNotifyValue += (pxReturnInfo->pSubackCodes[ 0 ] << 24);
    }

    mmosal_queue_push(pxSubQueue, &ulNotifyValue, 0);
}


static inline MQTTStatus_t prvSendSubRequest(MQTTAgentContext_t * pxAgentCtx,
                                             MQTTSubscribeInfo_t * pxSubInfo,
                                             MQTTSubAckStatus_t * pxSubAckStatus,
                                             uint32_t xTimeout)
{
    MQTTStatus_t xStatus;
    struct mmosal_queue *pxSubQueue;

    pxSubQueue = mmosal_queue_create(1, sizeof(uint32_t), "QUEUE_prvSendSubRequest");

    MQTTAgentSubscribeArgs_t xSubscribeArgs =
    {
        .numSubscriptions = 1,
        .pSubscribeInfo   = pxSubInfo,
    };

    MQTTAgentCommandInfo_t xCommandInfo =
    {
        .blockTimeMs                 = xTimeout,
        .cmdCompleteCallback         = prvSubscribeRqCallback,
        .pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) pxSubQueue,
    };

    MMOSAL_ASSERT(pxAgentCtx);
    MMOSAL_ASSERT(pxSubInfo);
    MMOSAL_ASSERT(pxSubAckStatus);

    xStatus = MQTTAgent_Subscribe(pxAgentCtx, &xSubscribeArgs, &xCommandInfo);

    if (xStatus == MQTTSuccess)
    {
        uint32_t ulNotifyValue = 0;

        if (mmosal_queue_pop(pxSubQueue, &ulNotifyValue, xTimeout))
        {
            *pxSubAckStatus = (MQTTSubAckStatus_t) (ulNotifyValue >> 24);
            xStatus = (MQTTStatus_t) (ulNotifyValue & 0x00FFFFFF);
        }
        else
        {
            xStatus = MQTTKeepAliveTimeout;
        }
    }
    mmosal_queue_delete(pxSubQueue);

    return xStatus;
}


/**
 * Subscribe synchronously to the specified topic.
 * @param xHandle The MQTT agent handle.
 * @param pcTopicFilter The topic filter string, does not need to be null terminated.
 * @param xTopicFilterLen The length of the topic filter string.
 * @param xRequestedQoS The requested MQTT QoS.
 * @param pxCallback The function to callback when data matching the filter is received.
 * @param pvCallbackCtx The context to pass to the callback function.
 * @return Returns @c MQTTSuccess when we have successfully subscribed to the topic.
 */
MQTTStatus_t MqttAgent_SubscribeSync(MQTTAgentHandle_t xHandle,
                                     const char * pcTopicFilter,
                                     size_t xTopicFilterLen,
                                     MQTTQoS_t xRequestedQoS,
                                     IncomingPubCallback_t pxCallback,
                                     void * pvCallbackCtx)
{
    MQTTStatus_t xStatus = MQTTSuccess;
    MQTTAgentTaskCtx_t * pxTaskCtx = (MQTTAgentTaskCtx_t *) xHandle;
    SubMgrCtx_t * pxCtx = &(pxTaskCtx->xSubMgrCtx);

    if ((xHandle == NULL) ||
        (pcTopicFilter == NULL) ||
        (pxCallback == NULL) ||
        !prvValidateQoS(xRequestedQoS))
    {
        xStatus = MQTTBadParameter;
    }

    if ((xTopicFilterLen == 0) || (xTopicFilterLen >= UINT16_MAX))
    {
        xStatus = MQTTBadParameter;
    }

    /* Acquire mutex */
    if ((xStatus == MQTTSuccess) &&
        bLockSubCtx(pxCtx))
    {
        size_t uxTargetSubIdx = MQTT_AGENT_MAX_SUBSCRIPTIONS;
        size_t uxTargetCbIdx = MQTT_AGENT_MAX_CALLBACKS;

        /* If no slot is found, return MQTTNoMemory */
        xStatus = MQTTNoMemory;

        for (size_t uxSubIdx = 0U; uxSubIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS; uxSubIdx++)
        {
            MQTTSubscribeInfo_t * const pxSubInfo = &(pxCtx->pxSubscriptions[ uxSubIdx ]);

            if ((pxCtx->pxSubscriptions[ uxSubIdx ].pTopicFilter == NULL) &&
                (uxTargetSubIdx == MQTT_AGENT_MAX_SUBSCRIPTIONS))
            {
                /* Check that the current context is indeed empty */
                MMOSAL_ASSERT(pxCtx->pxSubscriptions[ uxSubIdx ].topicFilterLength == 0);

                uxTargetSubIdx = uxSubIdx;
                xStatus = MQTTSuccess;

                /* Reset SubAckStatus to trigger a subscribe op */
                pxCtx->pxSubAckStatus[ uxTargetSubIdx ] = MQTTSubAckFailure;
            }
            else if (strncmp(pxSubInfo->pTopicFilter, pcTopicFilter,
                     pxSubInfo->topicFilterLength) == 0)
            {
                xRequestedQoS = prvGetNewQoS(pxSubInfo->qos, xRequestedQoS);
                xStatus = MQTTSuccess;
                uxTargetSubIdx = uxSubIdx;

                /* If QoS differs, trigger a subscribe op */
                if (pxSubInfo->qos != xRequestedQoS)
                {
                    pxCtx->pxSubAckStatus[ uxTargetSubIdx ] = MQTTSubAckFailure;
                }

                break;
            }
            else
            {
                /* Empty */
            }
        }

        /* Add Callback to list */
        if (xStatus == MQTTSuccess)
        {
            /* If no slot is found, return MQTTNoMemory */
            xStatus = MQTTNoMemory;

            /* Find matching or empty callback context */
            for (size_t uxCbIdx = 0U; uxCbIdx < MQTT_AGENT_MAX_CALLBACKS; uxCbIdx++)
            {
                if ((uxTargetCbIdx == MQTT_AGENT_MAX_CALLBACKS) &&
                    (pxCtx->pxCallbacks[ uxCbIdx ].pxSubInfo == NULL))
                {
                    uxTargetCbIdx = uxCbIdx;
                    xStatus = MQTTSuccess;
                }
                else if (prvMatchCbCtx(&(pxCtx->pxCallbacks[ uxCbIdx ]),
                                       &(pxCtx->pxSubscriptions[ uxTargetSubIdx ]),
                                       pxCallback,
                                       pvCallbackCtx))
                {
                    uxTargetCbIdx = uxCbIdx;
                    xStatus = MQTTSuccess;
                    break;
                }
            }
        }

        /*
         * Populate the subscription entry (by copying topic filter to heap)
         */
        if ((xStatus == MQTTSuccess) &&
            (pxCtx->pxSubAckStatus[ uxTargetSubIdx ] == MQTTSubAckFailure))
        {
            if (pxCtx->pxSubscriptions[ uxTargetSubIdx ].pTopicFilter == NULL)
            {
                char * pcDupTopicFilter = (char *) mmosal_malloc(xTopicFilterLen + 1);

                if (pcDupTopicFilter == NULL)
                {
                    xStatus = MQTTNoMemory;
                }
                else
                {
                    (void) strncpy(pcDupTopicFilter, pcTopicFilter, xTopicFilterLen + 1);

                    /* Ensure null terminated */
                    pcDupTopicFilter[ xTopicFilterLen ] = '\00';

                    pxCtx->pxSubscriptions[ uxTargetSubIdx ].pTopicFilter = pcDupTopicFilter;
                    pxCtx->pxSubscriptions[ uxTargetSubIdx ].topicFilterLength =
                                                                (uint16_t) xTopicFilterLen;

                    pxCtx->uxSubscriptionCount++;
                }
            }

            pxCtx->pxSubscriptions[ uxTargetSubIdx ].qos = xRequestedQoS;
        }

        /*
         * Populate the callback entry
         */
        if ((xStatus == MQTTSuccess) &&
            (pxCtx->pxCallbacks[ uxTargetCbIdx ].pxSubInfo == NULL))
        {
            pxCtx->pxCallbacks[ uxTargetCbIdx ].pxSubInfo =
                                                &(pxCtx->pxSubscriptions[ uxTargetSubIdx ]);
            pxCtx->pxCallbacks[ uxTargetCbIdx ].pxIncomingPublishCallback = pxCallback;
            pxCtx->pxCallbacks[ uxTargetCbIdx ].pvIncomingPublishCallbackContext = pvCallbackCtx;

            /* Increment subscription reference count. */
            pxCtx->pulSubCbCount[ uxTargetSubIdx ]++;

            pxCtx->uxCallbackCount++;
        }

        (void) bUnlockSubCtx(pxCtx);

        if ((xStatus == MQTTSuccess) &&
            (pxCtx->pxSubAckStatus[ uxTargetSubIdx ] == MQTTSubAckFailure))
        {
            xStatus = prvSendSubRequest(&(pxTaskCtx->xAgentContext),
                                        &(pxCtx->pxSubscriptions[ uxTargetSubIdx ]),
                                        &(pxCtx->pxSubAckStatus[ uxTargetSubIdx ]),
                                        UINT32_MAX);
        }
    }
    else
    {
        xStatus = MQTTIllegalState;
        printf("ERR:Failed to acquire MQTTAgent mutex.\n");
    }

    return xStatus;
}


static void prvAgentRequestCallback(MQTTAgentCommandContext_t * pxCommandContext,
                                     MQTTAgentReturnInfo_t * pxReturnInfo)
{
    struct mmosal_queue * pxUnsubQueue = (struct mmosal_queue *) pxCommandContext;

    MMOSAL_ASSERT(pxReturnInfo);

    uint32_t ulNotifyValue = pxReturnInfo->returnCode;

    mmosal_queue_push(pxUnsubQueue, &ulNotifyValue, 0);
}


static inline MQTTStatus_t prvSendUnsubRequest(MQTTAgentContext_t * pxAgentCtx,
                                                const char * pcTopicFilter,
                                                size_t xTopicFilterLen,
                                                MQTTQoS_t xQos,
                                                uint32_t xTimeout)
{
    MQTTStatus_t xStatus = MQTTSuccess;
    struct mmosal_queue *pxUnsubQueue;

    pxUnsubQueue = mmosal_queue_create(1, sizeof(uint32_t), "QUEUE_prvSendUnsubRequest");

    MQTTSubscribeInfo_t xSubInfo =
    {
        .pTopicFilter      = pcTopicFilter,
        .topicFilterLength = (uint16_t) xTopicFilterLen,
        .qos               = xQos,
    };

    MQTTAgentSubscribeArgs_t xSubscribeArgs =
    {
        .numSubscriptions = 1,
        .pSubscribeInfo   = &xSubInfo,
    };

    MQTTAgentCommandInfo_t xCommandInfo =
    {
        .blockTimeMs                 = xTimeout,
        .cmdCompleteCallback         = prvAgentRequestCallback,
        .pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *) pxUnsubQueue,
    };

    xStatus = MQTTAgent_Unsubscribe(pxAgentCtx, &xSubscribeArgs, &xCommandInfo);

    if (xStatus == MQTTSuccess)
    {
        uint32_t ulNotifyValue = 0;

        if (mmosal_queue_pop(pxUnsubQueue, &ulNotifyValue, xTimeout))
        {
            xStatus = (MQTTStatus_t) ulNotifyValue;
        }
        else
        {
            xStatus = MQTTKeepAliveTimeout;
        }
    }
    mmosal_queue_delete(pxUnsubQueue);

    return xStatus;
}


/**
 * Unsubscribe from the specified topic, blocks till completed.
 * @param xHandle Handle of the MQTT Agent.
 * @param pcTopicFilter The topic filter string, need not be null terminated.
 * @param xTopicFilterLen The topic filter string length.
 * @param pxCallback The previously registered callback to remove.
 * @param pvCallbackCtx The previously registered context to remove.
 * @return Returns @c MQTTSuccess if successfully removed.
 */
MQTTStatus_t MqttAgent_UnSubscribeSync(MQTTAgentHandle_t xHandle,
                                        const char * pcTopicFilter,
                                        size_t xTopicFilterLen,
                                        IncomingPubCallback_t pxCallback,
                                        void * pvCallbackCtx)
{
    MQTTStatus_t xStatus = MQTTSuccess;
    MQTTAgentTaskCtx_t * pxTaskCtx = (MQTTAgentTaskCtx_t *) xHandle;
    SubMgrCtx_t * pxCtx = &(pxTaskCtx->xSubMgrCtx);
    uint32_t ulCallbackCount = 0;

    if ((xHandle == NULL) ||
        (pcTopicFilter == NULL) ||
        (pxCallback == NULL))
    {
        xStatus = MQTTBadParameter;
    }

    if ((xTopicFilterLen == 0) || (xTopicFilterLen >= UINT16_MAX))
    {
        xStatus = MQTTBadParameter;
    }

    if (xStatus == MQTTSuccess)
    {
        xStatus = MQTTNoDataAvailable;

        /* Acquire mutex */
        if (bLockSubCtx(pxCtx))
        {
            /* Find matching subscription context */
            for (size_t uxIdx = 0U; uxIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS; uxIdx++)
            {
                if ((xTopicFilterLen == pxCtx->pxSubscriptions[ uxIdx ].topicFilterLength) &&
                    (strncmp(pxCtx->pxSubscriptions[ uxIdx ].pTopicFilter,
                             pcTopicFilter,
                             pxCtx->pxSubscriptions[ uxIdx ].topicFilterLength) == 0))
                {
                    ulCallbackCount = pxCtx->pulSubCbCount[ uxIdx ];

                    if (ulCallbackCount > 0)
                    {
                        pxCtx->pulSubCbCount[ uxIdx ] = ulCallbackCount - 1;
                    }

                    xStatus = MQTTSuccess;
                    break;
                }
            }

            (void) bUnlockSubCtx(pxCtx);
        }
        else
        {
            xStatus = MQTTIllegalState;
            printf("ERR:Failed to acquire MQTTAgent mutex.\n");
        }

        /* Send unsubscribe request if only one callback is left for this subscription */
        if ((xStatus == MQTTSuccess) &&
            (ulCallbackCount == 1))
        {
            /* Use a reasonable timeout value here */
            xStatus = prvSendUnsubRequest(&(pxTaskCtx->xAgentContext),
                                          pcTopicFilter,
                                          xTopicFilterLen,
                                          MQTTQoS1,
                                          UINT32_MAX);
            /* If this step fails (due to network error, etc), chances are we are still subscribed,
             * so return an error and let the application try again later
             */
        }

        if (xStatus == MQTTSuccess)
        {
            /* Acquire mutex */
            if (bLockSubCtx(pxCtx))
            {
                MQTTSubscribeInfo_t * pxSubInfo = NULL;
                size_t uxSubInfoIdx = MQTT_AGENT_MAX_SUBSCRIPTIONS;
                /* If no subscription context matches the given topic filter then we return
                * MQTTBadParameter. */
                xStatus = MQTTBadParameter;

                /* Find matching subscription context again */
                for (size_t uxIdx = 0U; uxIdx < MQTT_AGENT_MAX_SUBSCRIPTIONS; uxIdx++)
                {
                    if ((pxCtx->pulSubCbCount[ uxIdx ] == 0) &&
                        (xTopicFilterLen == pxCtx->pxSubscriptions[ uxIdx ].topicFilterLength) &&
                        (strncmp(pxCtx->pxSubscriptions[ uxIdx ].pTopicFilter,
                                pcTopicFilter,
                                pxCtx->pxSubscriptions[ uxIdx ].topicFilterLength) == 0))
                    {
                        uxSubInfoIdx = uxIdx;
                        pxSubInfo = &(pxCtx->pxSubscriptions[ uxIdx ]);
                        xStatus = MQTTSuccess;
                        break;
                    }
                }

                if (xStatus == MQTTSuccess)
                {
                    xStatus = MQTTNoDataAvailable;

                    /* Find matching callback context, and remove it. */
                    for (size_t uxIdx = 0U; uxIdx < MQTT_AGENT_MAX_CALLBACKS; uxIdx++)
                    {
                        SubCallbackElement_t * pxCbCtx = &(pxCtx->pxCallbacks[ uxIdx ]);

                        if (prvMatchCbCtx(pxCbCtx, pxSubInfo, pxCallback, pvCallbackCtx))
                        {
                            xStatus = MQTTSuccess;
                            pxCbCtx->pvIncomingPublishCallbackContext = NULL;
                            pxCbCtx->pxIncomingPublishCallback = NULL;
                            pxCbCtx->pxSubInfo = NULL;

                            MMOSAL_ASSERT(pxCtx->uxCallbackCount > 0);

                            pxCtx->uxCallbackCount--;

                            prvCompressCallbackList(pxCtx->pxCallbacks, &(pxCtx->uxCallbackCount));
                            break;
                        }
                    }

                    if ((xStatus == MQTTSuccess) &&
                        (ulCallbackCount == 1) &&
                        (pxCtx->pulSubCbCount[ uxSubInfoIdx ] == 0))
                    {
                        /* Free heap allocated topic filter */
                        mmosal_free((void *) pxSubInfo->pTopicFilter);

                        pxSubInfo->pTopicFilter = NULL;
                        pxSubInfo->topicFilterLength = 0;
                        pxSubInfo->qos = (MQTTQoS_t) 0;
                        pxCtx->pxSubAckStatus[ uxSubInfoIdx ] = MQTTSubAckFailure;

                        MMOSAL_ASSERT(pxCtx->uxSubscriptionCount > 0);

                        if (pxCtx->uxSubscriptionCount > 0)
                        {
                            pxCtx->uxSubscriptionCount--;
                        }
                    }
                }

                (void) bUnlockSubCtx(pxCtx);
            }
            else
            {
                xStatus = MQTTIllegalState;
                printf("ERR:Failed to acquire MQTTAgent mutex.\n");
            }
        }
    }

    return xStatus;
}

/**
 * Creates the MQTT Agent task
 */
void start_mqtt_agent_task(void)
{
    mmosal_task_create(vMQTTAgentTask, NULL, MMOSAL_TASK_PRI_LOW, 1280, "MQTTAgent");
}
