/*
 * FreeRTOS STM32 Reference Integration
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 */

/**
 * @file
 * @brief Over The Air Update demo using coreMQTT Agent.
 *
 * The file demonstrates how to perform Over The Air update using OTA agent and coreMQTT
 * library. It creates an OTA agent task which manages the OTA firmware update
 * for the device. The example also provides implementations to subscribe, publish,
 * and receive data from an MQTT broker. The implementation uses coreMQTT agent which manages
 * thread safety of the MQTT operations and allows OTA agent to share the same MQTT
 * broker connection with other tasks. OTA agent invokes the callback implementations to
 * publish job related control information, as well as receive chunks
 * of pre-signed firmware image from the MQTT broker.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html
 * See https://freertos.org/ota/ota-mqtt-agent-demo.html
 */

/** Current log level */
#define LOG_LEVEL    LOG_INFO

/* Standard includes */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/unistd.h>

/* Kernel includes */
#include "mmosal.h"

#include "ota_config.h"

/* MQTT library includes */
#include "core_mqtt_agent.h"

/* Subscription manager header include */
#include "subscription_manager.h"

/* OTA Library include */
#include "ota.h"

/* OTA Library Interface include. */
#include "ota_os_mmosal.h"
#include "ota_mqtt_interface.h"

/* Include firmware version struct definition. */
#include "ota_appversion32.h"

/* Include platform abstraction header. */
#include "ota_pal.h"

#include "mqtt_agent_task.h"
#include "ota_update_task.h"

#include "mmconfig.h"

#ifdef TFM_PSA_API
#include "tfm_fwu_defs.h"
#include "psa/update.h"
#endif

/*------------- Demo configurations -------------------------*/

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define kOta_MAX_FILE_PATH_SIZE              (260)

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define kOta_MAX_STREAM_NAME_SIZE            (128)

/**
 * @brief The delay used in the OTA demo task to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otaexampleTASK_DELAY_MS                   (30 * 1000U)

/**
 * @brief The timeout for an OTA job in the OTA demo task
 */
#define otaexampleOTA_UPDATE_TIMEOUT_MS           (120 * 1000U)

/**
 * @brief The maximum time for which OTA demo waits for an MQTT operation to be complete.
 * This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE and non
 * QOS0 publishes.
 */
#define otaexampleMQTT_TIMEOUT_MS                 (10 * 1000U)

/**
 * @brief The common prefix for all OTA topics.
 *
 * Thing name is substituted with a wildcard symbol `+`. OTA agent
 * registers with MQTT broker with the thing name in the topic. This topic
 * filter is used to match incoming packet received and route them to OTA.
 * Thing name is not needed for this matching.
 */
#define OTA_TOPIC_PREFIX                          "$aws/things"

/**
 * @brief Length of OTA topics prefix.
 */
#define OTA_PREFIX_LENGTH                         (sizeof(OTA_TOPIC_PREFIX) - 1UL)

/**
 * @brief Wildcard topic filter for job notification.
 * The filter is used to match the constructed job notify topic filter from OTA agent and register
 * appropriate callback for it.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER               OTA_TOPIC_PREFIX "/+/jobs/notify-next"

/**
 * @brief Length of job notification topic filter.
 */
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LENGTH        (sizeof(OTA_JOB_NOTIFY_TOPIC_FILTER) - 1UL)

/**
 * @brief Wildcard topic filter for matching job response messages.
 * This topic filter is used to match the responses from OTA service for OTA agent job requests. THe
 * topic filter is a reserved topic which is not subscribed with MQTT broker.
 */
#define OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER    OTA_TOPIC_PREFIX "/+/jobs/$next/get/#"

/**
 * @brief Wildcard topic filter for matching OTA job update messages (used to detect cancellation).
 */
#define OTA_JOB_UPDATE_TOPIC_FILTER               OTA_TOPIC_PREFIX "/+/jobs/+/update/rejected"

/** The topic filter */
#define OTA_JOB_TOPIC_FILTER                      OTA_TOPIC_PREFIX "/+/jobs/#"
/** length of topic filter */
#define OTA_JOB_TOPIC_FILTER_LEN                  (sizeof(OTA_TOPIC_PREFIX "/+/jobs/#") - 1)

/**
 * @brief Wildcard topic filter for matching OTA data packets.
 * The filter is used to match the constructed data stream topic filter from OTA agent and
 * register appropriate callback for it.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER              OTA_TOPIC_PREFIX "/+/streams/#"

/**
 * @brief Length of data stream topic filter.
 */
#define OTA_DATA_STREAM_TOPIC_FILTER_LENGTH       ((uint16_t) \
                                                   (sizeof(OTA_DATA_STREAM_TOPIC_FILTER) - 1))

/**
 * @brief Size of the buffer used decode the @c CBOR message.
 */
#define kOtaCborDecodeBufferSize                 (1U << otaconfigLOG2_FILE_BLOCK_SIZE)

/**
 * @brief Starting index of client identifier within OTA topic.
 */
#define OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX    (OTA_PREFIX_LENGTH + 1)

/**
 * @brief Used to clear bits in a task's notification value.
 */
#define otaexampleMAX_UINT32                     (0xffffffff)

/**
 * @brief Task priority of OTA agent.
 */
#define otaexampleAGENT_TASK_PRIORITY            MMOSAL_TASK_PRI_LOW

/**
 * @brief Maximum stack size of OTA agent task.
 */
#define otaexampleAGENT_TASK_STACK_SIZE          (768)

/** Names of all the OTA states */
static const char *pOtaAgentStateStrings[OtaAgentStateAll + 1] =
{
    "Init",
    "Ready",
    "RequestingJob",
    "WaitingForJob",
    "CreatingFile",
    "RequestingFileBlock",
    "WaitingForFileBlock",
    "ClosingFile",
    "Suspended",
    "ShuttingDown",
    "Stopped",
    "All"
};

/** Static variable to hold the notify value */
static uint32_t ulNotifyValue = 0UL;

/** Pointer to post update callback function */
static ota_postupdate_cb_fn_t ota_task_postupdate_callback = NULL;

/**
 * @brief A statically allocated array of event buffers used by the OTA agent.
 * Maximum number of buffers are determined by how many chunks are requested
 * by OTA agent at a time along with an extra buffer to handle control message.
 * The size of each buffer is determined by the maximum size of firmware image
 * chunk, and other metadata send along with the chunk.
 */
typedef struct OtaEventBufferPool
{
    /** The event buffer */
    OtaEventData_t eventBuffer[otaconfigMAX_NUM_OTA_DATA_BUFFERS];
    /** Mutex to access the event buffer */
    struct mmosal_mutex *lock;
} OtaEventBufferPool_t;

