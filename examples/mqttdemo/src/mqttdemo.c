/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MQTT example to demonstrate connecting to an MQTT broker, subscribing to a topic
 *        and publishing to a topic.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * @ref mqttdemo.c is an example application that demonstrates how to use coreMQTT to connect
 * to an MQTT broker and publish/subscribe to messages.  This example was written based on the
 * tutorial at https://www.freertos.org/mqtt/basic-mqtt-example.html.
 *
 * In this example we attempt to connect to the public MQTT broker at @c test.mosquitto.org
 * on port 1883.  This example connects on the clear, but can be easily converted to use TLS
 * by supplying @c NetworkCredentials_t to @c transport_connect().
 *
 * @note This example is single threaded and so publishes & subscribes in lock step in a
 * single thread. If asynchronous publishes/subscribes are required, then this code will
 * have to be modified to use a message queue and a single thread to handle the MQTT
 * publishes/subscribes. Alternatively you may use the coreMQTT-Agent module which does
 * this for you.
 *
 * # Getting Started
 *
 * ## Using the app only
 *
 * Simply compiling and running the application will show the application connecting to
 * the public MQTT broker and then publishing a message to @c "/MorseMicro/<clientid>/topic"
 * The @c clientid is unique to each device and in this example is based on the MAC address
 * of the device.
 *
 * The device also subscribes to the same topic, so if all is well you should see the
 * message _G'day World_ being displayed on the console.
 *
 * @note This application requires internet connectivity and DNS to connect to the public
 * MQTT broker.  Ensure your access point has DHCP and internet connectivity enabled.
 *
 * ## Verifying the result with a third party MQTT Client
 *
 * You can use a third party MQTT Client such as Mosquitto to independently verify the
 * working of the application.  You may download Mosquitto from
 * @c https://mosquitto.org/download/
 *
 * The above link also includes the Mosquitto MQTT server, which you may use to create
 * a private MQTT broker for your application.
 *
 * Run the mosquitto client using the following command:
 * @code
 * mosquitto_sub -h test.mosquitto.org -p 1883 -t /MorseMicro/<clientid>/topic
 * @endcode
 * Replace @c "<clientid>" with the client id of your device as displayed on the console.
 *
 * Now run the application.  You should see the message _G'day World!_ appear on the
 * MQTT client which confirms that the application has successfully connected to the
 * MQTT broker and published a message.
 *
 * @note You may use @c mosquitto_pub to publish messages too.  Documentation for
 * @c mosquitto_sub is here: @c https://mosquitto.org/man/mosquitto_sub-1.html. And
 * documentation for mosquitto_pub is here: @c https://mosquitto.org/man/mosquitto_pub-1.html.
 *
 * ## Configuration
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
*/

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmipal.h"
#include "mbedtls/build_info.h"
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "core_mqtt.h"
#include "mm_app_common.h"

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique to ensure no two clients connecting to the same broker use the
 * same client identifier.
 */
#define CLIENT_ID_PREFIX                "MM_Client_%s"

/** @brief Broker address to connect to.  This is a public test server and is not
 * guaranteed to be always available.  For production applications or extensive
 * testing you should setup your own server.  Mosquitto is one such free MQTT
 * server that you can use.
 */
#define MQTT_BROKER_ENDPOINT            "test.mosquitto.org"

/** @brief Broker port.  Usually 1883 is used for communications in the clear
 * and 8883 is used for TLS encrypted communications.
 */
#define MQTT_BROKER_PORT                1883


/** @brief Keep alive Delay */
#define KEEP_ALIVE_TIMEOUT_SECONDS      60
/** @brief Receive timeout */
#define CONNACK_RECV_TIMEOUT_MS         1000

/**
 * @brief Delay in ms between publishes
 * @note This is a single threaded demo, so increasing this delay will cause the
 *       application to sleep for extended periods of time and not respond to other messages.
 */
#define DELAY_BETWEEN_PUBLISHES         1000

/** @brief Number of topics we subscribe to */
#define TOPIC_COUNT                     1

/** @brief Topic to publish/subscribe, we include the client ID to keep it unique. */
#define TOPIC_FORMAT                    "/MorseMicro/%s/topic"

