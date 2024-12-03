#
# Copyright 2023-2024 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(IP_STACK),)
$(error IP_STACK not defined)
endif

MMIPERF_DIR = src/mmiperf

MMIPERF_SRCS_C += common/mmiperf_common.c
MMIPERF_SRCS_C += common/mmiperf_data.c
MMIPERF_SRCS_C += common/mmiperf_list.c
MMIPERF_SRCS_H += common/mmiperf_private.h


ifeq ($(IP_STACK),lwip)
MMIPERF_SRCS_C += lwip/mmiperf_tcp.c
MMIPERF_SRCS_C += lwip/mmiperf_udp.c
MMIPERF_SRCS_H += lwip/mmiperf_lwip.h
else
MMIPERF_SRCS_C += freertosplustcp/mmiperf_tcp.c
MMIPERF_SRCS_C += freertosplustcp/mmiperf_udp.c
MMIPERF_SRCS_C += freertosplustcp/mmiperf_freertosplustcp_common.c
BUILD_DEFINES += MMIPERF_STACK_SIZE=640
endif

MMIPERF_SRCS_H += mmiperf.h

MMIOT_SRCS_C += $(addprefix $(MMIPERF_DIR)/,$(MMIPERF_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMIPERF_DIR)/,$(MMIPERF_SRCS_H))

MMIOT_INCLUDES += $(MMIPERF_DIR)
