#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_DEFENDER_DIR = src/aws/AWS-Defender/source

# C files to be included while compiling
AWS_DEFENDER_SRCS_C += defender.c

# Header files for dependancy checking
AWS_DEFENDER_SRCS_H += include/defender.h
AWS_DEFENDER_SRCS_H += include/defender_config_defaults.h

MMIOT_SRCS_C += $(addprefix $(AWS_DEFENDER_DIR)/,$(AWS_DEFENDER_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_DEFENDER_DIR)/,$(AWS_DEFENDER_SRCS_H))

MMIOT_INCLUDES += $(AWS_DEFENDER_DIR)/include
