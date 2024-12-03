#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

AWS_SHADOW_DIR = src/aws/AWS-Shadow/source

# C files to be included while compiling
AWS_SHADOW_SRCS_C += shadow.c

# Header files for dependancy checking
AWS_SHADOW_SRCS_H += include/shadow.h

MMIOT_SRCS_C += $(addprefix $(AWS_SHADOW_DIR)/,$(AWS_SHADOW_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(AWS_SHADOW_DIR)/,$(AWS_SHADOW_SRCS_H))

MMIOT_INCLUDES += $(AWS_SHADOW_DIR)/include