/** @brief Message to publish/subscribe */
#define EXAMPLE_MESSAGE                 "G'day World!"
/** Length of MAC address string (i.e., "XX:XX:XX:XX:XX:XX") including terminator. */
#define MAC_ADDR_STR_LEN    (18)

/** Statically allocated buffer for MQTT */
static unsigned char buf[1024];


/**
 * @brief This callback gets called when a published message matches
 *        one of our subscribed topics.
 * @param pxPublishInfo The received message
 */
static void MQTTProcessIncomingPublish(MQTTPublishInfo_t * pxPublishInfo)
{
    /* Strings are not zero terminated, so we need to explicitly copy and terminate them */
    static char tmptopic[80];
    static char tmppayload[128];
    size_t topic_name_length;
    size_t payload_length;

    topic_name_length = pxPublishInfo->topicNameLength;
    if (topic_name_length >= sizeof(tmptopic))
    {
        topic_name_length = sizeof(tmptopic) - 1;
    }
    strncpy(tmptopic, pxPublishInfo->pTopicName, topic_name_length);
    tmptopic[topic_name_length] = '\0';

    payload_length = pxPublishInfo->payloadLength;
    if (payload_length >= sizeof(tmppayload))
    {
        payload_length = sizeof(tmppayload) - 1;
    }
    strncpy(tmppayload, (char*)pxPublishInfo->pPayload, payload_length);
    tmppayload[payload_length] = '\0';

    printf("Incoming Topic: %s\n"
           "Incoming Message : %s\n",
           tmptopic,
           tmppayload);
}

/**
 * @brief This callback gets called whenever we receive an @c ACK from the server.
 * @param pxIncomingPacket The incoming packet
 * @param usPacketId The packet ID
 */
static void MQTTProcessResponse(MQTTPacketInfo_t * pxIncomingPacket,
                                uint16_t usPacketId)
{
    MQTTStatus_t xResult = MQTTSuccess;
    uint8_t * pucPayload = NULL;
    size_t ulSize = 0;

    (void) usPacketId;

    switch (pxIncomingPacket->type)
    {
    case MQTT_PACKET_TYPE_SUBACK:
        /* A SUBACK from the broker, containing the server response to our
         * subscription request, has been received.  It contains the status
         * code indicating server approval/rejection for the subscription to
         * the single topic requested. The SUBACK will be parsed to obtain
         * the status code, and this status code will be stored in
         * #xTopicFilterContext. */
        xResult = MQTT_GetSubAckStatusCodes(pxIncomingPacket,
                                            &pucPayload,
                                            &ulSize);

        /* MQTT_GetSubAckStatusCodes always returns success if called with
         * packet info from the event callback and non-NULL parameters. */
        MMOSAL_ASSERT(xResult == MQTTSuccess);
        break;

    case MQTT_PACKET_TYPE_UNSUBACK:
        /* We should check which topic was unsubscribed to by looking at the packetid */
        printf("Unsubscribed from requested topic\n");
        break;

    case MQTT_PACKET_TYPE_PINGRESP:
        /* Nothing to be done from application as library handles
         * PINGRESP with the use of MQTT_ProcessLoop API function. */
        printf("WARNING: PINGRESP should not be handled by the application "
                "callback when using MQTT_ProcessLoop.\n");
        break;

    /* Any other packet type is invalid. */
    default:
        printf("MQTTProcessResponse() called with unknown packet type:(%02X).\n",
                pxIncomingPacket->type);
    }
}

/**
 * @brief This is a callback from MQTT_Process whenever a packet is received from the server.
 * @param pxMQTTContext The MQTT context
 * @param pxPacketInfo The packet info
 * @param pxDeserializedInfo The de-serialized packet info
 */
