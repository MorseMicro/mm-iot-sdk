#
# Copyright 2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

MMAGIC_CONTROLLER_DIR = src/mmagic/controller

MMIOT_INCLUDES += $(MMAGIC_CONTROLLER_DIR)/

MMAGIC_CONTROLLER_SRCS_H += mmagic_controller.h

MMAGIC_CONTROLLER_SRCS_C += mmagic_controller.c

MMAGIC_CONTROLLER_SRCS_H += mmagic_datalink_controller.h

MMIOT_SRCS_C += $(addprefix $(MMAGIC_CONTROLLER_DIR)/,$(MMAGIC_CONTROLLER_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMAGIC_CONTROLLER_DIR)/,$(MMAGIC_CONTROLLER_SRCS_H))