/**
 * @brief The structure wraps the static buffers allocated by an OTA application
 * and used by OTA Agent. Static buffer should be in scope as long as the OTA Agent
 * task is active.
 */
typedef struct OtaAppStaticBuffer
{
    /**
     * @brief Buffer used to store the firmware image file path.
     * Buffer is passed to the OTA agent during initialization.
     */
    uint8_t updateFilePath[kOta_MAX_FILE_PATH_SIZE];

    /**
     * @brief Buffer used to store the code signing certificate file path.
     * Buffer is passed to the OTA agent during initialization.
     */
    uint8_t certFilePath[kOta_MAX_FILE_PATH_SIZE];

    /**
     * @brief Buffer used to store the name of the data stream.
     * Buffer is passed to the OTA agent during initialization.
     */
    uint8_t streamName[kOta_MAX_STREAM_NAME_SIZE];

    /**
     * @brief Buffer used decode the @c CBOR message from the MQTT payload.
     * Buffer is passed to the OTA agent during initialization.
     */
    uint8_t decodeMem[kOtaCborDecodeBufferSize];

    /**
     * @brief Application buffer used to store the bitmap for requesting firmware image
     * chunks from MQTT broker. Buffer is passed to the OTA agent during initialization.
     */
    uint8_t bitmap[OTA_MAX_BLOCK_BITMAP_SIZE];

    /** The event buffer pool */
    OtaEventBufferPool_t eventBufferPool;
} OtaAppStaticBuffer_t;

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    /** Binary semaphore to be given on completion of the command. */
    struct mmosal_semb *semb;
};

/**
 * @brief Function used by OTA agent to publish control messages to the MQTT broker.
 *
 * The implementation uses MQTT agent to queue a publish request. It then waits
 * for the request complete notification from the agent. The notification along with result of the
 * operation is sent back to the caller task using Notify API. For publishes involving QOS 1 and
 * QOS2 the operation is complete once an acknowledgment (@c PUBACK) is received. OTA agent uses
 * this function to fetch new job, provide status update and send other control related messages
 * to the MQTT broker.
 *
 * @param[in] pacTopic Topic to publish the control packet to.
 * @param[in] topicLen Length of the topic string.
 * @param[in] pMsg     Message to publish.
 * @param[in] msgSize  Size of the message to publish.
 * @param[in] qos      Qos for the publish.
 * @return             @c OtaMqttSuccess if successful. Appropriate error code otherwise.
 */
static OtaMqttStatus_t prvMQTTPublish(const char * const pacTopic,
                                      uint16_t topicLen,
                                      const char *pMsg,
                                      uint32_t msgSize,
                                      uint8_t qos);

/**
 * @brief Function used by OTA agent to subscribe for a control or data packet from the MQTT broker.
 *
 * The implementation queues a SUBSCRIBE request for the topic filter with the MQTT agent. It then
 * waits for
 * a notification of the request completion. Notification will be sent back to caller task,
 * using Notify API. MQTT agent also stores a callback provided by this function with
 * the associated topic filter. The callback will be used to
 * route any data received on the matching topic to the OTA agent. OTA agent uses this function
 * to subscribe to all topic filters necessary for receiving job related control messages as
 * well as firmware image chunks from MQTT broker.
 *
 * @param[in] pTopicFilter      The topic filter used to subscribe for packets.
 * @param[in] topicFilterLength Length of the topic filter string.
 * @param[in] ucQoS             Intended qos value for the messages received on this topic.
 * @return                      @c OtaMqttSuccess if successful. Appropriate error code otherwise.
 */
static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t ucQoS);

/**
 * @brief Function is used by OTA agent to unsubscribe a topic filter from MQTT broker.
 *
 * The implementation queues an UNSUBSCRIBE request for the topic filter with the MQTT agent. It
 * then waits
 * for a successful completion of the request from the agent. Notification along with results of
 * operation is sent using Notify API to the caller task. MQTT agent also removes the topic filter
 * subscription from its memory so any future
 * packets on this topic will not be routed to the OTA agent.
 *
 * @param[in] pTopicFilter      Topic filter to be unsubscribed.
 * @param[in] topicFilterLength Length of the topic filter.
 * @param[in] ucQoS             Qos value for the topic.
 * @return                      @c OtaMqttSuccess if successful. Appropriate error code otherwise.
 *
 */
static OtaMqttStatus_t prvMQTTUnsubscribe(const char *pTopicFilter,
                                          uint16_t topicFilterLength,
                                          uint8_t ucQoS);

/**
 * @brief Initialize the OTA event buffer pool.
 *
 * @param[in] pxBufferPool Pointer to the event buffer pool to be initialized.
 * @return                 true if Event Buffer pool is initialized.
 */
static bool prvOTAEventBufferPoolInit(OtaEventBufferPool_t *pxBufferPool);

/**
 * @brief Fetch an unused OTA event buffer from the pool.
 *
 * Demo uses a simple statically allocated array of fixed size event buffers. The
 * number of event buffers is configured by the parameter @c otaconfigMAX_NUM_OTA_DATA_BUFFERS
 * within ota_config.h. This function is used to fetch a free buffer from the pool for processing
 * by the OTA agent task. It uses a mutex for thread safe access to the pool.
 *
 * @param[in] pxBufferPool Pointer to the Event Buffer pool.
 * @return                 A pointer to an unused buffer from the pool. NULL if there are no buffers
 *                         available.
 */
static OtaEventData_t *prvOTAEventBufferGet(OtaEventBufferPool_t *pxBufferPool);

/**
 * @brief Free an event buffer back to pool
 *
 * OTA demo uses a statically allocated array of fixed size event buffers . The
 * number of event buffers is configured by the parameter @c otaconfigMAX_NUM_OTA_DATA_BUFFERS
 * within ota_config.h. The function is used by the OTA application callback to free a buffer,
 * after OTA agent has completed processing with the event. The access to the pool is made thread
 * safe
 * using a mutex.
 *
 * @param[in] pxBufferPool Pointer to the Event Buffer pool.
 * @param[in] pxBuffer     Pointer to the buffer to be freed.
 */
static void prvOTAEventBufferFree(OtaEventBufferPool_t *pxBufferPool,
                                  OtaEventData_t * const pxBuffer);

/**
 * @brief The function which runs the OTA agent task.
 *
 * The function runs the OTA Agent Event processing loop, which waits for
 * any events for OTA agent and process them. The loop never returns until the OTA agent
 * is shutdown. The tasks exits gracefully by freeing up all resources in the event of an
 *  OTA agent shutdown.
 *
 * @param[in] pvParam Any parameters to be passed to OTA agent task.
 */
