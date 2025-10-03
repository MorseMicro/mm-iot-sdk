/*
 * Copyright 2023-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core_mqtt.h"
#include "mqtt_agent_config.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmutils.h"

/* The application must define this to specify the keys of the config store. */
#include "aws_iot_config.h"

/**
 * Specifies the maximum size of a generated Client ID.
 */
#define MAX_CLIENT_ID_LEN                     64
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
 * This is the maximum size of any placeholder files, we determine the file is a dummy
 * if it is less than this size.
 */
#define DUMMY_FILE_SIZE                       100

/* Ensure defines are present for optional configuration items if not defined.
 * By setting to an empty string, the mmconfig_ function will return no value.
 */
#ifndef CONFIG_KEY_MQTT_USERNAME
#define CONFIG_KEY_MQTT_USERNAME ""
#endif

#ifndef CONFIG_KEY_MQTT_PASSWORD
#define CONFIG_KEY_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_KEY_MQTT_KEEPALIVE
#define CONFIG_KEY_MQTT_KEEPALIVE ""
#endif

char *mqtt_agent_config_get_default_user_agent_string(void)
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

void mqtt_agent_config_initialise_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    memset((void *)pxConnectInfo, 0x00, sizeof(MQTTConnectInfo_t));

    /* Always start the initial connection with a clean session */
    pxConnectInfo->cleanSession = true;
    pxConnectInfo->keepAliveSeconds = KEEP_ALIVE_INTERVAL_S;
    char keep_alive_str[8];
    if (mmconfig_read_string(CONFIG_KEY_MQTT_KEEPALIVE, keep_alive_str, sizeof(keep_alive_str)) > 0)
    {
        /* Setting 0 will disable keep alive. We do not guard against an invalid
         * configuration value causing keep alive to be disabled. */
        pxConnectInfo->keepAliveSeconds = atoi(keep_alive_str);
    }

    /* Ignore return value for now, as it includes the NULL terminator in length if successful */
    mmconfig_alloc_and_load(CONFIG_KEY_MQTT_USERNAME, (void **)&pxConnectInfo->pUserName);
    if (pxConnectInfo->pUserName == NULL)
    {
        pxConnectInfo->pUserName = mqtt_agent_config_get_default_user_agent_string();
    }
    pxConnectInfo->userNameLength = strlen(pxConnectInfo->pUserName);

    /* Ignore return value for now, as it includes the NULL terminator in length if successful */
    mmconfig_alloc_and_load(CONFIG_KEY_MQTT_PASSWORD, (void **)&pxConnectInfo->pPassword);
    pxConnectInfo->passwordLength = pxConnectInfo->pPassword ==
        NULL ? 0 : strlen(pxConnectInfo->pPassword);

    pxConnectInfo->clientIdentifierLength = mmconfig_alloc_and_load(AWS_KEY_THING_NAME,
                                                                    (void **)&pxConnectInfo->
                                                                        pClientIdentifier) - 1;
    if (pxConnectInfo->pClientIdentifier == NULL)
    {
        /* If thingname is not found generate one using the MAC address */
        uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
        char *client_id = (char *)mmosal_malloc(MAX_CLIENT_ID_LEN);
        MMOSAL_ASSERT(mmwlan_get_mac_addr(mac_addr) == MMWLAN_SUCCESS);
        snprintf(client_id, MAX_CLIENT_ID_LEN,
                 "MM_Client_ID_%02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        pxConnectInfo->pClientIdentifier = client_id;
        pxConnectInfo->clientIdentifierLength =
            strlen(pxConnectInfo->pClientIdentifier);
    }
}

MQTTStatus_t mqtt_agent_config_validate_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    if (pxConnectInfo->pClientIdentifier == NULL ||
        pxConnectInfo->pClientIdentifier[0] == '<')
    {
        printf("Please specify a valid pClientIdentifier.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        return MQTTBadParameter;
    }
    return MQTTSuccess;
}

void mqtt_agent_config_cleanup_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    if (pxConnectInfo->pUserName != NULL &&
        pxConnectInfo->pUserName != mqtt_agent_config_get_default_user_agent_string())
    {
        /* pUserName is non NULL, and does not point to the static userAgent buffer, so must have
         * been allocated by mmconfig_alloc_and_load */
        mmosal_free((void *)pxConnectInfo->pUserName);
    }
    if (pxConnectInfo->pPassword != NULL)
    {
        mmosal_free((void *)pxConnectInfo->pPassword);
    }
    /* initlisation allocates pClientIdentifier, so we should free it if it is not NULL */
    if (pxConnectInfo->pClientIdentifier != NULL)
    {
        /* Free the memory as we failed */
        mmosal_free((void *)pxConnectInfo->pClientIdentifier);
    }
    memset((void *)pxConnectInfo, 0x00, sizeof(MQTTConnectInfo_t));
}

