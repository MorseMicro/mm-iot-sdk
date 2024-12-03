/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

/*
 * The Fleet Provisioning library provides macros and helper functions for
 * assembling MQTT topics strings, and for determining whether an incoming MQTT
 * message is related to the Fleet Provisioning API of AWS IoT Core. This demo
 * uses the coreMQTT library. This demo requires using the AWS IoT Core broker
 * as Fleet Provisioning is an AWS IoT Core feature.
 *
 * This demo provisions a device certificate using the provisioning by claim
 * workflow with a Certificate Signing Request (CSR). The demo connects to AWS
 * IoT Core using provided claim credentials (whose certificate needs to be
 * registered with IoT Core before running this demo), subscribes to the
 * CreateCertificateFromCsr topics, and obtains a certificate. It then
 * subscribes to the RegisterThing topics and activates the certificate and
 * obtains a Thing using the provisioning template. Finally, it reconnects to
 * AWS IoT Core using the new credentials.
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Morse includes. */
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"

/* TinyCBOR library for CBOR encoding and decoding operations. */
#include "cbor.h"

/* mbedTLS includes. */
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

/* MQTT library includes */
#include "core_mqtt_agent.h"
#include "subscription_manager.h"

/* AWS IoT Fleet Provisioning Library. */
#include "fleet_provisioning.h"
#include "fleet_provisioning_task.h"
#include "fleet_provisioning_serializer.h"

/* The maximum topic length we support */
#define TOPIC_BUFFER_LENGTH                 256

/**
 * Subject name to use when creating the certificate signing request
 * for provisioning the demo client using the Fleet Provisioning
 * @c CreateCertificateFromCsr API.
 *
 * This is passed to @c MbedTLS; see
 * @c https://tls.mbed.org/api/x509__csr_8h.html#a954eae166b125cea2115b7db8c896e90
 */
#ifndef CSR_SUBJECT_NAME
    #define CSR_SUBJECT_NAME    "CN=Fleet Provisioning Demo"
#endif

/**
 * @brief Size of AWS IoT Thing name buffer.
 *
 * See @c https://docs.aws.amazon.com/iot/latest/apireference/API_CreateThing.html#iot-CreateThing-request-thingName
 */
#define MAX_THING_NAME_LENGTH                128

/**
 * Size of serial number string.
 */
#define MAX_DEVICE_SERIAL_LENGTH                 20

/**
 * @brief Size of buffer in which to hold the certificate.
 */
#define CERT_BUFFER_LENGTH                             1024

/**
 * @brief Size of buffer in which to hold the certificate id.
 *
 * See @c https://docs.aws.amazon.com/iot/latest/apireference/API_Certificate.html#iot-Type-Certificate-certificateId
 */
#define CERT_ID_BUFFER_LENGTH                          64

/**
 * @brief Size of buffer in which to hold the certificate ownership token.
 */
#define OWNERSHIP_TOKEN_BUFFER_LENGTH                  512

/**
 * @brief Size of working payload buffer.
 */
#define PAYLOAD_BUFFER_SIZE                            1024

/**
 * The maximum time to wait for an MQTT operation to be complete.
 * This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE
 * and non QOS0 publishes.
 */
#define MQTT_TIMEOUT_MS                                10000

/**
 * @brief Status values of the Fleet Provisioning response.
 */
typedef enum
{
    ResponseNotReceived,
    ResponseCSRAccepted,
    ResponseCSRRejected,
    ResponseProvAccepted,
    ResponseProvRejected
} ResponseStatus_t;


/*-----------------------------------------------------------*/

/** Pointer to provisioning task */
static struct mmosal_task *provisioning_task = NULL;

/** Status reported from the MQTT publish callback */
static ResponseStatus_t xResponseStatus;

/** Buffer to hold responses received from the AWS IoT Fleet Provisioning API */
static uint8_t* pucPayloadBuffer = NULL;

