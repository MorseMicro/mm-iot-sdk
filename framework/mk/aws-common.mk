#
# Copyright 2023-2025 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_COMMON_DIR = src/aws/common

# C files to be included while compiling
AWS_COMMON_SRCS_C += fleet_provisioning_task.c
AWS_COMMON_SRCS_C += shadow_device_task.c
AWS_COMMON_SRCS_C += mqtt_agent_config.c
AWS_COMMON_SRCS_C += ota_update_task.c
AWS_COMMON_SRCS_C += ota_pal.c
AWS_COMMON_SRCS_C += ota_os_mmosal.c
AWS_COMMON_SRCS_C += fleet_provisioning_serializer.c

# Header files for dependency checking
AWS_COMMON_SRCS_H += include/fleet_provisioning_serializer.h
AWS_COMMON_SRCS_H += include/fleet_provisioning_task.h
AWS_COMMON_SRCS_H += include/ota_os_mmosal.h
AWS_COMMON_SRCS_H += include/ota_pal.h
AWS_COMMON_SRCS_H += include/ota_update_task.h
AWS_COMMON_SRCS_H += include/shadow_device_task.h

MMIOT_SRCS_C += $(addprefix $(AWS_COMMON_DIR)/,$(AWS_COMMON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_COMMON_DIR)/,$(AWS_COMMON_SRCS_H))

CFLAGS-$(AWS_COMMON_DIR) += -Wno-c++-compat -Wno-error

BUILD_DEFINES += otaconfigOTA_FILE_TYPE=FILE

MMIOT_INCLUDES += $(AWS_COMMON_DIR)/include
MMIOT_INCLUDES += $(AWS_COMMON_DIR)/portable/os