void mqtt_agent_config_initialise_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    /* Read endpoint from config store */
    mmconfig_alloc_and_load(AWS_KEY_ENDPOINT, (void **)&pxBrokerEndpoint->pcMqttEndpoint);

    /* Read port from config store */
    char port_str[8];
    if (mmconfig_read_string(AWS_KEY_PORT, port_str, sizeof(port_str)) > 0)
    {
        pxBrokerEndpoint->ulMqttPort = atoi(port_str);
    }
    else
    {
        /* If nothing specified assume default 443 */
        pxBrokerEndpoint->ulMqttPort = 443;
    }

    pxBrokerEndpoint->secure = true;
}

MQTTStatus_t mqtt_agent_config_validate_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    MQTTStatus_t result = MQTTSuccess;
    if (pxBrokerEndpoint->pcMqttEndpoint == NULL || pxBrokerEndpoint->pcMqttEndpoint[0] == '<')
    {
        printf("Could not find key [aws.endpoint] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        result = MQTTBadParameter;
    }
    if (pxBrokerEndpoint->ulMqttPort == 0)
    {
        printf("Invalid value specified for key [aws.port] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        result = MQTTBadParameter;
    }
    return result;
}

void mqtt_agent_config_cleanup_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    // initlisation allocates pcMqttEndpoint, so we should free it if it is not NULL
    if (pxBrokerEndpoint->pcMqttEndpoint != NULL)
    {
        mmosal_free((void *)pxBrokerEndpoint->pcMqttEndpoint);
        pxBrokerEndpoint->pcMqttEndpoint = NULL;
    }
}

void mqtt_agent_config_initialise_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    /* ALPN protocols must be a NULL-terminated list of strings. */
    static const char *pcAlpnProtocols[] = { "x-amzn-mqtt-ca", NULL };

    /* Setup security */
    pxCredentials->pAlpnProtos = pcAlpnProtocols;

    /* Load Root CA, first determine size to allocate */
    int len;
    uint8_t *allocptr;
    len = mmconfig_read_bytes(AWS_KEY_ROOT_CA, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t *)mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_ROOT_CA, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            pxCredentials->pRootCa = allocptr;
            pxCredentials->rootCaSize = (size_t)len + 1;
        }
        else
        {
            printf("Unable to allocate memory for ROOT CERT.\n");
        }
    }

    /* Load device certificate */
    len = mmconfig_read_bytes(AWS_KEY_DEVICE_CERTIFICATE, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t *)mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_DEVICE_CERTIFICATE, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            pxCredentials->pClientCert = allocptr;
            pxCredentials->clientCertSize = (size_t)len + 1;
        }
        else
        {
            printf("Unable to allocate memory for DEVICE CERT.\n");
        }
    }

    /* Load device private keys */
    len = mmconfig_read_bytes(AWS_KEY_DEVICE_KEYS, NULL, 0, 0);
    if (len > DUMMY_FILE_SIZE)
    {
        /* Looks like we have a valid key */
        allocptr = (uint8_t *)mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes(AWS_KEY_DEVICE_KEYS, allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len] = 0;
            pxCredentials->pPrivateKey = allocptr;
            pxCredentials->privateKeySize = (size_t)len + 1;
        }
        else
        {
            printf("Unable to allocate memory for DEVICE CERT.\n");
        }
    }
}

MQTTStatus_t mqtt_agent_config_validate_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    MQTTStatus_t result = MQTTSuccess;
    if (pxCredentials->pRootCa == NULL)
    {
        printf("Could not find a valid key [aws.rootca] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        result = MQTTBadParameter;
    }

    if (pxCredentials->pClientCert == NULL)
    {
        printf("Could not find a valid  key [aws.devicecert] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        result = MQTTBadParameter;
    }
    if (pxCredentials->pPrivateKey == NULL)
    {
        printf("Could not find a valid  key [aws.devicekeys] in config store.\n");
        printf("Please refer to the documentation in aws_iot.c for "
               "instructions on configuring the device for this application.\n");
        result = MQTTBadParameter;
    }
    return result;
}

void mqtt_agent_config_cleanup_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    if (pxCredentials->pRootCa != NULL)
    {
        mmosal_free((void *)pxCredentials->pRootCa);
    }
    if (pxCredentials->pClientCert != NULL)
    {
        mmosal_free((void *)pxCredentials->pClientCert);
    }
    if (pxCredentials->pPrivateKey != NULL)
    {
        mmosal_free((void *)pxCredentials->pPrivateKey);
    }
    memset((void *)pxCredentials, 0, sizeof(NetworkCredentials_t));
}

void mqtt_agent_broker_connection_event(MQTTStatus_t mqtt_status,
                                        TransportStatus_t transport_status)
{
    MM_UNUSED(mqtt_status);
    MM_UNUSED(transport_status);
}
