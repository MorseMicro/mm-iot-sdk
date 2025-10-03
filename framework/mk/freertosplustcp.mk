#
# Copyright 2023 Morse Micro
#
# SPDX-License-Identifier: Apache-2.0
#

FREERTOSTCP_DIR = src/freertos-libs/freertos-plus-tcp

FREERTOSTCP_SRCS_C += source/FreeRTOS_ARP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_BitConfig.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DHCP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DHCPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DNS.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DNS_Cache.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DNS_Callback.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DNS_Networking.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_DNS_Parser.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_ICMP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IP_Timers.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IP_Utils.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv4_Sockets.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv4_Utils.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv6_Sockets.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_IPv6_Utils.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_ND.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_RA.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_Routing.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_Sockets.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_Stream_Buffer.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_IP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_IP_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_IP_IPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Reception.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_State_Handling.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_State_Handling_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_State_Handling_IPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Transmission.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Transmission_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Transmission_IPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Utils.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Utils_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_Utils_IPv6.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_TCP_WIN.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_Tiny_TCP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_UDP_IP.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_UDP_IPv4.c
FREERTOSTCP_SRCS_C += source/FreeRTOS_UDP_IPv6.c

FREERTOSTCP_SRCS_C += source/portable/BufferManagement/BufferAllocation_2.c

MMIOT_SRCS_C += $(addprefix $(FREERTOSTCP_DIR)/,$(FREERTOSTCP_SRCS_C))

POSIX_COMPAT_DIR = src/freertos-libs/posix_compat

MMIOT_INCLUDES += $(FREERTOSTCP_DIR)/source/include
MMIOT_INCLUDES += $(FREERTOSTCP_DIR)/source/portable/Compiler/GCC
MMIOT_INCLUDES += $(POSIX_COMPAT_DIR)/include

CFLAGS-$(FREERTOSTCP_DIR) += -Wno-format
CFLAGS-$(FREERTOSTCP_DIR) += -Wno-error=c++-compat
CFLAGS-$(FREERTOSTCP_DIR) += -Wno-unused-parameter
CFLAGS-$(FREERTOSTCP_DIR)/source/portable/BufferManagement += -Wno-error=maybe-uninitialized

# We currently use BufferAllocation_2 which allocates on the heap. We thus
# need to increase our heap size so that we have sufficient space to allocate
# buffers. Also an extra 16K is needed for the sector memory buffer pool utilized
# by TCP windows, see prvCreateSectors().
BUILD_DEFINES += IP_STACK_HEAP=60416

BUILD_DEFINES += ipconfigUSE_IPv4=$(MMIPAL_IPV4_ENABLED)
BUILD_DEFINES += ipconfigUSE_IPv6=$(MMIPAL_IPV6_ENABLED)
