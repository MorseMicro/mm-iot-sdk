/*
 * FreeRTOS+TCP V4.3.1
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */


/* Include Unity header */
#include <unity.h>

/* Include standard libraries */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "list.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"

BaseType_t xNetworkUp;

volatile BaseType_t xInsideInterrupt = pdFALSE;

struct xNetworkInterface * pxNetworkInterfaces = NULL;

/** @brief A list of all network end-points.  Each element has a next pointer. */
struct xNetworkEndPoint * pxNetworkEndPoints = NULL;

const MACAddress_t xLLMNR_MacAddress = { { 0x01, 0x00, 0x5e, 0x00, 0x00, 0xfc } };

const MACAddress_t xLLMNR_MacAddressIPv6 = { { 0x33, 0x33, 0x00, 0x01, 0x00, 0x03 } };

const MACAddress_t xMDNS_MacAddress = { { 0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb } };

const MACAddress_t xMDNS_MacAddressIPv6 = { { 0x33, 0x33, 0x00, 0x00, 0x00, 0xFB } };

void vPortEnterCritical( void )
{
}
void vPortExitCritical( void )
{
}

void * pvPortMalloc( size_t xNeeded )
{
    return malloc( xNeeded );
}

void vPortFree( void * ptr )
{
    free( ptr );
}