static void prvOTAAgentTask(void *pvParam);

/**
 * @brief Callback invoked for firmware image chunks received from MQTT broker.
 *
 * Function gets invoked for the firmware image blocks received on OTA data stream topic.
 * The function is registered with MQTT agent's subscription manager along with the
 * topic filter for data stream. For each packet received, the
 * function fetches a free event buffer from the pool and queues the firmware image chunk for
 * OTA agent task processing.
 *
 * @param[in] pxSubscriptionContext Context which is passed unmodified from the MQTT agent.
 * @param[in] pPublishInfo          Pointer to the structure containing the details of the MQTT
 *                                  packet.
 */
static void prvProcessIncomingData(void *pxSubscriptionContext,
                                   MQTTPublishInfo_t *pPublishInfo);

/**
 * @brief Callback invoked for job control messages from MQTT broker.
 *
 * Callback gets invoked for any OTA job related control messages from the MQTT broker.
 * The function is registered with MQTT agent's subscription manager along with the topic filter for
 * job stream. The function fetches a free event buffer from the pool and queues the appropriate
 * event type
 * based on the control message received.
 *
 * @param[in] pxSubscriptionContext Context which is passed unmodified from the MQTT agent.
 * @param[in] pPublishInfo          Pointer to the structure containing the details of MQTT packet.
 */
static void prvProcessIncomingJobMessage(void *pxSubscriptionContext,
                                         MQTTPublishInfo_t *pPublishInfo);

/**
 * @brief Matches a client identifier within an OTA topic.
 * This function is used to validate that topic is valid and intended for this device thing name.
 *
 * @param[in] pTopic                 Pointer to the topic
 * @param[in] topicNameLength        length of the topic
 * @param[in] pClientIdentifier      Client identifier, should be null terminated.
 * @param[in] clientIdentifierLength Length of the client identifier.
 * @return                           true if client identifier is found within the topic at the
 *                                   right index.
 */
static bool prvMatchClientIdentifierInTopic(const char *pTopic,
                                            size_t topicNameLength,
                                            const char *pClientIdentifier,
                                            size_t clientIdentifierLength);

/**
 * @brief Returns true if the OTA Agent is currently executing a job.
 * @return true if OTA agent is currently active.
 */
static inline bool xIsOtaAgentActive(void);

/**
 * @brief Static buffer allocated by application and used by OTA Agent.
 * Buffer is allocated in the global scope outside of function call stack.
 */
static OtaAppStaticBuffer_t xAppStaticBuffer = { 0 };

/**
 * @brief Pointer which holds the thing name received from key value store.
 */

static char *pcThingName = NULL;

/**
 * @brief Variable which holds the length of the thing name.
 */
static size_t uxThingNameLength = 0UL;

/*---------------------------------------------------------*/

static bool prvOTAEventBufferPoolInit(OtaEventBufferPool_t *pxBufferPool)
{
    bool poolInit = false;

    MMOSAL_ASSERT(pxBufferPool != NULL);

    memset(pxBufferPool->eventBuffer, 0x00, sizeof(pxBufferPool->eventBuffer));

    pxBufferPool->lock = mmosal_mutex_create("OTA_MUTEX");

    if (pxBufferPool->lock != NULL)
    {
        poolInit = true;
    }

    return poolInit;
}

/*---------------------------------------------------------*/

static void prvOTAEventBufferFree(OtaEventBufferPool_t *pxBufferPool,
                                  OtaEventData_t * const pxBuffer)
{
    MMOSAL_ASSERT(pxBufferPool != NULL);

    if (mmosal_mutex_get(pxBufferPool->lock, UINT32_MAX))
    {
        pxBuffer->bufferUsed = false;
        (void)mmosal_mutex_release(pxBufferPool->lock);
    }
    else
    {
        LogError(("Failed to get buffer semaphore."));
    }
}

/*-----------------------------------------------------------*/

static OtaEventData_t *prvOTAEventBufferGet(OtaEventBufferPool_t *pxBufferPool)
{
    uint32_t ulIndex = 0;
    OtaEventData_t *pFreeBuffer = NULL;

    MMOSAL_ASSERT(pxBufferPool != NULL);

    if (mmosal_mutex_get(pxBufferPool->lock, UINT32_MAX))
    {
        for (ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++)
        {
            if (pxBufferPool->eventBuffer[ulIndex].bufferUsed == false)
            {
                pxBufferPool->eventBuffer[ulIndex].bufferUsed = true;
                pFreeBuffer = &pxBufferPool->eventBuffer[ulIndex];
                break;
            }
        }

        (void)mmosal_mutex_release(pxBufferPool->lock);
    }
    else
    {
        LogError(("Failed to get buffer semaphore."));
    }

    return pFreeBuffer;
}

/*-----------------------------------------------------------*/
static void prvOTAAgentTask(void *pvParam)
{
    OTA_EventProcessingTask(pvParam);
    mmosal_task_delete(NULL);
}

/*-----------------------------------------------------------*/

#ifdef TFM_PSA_API
static bool prvGetImageInfo(uint8_t ucSlot,
                            uint32_t ulImageType,
                            psa_image_info_t *pImageInfo)
{
    psa_status_t xPSAStatus;
    bool xStatus = false;
    psa_image_id_t ulImageID = FWU_CALCULATE_IMAGE_ID(ucSlot, ulImageType, 0);

    xPSAStatus = psa_fwu_query(ulImageID, pImageInfo);

    if (xPSAStatus == PSA_SUCCESS)
    {
        xStatus = true;
    }
    else
    {
        LogError("Failed to query image info for slot %u", ucSlot);
        xStatus = false;
    }

    return xStatus;
}

#endif /* ifdef TFM_PSA_API */

/*-----------------------------------------------------------*/

#ifdef TFM_PSA_API

/**
 * @brief Checks versions if active version has higher version than stage version.
 *
 * @param[in] pActiveVersion Active version.
 * @param[in] pStageVersion  Stage version.
 *
 * @return                   true if active version is higher than stage version. false otherwise.
 *
 */
