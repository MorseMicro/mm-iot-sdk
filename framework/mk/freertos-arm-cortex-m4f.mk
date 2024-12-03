#
# Copyright 2022-2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

FREERTOS_CM4F_DIR = src/freertos/portable/GCC/ARM_CM4F
FREERTOS_CM4F_SRCS_C += port.c

FREERTOS_CM4F_SRCS_H += portmacro.h

MMIOT_SRCS_C += $(addprefix $(FREERTOS_CM4F_DIR)/,$(FREERTOS_CM4F_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(FREERTOS_CM4F_DIR)/,$(FREERTOS_CM4F_SRCS_H))

MMIOT_INCLUDES += $(FREERTOS_CM4F_DIR)