static void EventCallback(MQTTContext_t * pxMQTTContext,
                          MQTTPacketInfo_t * pxPacketInfo,
                          MQTTDeserializedInfo_t * pxDeserializedInfo)
{
    /* The MQTT context is not used for this demo. */
    (void) pxMQTTContext;

    if ((pxPacketInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
    {
        MQTTProcessIncomingPublish(pxDeserializedInfo->pPublishInfo);
    }
    else
    {
        MQTTProcessResponse(pxPacketInfo, pxDeserializedInfo->packetIdentifier);
    }
}

/**
 * @brief Initializes an MQTT connection with the server
 * @param pxMQTTContext The MQTT context
 * @param pxNetworkContext The network context (socket)
 * @param clientID Our unique Client ID string
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t CreateMQTTConnectionToBroker(MQTTContext_t * pxMQTTContext,
                                            NetworkContext_t * pxNetworkContext,
                                            char * clientID)
{
    MQTTStatus_t xResult;
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent;
    TransportInterface_t xTransport;
    MQTTFixedBuffer_t xBuffer;

    xBuffer.pBuffer = buf;
    xBuffer.size = sizeof(buf);

    /* Fill in Transport Interface send and receive function pointers. */
    memset(&xTransport, 0, sizeof(xTransport));
    xTransport.pNetworkContext = pxNetworkContext;
    xTransport.send = transport_send;
    xTransport.recv = transport_recv;

    /* Initialize MQTT library. */
    xResult = MQTT_Init(pxMQTTContext,
                        &xTransport,
                        mmosal_get_time_ms,
                        EventCallback,
                        &xBuffer);
    if (xResult != MQTTSuccess)
    {
        return xResult;
    }

    /* Many fields not used in this demo so start with everything at 0. */
    (void) memset((void *) &xConnectInfo, 0x00, sizeof(xConnectInfo));

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean
     * session will ensure that the broker does not store any data when this
     * client gets disconnected. */
    xConnectInfo.cleanSession = true;

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    xConnectInfo.pClientIdentifier = clientID;
    xConnectInfo.clientIdentifierLength = (uint16_t) strlen(clientID);

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value.  In the absence of sending any other
     * Control Packets, the Client MUST send a PINGREQ Packet. */
    xConnectInfo.keepAliveSeconds = KEEP_ALIVE_TIMEOUT_SECONDS;

    /* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
     * is passed as NULL. */
    xResult = MQTT_Connect(pxMQTTContext,
                           &xConnectInfo,
                           NULL,
                           CONNACK_RECV_TIMEOUT_MS,
                           &xSessionPresent);
    return xResult;
}

/**
 * @brief Subscribes to the specified topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic to subscribe to
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTSubscribe(MQTTContext_t * pxMQTTContext,
                           const char * topic)
{
    MQTTStatus_t xResult = MQTTSuccess;
    MQTTSubscribeInfo_t xMQTTSubscription[ TOPIC_COUNT ];
    uint16_t usSubscribePacketIdentifier;

    /* Some fields not used by this demo so start with everything at 0. */
    (void) memset((void *) &xMQTTSubscription, 0x00, sizeof(xMQTTSubscription));

    /* Each packet requires a unique ID. */
    usSubscribePacketIdentifier = MQTT_GetPacketId(pxMQTTContext);

    /* Subscribe to the pcExampleTopic topic filter. This example subscribes
     * to only one topic and uses QoS0. */
    xMQTTSubscription[ 0 ].qos = MQTTQoS0;
    xMQTTSubscription[ 0 ].pTopicFilter = topic;
    xMQTTSubscription[ 0 ].topicFilterLength = strlen(topic);

    /* The client is already connected to the broker. Subscribe to the topic
     * as specified in pcExampleTopic by sending a subscribe packet then
     * waiting for a subscribe acknowledgment (SUBACK). */
    xResult = MQTT_Subscribe(pxMQTTContext,
                             xMQTTSubscription,
                             1, /* Only subscribing to one topic. */
                             usSubscribePacketIdentifier);
    if (xResult != MQTTSuccess)
    {
        return xResult;
    }

    /* Process incoming packet from the broker. After sending the
     * subscribe, the client may receive a publish before it receives a
     * subscribe ack. Therefore, call generic incoming packet processing
     * function. Since this demo is subscribing to the topic to which no
     * one is publishing, probability of receiving Publish message before
     * subscribe ack is zero; but application must be ready to receive any
     * packet.  This demo uses the generic packet processing function
     * everywhere to highlight this fact. Note there is a separate demo that
     * shows how to use coreMQTT in a thread safe way – in which case the
     * MQTT protocol runs in the background and this call is not required. */
    xResult = MQTT_ProcessLoop(pxMQTTContext);
    return xResult;
}