static bool prvCheckVersion(psa_image_info_t *pActiveVersion,
                            psa_image_info_t *pStageVersion)
{
    bool xStatus = false;
    AppVersion32_t xActiveFirmwareVersion = { 0 };
    AppVersion32_t xStageFirmwareVersion = { 0 };

    xActiveFirmwareVersion.u.x.major = pActiveVersion->version.iv_major;
    xActiveFirmwareVersion.u.x.minor = pActiveVersion->version.iv_minor;
    xActiveFirmwareVersion.u.x.build = (uint16_t)pActiveVersion->version.iv_revision;

    xStageFirmwareVersion.u.x.major = pStageVersion->version.iv_major;
    xStageFirmwareVersion.u.x.minor = pStageVersion->version.iv_minor;
    xStageFirmwareVersion.u.x.build = (uint16_t)pStageVersion->version.iv_revision;

    if (xActiveFirmwareVersion.u.unsignedVersion32 > xStageFirmwareVersion.u.unsignedVersion32)
    {
        xStatus = true;
    }

    return xStatus;
}

#endif /* ifdef TFM_PSA_API */

/*-----------------------------------------------------------*/

#ifdef TFM_PSA_API

/**
 * @brief Checks versions of an image type for rollback protection.
 *
 * @param[in] ulImageType Image Type for which the version needs to be checked.
 *
 * @return                true if the version is higher than previous version. false otherwise.
 *
 */
static bool prvImageVersionCheck(uint32_t ulImageType)
{
    psa_image_info_t xActiveImageInfo = { 0 };
    psa_image_info_t xStageImageInfo = { 0 };

    bool xStatus = false;

    xStatus = prvGetImageInfo(FWU_IMAGE_ID_SLOT_ACTIVE, ulImageType, &xActiveImageInfo);

    if ((xStatus == true) && (xActiveImageInfo.state == PSA_IMAGE_PENDING_INSTALL))
    {
        xStatus = prvGetImageInfo(FWU_IMAGE_ID_SLOT_STAGE, ulImageType, &xStageImageInfo);

        if (xStatus == true)
        {
            xStatus = prvCheckVersion(&xActiveImageInfo, &xStageImageInfo);

            if (xStatus == false)
            {
                LogError("PSA Image type %d version validation failed, old version: "
                         "%u.%u.%u new version: %u.%u.%u",
                         ulImageType,
                         xStageImageInfo.version.iv_major,
                         xStageImageInfo.version.iv_minor,
                         xStageImageInfo.version.iv_revision,
                         xActiveImageInfo.version.iv_major,
                         xActiveImageInfo.version.iv_minor,
                         xActiveImageInfo.version.iv_revision);
            }
            else
            {
                LogError("PSA Image type %d version validation succeeded, old version: "
                         "%u.%u.%u new version: %u.%u.%u",
                         ulImageType,
                         xStageImageInfo.version.iv_major,
                         xStageImageInfo.version.iv_minor,
                         xStageImageInfo.version.iv_revision,
                         xActiveImageInfo.version.iv_major,
                         xActiveImageInfo.version.iv_minor,
                         xActiveImageInfo.version.iv_revision);
            }
        }
    }

    return xStatus;
}

#endif /* ifdef TFM_PSA_API */

/*-----------------------------------------------------------*/

#ifdef TFM_PSA_API

/**
 * @brief Get Secure and Non Secure Image versions.
 *
 * @param[out] pSecureVersion    Pointer to secure version struct.
 * @param[out] pNonSecureVersion Pointer to non-secure version struct.
 *
 * @return                       true if version was fetched successfully.
 *
 */
static bool prvGetImageVersion(AppVersion32_t *pSecureVersion,
                               AppVersion32_t *pNonSecureVersion)
{
    psa_image_info_t xImageInfo = { 0 };
    bool xStatus = false;

    xStatus = prvGetImageInfo(FWU_IMAGE_ID_SLOT_ACTIVE, FWU_IMAGE_TYPE_SECURE, &xImageInfo);

    if (xStatus == true)
    {
        pSecureVersion->u.x.major = xImageInfo.version.iv_major;
        pSecureVersion->u.x.minor = xImageInfo.version.iv_minor;
        pSecureVersion->u.x.build = (uint16_t)xImageInfo.version.iv_revision;
    }

    if (xStatus == true)
    {
        xStatus = prvGetImageInfo(FWU_IMAGE_ID_SLOT_ACTIVE, FWU_IMAGE_TYPE_NONSECURE, &xImageInfo);
    }

    if (xStatus == true)
    {
        pNonSecureVersion->u.x.major = xImageInfo.version.iv_major;
        pNonSecureVersion->u.x.minor = xImageInfo.version.iv_minor;
        pNonSecureVersion->u.x.build = (uint16_t)xImageInfo.version.iv_revision;
    }

    return xStatus;
}

#endif /* ifdef TFM_PSA_API */

/*-----------------------------------------------------------*/

/**
 * @brief The OTA agent has completed the update job or it is in
 * self test mode. If it was accepted, we want to activate the new image.
 * This typically means we should reset the device to run the new firmware.
 * If now is not a good time to reset the device, it may be activated later
 * by your user code. If the update was rejected, just return without doing
 * anything and we will wait for another job. If it reported that we should
 * start test mode, normally we would perform some kind of system checks to
 * make sure our new firmware does the basic things we think it should do
 * but we will just go ahead and set the image as accepted for demo purposes.
 * The accept function varies depending on your platform. Refer to the OTA
 * PAL implementation for your platform in aws_ota_pal.c to see what it
 * does for you.
 *
 * @param[in] event Specify if this demo is running with the AWS IoT
 * MQTT server. Set this to `false` if using another MQTT server.
 * @param[in] pData Data associated with the event.
 */
