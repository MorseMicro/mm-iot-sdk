/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

#include "FreeRTOS_DNS.h"

#define addrinfo freertos_addrinfo

#define getaddrinfo(_name, _service, _hints, _result) \
    FreeRTOS_getaddrinfo(_name, _service, _hints, _result)

#define freeaddrinfo(_info) FreeRTOS_freeaddrinfo(_info)