/** Buffer to generate the CSR and receive the signed certificate in */
static char* certificate_buffer = NULL;

/** Buffer to hold the generated private key in till its ready to be provisioned */
static char* private_key_buffer = NULL;

/** Buffer to hold the generated private key in till its ready to be provisioned */
static char* thing_name_buffer = NULL;

/** Buffer to hold the register thing accepted topic */
static char* register_thing_accepted_topic = NULL;
static uint16_t register_thing_accepted_topic_len = 0;

/** Buffer to hold the register thing rejected topic */
static char* register_thing_rejected_topic = NULL;
static uint16_t register_thing_rejected_topic_len = 0;

/** Buffer to hold the register thing publish topic */
static char* register_thing_publish_topic = NULL;
static uint16_t register_thing_publish_topic_len = 0;

/** Buffer to hold our serial number */
static char device_serial[MAX_DEVICE_SERIAL_LENGTH] = { 0 };

static size_t certificate_len = 0;

/** Buffer for holding the certificate ID. */
static char certificateId[ CERT_ID_BUFFER_LENGTH ];
static size_t certificateIdLength = 0;

/** Buffer for holding the certificate ownership token. */
static char ownershipToken[ OWNERSHIP_TOKEN_BUFFER_LENGTH ];
static size_t ownershipTokenLength = 0;

/*-----------------------------------------------------------*/

/**
 * @brief Subscribe to the @c CreateCertificateFromCsr accepted and rejected topics.
 */
static bool prvSubscribeToCsrResponseTopics(void);

/**
 * @brief Unsubscribe from the @c CreateCertificateFromCsr accepted and rejected topics.
 */
static bool prvUnsubscribeFromCsrResponseTopics(void);

/**
 * @brief Subscribe to the @c RegisterThing accepted and rejected topics.
 */
static bool prvSubscribeToRegisterThingResponseTopics(void);

/**
 * @brief Unsubscribe from the @c RegisterThing accepted and rejected topics.
 */
static bool prvUnsubscribeFromRegisterThingResponseTopics(void);

static bool fpMQTTPublish(const char * const pacTopic, uint16_t topicLen,
                   const char * pMsg, uint32_t msgSize)
{
    MQTTStatus_t mqttStatus = MQTTBadParameter;
    MQTTAgentHandle_t xMQTTAgentHandle = xGetMqttAgentHandle();

    /* Static as we are passing pointers to these structures */
    static MQTTPublishInfo_t publishInfo;
    static MQTTAgentCommandInfo_t xCommandParams;

    memset(&publishInfo, 0, sizeof(publishInfo));
    memset(&xCommandParams, 0, sizeof(xCommandParams));

    publishInfo.pTopicName = pacTopic;
    publishInfo.topicNameLength = topicLen;
    publishInfo.qos = MQTTQoS1;
    publishInfo.pPayload = pMsg;
    publishInfo.payloadLength = msgSize;

    xCommandParams.blockTimeMs = MQTT_TIMEOUT_MS;
    xCommandParams.cmdCompleteCallback = NULL;
    xCommandParams.pCmdCompleteCallbackContext = NULL;

    if (xMQTTAgentHandle == NULL)
    {
        return false;
    }

    mqttStatus = MQTTAgent_Publish(xMQTTAgentHandle, &publishInfo, &xCommandParams);

    return (mqttStatus == MQTTSuccess);
}

/**
 * Callback to receive the incoming publish messages from the MQTT
 * broker. Sets @c xResponseStatus if the correct response was received.
 *
 * @param[in] pxPublishInfo Pointer to publish info of the incoming publish.
 * @param[in] usPacketIdentifier Packet identifier of the incoming publish.
 */