static void otaAppCallback(OtaJobEvent_t event,
                           void *pData)
{
    OtaErr_t err = OtaErrUninitialized;

    switch (event)
    {
    case OtaJobEventActivate:
        LogInfo(("Received OtaJobEventActivate callback from OTA Agent."));

        /**
         * Activate the new firmware image immediately. Applications can choose to postpone
         * the activation to a later stage if needed.
         */
        err = OTA_ActivateNewImage();

        /**
         * Activation of the new image failed. This indicates an error that requires a follow
         * up through manual activation by resetting the device. The demo reports the error
         * and shuts down the OTA agent.
         */
        LogError(("New image activation failed."));
        break;

    case OtaJobEventFail:

        /**
         * No user action is needed here. OTA agent handles the job failure event.
         */
        LogInfo(("Received an OtaJobEventFail notification from OTA Agent."));

        break;

    case OtaJobEventStartTest:

        /* This demo just accepts the image since it was a good OTA update and networking
         * and services are all working (or we would not have made it this far). If this
         * were some custom device that wants to test other things before validating new
         * image, this would be the place to kick off those tests before calling
         * OTA_SetImageState() with the final result of either accepted or rejected. */

        LogInfo(("Received OtaJobEventStartTest callback from OTA Agent."));

#ifdef TFM_PSA_API
        {
            /*
             * Do version check validation here, given that OTA Agent library does not handle
             * runtime version check of secure or non-secure images.
             */
            if ((prvImageVersionCheck(FWU_IMAGE_TYPE_SECURE) == true) &&
                (prvImageVersionCheck(FWU_IMAGE_TYPE_NONSECURE) == true))
            {
                err = OTA_SetImageState(OtaImageStateAccepted);
            }
            else
            {
                err = OTA_SetImageState(OtaImageStateRejected);

                if (err == OtaErrNone)
                {
                    /* Slight delay to flush the logs. */
                    mmosal_task_sleep(500);
                    /*  Reset the device, to revert back to the old image. */
                    psa_fwu_request_reboot();
                    LogError(("Failed to reset the device to revert the image."));
                }
                else
                {
                    LogError(("Unable to reject the image which failed self test."));
                }
            }
        }
#else /* ifdef TFM_PSA_API */
        {
            err = OTA_SetImageState(OtaImageStateAccepted);
        }
#endif /* ifdef TFM_PSA_API */

        if (err == OtaErrNone)
        {
            LogInfo(("New image validation succeeded in self test mode."));
        }
        else
        {
            LogError(("Failed to set image state as accepted with error %d.", err));
        }

        break;

    case OtaJobEventProcessed:

        LogDebug(("OTA Event processing completed. Freeing the event buffer to pool."));
        MMOSAL_ASSERT(pData != NULL);
        prvOTAEventBufferFree(&xAppStaticBuffer.eventBufferPool, (OtaEventData_t *)pData);

        break;

    case OtaJobEventSelfTestFailed:
        LogDebug(("Received OtaJobEventSelfTestFailed callback from OTA Agent."));

        /* Requires manual activation of previous image as self-test for
         * new image downloaded failed.*/
        LogError(("OTA Self-test failed for new image. shutting down OTA Agent."));
        break;

    case OtaJobEventUpdateComplete:
        LogInfo(("OTA Update Complete"));
        break;

    default:
        LogWarn(("Received an unhandled callback event from OTA Agent, event = %d", event));

        break;
    }
}

/*-----------------------------------------------------------*/

static void prvProcessIncomingData(void *pxContext,
                                   MQTTPublishInfo_t *pPublishInfo)
{
    bool isMatch = false;
    OtaEventData_t *pData;
    OtaEventMsg_t eventMsg = { 0 };

    (void)pxContext;

    MMOSAL_ASSERT(pPublishInfo != NULL);

    isMatch = prvMatchClientIdentifierInTopic(pPublishInfo->pTopicName,
                                              pPublishInfo->topicNameLength,
                                              pcThingName,
                                              uxThingNameLength);

    if (isMatch)
    {
        if (pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE)
        {
            LogDebug(("Received OTA image block, size %d.\n\n", pPublishInfo->payloadLength));

            pData = prvOTAEventBufferGet(&xAppStaticBuffer.eventBufferPool);

            if (pData != NULL)
            {
                memcpy(pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength);
                pData->dataLength = pPublishInfo->payloadLength;
                eventMsg.eventId = OtaAgentEventReceivedFileBlock;
                eventMsg.pEventData = pData;

                /* Send job document received event. */
                MMOSAL_ASSERT(OTA_SignalEvent(&eventMsg) == true);
            }
            else
            {
                LogError(("No OTA data buffers available.\r\n"));
            }
        }
        else
        {
            LogError(("Received OTA data block of size (%d) larger than maximum size(%d). ",
                      pPublishInfo->payloadLength,
                      OTA_DATA_BLOCK_SIZE));
        }
    }
    else
    {
        LogWarn(("Received data block on an unsolicited topic: %.*s ",
                 pPublishInfo->topicNameLength,
                 pPublishInfo->pTopicName));
    }
}

/*-----------------------------------------------------------*/

static void prvProcessIncomingJobMessage(void *pxSubscriptionContext,
                                         MQTTPublishInfo_t *pPublishInfo)
{
    OtaEventData_t *pData;
    OtaEventMsg_t eventMsg = { 0 };
    bool isMatch = false;

    (void)pxSubscriptionContext;
    MMOSAL_ASSERT(pPublishInfo != NULL);
    MMOSAL_ASSERT(pcThingName != NULL);

    isMatch = prvMatchClientIdentifierInTopic(pPublishInfo->pTopicName,
                                              pPublishInfo->topicNameLength,
                                              pcThingName,
                                              strlen(pcThingName));

    if (isMatch)
    {
        if (pPublishInfo->payloadLength <= OTA_DATA_BLOCK_SIZE)
        {
            LogInfo(("Received OTA job message, size: %d.\n\n", pPublishInfo->payloadLength));
            pData = prvOTAEventBufferGet(&xAppStaticBuffer.eventBufferPool);

            if (pData != NULL)
            {
                memcpy(pData->data, pPublishInfo->pPayload, pPublishInfo->payloadLength);
                pData->dataLength = pPublishInfo->payloadLength;
                eventMsg.eventId = OtaAgentEventReceivedJobDocument;
                eventMsg.pEventData = pData;

                /* Send job document received event. */
                MMOSAL_ASSERT(OTA_SignalEvent(&eventMsg) == true);
            }
            else
            {
                LogError(("No OTA data buffers available.\r\n"));
            }
        }
        else
        {
            LogError(("Received OTA job message size (%d) larger than maximum size (%d).\n\n",
                      pPublishInfo->payloadLength, OTA_DATA_BLOCK_SIZE));
        }
    }
    else
    {
        LogWarn(("Received a job message on an unsolicited topic. topic: %.*s ",
                 pPublishInfo->topicNameLength,
                 pPublishInfo->pTopicName));
    }
}

/*-----------------------------------------------------------*/

/**
 * returns the callback for a given topic
 *
 * @param  pcTopicFilter       The topic filter
 * @param  usTopicFilterLength length of topic filter
 * @return                     The callback if found, NULL otherwise
 */
static IncomingPubCallback_t prvGetPublishCallbackFromTopic(const char *pcTopicFilter,
                                                            size_t usTopicFilterLength)
{
    bool xIsMatch = false;
    IncomingPubCallback_t xCallback = NULL;

    (void)MQTT_MatchTopic(pcTopicFilter,
                          usTopicFilterLength,
                          OTA_JOB_TOPIC_FILTER,
                          OTA_JOB_TOPIC_FILTER_LEN,
                          &xIsMatch);

    if (xIsMatch == true)
    {
        xCallback = prvProcessIncomingJobMessage;
    }

    if (xIsMatch == false)
    {
        (void)MQTT_MatchTopic(pcTopicFilter,
                              usTopicFilterLength,
                              OTA_DATA_STREAM_TOPIC_FILTER,
                              OTA_DATA_STREAM_TOPIC_FILTER_LENGTH,
                              &xIsMatch);

        if (xIsMatch == true)
        {
            xCallback = prvProcessIncomingData;
        }
    }

    return xCallback;
}

