#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MMCONFIG_DIR = src/mmconfig

MMIOT_INCLUDES += $(MMCONFIG_DIR)

MMCONFIG_SRCS_C += mmconfig.c
MMCONFIG_SRCS_H += mmconfig.h

MMIOT_SRCS_C += $(addprefix $(MMCONFIG_DIR)/,$(MMCONFIG_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMCONFIG_DIR)/,$(MMCONFIG_SRCS_H))
