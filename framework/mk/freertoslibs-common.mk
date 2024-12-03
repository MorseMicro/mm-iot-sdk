#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

FREERTOS_LIBS_COMMON_DIR = src/freertos-libs/common

# C files to be included while compiling
FREERTOS_LIBS_COMMON_SRCS_C += transport_interface.c

# Header files for dependancy checking
FREERTOS_LIBS_COMMON_SRCS_H += include/transport_interface.h

MMIOT_SRCS_C += $(addprefix $(FREERTOS_LIBS_COMMON_DIR)/,$(FREERTOS_LIBS_COMMON_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(FREERTOS_LIBS_COMMON_DIR)/,$(FREERTOS_LIBS_COMMON_SRCS_H))

MMIOT_INCLUDES += $(FREERTOS_LIBS_COMMON_DIR)/include

CFLAGS-src/freertos-libs += -Wno-c++-compat
CFLAGS-src/freertos-libs += -Wno-type-limits
CFLAGS-src/freertos-libs += -Wno-unused-but-set-variable
CFLAGS-src/freertos-libs += -Wno-unused-parameter
CFLAGS-src/freertos-libs += -Wno-unused-variable