/*-----------------------------------------------------------*/

static bool prvMatchClientIdentifierInTopic(const char *pTopic,
                                            size_t topicNameLength,
                                            const char *pClientIdentifier,
                                            size_t clientIdentifierLength)
{
    bool isMatch = false;
    size_t idx, matchIdx = 0;

    for (idx = OTA_TOPIC_CLIENT_IDENTIFIER_START_IDX; idx < topicNameLength; idx++)
    {
        if (matchIdx == clientIdentifierLength)
        {
            if (pTopic[idx] == '/')
            {
                isMatch = true;
            }

            break;
        }
        else
        {
            if (pClientIdentifier[matchIdx] != pTopic[idx])
            {
                break;
            }
        }

        matchIdx++;
    }

    return isMatch;
}

/*-----------------------------------------------------------*/

/**
 * Callback on command completion
 *
 * @param pCommandContext The command context
 * @param pxReturnInfo    The return code to pass
 */
static void prvCommandCallback(MQTTAgentCommandContext_t *pCommandContext,
                               MQTTAgentReturnInfo_t *pxReturnInfo)
{
    MMOSAL_ASSERT(pCommandContext != NULL);
    MMOSAL_ASSERT(pCommandContext->semb != NULL);
    MMOSAL_ASSERT(pxReturnInfo != NULL);

    ulNotifyValue = pxReturnInfo->returnCode;
    mmosal_semb_give(pCommandContext->semb);
}

/*-----------------------------------------------------------*/

static OtaMqttStatus_t prvMQTTSubscribe(const char *pTopicFilter,
                                        uint16_t topicFilterLength,
                                        uint8_t ucQoS)
{
    MQTTStatus_t mqttStatus;
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    IncomingPubCallback_t xPublishCallback;
    MQTTAgentHandle_t xMQTTAgentHandle = NULL;

    MMOSAL_ASSERT(pTopicFilter != NULL);
    MMOSAL_ASSERT(topicFilterLength > 0);

    xPublishCallback = prvGetPublishCallbackFromTopic(pTopicFilter, topicFilterLength);

    xMQTTAgentHandle = xGetMqttAgentHandle();

    if ((xMQTTAgentHandle == NULL) ||
        (xPublishCallback == NULL))
    {
        otaRet = OtaMqttSubscribeFailed;
    }
    else
    {
        mqttStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                             pTopicFilter,
                                             topicFilterLength,
                                             ucQoS,
                                             xPublishCallback,
                                             NULL);

        if (mqttStatus != MQTTSuccess)
        {
            LogError(("Failed to subscribe to topic with error = %u.",
                      mqttStatus));

            otaRet = OtaMqttSubscribeFailed;
        }
        else
        {
            LogInfo(("Subscribed to topic %.*s.\n\n",
                     topicFilterLength,
                     pTopicFilter));

            otaRet = OtaMqttSuccess;
        }
    }

    return otaRet;
}

static OtaMqttStatus_t prvMQTTPublish(const char * const pacTopic,
                                      uint16_t topicLen,
                                      const char *pMsg,
                                      uint32_t msgSize,
                                      uint8_t qos)
{
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTPublishInfo_t publishInfo = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };
    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentHandle_t xMQTTAgentHandle = NULL;

    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = qos;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    ulNotifyValue = 0;
    xCommandContext.semb = mmosal_semb_create("mqttpub");
    if (xCommandContext.semb == NULL)
    {
        return OtaMqttPublishFailed;
    }

    xCommandParams.blockTimeMs = otaexampleMQTT_TIMEOUT_MS;
    xCommandParams.cmdCompleteCallback = prvCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    xMQTTAgentHandle = xGetMqttAgentHandle();

    if (xMQTTAgentHandle == NULL)
    {
        otaRet = OtaMqttPublishFailed;
    }
    else
    {
        mqttStatus = MQTTAgent_Publish(xMQTTAgentHandle,
                                       &publishInfo,
                                       &xCommandParams);

        /* Wait for command to complete so MQTTSubscribeInfo_t remains in scope for the
         * duration of the command. */
        if (mqttStatus == MQTTSuccess)
        {
            bool ok = mmosal_semb_wait(xCommandContext.semb, UINT32_MAX);
            if (!ok)
            {
                mqttStatus = MQTTSendFailed;
            }
            else
            {
                mqttStatus = (MQTTStatus_t)(ulNotifyValue);
            }
        }

        if (mqttStatus != MQTTSuccess)
        {
            LogError(("Failed to send publish packet to broker with error = %u.", mqttStatus));
            otaRet = OtaMqttPublishFailed;
        }
        else
        {
            LogInfo(("Sent publish packet to broker %.*s to broker.\n\n",
                     topicLen,
                     pacTopic));

            otaRet = OtaMqttSuccess;
        }
    }

    if (xCommandContext.semb != NULL)
    {
        mmosal_semb_delete(xCommandContext.semb);
    }

    return otaRet;
}

static OtaMqttStatus_t prvMQTTUnsubscribe(const char *pTopicFilter,
                                          uint16_t topicFilterLength,
                                          uint8_t ucQoS)
{
    MQTTStatus_t mqttStatus;
    OtaMqttStatus_t otaRet = OtaMqttSuccess;
    IncomingPubCallback_t xPublishCallback;
    MQTTAgentHandle_t xMQTTAgentHandle = NULL;
    (void)ucQoS;

    MMOSAL_ASSERT(pTopicFilter != NULL);
    MMOSAL_ASSERT(topicFilterLength > 0);

    xPublishCallback = prvGetPublishCallbackFromTopic(pTopicFilter, topicFilterLength);

    xMQTTAgentHandle = xGetMqttAgentHandle();

    if ((xMQTTAgentHandle == NULL) ||
        (xPublishCallback == NULL))
    {
        otaRet = OtaMqttUnsubscribeFailed;
    }
    else
    {
        mqttStatus = MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                               pTopicFilter,
                                               topicFilterLength,
                                               xPublishCallback,
                                               NULL);

        if (mqttStatus != MQTTSuccess)
        {
            LogError(("Failed to unsubscribe from topic %.*s with error = %u.",
                      topicFilterLength,
                      pTopicFilter,
                      mqttStatus));

            otaRet = OtaMqttUnsubscribeFailed;
        }
        else
        {
            LogInfo(("Unsubscribed from topic %.*s.\n\n",
                     topicFilterLength,
                     pTopicFilter));

            otaRet = OtaMqttSuccess;
        }
    }

    return otaRet;
}

