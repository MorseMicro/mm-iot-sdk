#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

SHA256_DIR = src/sha256

MMIOT_INCLUDES += $(SHA256_DIR)

SHA256_SRCS_C += sha256.c
SHA256_SRCS_H += sha256.h

MMIOT_SRCS_C += $(addprefix $(SHA256_DIR)/,$(SHA256_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(SHA256_DIR)/,$(SHA256_SRCS_H))
