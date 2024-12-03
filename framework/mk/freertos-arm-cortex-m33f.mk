#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

FREERTOS_CM33F_DIR = src/freertos/portable/GCC/ARM_CM33_NTZ/non_secure

FREERTOS_CM33F_SRCS_C += port.c
FREERTOS_CM33F_SRCS_C += portasm.c

FREERTOS_CM33F_SRCS_H += portmacro.h
FREERTOS_CM33F_SRCS_H += portasm.h

MMIOT_SRCS_C += $(addprefix $(FREERTOS_CM33F_DIR)/,$(FREERTOS_CM33F_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(FREERTOS_CM33F_DIR)/,$(FREERTOS_CM33F_SRCS_H))

MMIOT_INCLUDES += $(FREERTOS_CM33F_DIR)