static void prvProcessCsrResponseAccepted(void * pxSubscriptionContext,
                                          MQTTPublishInfo_t * pxPublishInfo)
{
    bool result = true;

    (void)pxSubscriptionContext;

    LogInfo(("Received accepted response from CreateCertificateFromCsr API."));

    /* From the response, extract the certificate, certificate ID, and
     * certificate ownership token. Returned strings are NULL terminated. */
    certificate_len = CERT_BUFFER_LENGTH;
    certificateIdLength = CERT_ID_BUFFER_LENGTH;
    ownershipTokenLength = OWNERSHIP_TOKEN_BUFFER_LENGTH;
    result = parseCsrResponse((const uint8_t*)pxPublishInfo->pPayload,
                                pxPublishInfo->payloadLength,
                                certificate_buffer,
                                &certificate_len,
                                certificateId,
                                &certificateIdLength,
                                ownershipToken,
                                &ownershipTokenLength);

    if (result)
    {
        LogInfo(("Received certificate with Id: %s", certificateId));
        xResponseStatus = ResponseCSRAccepted;
    }
    mmosal_task_notify(provisioning_task);
}

/**
 * Callback if the subscription was rejected by the MQTT broker.
 * Sets @c xResponseStatus to indicate failure.
 *
 * @param[in] pxPublishInfo Pointer to publish info of the incoming publish.
 * @param[in] usPacketIdentifier Packet identifier of the incoming publish.
 */
static void prvProcessCsrResponseRejected(void * pxSubscriptionContext,
                                          MQTTPublishInfo_t * pxPublishInfo)
{
    (void)pxSubscriptionContext;
    (void)pxPublishInfo;

    LogError(("Received rejected response from CreateCertificateFromCsr API."));

    xResponseStatus = ResponseCSRRejected;
    mmosal_task_notify(provisioning_task);
}

/**
 * Callback to receive the incoming publish messages from the MQTT
 * broker. Sets @c xResponseStatus if the correct response was received.
 *
 * @param[in] pxPublishInfo Pointer to publish info of the incoming publish.
 * @param[in] usPacketIdentifier Packet identifier of the incoming publish.
 */
static void prvProcessRegisterThingAccepted(void * pxSubscriptionContext,
                                            MQTTPublishInfo_t * pxPublishInfo)
{
    bool result = true;

    (void)pxSubscriptionContext;

    LogInfo(("Received accepted response from Fleet Provisioning RegisterThing API."));

    /* Extract the Thing name from the response. */
    size_t xThingNameLength = MAX_THING_NAME_LENGTH;
    result = parseRegisterThingResponse((const uint8_t*)pxPublishInfo->pPayload,
                                        pxPublishInfo->payloadLength,
                                        thing_name_buffer,
                                        &xThingNameLength);

    if (result)
    {
        LogInfo(("Received AWS IoT Thing name: %s", thing_name_buffer));
        xResponseStatus = ResponseProvAccepted;
    }
    mmosal_task_notify(provisioning_task);
}

/**
 * Callback if the subscription was rejected by the MQTT broker.
 * Sets @c xResponseStatus to indicate failure.
 *
 * @param[in] pxPublishInfo Pointer to publish info of the incoming publish.
 * @param[in] usPacketIdentifier Packet identifier of the incoming publish.
 */
static void prvProcessRegisterThingRejected(void * pxSubscriptionContext,
                                            MQTTPublishInfo_t * pxPublishInfo)
{
    (void)pxSubscriptionContext;
    (void)pxPublishInfo;

    LogError(("Received rejected response from Fleet Provisioning RegisterThing API."));

    xResponseStatus = ResponseProvRejected;
    mmosal_task_notify(provisioning_task);
}

/*-----------------------------------------------------------*/

/**
 * Subscribe to the @c CreateCertificateFromCsr accepted and rejected topics.
 */