/*-----------------------------------------------------------*/

/**
 * Sets up the function table for this OTA interface
 *
 * @param pOtaInterfaces Pointer to the interface
 */
static void prvSetOtaInterfaces(OtaInterfaces_t *pOtaInterfaces)
{
    MMOSAL_ASSERT(pOtaInterfaces != NULL);

    /* Initialize OTA library OS Interface. */
    pOtaInterfaces->os.event.init = OtaInitEvent_MMOSAL;
    pOtaInterfaces->os.event.send = OtaSendEvent_MMOSAL;
    pOtaInterfaces->os.event.recv = OtaReceiveEvent_MMOSAL;
    pOtaInterfaces->os.event.deinit = OtaDeinitEvent_MMOSAL;
    pOtaInterfaces->os.timer.start = OtaStartTimer_MMOSAL;
    pOtaInterfaces->os.timer.stop = OtaStopTimer_MMOSAL;
    pOtaInterfaces->os.timer.delete = OtaDeleteTimer_MMOSAL;
    pOtaInterfaces->os.mem.malloc = Malloc_MMOSAL;
    pOtaInterfaces->os.mem.free = Free_MMOSAL;

    /* Initialize the OTA library MQTT Interface.*/
    pOtaInterfaces->mqtt.subscribe = prvMQTTSubscribe;
    pOtaInterfaces->mqtt.publish = prvMQTTPublish;
    pOtaInterfaces->mqtt.unsubscribe = prvMQTTUnsubscribe;

    /* Initialize the OTA library PAL Interface.*/
    pOtaInterfaces->pal.getPlatformImageState = otaPal_GetPlatformImageState;
    pOtaInterfaces->pal.setPlatformImageState = otaPal_SetPlatformImageState;
    pOtaInterfaces->pal.writeBlock = otaPal_WriteBlock;
    pOtaInterfaces->pal.activate = otaPal_ActivateNewImage;
    pOtaInterfaces->pal.closeFile = otaPal_CloseFile;
    pOtaInterfaces->pal.reset = otaPal_ResetDevice;
    pOtaInterfaces->pal.abort = otaPal_Abort;
    pOtaInterfaces->pal.createFile = otaPal_CreateFileForRx;
}

/**
 * Setup buffers and lengths for this OTA interface
 *
 * @param pOtaAppBuffer Structure containing buffer pointers and lengths
 */
static void prvSetOTAAppBuffer(OtaAppBuffer_t *pOtaAppBuffer)
{
    pOtaAppBuffer->pUpdateFilePath = xAppStaticBuffer.updateFilePath;
    pOtaAppBuffer->updateFilePathsize = kOta_MAX_FILE_PATH_SIZE;
    pOtaAppBuffer->pCertFilePath = xAppStaticBuffer.certFilePath;
    pOtaAppBuffer->certFilePathSize = kOta_MAX_FILE_PATH_SIZE;
    pOtaAppBuffer->pStreamName = xAppStaticBuffer.streamName;
    pOtaAppBuffer->streamNameSize = kOta_MAX_STREAM_NAME_SIZE;
    pOtaAppBuffer->pDecodeMemory = xAppStaticBuffer.decodeMem;
    pOtaAppBuffer->decodeMemorySize = kOtaCborDecodeBufferSize;
    pOtaAppBuffer->pFileBitmap = xAppStaticBuffer.bitmap;
    pOtaAppBuffer->fileBitmapSize = OTA_MAX_BLOCK_BITMAP_SIZE;
}

/**
 * Returns true if the OTA agent is active
 *
 * @return true if OTA agent is active
 */
static inline bool xIsOtaAgentActive(void)
{
    bool xResult;

    switch (OTA_GetState())
    {
    case OtaAgentStateRequestingJob:
    case OtaAgentStateCreatingFile:
    case OtaAgentStateRequestingFileBlock:
    case OtaAgentStateWaitingForFileBlock:
    case OtaAgentStateClosingFile:
        xResult = true;
        break;

    case OtaAgentStateWaitingForJob:
    case OtaAgentStateNoTransition:
    case OtaAgentStateInit:
    case OtaAgentStateReady:
    case OtaAgentStateStopped:
    case OtaAgentStateSuspended:
    case OtaAgentStateShuttingDown:
    default:
        xResult = false;
        break;
    }

    return xResult;
}

/**
 * The OTA update task loop.
 * @param pvParam Not used.
 */
