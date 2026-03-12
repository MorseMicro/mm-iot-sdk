#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(IP_STACK),)
$(error IP_STACK not defined)
endif

MMIPAL_DIR = src/mmipal

ifeq ($(MMIPAL_IPV4_ENABLED),)
MMIPAL_IPV4_ENABLED = 1
endif
ifeq ($(MMIPAL_IPV6_ENABLED),)
MMIPAL_IPV6_ENABLED = 0
endif
ifeq ($(ON_DEMAND_TIMERS_ENABLED),)
ON_DEMAND_TIMERS_ENABLED = 0
endif

BUILD_DEFINES += MMIPAL_IPV4_ENABLED=$(MMIPAL_IPV4_ENABLED)
BUILD_DEFINES += MMIPAL_IPV6_ENABLED=$(MMIPAL_IPV6_ENABLED)
BUILD_DEFINES += ON_DEMAND_TIMERS_ENABLED=$(ON_DEMAND_TIMERS_ENABLED)

ifeq ($(IP_STACK),lwip)
MMIPAL_SRCS_C += lwip/mmipal_lwip.c
MMIPAL_SRCS_C += lwip/mmnetif.c
MMIPAL_SRCS_H += lwip/mmnetif.h
MMIOT_INCLUDES += $(MMIPAL_DIR)/lwip
else
# Warning: FreeRTOS-plus-TCP support is deprecated and will be removed in a future release.
MMIPAL_SRCS_C += freertosplustcp/mmipal_freertosplustcp.c
endif

MMIPAL_SRCS_H += mmipal.h

MMIOT_SRCS_C += $(addprefix $(MMIPAL_DIR)/,$(MMIPAL_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMIPAL_DIR)/,$(MMIPAL_SRCS_H))

MMIOT_INCLUDES += $(MMIPAL_DIR)
