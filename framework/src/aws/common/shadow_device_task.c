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
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file
 * This file implements the shadow device task.
 * This task is responsible for publishing and subscribing to topics using the AWS Shadow
 * service. It also handles incoming MQTT messages from the AWS Shadow service. This version
 * of the Device Shadow API provides macros and helper functions for assembling MQTT topics
 * strings, and for determining whether an incoming MQTT message is related to the
 * device shadow.
 *
 * This example assumes there is a @c powerOn state in the device shadow. It does the
 * following operations:
 * 1. Assemble strings for the MQTT topics of device shadow, by using macros defined by the Device Shadow library.
 * 2. Subscribe to those MQTT topics using the MQTT Agent.
 * 3. Register callbacks for incoming shadow topic publishes with the subscription_manager.
 * 3. Publish to report the current state of @c powerOn.
 * 5. Check if @c powerOn has been changed and send an update if so.
 * 6. If a publish to update reported state was sent, wait until either
 *    @c prvIncomingPublishUpdateAcceptedCallback or @c prvIncomingPublishUpdateRejectedCallback
 *    handle the response.
 * 7. Wait until time for next check and repeat from step 5.
 *
 * Meanwhile, when @c prvIncomingPublishUpdateDeltaCallback receives changes to the shadow state,
 * it will apply them on the device.
 */

/* Standard includes. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* MQTT library includes. */
#include "core_mqtt_agent.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* Shadow API header. */
#include "shadow.h"

/* App includes */
#include "shadow_device_task.h"

/* Morse includes. */
#include "mmosal.h"
#include "mmhal.h"
#include "mmconfig.h"

/**
 * The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define MAX_COMMAND_SEND_BLOCK_TIME_MS  (60 * 1000)

/**
 * Maximum length allocated for the MQTT topic
 */
#define MAX_TOPIC_LEN                   128

/**
 * Mutex to serialise calls to @c aws_publish_shadow().
 */
static struct mmosal_mutex *publish_mutex = NULL;

/**
 * The thing name.
 */
static char *pcThingName = NULL;

/**
 * Subscribe to the the specified device shadow topic.
 *
 * @return true if the subscribe is successful; false otherwise.
 */
static bool prvSubscribeToTopic(const char* pcShadowName,
                                 ShadowTopicStringType_t topic,
                                 IncomingPubCallback_t callback,
                                 void *ctx)
{
    int xStatus = MQTTSuccess;
    char TopicBuffer[MAX_TOPIC_LEN];
    uint16_t TopicLen;
    MQTTAgentHandle_t xAgentHandle = xGetMqttAgentHandle();

    /* strlen does not like NULL even though it is supposed to accept it,
     * so convert NULL's to empty string */
    if (pcShadowName == NULL)
    {
        pcShadowName = "";
    }

    xStatus = Shadow_AssembleTopicString(topic,
                                         pcThingName,
                                         strlen(pcThingName),
                                         pcShadowName,
                                         strlen(pcShadowName),
                                         TopicBuffer,
                                         MAX_TOPIC_LEN,
                                         &TopicLen);
    if (xStatus == MQTTSuccess)
    {
        xStatus = MqttAgent_SubscribeSync(xAgentHandle,
                                         TopicBuffer,
                                         TopicLen,
                                         MQTTQoS1,
                                         callback,
                                         ctx);
    }

    if (xStatus != MQTTSuccess)
    {
        printf("ERR:Failed to subscribe to topic: %s\n", TopicBuffer);
        return false;
    }

    return true;
}

/**
 * Unsubscribe to the the specified device shadow topic.
 *
 * @return true if the subscribe is successful; false otherwise.
 */
static bool prvUnSubscribeToTopic(const char* pcShadowName,
                                 ShadowTopicStringType_t topic,
                                 IncomingPubCallback_t callback,
                                 void *ctx)
{
    int xStatus = MQTTSuccess;
    char TopicBuffer[MAX_TOPIC_LEN];
    uint16_t TopicLen;
    MQTTAgentHandle_t xAgentHandle = xGetMqttAgentHandle();

    /* strlen does not like NULL even though it is supposed to accept it,
     * so convert NULL's to empty string */
    if (pcShadowName == NULL)
    {
        pcShadowName = "";
    }

    xStatus = Shadow_AssembleTopicString(topic,
                                         pcThingName,
                                         strlen(pcThingName),
                                         pcShadowName,
                                         strlen(pcShadowName),
                                         TopicBuffer,
                                         MAX_TOPIC_LEN,
                                         &TopicLen);

    if (xStatus == MQTTSuccess)
    {
        xStatus = MqttAgent_UnSubscribeSync(xAgentHandle,
                                            TopicBuffer,
                                            TopicLen,
                                            callback,
                                            ctx);
    }

    if (xStatus != MQTTSuccess)
    {
        printf("ERR:Failed to unsubscribe to topic: %s\n", TopicBuffer);
        return false;
    }

    return true;
}

