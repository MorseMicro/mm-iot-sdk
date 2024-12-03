#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_OTA_DIR = src/aws/AWS-OTA/source

# C files to be included while compiling
AWS_OTA_SRCS_C += ota.c
AWS_OTA_SRCS_C += ota_base64.c
AWS_OTA_SRCS_C += ota_cbor.c
AWS_OTA_SRCS_C += ota_http.c
AWS_OTA_SRCS_C += ota_interface.c
AWS_OTA_SRCS_C += ota_mqtt.c

# Header files for dependancy checking
AWS_OTA_SRCS_H += include/ota_appversion32.h
AWS_OTA_SRCS_H += include/ota_base64_private.h
AWS_OTA_SRCS_H += include/ota_cbor_private.h
AWS_OTA_SRCS_H += include/ota_config_defaults.h
AWS_OTA_SRCS_H += include/ota.h
AWS_OTA_SRCS_H += include/ota_http_interface.h
AWS_OTA_SRCS_H += include/ota_http_private.h
AWS_OTA_SRCS_H += include/ota_interface_private.h
AWS_OTA_SRCS_H += include/ota_mqtt_interface.h
AWS_OTA_SRCS_H += include/ota_mqtt_private.h
AWS_OTA_SRCS_H += include/ota_os_interface.h
AWS_OTA_SRCS_H += include/ota_platform_interface.h
AWS_OTA_SRCS_H += include/ota_private.h

MMIOT_SRCS_C += $(addprefix $(AWS_OTA_DIR)/,$(AWS_OTA_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_OTA_DIR)/,$(AWS_OTA_SRCS_H))

CFLAGS-$(AWS_OTA_DIR) += -Wno-c++-compat -Wno-error

MMIOT_INCLUDES += $(AWS_OTA_DIR)/include
MMIOT_INCLUDES += $(AWS_OTA_DIR)/portable/os
