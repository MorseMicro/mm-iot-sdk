#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE_MQTT_AGENT_DIR = src/freertos-libs/coreMQTT-Agent/source

# C files to be included while compiling
CORE_MQTT_AGENT_SRCS_C += core_mqtt_agent.c
CORE_MQTT_AGENT_SRCS_C += core_mqtt_agent_command_functions.c

# Header files for dependancy checking
CORE_MQTT_AGENT_SRCS_H += include/core_mqtt_agent_command_functions.h
CORE_MQTT_AGENT_SRCS_H += include/core_mqtt_agent_config_defaults.h
CORE_MQTT_AGENT_SRCS_H += include/core_mqtt_agent_default_logging.h
CORE_MQTT_AGENT_SRCS_H += include/core_mqtt_agent.h
CORE_MQTT_AGENT_SRCS_H += include/core_mqtt_agent_message_interface.h

MMIOT_SRCS_C += $(addprefix $(CORE_MQTT_AGENT_DIR)/,$(CORE_MQTT_AGENT_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CORE_MQTT_AGENT_DIR)/,$(CORE_MQTT_AGENT_SRCS_H))

MMIOT_INCLUDES += $(CORE_MQTT_AGENT_DIR)/include
