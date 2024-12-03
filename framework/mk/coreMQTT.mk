#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE_MQTT_DIR = src/freertos-libs/coreMQTT/source

# C files to be included while compiling
CORE_MQTT_SRCS_C += core_mqtt.c
CORE_MQTT_SRCS_C += core_mqtt_serializer.c
CORE_MQTT_SRCS_C += core_mqtt_state.c

# Header files for dependancy checking
CORE_MQTT_SRCS_H += include/core_mqtt_config_defaults.h
CORE_MQTT_SRCS_H += include/core_mqtt_default_logging.h
CORE_MQTT_SRCS_H += include/core_mqtt.h
CORE_MQTT_SRCS_H += include/core_mqtt_serializer.h
CORE_MQTT_SRCS_H += include/core_mqtt_state.h

MMIOT_SRCS_C += $(addprefix $(CORE_MQTT_DIR)/,$(CORE_MQTT_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CORE_MQTT_DIR)/,$(CORE_MQTT_SRCS_H))

MMIOT_INCLUDES += $(CORE_MQTT_DIR)/include
