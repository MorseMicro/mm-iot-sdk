#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MQTT_AGENT_TASK_DIR = src/freertos-libs/mqtt-agent-task

# C files to be included while compiling
MQTT_AGENT_TASK_SRCS_C += mqtt_agent_task.c
MQTT_AGENT_TASK_SRCS_C += command_pool.c

# Header files for dependancy checking
MQTT_AGENT_TASK_SRCS_H += include/mqtt_agent_task.h
MQTT_AGENT_TASK_SRCS_H += include/command_pool.h
MQTT_AGENT_TASK_SRCS_H += include/subscription_manager.h
MQTT_AGENT_TASK_SRCS_H += include/mqtt_agent_config.h

MMIOT_SRCS_C += $(addprefix $(MQTT_AGENT_TASK_DIR)/,$(MQTT_AGENT_TASK_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MQTT_AGENT_TASK_DIR)/,$(MQTT_AGENT_TASK_SRCS_H))

MMIOT_INCLUDES += $(MQTT_AGENT_TASK_DIR)/include

CFLAGS-src/freertos-libs += -Wno-c++-compat
CFLAGS-src/freertos-libs += -Wno-type-limits
CFLAGS-src/freertos-libs += -Wno-unused-but-set-variable
CFLAGS-src/freertos-libs += -Wno-unused-parameter
CFLAGS-src/freertos-libs += -Wno-unused-variable
