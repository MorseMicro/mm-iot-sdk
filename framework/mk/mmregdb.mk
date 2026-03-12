#
# Copyright 2025 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

REGDB_DIR = src/mmregdb

MMIOT_INCLUDES += $(REGDB_DIR)

REGDB_SRCS_C += mmregdb.c
REGDB_SRCS_H += mmregdb.h

MMIOT_SRCS_C += $(addprefix $(REGDB_DIR)/,$(REGDB_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(REGDB_DIR)/,$(REGDB_SRCS_H))