/**
 * @brief Unsubscribes from the specified topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic to unsubscribe from
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTUnsubscribeFromTopic(MQTTContext_t * pxMQTTContext,
                                      const char * topic)
{
    MQTTStatus_t xResult;
    MQTTSubscribeInfo_t xMQTTSubscription[ TOPIC_COUNT ];
    uint16_t usUnsubscribePacketIdentifier;

    /* Some fields not used by this demo so start with everything at 0. */
    (void) memset((void *) &xMQTTSubscription, 0x00, sizeof(xMQTTSubscription));

    /* Subscribe to the pcExampleTopic topic filter. This example subscribes
     * to only one topic and uses QoS0. */
    xMQTTSubscription[ 0 ].qos = MQTTQoS0;
    xMQTTSubscription[ 0 ].pTopicFilter = topic;
    xMQTTSubscription[ 0 ].topicFilterLength = (uint16_t) strlen(
                                                    topic);

    /* Each packet requires a unique ID. */
    usUnsubscribePacketIdentifier = MQTT_GetPacketId(pxMQTTContext);

    /* Send UNSUBSCRIBE packet. */
    xResult = MQTT_Unsubscribe(pxMQTTContext,
                               xMQTTSubscription,
                               sizeof(xMQTTSubscription) / sizeof(
                               MQTTSubscribeInfo_t),
                               usUnsubscribePacketIdentifier);

    return xResult;
}

