#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_COMMON_DIR = src/aws/common

# C files to be included while compiling
AWS_COMMON_SRCS_C += fleet_provisioning_task.c
AWS_COMMON_SRCS_C += shadow_device_task.c
AWS_COMMON_SRCS_C += mqtt_agent_task.c
AWS_COMMON_SRCS_C += ota_update_task.c
AWS_COMMON_SRCS_C += ota_pal.c
AWS_COMMON_SRCS_C += ota_os_mmosal.c
AWS_COMMON_SRCS_C += command_pool.c
AWS_COMMON_SRCS_C += fleet_provisioning_serializer.c

# Header files for dependency checking
AWS_COMMON_SRCS_H += include/command_pool.h
AWS_COMMON_SRCS_H += include/core_mqtt_config.h
AWS_COMMON_SRCS_H += include/core_sntp_config.h
AWS_COMMON_SRCS_H += include/fleet_provisioning_serializer.h
AWS_COMMON_SRCS_H += include/fleet_provisioning_task.h
AWS_COMMON_SRCS_H += include/mqtt_agent_task.h
AWS_COMMON_SRCS_H += include/ota_os_mmosal.h
AWS_COMMON_SRCS_H += include/ota_pal.h
AWS_COMMON_SRCS_H += include/ota_update_task.h
AWS_COMMON_SRCS_H += include/shadow_device_task.h
AWS_COMMON_SRCS_H += include/subscription_manager.h

MMIOT_SRCS_C += $(addprefix $(AWS_COMMON_DIR)/,$(AWS_COMMON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_COMMON_DIR)/,$(AWS_COMMON_SRCS_H))

CFLAGS-$(AWS_COMMON_DIR) += -Wno-c++-compat -Wno-error

BUILD_DEFINES += otaconfigOTA_FILE_TYPE=FILE

MMIOT_INCLUDES += $(AWS_COMMON_DIR)/include
MMIOT_INCLUDES += $(AWS_COMMON_DIR)/portable/os
