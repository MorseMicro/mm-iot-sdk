#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(IP_STACK),)
$(error IP_STACK not defined)
endif

CORE_SNTP_DIR = src/freertos-libs/coreSNTP/source
CORE_COMMON_DIR = src/freertos-libs/common

# C files to be included while compiling
CORE_SNTP_SRCS_C += core_sntp_client.c
CORE_SNTP_SRCS_C += core_sntp_serializer.c
CORE_COMMON_SRCS_C += sntp_client-$(IP_STACK).c

# Header files for dependancy checking
CORE_SNTP_SRCS_H += include/core_sntp_client.h
CORE_SNTP_SRCS_H += include/core_sntp_config_defaults.h
CORE_SNTP_SRCS_H += include/core_sntp_serializer.h
CORE_COMMON_SRCS_H += include/sntp_client.h

MMIOT_SRCS_C += $(addprefix $(CORE_SNTP_DIR)/,$(CORE_SNTP_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CORE_SNTP_DIR)/,$(CORE_SNTP_SRCS_H))
MMIOT_SRCS_C += $(addprefix $(CORE_COMMON_DIR)/,$(CORE_COMMON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(CORE_COMMON_DIR)/,$(CORE_COMMON_SRCS_H))

MMIOT_INCLUDES += $(CORE_SNTP_DIR)/include
MMIOT_INCLUDES += $(CORE_COMMON_DIR)/include