/**
 * The callback to execute when there is an incoming publish on the
 * topic for delta updates.
 *
 * @param pvCtx          The context passed when subscribing to this topic.
 * @param pxPublishInfo  The published data structure
 */
static void prvIncomingPublishUpdateDeltaCallback(void * pvCtx,
                                                  MQTTPublishInfo_t * pxPublishInfo)
{
    MMOSAL_ASSERT(pvCtx != NULL);
    MMOSAL_ASSERT(pxPublishInfo != NULL);
    MMOSAL_ASSERT(pxPublishInfo->pPayload != NULL);

    shadow_update_cb_fn_t pUserCallback = (shadow_update_cb_fn_t) pvCtx;
    pUserCallback((char*)pxPublishInfo->pPayload, pxPublishInfo->payloadLength, UPDATE_DELTA);
}

/**
 * The callback to execute when there is an incoming publish on the
 * topic for accepted messages.
 *
 * @param pvCtx          The context passed when subscribing to this topic.
 * @param pxPublishInfo  The published data structure
 */
static void prvIncomingPublishUpdateAcceptedCallback(void * pvCtx,
                                                      MQTTPublishInfo_t * pxPublishInfo)
{
    MMOSAL_ASSERT(pvCtx != NULL);
    MMOSAL_ASSERT(pxPublishInfo != NULL);
    MMOSAL_ASSERT(pxPublishInfo->pPayload != NULL);

    shadow_update_cb_fn_t pUserCallback = (shadow_update_cb_fn_t) pvCtx;
    pUserCallback((char*)pxPublishInfo->pPayload, pxPublishInfo->payloadLength, UPDATE_ACCEPTED);
}


/**
 * The callback to execute when there is an incoming publish on the
 * topic for rejected messages.
 *
 * @param pvCtx          The context passed when subscribing to this topic.
 * @param pxPublishInfo  The published data structure
 */
static void prvIncomingPublishUpdateRejectedCallback(void * pvCtx,
                                                     MQTTPublishInfo_t * pxPublishInfo)
{
    MMOSAL_ASSERT(pvCtx != NULL);
    MMOSAL_ASSERT(pxPublishInfo != NULL);
    MMOSAL_ASSERT(pxPublishInfo->pPayload != NULL);

    shadow_update_cb_fn_t pUserCallback = (shadow_update_cb_fn_t) pvCtx;
    pUserCallback((char*)pxPublishInfo->pPayload, pxPublishInfo->payloadLength, UPDATE_REJECTED);
}

/*-----------------------------------------------------------*/

/**
 * Publishes to the shadow update topic: @c "$aws/things/thingName/shadow/update"
 *
 * @param pcShadowName Name of the shadow, NULL or empty string for classic shadow.
 * @param json         The JSON document to publish.
 * @return             true on success.
 */