static bool prvSubscribeToCsrResponseTopics(void)
{
    MQTTStatus_t xMQTTStatus;
    MQTTAgentHandle_t xMQTTAgentHandle = xGetMqttAgentHandle();

    xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                          FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC,
                                          strlen(FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC),
                                          MQTTQoS1,
                                          prvProcessCsrResponseAccepted,
                                          NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
        LogError(("Failed to subscribe to fleet provisioning topic: %s.",
                  FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC));
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                              FP_CBOR_CREATE_CERT_REJECTED_TOPIC,
                                              strlen(FP_CBOR_CREATE_CERT_REJECTED_TOPIC),
                                              MQTTQoS1,
                                              prvProcessCsrResponseRejected,
                                              NULL);
        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to subscribe to fleet provisioning topic: %s.",
                      FP_CBOR_CREATE_CERT_REJECTED_TOPIC));
        }
    }

    return xMQTTStatus == MQTTSuccess;
}
/*-----------------------------------------------------------*/

/**
 * Unsubscribe from the @c CreateCertificateFromCsr accepted and rejected topics.
 */
static bool prvUnsubscribeFromCsrResponseTopics(void)
{
    MQTTStatus_t xMQTTStatus;
    MQTTAgentHandle_t xMQTTAgentHandle = xGetMqttAgentHandle();

    xMQTTStatus = MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                            FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC,
                                            strlen(FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC),
                                            prvProcessCsrResponseAccepted,
                                            NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
        LogError(("Failed to unsubscribe from fleet provisioning topic: %s.",
                  FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC));
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        xMQTTStatus = MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                                FP_CBOR_CREATE_CERT_REJECTED_TOPIC,
                                                strlen(FP_CBOR_CREATE_CERT_REJECTED_TOPIC),
                                                prvProcessCsrResponseRejected,
                                                NULL);

        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to unsubscribe from fleet provisioning topic: %s.",
                      FP_CBOR_CREATE_CERT_REJECTED_TOPIC));
        }
    }

    return xMQTTStatus == MQTTSuccess;
}
/*-----------------------------------------------------------*/

/**
 * Subscribe to the @c RegisterThing accepted and rejected topics.
 */
static bool prvSubscribeToRegisterThingResponseTopics(void)
{
    MQTTStatus_t xMQTTStatus;
    MQTTAgentHandle_t xMQTTAgentHandle = xGetMqttAgentHandle();

    xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                          register_thing_accepted_topic,
                                          register_thing_accepted_topic_len,
                                          MQTTQoS1,
                                          prvProcessRegisterThingAccepted,
                                          NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
        LogError(("Failed to subscribe to fleet provisioning topic: %s.",
                  register_thing_accepted_topic));
    }

    if (xMQTTStatus == MQTTSuccess)
    {
        xMQTTStatus =
            MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                    register_thing_rejected_topic,
                                    register_thing_rejected_topic_len,
                                    MQTTQoS1,
                                    prvProcessRegisterThingRejected,
                                    NULL);
        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to subscribe to fleet provisioning topic: %s.",
                      register_thing_rejected_topic));
        }
    }

    return xMQTTStatus == MQTTSuccess;
}
/*-----------------------------------------------------------*/

/**
 * Unsubscribe from the @c RegisterThing accepted and rejected topics.
 */
static bool prvUnsubscribeFromRegisterThingResponseTopics(void)
{
    MQTTStatus_t xMQTTStatus;
    MQTTAgentHandle_t xMQTTAgentHandle = xGetMqttAgentHandle();

    xMQTTStatus =
        MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                  register_thing_accepted_topic,
                                  register_thing_accepted_topic_len,
                                  prvProcessRegisterThingAccepted,
                                  NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
        LogError(("Failed to unsubscribe from fleet provisioning topic: %s.",
                  register_thing_accepted_topic));
    }

    if (xMQTTStatus == MQTTSuccess)
    {
    xMQTTStatus =
        MqttAgent_UnSubscribeSync(xMQTTAgentHandle,
                                  register_thing_rejected_topic,
                                  register_thing_rejected_topic_len,
                                  prvProcessRegisterThingRejected,
                                  NULL);
        if (xMQTTStatus != MQTTSuccess)
        {
            LogError(("Failed to unsubscribe from fleet provisioning topic: %s.",
                      register_thing_rejected_topic));
        }
    }

    return xMQTTStatus == MQTTSuccess;
}
/*-----------------------------------------------------------*/

