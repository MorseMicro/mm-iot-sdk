#
# Copyright 2023-2026 Morse Micro
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
MMIPERF_SRCS_C += lwip/mmiperf_lwip_common.c
MMIPERF_SRCS_C += lwip/mmiperf_tcp_common.c
MMIPERF_SRCS_C += lwip/mmiperf_tcp_client.c
MMIPERF_SRCS_C += lwip/mmiperf_tcp_server.c
MMIPERF_SRCS_C += lwip/mmiperf_udp_client.c
MMIPERF_SRCS_C += lwip/mmiperf_udp_server.c
MMIPERF_SRCS_H += lwip/mmiperf_lwip.h
else
$(error Unknown IP_STACK '$(IP_STACK)' specified)
endif

MMIPERF_SRCS_H += mmiperf.h

MMIOT_SRCS_C += $(addprefix $(MMIPERF_DIR)/,$(MMIPERF_SRCS_C))
MMIOT_SRCS_H += $(addprefix $(MMIPERF_DIR)/,$(MMIPERF_SRCS_H))

MMIOT_INCLUDES += $(MMIPERF_DIR)
