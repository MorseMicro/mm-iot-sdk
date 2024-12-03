/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

#include "FreeRTOS_Sockets.h"

#define AF_UNSPEC               (0)
#define AF_INET                 FREERTOS_AF_INET
#define AF_INET6                FREERTOS_AF_INET6

#define SOCK_DGRAM              FREERTOS_SOCK_DGRAM
#define IPPROTO_UDP             FREERTOS_IPPROTO_UDP
#define SOCK_STREAM             FREERTOS_SOCK_STREAM
#define IPPROTO_TCP             FREERTOS_IPPROTO_TCP


#define inet_ntop(_family, _source, _dest, _size) \
    FreeRTOS_inet_ntop(_family, _source, _dest, _size)
#define inet_pton(_family, _source, _dest) \
    FreeRTOS_inet_pton(_family, _source, _dest)

#define sockaddr_in freertos_sockaddr
#define sockaddr_in6 freertos_sockaddr

#ifndef sin_addr
#define sin_addr    sin_address.ulIP_IPv4
#endif

#ifndef sin6_addr
#define sin6_addr    sin_address.xIP_IPv6.ucBytes
#endif