void vOTAUpdateTask(void *pvParam)
{
    (void)pvParam;
    /* FreeRTOS APIs return status. */
    bool xResult = true;

    /* OTA library return status. */
    OtaErr_t otaRet = OtaErrNone;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t eventMsg = { 0 };

    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t otaInterfaces;

    MQTTStatus_t xMQTTStatus = MQTTBadParameter;

    MQTTAgentHandle_t xMQTTAgentHandle = NULL;

    /**
     * @brief Structure containing all application allocated buffers used by the OTA agent.
     * Structure is passed to the OTA agent during initialization.
     */
    OtaAppBuffer_t otaAppBuffer = { 0 };

    /* Check if we just did an update and a delete_file is pending */
    static char delete_path[128];
    if (mmconfig_read_string("DELETE_FILE", delete_path, sizeof(delete_path)) > 0)
    {
        if (ota_task_postupdate_callback)
        {
            /* A status code of 0 means success, provided UPDATE_IMAGE and BOOTLOADER_ERROR keys do
             * not exist */
            int bootloader_status = 0;

            if (mmconfig_read_string("UPDATE_IMAGE", NULL, 0) > 0)
            {
                /* If we still have an UPDATE_IMAGE key, then it is likely the update failed */
                bootloader_status = -1;
            }

            /* Check if a BOOTLOADER_ERROR key exists and cleanup */
            (void)mmconfig_read_int("BOOTLOADER_ERROR", &bootloader_status);

            /* Call the postupdate callback with the update status */
            ota_task_postupdate_callback(delete_path, bootloader_status);
        }

        /* Delete the file */
        unlink(delete_path);

        /* Cleanup these keys post update */
        mmconfig_delete_key("UPDATE_ATTEMPTS");
        mmconfig_delete_key("IMAGE_SIGNATURE");
        mmconfig_delete_key("UPDATE_IMAGE");
        mmconfig_delete_key("BOOTLOADER_ERROR");
        mmconfig_delete_key("DELETE_FILE");
    }

    /* Set OTA Library interfaces.*/
    prvSetOtaInterfaces(&otaInterfaces);

    /* Set OTA buffers for use by OTA agent. */
    prvSetOTAAppBuffer(&otaAppBuffer);

#ifndef TFM_PSA_API
    {
        /*
         * Application defined firmware version is only used in Non-Trustzone.
         */

        LogInfo(("OTA Agent: Application version %u.%u.%u",
                 appFirmwareVersion.u.x.major,
                 appFirmwareVersion.u.x.minor,
                 appFirmwareVersion.u.x.build));
    }
#else
    {
        AppVersion32_t xSecureVersion = { 0 }, xNSVersion = { 0 };
        prvGetImageVersion(&xSecureVersion, &xNSVersion);
        LogInfo(("OTA Agent: Secure Image version %u.%u.%u, Non-secure Image Version: %u.%u.%u",
                 xSecureVersion.u.x.major,
                 xSecureVersion.u.x.minor,
                 xSecureVersion.u.x.build,
                 xNSVersion.u.x.major,
                 xNSVersion.u.x.minor,
                 xNSVersion.u.x.build));
    }
#endif /* ifndef TFM_PSA_API */

    /****************************** Init OTA Library. ******************************/

    if (xResult)
    {
        /* Fetch thing name from key value store. */
        uxThingNameLength = mmconfig_alloc_and_load(AWS_KEY_THING_NAME,
                                                    (void **)&pcThingName) - 1;

        if ((pcThingName == NULL) ||
            (uxThingNameLength == 0))
        {
            xResult = false;
            LogError(("Failed to load thing name from key value store."));
        }
    }

    if (xResult)
    {
        LogInfo(("Waiting until MQTT Agent is connected."));

        /* Wait for first mqtt connection */
        vSleepUntilMQTTAgentConnected();
        xMQTTAgentHandle = xGetMqttAgentHandle();
    }

    if (xResult && (xMQTTAgentHandle != NULL))
    {
        xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                              OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER,
                                              strlen(OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER),
                                              MQTTQoS0,
                                              prvProcessIncomingJobMessage,
                                              NULL);

        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to subscribe to Job Accepted response topic filter."));
            xResult = false;
        }
    }

    if (xResult && (xMQTTAgentHandle != NULL))
    {
        xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                              OTA_JOB_NOTIFY_TOPIC_FILTER,
                                              strlen(OTA_JOB_NOTIFY_TOPIC_FILTER),
                                              MQTTQoS0,
                                              prvProcessIncomingJobMessage,
                                              NULL);

        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to subscribe to Job Update topic filter."));
            xResult = false;
        }
    }

    if (xResult)
    {
        /* Initialize event buffer pool. */
        xResult = prvOTAEventBufferPoolInit(&xAppStaticBuffer.eventBufferPool);
    }

    if (xResult)
    {
        if ((otaRet = OTA_Init(&otaAppBuffer,
                               &otaInterfaces,
                               (const uint8_t *)(pcThingName),
                               otaAppCallback)) != OtaErrNone)
        {
            LogError(("Failed to initialize OTA Agent, exiting = %u.",
                      otaRet));
            xResult = false;
        }
    }

    if (xResult)
    {
        if (!mmosal_task_create(prvOTAAgentTask, NULL,
                                otaexampleAGENT_TASK_PRIORITY,
                                otaexampleAGENT_TASK_STACK_SIZE,
                                "OTAAgent"))
        {
            LogError(("Failed to start OTA Agent task"));
        }
    }

    /***************************Start OTA demo loop. ******************************/

    if (xResult)
    {
        /* Start the OTA Agent.*/
        eventMsg.eventId = OtaAgentEventStart;
        (void)OTA_SignalEvent(&eventMsg);

        do
        {
            /* OTA library packet statistics per job.*/
            OtaAgentStatistics_t otaStatistics = { 0 };

            /* Get OTA statistics for currently executing job. */
            if ((xIsOtaAgentActive() == true) &&
                (OTA_GetStatistics(&otaStatistics) == OtaErrNone))
            {
                (void)pOtaAgentStateStrings;
                LogInfo(("State: %s   Received: %lu   Queued: %lu   Processed: %lu   Dropped: %lu",
                         pOtaAgentStateStrings[OTA_GetState()],
                         otaStatistics.otaPacketsReceived,
                         otaStatistics.otaPacketsQueued,
                         otaStatistics.otaPacketsProcessed,
                         otaStatistics.otaPacketsDropped));
            }

            mmosal_task_sleep(otaexampleTASK_DELAY_MS);
        } while (OTA_GetState() != OtaAgentStateStopped);
    }

    LogInfo(("OTA agent task stopped. Exiting OTA demo."));

    if (xMQTTStatus == MQTTSuccess)
    {
        xMQTTStatus = MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                                OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER,
                                                strlen(OTA_JOB_ACCEPTED_RESPONSE_TOPIC_FILTER),
                                                prvProcessIncomingJobMessage,
                                                NULL);

        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("MQTT unsubscribe request failed."));
        }
    }

    if (pcThingName != NULL)
    {
        mmosal_free(pcThingName);
        pcThingName = NULL;
    }

    mmosal_task_delete(NULL);
}

/**
 * Suspends the OTA update
 */
void vSuspendOTAUpdate(void)
{
    if ((OTA_GetState() != OtaAgentStateSuspended) && (OTA_GetState() != OtaAgentStateStopped))
    {
        OTA_Suspend();

        while ((OTA_GetState() != OtaAgentStateSuspended) &&
               (OTA_GetState() != OtaAgentStateStopped))
        {
            mmosal_task_sleep(otaexampleTASK_DELAY_MS);
        }
    }
}

/**
 * Resumes OTA update
 */
void vResumeOTAUpdate(void)
{
    if (OTA_GetState() == OtaAgentStateSuspended)
    {
        OTA_Resume();

        while (OTA_GetState() == OtaAgentStateSuspended)
        {
            mmosal_task_sleep(otaexampleTASK_DELAY_MS);
        }
    }
}

void start_ota_update_task(ota_preupdate_cb_fn_t preupdate_cb, ota_postupdate_cb_fn_t postupdate_cb)
{
    /* Setup callbacks */
    ota_pal_preupdate_callback = preupdate_cb;
    ota_task_postupdate_callback = postupdate_cb;

    mmosal_task_create(vOTAUpdateTask, NULL, MMOSAL_TASK_PRI_LOW, 512, "OTAUpdateTask");
}
