#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_FLEET_PROVISIONING_DIR = src/aws/AWS-Fleet-Provisioning/source

# C files to be included while compiling
AWS_FLEET_PROVISIONING_SRCS_C += fleet_provisioning.c

# Header files for dependancy checking
AWS_FLEET_PROVISIONING_SRCS_H += include/fleet_provisioning.h
AWS_FLEET_PROVISIONING_SRCS_H += include/fleet_provisioning_config_defaults.h

MMIOT_SRCS_C += $(addprefix $(AWS_FLEET_PROVISIONING_DIR)/,$(AWS_FLEET_PROVISIONING_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_FLEET_PROVISIONING_DIR)/,$(AWS_FLEET_PROVISIONING_SRCS_H))

CFLAGS-$(AWS_FLEET_PROVISIONING_DIR) += -Wno-c++-compat -Wno-error

MMIOT_INCLUDES += $(AWS_FLEET_PROVISIONING_DIR)/include
