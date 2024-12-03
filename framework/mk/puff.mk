#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

PUFF_DIR = src/puff

MMIOT_INCLUDES += $(PUFF_DIR)

PUFF_SRCS_C += puff.c
PUFF_SRCS_H += puff.h

MMIOT_SRCS_C += $(addprefix $(PUFF_DIR)/,$(PUFF_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(PUFF_DIR)/,$(PUFF_SRCS_H))