/**
 * @brief Publish a message to the specified MQTT topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic top publish to
 * @param payload A pointer to the binary or text data to publish
 * @param payloadLength The length of the data to publish
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTPublishToTopic(MQTTContext_t * pxMQTTContext,
                                const char * topic,
                                void * payload, size_t payloadLength)
{
    MQTTStatus_t xResult;
    MQTTPublishInfo_t xMQTTPublishInfo;

    /* Some fields are not used by this demo so start with everything at 0. */
    (void) memset((void *) &xMQTTPublishInfo, 0x00, sizeof(xMQTTPublishInfo));

    /* This demo uses QoS0. */
    xMQTTPublishInfo.qos = MQTTQoS0;
    xMQTTPublishInfo.retain = false;
    xMQTTPublishInfo.pTopicName = topic;
    xMQTTPublishInfo.topicNameLength = (uint16_t) strlen(topic);
    xMQTTPublishInfo.pPayload = payload;
    xMQTTPublishInfo.payloadLength = payloadLength;

    /* Send PUBLISH packet. Packet ID is not used for a QoS0 publish. */
    xResult = MQTT_Publish(pxMQTTContext, &xMQTTPublishInfo, 0U);
    return xResult;
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nMorse MQTT Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    uint32_t ulPublishCount = 0U;
    const uint32_t ulMaxPublishCount = 5UL;
    NetworkContext_t xNetworkContext = { 0 };
    MQTTContext_t xMQTTContext;
    MQTTStatus_t xMQTTStatus;
    TransportStatus_t xNetworkStatus;

    /* Save space on stack by allocating static, no need to make this global */
    static char client_id[48];
    static char topic[80];
    static char server[80];
    static char message[80];
    uint32_t port = MQTT_BROKER_PORT;
    uint32_t publish_delay = DELAY_BETWEEN_PUBLISHES;

    /* Generate Client ID & topic from MAC */
    uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
    char mac_address_str[MAC_ADDR_STR_LEN];
    enum mmwlan_status status = mmwlan_get_mac_addr(mac_addr);
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to read MAC address (status code %d)\n", status);
        return;
    }
    snprintf(mac_address_str, sizeof(mac_address_str), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    snprintf(client_id, sizeof(client_id), CLIENT_ID_PREFIX, mac_address_str);
    snprintf(topic, sizeof(topic), TOPIC_FORMAT, client_id);

    /* Read from config store */
    mmconfig_read_string("mqtt.clientid", client_id, sizeof(client_id));
    mmconfig_read_string("mqtt.topic", topic, sizeof(topic));
    mmconfig_read_uint32("mqtt.port", &port);
    mmconfig_read_uint32("mqtt.publish_delay", &publish_delay);

    strncpy(server, MQTT_BROKER_ENDPOINT, sizeof(server));
    mmconfig_read_string("mqtt.server", server, sizeof(server));

    strncpy(message, EXAMPLE_MESSAGE, sizeof(message));
    mmconfig_read_string("mqtt.message", message, sizeof(message));

    /*************************** Connect. *********************************/

    /* Attempt to connect to the MQTT broker.  The socket is returned in
     * the network context structure. We set NetworkCredentials to NULL to connect in the clear.
     * Set this parameter if you wish to connect with TLS */
    printf("Connecting to server socket on %s:%ld...", server, port);
    xNetworkStatus = transport_connect(&xNetworkContext, server, (uint16_t) port, NULL);
    if (xNetworkStatus != TRANSPORT_SUCCESS)
    {
        printf("failed with code %d\n", xNetworkStatus);
        return;
    }
    printf("ok\n");

    /* Connect to the MQTT broker using the already connected TCP socket. */
    printf("Client %s Creating MQTT connection with broker....", client_id);
    xMQTTStatus = CreateMQTTConnectionToBroker(&xMQTTContext, &xNetworkContext, client_id);
    if (xMQTTStatus != MQTTSuccess)
    {
        printf("failed with code %d\n", xMQTTStatus);
        transport_disconnect(&xNetworkContext);
        return;
    }
    printf("ok\n");

    /**************************** Subscribe. ******************************/

    /* Subscribe to the test topic. */
    printf("Subscribing to topic %s...", topic);
    xMQTTStatus = MQTTSubscribe(&xMQTTContext, topic);
    if (xMQTTStatus != MQTTSuccess)
    {
        printf("failed with code %d\n", xMQTTStatus);
        goto quit;
    }
    printf("ok\n");

    /******************* Publish and Keep Alive Loop. *********************/

    /* Publish messages with QoS0, then send and process Keep Alive messages. */
    for (ulPublishCount = 0; ulPublishCount < ulMaxPublishCount; ulPublishCount++)
    {
        printf("Publishing to topic %s...", topic);
        xMQTTStatus = MQTTPublishToTopic(&xMQTTContext, topic,
                                        message, strlen(message));
        if (xMQTTStatus != MQTTSuccess)
        {
            printf("failed with code %d\n", xMQTTStatus);
            goto quit;
        }
        printf("ok\n");

        /* Process the incoming publish echo. Since the application subscribed
         * to the same topic, the broker will send the same publish message
         * back to the application.  Note there is a separate demo that
         * shows how to use coreMQTT in a thread safe way - in which case the
         * MQTT protocol runs in the background and this call is not
         * required. */
        xMQTTStatus = MQTT_ProcessLoop(&xMQTTContext);
        if (xMQTTStatus != MQTTSuccess)
        {
            printf("MQTT_ProcessLoop() failed with code %d\n", xMQTTStatus);
            goto quit;
        }

        /* Leave the connection idle for some time. */
        mmosal_task_sleep(publish_delay);
    }

    /******************** Unsubscribe from the topic. *********************/

    xMQTTStatus = MQTTUnsubscribeFromTopic(&xMQTTContext, topic);
    if (xMQTTStatus != MQTTSuccess)
    {
        printf("MQTTUnsubscribeFromTopic() failed with code %d\n", xMQTTStatus);
        goto quit;
    }

    /* Process the incoming packet from the broker.  Note there is a separate
     * demo that shows how to use coreMQTT in a thread safe way - in which case
     * the MQTT protocol runs in the background and this call is not required. */
    xMQTTStatus = MQTT_ProcessLoop(&xMQTTContext);
    if (xMQTTStatus != MQTTSuccess)
    {
        printf("MQTT_ProcessLoop() failed with code %d\n", xMQTTStatus);
        goto quit;
    }

    /**************************** Disconnect. *****************************/

quit:
    /* Disconnect from broker. */
    printf("Disconnecting from server and closing socket.\n");
    xMQTTStatus = MQTT_Disconnect(&xMQTTContext);
    if (xMQTTStatus != MQTTSuccess)
    {
        printf("MQTT_Disconnect() failed with code %d\n", xMQTTStatus);
    }

    /* Close the network connection. */
    transport_disconnect(&xNetworkContext);
}
