#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MMUTILS_DIR = src/mmutils

MMUTILS_SRCS_C += mmutils_wlan.c
MMUTILS_SRCS_C += mmbuf.c
MMUTILS_SRCS_C += mmcrc.c

MMUTILS_SRCS_H += mmutils.h
MMUTILS_SRCS_H +=mmbuf.h
MMUTILS_SRCS_H +=mmcrc.h

MMIOT_SRCS_C += $(addprefix $(MMUTILS_DIR)/,$(MMUTILS_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMUTILS_DIR)/,$(MMUTILS_SRCS_H))

MMIOT_INCLUDES += $(MMUTILS_DIR)
