/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "mmutils.h"
#include "mqtt_agent_task.h"
#include "transport_interface.h"
#include "core_mqtt_config.h"

static uint32_t old_transport_timeout = 0;

uint32_t mqtt_agent_task_powersave_next_deadline(MQTTContext_t *pContext)
{
    uint32_t closest_deadline = UINT32_MAX;
    uint32_t now = pContext->getTime();

    if (pContext->waitingForPingResp == true)
    {
        const uint32_t timeElapsed = now - pContext->pingReqSendTimeMs;
        closest_deadline = MM_MIN(closest_deadline, MQTT_PINGRESP_TIMEOUT_MS - timeElapsed);
    }

    uint32_t packetTxTimeoutMs =
        MM_MIN(1000U * (uint32_t)pContext->keepAliveIntervalSec, PACKET_TX_TIMEOUT_MS);
    if (packetTxTimeoutMs != 0U)
    {
        const uint32_t timeElapsed = now - pContext->lastPacketTxTime;
        closest_deadline = MM_MIN(closest_deadline, packetTxTimeoutMs - timeElapsed);
    }

    if (PACKET_RX_TIMEOUT_MS != 0U)
    {
        const uint32_t timeElapsed = now - pContext->lastPacketRxTime;
        closest_deadline = MM_MIN(closest_deadline, PACKET_RX_TIMEOUT_MS - timeElapsed);
    }

    /* Currently doesn't check QoS > 0 messages deadlines, will be needed once we enable QoS > 0 */

    return closest_deadline;
}

static void mqtt_agent_task_recv_packet(NetworkContext_t *ctx, void *arg)
{
    MM_UNUSED(ctx);

    if (mqtt_agent_task_get_state() != MQTT_AGENT_TASK_CONNECTED)
    {
        return;
    }

    MQTTAgentContext_t *agentctx = (MQTTAgentContext_t *)arg;
    MQTTAgentCommandInfo_t commandinfo = { 0 };
    MQTTAgent_ProcessLoop(agentctx, &commandinfo);
}

void mqtt_agent_task_powersave_start_commandloop(NetworkContext_t *netctx,
                                                 MQTTAgentContext_t *mqttctx)
{
    if(!MQTT_AGENT_TASK_USE_RX_CALLBACK)
    {
        return;
    }

    old_transport_timeout = netctx->receiveTimeoutMs;
    netctx->receiveTimeoutMs = 1; /* 0 means infinite */
    int32_t err = transport_register_recv_callback(netctx, mqtt_agent_task_recv_packet, mqttctx);
    MMOSAL_ASSERT(err == 0);
}

void mqtt_agent_task_powersave_stop_commandloop(NetworkContext_t *netctx)
{
    if(!MQTT_AGENT_TASK_USE_RX_CALLBACK)
    {
        return;
    }

    netctx->receiveTimeoutMs = old_transport_timeout;
}
