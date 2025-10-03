/*
 * FreeRTOS STM32 Reference Integration
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023-2025 Morse Micro
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
 * @file mqtt_agent_task.h
 * The task loop and associated functions for MQTT Agent.
 */

#ifndef _MQTT_AGENT_TASK_H_
#define _MQTT_AGENT_TASK_H_

#include <stdbool.h>
#include "core_mqtt_serializer.h"

struct MQTTAgentTaskCtx;
typedef struct MQTTAgentContext *MQTTAgentHandle_t;

enum MQTTAgentTaskState {
    MQTT_AGENT_TASK_INIT,
    MQTT_AGENT_TASK_CONNECTING,
    MQTT_AGENT_TASK_CONNECTED,
    MQTT_AGENT_TASK_TERMINATED
};

MQTTAgentHandle_t xGetMqttAgentHandle(void);

/**
 * Blocks until the MQTT agent task is ready (trying to connect to the broker).
 * @warning: Will block forever if initialization fails
 */
void vSleepUntilMQTTAgentReady(void);

/**
 * Blocks until the MQTT agent task is connected to the broker.
 * @warning: Will block forever if initialization fails
 */
void vSleepUntilMQTTAgentConnected(void);

enum MQTTAgentTaskState mqtt_agent_task_get_state(void);

void start_mqtt_agent_task(void);

MQTTStatus_t mqtt_agent_task_init_get_error_code(void);

void mqtt_agent_task_set_exit_flag(bool newval);

#endif /* ifndef _MQTT_AGENT_TASK_H_ */