bool generateKeyAndCsr(char * pCsrBuffer,
                       size_t csrBufferLength,
                       size_t * pOutCsrLength)
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_pk_context privKey;
    mbedtls_x509write_csr req;
    int32_t mbedtlsRet = -1;

    mbedtls_pk_init(&privKey);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_x509write_csr_init(&req);
    mbedtls_x509write_csr_set_md_alg(&req, MBEDTLS_MD_SHA256);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          (const unsigned char *) device_serial, strlen(device_serial));

    mbedtlsRet = mbedtls_pk_setup(&privKey, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (mbedtlsRet == 0)
    {
        mbedtlsRet = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                                         mbedtls_pk_ec(privKey),
                                         mbedtls_ctr_drbg_random,
                                         &ctr_drbg);
    }

    if (mbedtlsRet == 0)
    {
        mbedtlsRet = mbedtls_pk_write_key_pem(&privKey, (unsigned char*)private_key_buffer,
                                              CERT_BUFFER_LENGTH);
    }

    if (mbedtlsRet == 0)
    {
        mbedtlsRet = mbedtls_x509write_csr_set_key_usage(&req, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
    }

    if (mbedtlsRet == 0)
    {
        mbedtlsRet = mbedtls_x509write_csr_set_ns_cert_type(&req,
                                                            MBEDTLS_X509_NS_CERT_TYPE_SSL_CLIENT);
    }

    if (mbedtlsRet == 0)
    {
        mbedtlsRet = mbedtls_x509write_csr_set_subject_name(&req, CSR_SUBJECT_NAME);
    }

    if (mbedtlsRet == 0)
    {
        mbedtls_x509write_csr_set_key(&req, &privKey);

        mbedtlsRet = mbedtls_x509write_csr_pem(&req, (unsigned char *) pCsrBuffer, csrBufferLength,
                                               mbedtls_ctr_drbg_random, &ctr_drbg);
    }

    /* Clean up. */
    mbedtls_x509write_csr_free(&req);
    mbedtls_pk_free(&privKey);

    *pOutCsrLength = strlen(pCsrBuffer);

    return(mbedtlsRet == 0);
}

/**
 * The Fleet Provisioning task, called only when fleet provisioning is required.
 *
 * This function uses the provided claim key and certificate files to connect to
 * AWS and generate a new device key and certificate with a CSR. Once AWS signs the
 * CSR, this routine then creates a new Thing with the Fleet Provisioning API and the
 * newly-created credentials. The task disconnects and reboots when it has successfully
 * created and provisioned the thing using the newly generated key/cert. The application
 * can then connect and proceed as normal with the newly created credentials at the next
 * boot.
 */
