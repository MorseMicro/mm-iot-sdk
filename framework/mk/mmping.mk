#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(IP_STACK),)
$(error IP_STACK not defined)
endif

MMPING_DIR = src/mmping

MMPING_SRCS_C += mmping_$(IP_STACK).c

MMPING_SRCS_H += mmping.h

MMIOT_SRCS_C += $(addprefix $(MMPING_DIR)/,$(MMPING_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMPING_DIR)/,$(MMPING_SRCS_H))

MMIOT_INCLUDES += $(MMPING_DIR)
