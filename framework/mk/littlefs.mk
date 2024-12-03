#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

LITTLEFS_DIR = src/littlefs

LITTLEFS_SRCS_C += lfs.c
LITTLEFS_SRCS_C += lfs_util.c

LITTLEFS_SRCS_H += lfs.h
LITTLEFS_SRCS_H += lfs_util.h

MMIOT_SRCS_C += $(addprefix $(LITTLEFS_DIR)/,$(LITTLEFS_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(LITTLEFS_DIR)/,$(LITTLEFS_SRCS_H))

MMIOT_INCLUDES += $(LITTLEFS_DIR)

BUILD_DEFINES += LFS_THREADSAFE

CFLAGS-$(LITTLEFS_DIR) += -Wno-c++-compat
CFLAGS-$(LITTLEFS_DIR) += -Wno-sign-compare