void fleet_provisioning_task(void * pvParameters)
{
    /* Not used */
    (void) pvParameters;

    /* FreeRTOS APIs return status. */
    bool xResult = false;
    size_t xPayloadLength = 0;
    size_t xCsrLength;

    MQTTAgentHandle_t xMQTTAgentHandle = NULL;

    /* Allocate buffers */
    pucPayloadBuffer   = (unsigned char*) mmosal_malloc(PAYLOAD_BUFFER_SIZE);
    certificate_buffer = (char*) mmosal_malloc(CERT_BUFFER_LENGTH);
    private_key_buffer = (char*) mmosal_malloc(CERT_BUFFER_LENGTH);
    thing_name_buffer  = (char*) mmosal_malloc(MAX_THING_NAME_LENGTH);

    /* Read provisioning template name - we know this key exists*/
    char provisioning_template[FP_TEMPLATENAME_MAX_LENGTH];
    mmconfig_read_string(AWS_KEY_PROVISIONING_TEMPLATE, provisioning_template,
                         FP_TEMPLATENAME_MAX_LENGTH);

    /* Setup topics */
    register_thing_accepted_topic = (char*) mmosal_malloc(TOPIC_BUFFER_LENGTH);
    MMOSAL_ASSERT(register_thing_accepted_topic);
    FleetProvisioning_GetRegisterThingTopic(register_thing_accepted_topic, TOPIC_BUFFER_LENGTH,
                                            FleetProvisioningCbor, FleetProvisioningAccepted,
                                            provisioning_template, strlen(provisioning_template),
                                            &register_thing_accepted_topic_len);

    register_thing_rejected_topic = (char*) mmosal_malloc(TOPIC_BUFFER_LENGTH);
    MMOSAL_ASSERT(register_thing_rejected_topic);
    FleetProvisioning_GetRegisterThingTopic(register_thing_rejected_topic, TOPIC_BUFFER_LENGTH,
                                            FleetProvisioningCbor, FleetProvisioningRejected,
                                            provisioning_template, strlen(provisioning_template),
                                            &register_thing_rejected_topic_len);

    register_thing_publish_topic = (char*) mmosal_malloc(TOPIC_BUFFER_LENGTH);
    MMOSAL_ASSERT(register_thing_publish_topic);
    FleetProvisioning_GetRegisterThingTopic(register_thing_publish_topic, TOPIC_BUFFER_LENGTH,
                                            FleetProvisioningCbor, FleetProvisioningPublish,
                                            provisioning_template, strlen(provisioning_template),
                                            &register_thing_publish_topic_len);


    LogInfo(("Waiting until MQTT Agent is connected."));

    /* Get our serial number */
    uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
    MMOSAL_ASSERT(mmwlan_get_mac_addr(mac_addr) == MMWLAN_SUCCESS);
    snprintf(device_serial, sizeof(device_serial), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    /* Wait for first mqtt connection */
    vSleepUntilMQTTAgentConnected();
    xMQTTAgentHandle = xGetMqttAgentHandle();

    if (xMQTTAgentHandle != NULL)
    {
        /* Subscribe to the CreateCertificateFromCsr accepted and rejected
         * topics. In this demo we use CBOR encoding for the payloads,
         * so we use the CBOR variants of the topics. */
        xResult = prvSubscribeToCsrResponseTopics();
        if (xResult == false)
        {
            LogError(("Failed to prvSubscribeToCsrResponseTopics()"));
        }
    }

    if (xResult)
    {
        xResult = generateKeyAndCsr(certificate_buffer, CERT_BUFFER_LENGTH,
                                    &xCsrLength);
        if (xResult == false)
        {
            LogError(("Failed to generateKeyAndCsr()"));
        }
    }

    if (xResult)
    {
        /* We use the CreateCertificatefromCsr API to obtain a client certificate
         * for a key on the device by means of sending a certificate signing
         * request (CSR). */
        xResult = generateCsrRequest(pucPayloadBuffer,
                                     PAYLOAD_BUFFER_SIZE,
                                     certificate_buffer,
                                     xCsrLength,
                                     &xPayloadLength);
        if (xResult == false)
        {
            LogError(("Failed to generateCsrRequest()"));
        }
    }

    if (xResult)
    {
        /* Publish the CSR to the CreateCertificatefromCsr API. */
        xResult = fpMQTTPublish(FP_CBOR_CREATE_CERT_PUBLISH_TOPIC,
                                FP_CBOR_CREATE_CERT_PUBLISH_LENGTH,
                                (char*) pucPayloadBuffer,
                                xPayloadLength);

        if (xResult == false)
        {
            LogError(("Failed to publish to fleet provisioning topic: %s.",
                     FP_CBOR_CREATE_CERT_PUBLISH_TOPIC));
        }
    }

    /* Wait for step to complete */
    mmosal_task_wait_for_notification(UINT32_MAX);

    /* Unsubscribe from topics. */
    prvUnsubscribeFromCsrResponseTopics();

    if (xResponseStatus == ResponseCSRAccepted)
    {
        /* Subscribe to the RegisterThing response topics. */
        xResult = prvSubscribeToRegisterThingResponseTopics();
        if (xResult == false)
        {
            LogError(("Failed to prvSubscribeToRegisterThingResponseTopics()"));
        }

        /* We then use the RegisterThing API to activate the received certificate,
        * provision AWS IoT resources according to the provisioning template, and
        * receive device configuration. */
        xResult = generateRegisterThingRequest(pucPayloadBuffer,
                                               PAYLOAD_BUFFER_SIZE,
                                               ownershipToken,
                                               ownershipTokenLength,
                                               device_serial,
                                               strlen(device_serial),
                                               &xPayloadLength);

        if (xResult)
        {
            /* Publish the RegisterThing request. */
            xResult = fpMQTTPublish(register_thing_publish_topic,
                                    register_thing_publish_topic_len,
                                    (char*) pucPayloadBuffer,
                                    xPayloadLength);

            if (xResult == false)
            {
                LogError(("Failed to publish to fleet provisioning topic: %s.",
                          register_thing_publish_topic));
            }
        }

        /* Wait for step to complete */
        mmosal_task_wait_for_notification(UINT32_MAX);

        /* Unsubscribe from topics. */
        prvUnsubscribeFromRegisterThingResponseTopics();
    }

    if (xResponseStatus == ResponseProvAccepted)
    {
        /* Provisioning was a success, write certificates and reboot */
        struct mmconfig_update_node certificate_node, privatekey_node, thingname_node;

        certificate_node.key  = AWS_KEY_DEVICE_CERTIFICATE;
        certificate_node.data = (void *) certificate_buffer;
        certificate_node.size = strlen(certificate_buffer) + 1;
        certificate_node.next = &privatekey_node;

        privatekey_node.key  = AWS_KEY_DEVICE_KEYS;
        privatekey_node.data = (void *) private_key_buffer;
        privatekey_node.size = strlen(private_key_buffer) + 1;
        privatekey_node.next = &thingname_node;

        thingname_node.key  = AWS_KEY_THING_NAME;
        thingname_node.data = (void *) thing_name_buffer;
        thingname_node.size = strlen(thing_name_buffer) + 1;
        thingname_node.next = NULL;

        /* This call will atomically write all the keys above. The config store is guaranteed
        * to accept all the changes on success or reject all the changes on failure. */
        mmconfig_write_update_node_list(&certificate_node);

        /* Reset device to start wuth new credentials */
        mmhal_reset();
    }

    /* Free memory */
    mmosal_free((void*)pucPayloadBuffer);
    mmosal_free((void*)certificate_buffer);
    mmosal_free((void*)private_key_buffer);
    mmosal_free((void*)thing_name_buffer);
    mmosal_free((void*)register_thing_publish_topic);
    mmosal_free((void*)register_thing_accepted_topic);
    mmosal_free((void*)register_thing_rejected_topic);
}

/**
 * This function provisions the device and resets. Returns only on failure.
 *
 * @note This function will replace the claim credentials in @c aws.devicecert and
 * @c aws.devicekeys with the newly generated credentials. To re-trigger the provisioning
 * process, the application may either restore the claim certificate and key from a saved
 * location or just reuse the device certificate and key for the new provisioning process
 * provided the device policy on AWS has permissions for the @c create-from-csr and
 * @c provisioning-templates resources.
 */
void do_fleet_provisioning(void)
{
    /* Start fleet provisioning in its own task due to stack requirements */
    provisioning_task = mmosal_task_create(fleet_provisioning_task,
                                        NULL,
                                        MMOSAL_TASK_PRI_LOW,
                                        2048,
                                        "FleetProv");
    /* Wait till completion, should not return on success */
    mmosal_task_join(provisioning_task);

    /* If we got here, then we failed to provision */
}
