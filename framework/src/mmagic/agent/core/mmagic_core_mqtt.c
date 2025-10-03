/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core_mqtt_agent_command_functions.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_serializer.h"
#include "mmagic_core_utils.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mqtt_agent_config.h"
#include "mqtt_agent_task.h"
#include "subscription_manager.h"
#include "transport_interface.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_types.h"

#include "m2m_api/mmagic_m2m_agent.h"

#define KEEP_ALIVE_INTERVAL_S (60U)
#define MAX_CLIENT_ID_LEN (64U)

/* Temporary storage for MQTT agent config
 * When MQTT connect is called, mmagic puts the config here and the MQTT Agent task
 * then copies it to its internal memory */
NetworkCredentials_t mqtt_agent_config_credentials;
MQTTConnectInfo_t mqtt_agent_config_connect_info;
MQTTBrokerEndpoint_t mqtt_agent_config_endpoint;

/* Context for MQTT callbacks (subscription and broker connection events) */
struct mmagic_mqtt_cb_context
{
    struct mmagic_data *core;
    uint8_t stream_id;
} mmagic_mqtt_cb_context;

void mmagic_core_mqtt_init(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

void mmagic_core_mqtt_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

/* MQTT Agent configuration */

void mqtt_agent_config_initialise_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    pxConnectInfo->cleanSession = true;
    pxConnectInfo->keepAliveSeconds = KEEP_ALIVE_INTERVAL_S;
    pxConnectInfo->pUserName = NULL;
    pxConnectInfo->pPassword = NULL;
    pxConnectInfo->pClientIdentifier = NULL;

    /* Copies username and password to heap allocated buffers */
    if (mqtt_agent_config_connect_info.userNameLength > 0)
    {
        pxConnectInfo->pUserName =
            (const char *)mmosal_malloc(mqtt_agent_config_connect_info.userNameLength + 1);
        if (pxConnectInfo->pUserName != NULL)
        {
            pxConnectInfo->userNameLength = mqtt_agent_config_connect_info.userNameLength;
            memcpy((void *)pxConnectInfo->pUserName, mqtt_agent_config_connect_info.pUserName,
                   mqtt_agent_config_connect_info.userNameLength + 1);
        }
    }
    if (mqtt_agent_config_connect_info.passwordLength > 0)
    {
        pxConnectInfo->pPassword =
            (const char *)mmosal_malloc(mqtt_agent_config_connect_info.passwordLength + 1);
        if (pxConnectInfo->pPassword != NULL)
        {
            pxConnectInfo->passwordLength = mqtt_agent_config_connect_info.passwordLength;
            memcpy((void *)pxConnectInfo->pPassword, mqtt_agent_config_connect_info.pPassword,
                   mqtt_agent_config_connect_info.passwordLength + 1);
        }
    }

    /* Generates client identifier from MAC address */
    uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
    char *client_id = (char *)mmosal_malloc(MAX_CLIENT_ID_LEN);
    if (client_id == NULL)
    {
        return;
    }
    MMOSAL_ASSERT(mmwlan_get_mac_addr(mac_addr) == MMWLAN_SUCCESS);
    snprintf(client_id, MAX_CLIENT_ID_LEN,
             "MM_Client_ID_%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    pxConnectInfo->pClientIdentifier = client_id;
    pxConnectInfo->clientIdentifierLength =
        strlen(pxConnectInfo->pClientIdentifier);
}

MQTTStatus_t mqtt_agent_config_validate_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    if (pxConnectInfo->pClientIdentifier == NULL)
    {
        return MQTTNoMemory;
    }
    if (pxConnectInfo->pUserName == NULL && pxConnectInfo->userNameLength > 0)
    {
        return MQTTNoMemory;
    }
    if (pxConnectInfo->pPassword == NULL && pxConnectInfo->passwordLength > 0)
    {
        return MQTTNoMemory;
    }
    return MQTTSuccess;
}

void mqtt_agent_config_cleanup_connect_info(MQTTConnectInfo_t *pxConnectInfo)
{
    if (pxConnectInfo->pUserName != NULL)
    {
        mmosal_free((void *)pxConnectInfo->pUserName);
    }
    if (pxConnectInfo->pPassword != NULL)
    {
        mmosal_free((void *)pxConnectInfo->pPassword);
    }
    if (pxConnectInfo->pClientIdentifier != NULL)
    {
        mmosal_free((void *)pxConnectInfo->pClientIdentifier);
    }
}

