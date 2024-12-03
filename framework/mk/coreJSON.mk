#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

CORE_JSON_DIR = src/freertos-libs/coreJSON/source

# C files to be included while compiling
CORE_JSON_SRCS_C += core_json.c

# Header files for dependancy checking
CORE_JSON_SRCS_H += include/core_json.h

MMIOT_SRCS_C += $(addprefix $(CORE_JSON_DIR)/,$(CORE_JSON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CORE_JSON_DIR)/,$(CORE_JSON_SRCS_H))

MMIOT_INCLUDES += $(CORE_JSON_DIR)/include
