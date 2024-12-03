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
 * @file mqtt_agent_task.h
 * The task loop and associated functions for MQTT Agent.
 */

#ifndef _MQTT_AGENT_TASK_H_
#define _MQTT_AGENT_TASK_H_

#include <stdbool.h>

struct MQTTAgentTaskCtx;
typedef struct MQTTAgentContext * MQTTAgentHandle_t;

MQTTAgentHandle_t xGetMqttAgentHandle(void);

/** Event group based mechanism that can be used to block tasks until agent is ready */
void vSleepUntilMQTTAgentReady(void);

void vSleepUntilMQTTAgentConnected(void);

bool xIsMqttAgentConnected(void);

void start_mqtt_agent_task(void);

#endif /* ifndef _MQTT_AGENT_TASK_H_ */