void mqtt_agent_config_initialise_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    pxBrokerEndpoint->pcMqttEndpoint = NULL;
    pxBrokerEndpoint->ulMqttPort = mqtt_agent_config_endpoint.ulMqttPort;
    pxBrokerEndpoint->secure = mqtt_agent_config_endpoint.secure;

    pxBrokerEndpoint->pcMqttEndpoint =
        (const char *)mmosal_malloc(mqtt_agent_config_endpoint.uxMqttEndpointLen + 1);
    if (pxBrokerEndpoint->pcMqttEndpoint != NULL)
    {
        pxBrokerEndpoint->uxMqttEndpointLen = mqtt_agent_config_endpoint.uxMqttEndpointLen;
        memcpy((void *)pxBrokerEndpoint->pcMqttEndpoint, mqtt_agent_config_endpoint.pcMqttEndpoint,
               mqtt_agent_config_endpoint.uxMqttEndpointLen + 1);
    }
}

MQTTStatus_t mqtt_agent_config_validate_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    return pxBrokerEndpoint->pcMqttEndpoint ? MQTTSuccess : MQTTNoMemory;
}

void mqtt_agent_config_cleanup_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint)
{
    if (pxBrokerEndpoint->pcMqttEndpoint != NULL)
    {
        mmosal_free((void *)pxBrokerEndpoint->pcMqttEndpoint);
    }
}

void mqtt_agent_config_initialise_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    memcpy(pxCredentials, &mqtt_agent_config_credentials, sizeof(mqtt_agent_config_credentials));
}

MQTTStatus_t mqtt_agent_config_validate_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    MM_UNUSED(pxCredentials);
    return MQTTSuccess;
}

void mqtt_agent_config_cleanup_tls_credentials(NetworkCredentials_t *pxCredentials)
{
    MM_UNUSED(pxCredentials);
}

void mqtt_agent_broker_connection_event(MQTTStatus_t mqtt_status,
                                        TransportStatus_t transport_status)
{
    enum mmagic_status status = MMAGIC_STATUS_OK;
    if (transport_status != TRANSPORT_SUCCESS)
    {
        status = mmagic_transport_status_to_mmagic_status(transport_status);
    }
    else if (mqtt_status != MQTTSuccess)
    {
        status = mmagic_mqtt_status_to_mmagic_status(mqtt_status);
    }
    struct mmagic_core_event_mqtt_broker_connection_args args = { mmagic_mqtt_cb_context.stream_id,
                                                                  status };
    mmagic_core_event_mqtt_broker_connection(mmagic_mqtt_cb_context.core, &args);
}

/* MMAGIC commands */