bool aws_publish_shadow(char *pcShadowName, char* json)
{
    int xStatus;

    /* The following are static as @c MQTTAgent_Publish passes the pointers to the
     * MQTTAgent task - so they need to be valid even after we exit this function. */
    static MQTTPublishInfo_t xPublishInfo;
    static MQTTAgentCommandInfo_t xCommandParams;
    static char TopicBuffer[MAX_TOPIC_LEN];
    static char PayloadBuffer[MAX_JSON_LEN];

    if (strlen(json) >= MAX_JSON_LEN)
    {
        return false;
    }

    /* strlen does not like NULL even though it is supposed to accept it,
     * so convert NULL's to empty string */
    if (pcShadowName == NULL)
    {
        pcShadowName = "";
    }

    uint16_t TopicLen;
    MQTTAgentHandle_t xAgentHandle = xGetMqttAgentHandle();

    bool ret = false;

    /* Create mutex on first use */
    if (publish_mutex == NULL)
    {
        publish_mutex = mmosal_mutex_create("shadow_publish_mutex");
    }

    /* Get the mutex before we access xPublishInfo */
    mmosal_mutex_get(publish_mutex, UINT32_MAX);

    xStatus = Shadow_AssembleTopicString(ShadowTopicStringTypeUpdate,
                                         pcThingName,
                                         strlen(pcThingName),
                                         pcShadowName,
                                         strlen(pcShadowName),
                                         TopicBuffer,
                                         MAX_TOPIC_LEN,
                                         &TopicLen);

    if (xStatus == MQTTSuccess)
    {
        /* Copy the payload to the static buffer */
        memset(PayloadBuffer, 0, sizeof(PayloadBuffer));
        strncpy(PayloadBuffer, json, MAX_JSON_LEN - 1);

        /* Set up MQTTPublishInfo_t for the update reports. */
        memset(&xPublishInfo, 0, sizeof(xPublishInfo));
        xPublishInfo.qos = MQTTQoS1;
        xPublishInfo.pTopicName = TopicBuffer;
        xPublishInfo.topicNameLength = TopicLen;
        xPublishInfo.pPayload = PayloadBuffer;
        xPublishInfo.payloadLength = strlen(PayloadBuffer);

        /* Set up the MQTTAgentCommandInfo_t for the publish.
        * We do not need a completion callback here since for publishes, we expect to get a
        * response on the appropriate topics for accepted or rejected reports, and for pings
        * we do not care about the completion. */
        xCommandParams.blockTimeMs = MAX_COMMAND_SEND_BLOCK_TIME_MS;
        xCommandParams.cmdCompleteCallback = NULL;
        xCommandParams.pCmdCompleteCallbackContext = NULL;

        xStatus = MQTTAgent_Publish(xAgentHandle,
                                    &xPublishInfo,
                                    &xCommandParams);
        if (xStatus == MQTTSuccess)
        {
            ret = true;
        }
    }

    /* Release the mutex */
    mmosal_mutex_release(publish_mutex);

    return ret;
}

/**
 * Creates the AWS Shadow device task for the specified shadow.
 * You may call this multiple times for multiple shadows.
 *
 * @param pcShadowName        Name of the named shadow, pass NULL if using classic shadow.
 * @param pfnUpdateCallback   Callback for shadow updates.
 * @return                    true on success.
 */
bool aws_create_shadow(char *pcShadowName, shadow_update_cb_fn_t pfnUpdateCallback)
{
    bool bStatus = true;

    /* Wait for MqttAgent to be ready. */
    vSleepUntilMQTTAgentReady();

    /* Wait for first mqtt connection */
    vSleepUntilMQTTAgentConnected();

    /* Fetch thing name from config store on first run */
    if (pcThingName == NULL)
    {
        mmconfig_alloc_and_load(AWS_KEY_THING_NAME, (void**) &pcThingName);
    }

    if (pcThingName == NULL)
    {
        printf("Could not find key for thing name in config store.\n");
        printf("Please refer to the user documentation for instructions\n"
                "on configuring the device for this application.\n");
        return false;
    }

    /* Subscribe to Shadow topics. */
    bStatus &= prvSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateDelta,
                                   prvIncomingPublishUpdateDeltaCallback, (void*)pfnUpdateCallback);
    bStatus &= prvSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateAccepted,
                                   prvIncomingPublishUpdateAcceptedCallback, (void*)pfnUpdateCallback);
    bStatus &= prvSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateRejected,
                                   prvIncomingPublishUpdateRejectedCallback, (void*)pfnUpdateCallback);

    return bStatus;
}

/**
 * Releases the AWS Shadow resources for the specified shadow.
 *
 * @param pcShadowName       Name of the named shadow, pass NULL if using classic shadow.
 * @param pfnUpdateCallback  Callback for shadow updates, required to match the correct resource.
 * @return                   true on success.
 */
void aws_close_shadow(char *pcShadowName, shadow_update_cb_fn_t pfnUpdateCallback)
{
    /* Unsubscribe to Shadow topics. */
    prvUnSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateDelta,
                          prvIncomingPublishUpdateDeltaCallback, (void*)pfnUpdateCallback);
    prvUnSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateAccepted,
                          prvIncomingPublishUpdateAcceptedCallback, (void*)pfnUpdateCallback);
    prvUnSubscribeToTopic(pcShadowName, ShadowTopicStringTypeUpdateRejected,
                          prvIncomingPublishUpdateRejectedCallback, (void*)pfnUpdateCallback);
}
