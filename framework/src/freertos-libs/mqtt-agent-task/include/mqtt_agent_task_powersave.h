/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "core_mqtt.h"
#include "core_mqtt_agent.h"

/**
 * @brief Calculate the next MQTT deadline (next time coreMQTT will need to do work).
 * @param pContext coreMQTT MQTT context.
 * @return time before the next deadline in milliseconds.
 */
uint32_t mqtt_agent_task_powersave_next_deadline(MQTTContext_t * pContext);

/**
 * @brief Hook to be called before starting the MQTT agent command loop.
 * @param netctx the network context structure (socket).
 * @param mqttctx the coreMQTT-Agent agent context.
 */
void mqtt_agent_task_powersave_start_commandloop(NetworkContext_t* netctx, MQTTAgentContext_t* mqttctx);

/**
 * @brief Hook to be called after the MQTT agent command loop has returned.
 * @param netctx the network context structure (socket).
 */
void mqtt_agent_task_powersave_stop_commandloop(NetworkContext_t* netctx);