enum mmagic_status mmagic_core_mqtt_start_agent(struct mmagic_data *core,
                                                const struct mmagic_core_mqtt_start_agent_cmd_args *
                                                cmd_args,
                                                struct mmagic_core_mqtt_start_agent_rsp_args *
                                                rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    /* One MQTT agent at a time for now */
    enum MQTTAgentTaskState state = mqtt_agent_task_get_state();
    if (state != MQTT_AGENT_TASK_INIT && state != MQTT_AGENT_TASK_TERMINATED)
    {
        return MMAGIC_STATUS_UNAVAILABLE;
    }

    /* TLS credentials */
    memset(&mqtt_agent_config_credentials, 0, sizeof(mqtt_agent_config_credentials));
    if (cmd_args->secure)
    {
        struct mmagic_tls_data *tls_data = mmagic_data_get_tls(core);
        enum mmagic_status status = mmagic_init_tls_credentials(&mqtt_agent_config_credentials,
                                                                tls_data);
        if (status != MMAGIC_STATUS_OK)
        {
            return status;
        }
    }

    /* MMagic stream, will be useful if we want to support multiple MQTT connections */
    enum mmagic_status stream_status = mmagic_m2m_agent_open_stream(core, NULL,
                                                                    &rsp_args->stream_id);
    if (stream_status != MMAGIC_STATUS_OK)
    {
        return stream_status;
    }

    /* Context for event callbacks */
    mmagic_mqtt_cb_context.core = core;
    mmagic_mqtt_cb_context.stream_id = rsp_args->stream_id;

    /* MQTT connection */
    /* The rest of the fields are managed by the mqtt_agent_config functions above */
    mqtt_agent_config_connect_info.pUserName = cmd_args->username.data;
    mqtt_agent_config_connect_info.userNameLength = cmd_args->username.len;
    mqtt_agent_config_connect_info.pPassword = cmd_args->password.data;
    mqtt_agent_config_connect_info.passwordLength = cmd_args->password.len;

    /* Broker endpoint */
    mqtt_agent_config_endpoint.pcMqttEndpoint = cmd_args->url.data;
    mqtt_agent_config_endpoint.uxMqttEndpointLen = cmd_args->url.len;
    mqtt_agent_config_endpoint.ulMqttPort = cmd_args->port;
    mqtt_agent_config_endpoint.secure = cmd_args->secure;

    /* Pointers to config values set above must stay valid indefinitely
     * TLS credentials are stored in mmagic_data so they do stay valid, though
     * they could be modified between MQTT reconnects
     * Data comming from the command arguments won't stay valid, so they're
     * copied into heap allocated buffers by the mqtt_agent_config functions
     * called by the agent task during its init process */
    start_mqtt_agent_task();

    /* Waits for the agent to be either connecting or errored
     * This is critical! If this function returns before the mqtt_agent_config functions
     * are called, then the connection and endpoint data will become invalid because
     * the mmagic command arguments will become invalid */
    while (mqtt_agent_task_get_state() == MQTT_AGENT_TASK_INIT)
    {
        mmosal_task_sleep(10);
    }

    MQTTStatus_t error_code = mqtt_agent_task_init_get_error_code();
    if (error_code != MQTTSuccess)
    {
        mmagic_m2m_agent_close_stream(core, rsp_args->stream_id);
        return mmagic_mqtt_status_to_mmagic_status(error_code);
    }
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_mqtt_publish(
    struct mmagic_data *core,
    const struct mmagic_core_mqtt_publish_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    if (cmd_args->qos != 0)
    {
        return MMAGIC_STATUS_NOT_SUPPORTED;
    }

    MQTTAgentHandle_t xAgentHandle = xGetMqttAgentHandle();
    MQTTPublishInfo_t xMQTTPublishInfo;
    MQTTAgentCommandFuncReturns_t returnFlags;
    memset((void *)&xMQTTPublishInfo, 0, sizeof(xMQTTPublishInfo));

    xMQTTPublishInfo.qos = (MQTTQoS_t)cmd_args->qos;
    xMQTTPublishInfo.retain = false;
    xMQTTPublishInfo.pTopicName = cmd_args->topic.data;
    xMQTTPublishInfo.topicNameLength = cmd_args->topic.len;

    xMQTTPublishInfo.pPayload = cmd_args->payload.data;
    xMQTTPublishInfo.payloadLength = cmd_args->payload.len;

    MQTTStatus_t status = MQTTAgentCommand_Publish(xAgentHandle, &xMQTTPublishInfo, &returnFlags);
    if (status != MQTTSuccess)
    {
        return mmagic_mqtt_status_to_mmagic_status(status);
    }
    return MMAGIC_STATUS_OK;
}

static void mqtt_agent_message_received_cb(void *context, MQTTPublishInfo_t *publish_info)
{
    MM_UNUSED(context);
    struct mmagic_core_event_mqtt_message_received_args args;
    memcpy(args.topic.data, publish_info->pTopicName, publish_info->topicNameLength);
    if (publish_info->pPayload != NULL)
    {
        memcpy(args.payload.data, publish_info->pPayload, publish_info->payloadLength);
    }
    args.topic.len = publish_info->topicNameLength;
    args.payload.len = publish_info->payloadLength;
    args.stream_id = mmagic_mqtt_cb_context.stream_id;
    mmagic_core_event_mqtt_message_received(mmagic_mqtt_cb_context.core, &args);
}

enum mmagic_status mmagic_core_mqtt_subscribe(
    struct mmagic_data *core,
    const struct mmagic_core_mqtt_subscribe_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    if (cmd_args->qos != 0)
    {
        return MMAGIC_STATUS_NOT_SUPPORTED;
    }

    /* Topic filter is copied to a heap allocated buffer managed by mqtt_agent_task */
    /* Careful if we decide to get rid of the subscription manager */
    MQTTStatus_t status = MqttAgent_SubscribeSync(
        xGetMqttAgentHandle(),
        cmd_args->topic.data,
        cmd_args->topic.len,
        (MQTTQoS_t)cmd_args->qos,
        mqtt_agent_message_received_cb,
        NULL);

    if (status != MQTTSuccess)
    {
        return mmagic_mqtt_status_to_mmagic_status(status);
    }
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_mqtt_stop_agent(
    struct mmagic_data *core,
    const struct mmagic_core_mqtt_stop_agent_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    mqtt_agent_task_set_exit_flag(true);

    MQTTAgentContext_t *ctx = (MQTTAgentContext_t *)xGetMqttAgentHandle();
    MQTTAgentCommandInfo_t cmdinfo = { 0 };
    MQTTAgent_Terminate(ctx, &cmdinfo);

    mmagic_m2m_agent_close_stream(core, cmd_args->stream_id);

    /* Waits for the MQTT agent task to terminate */
    while (mqtt_agent_task_get_state() != MQTT_AGENT_TASK_TERMINATED)
    {
        mmosal_task_sleep(10);
    }

    return MMAGIC_STATUS_OK;
}
