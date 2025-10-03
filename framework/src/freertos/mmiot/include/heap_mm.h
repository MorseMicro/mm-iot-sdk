/*
 * Copyright 2021-2022 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef HEAP_4_5_H__
#define HEAP_4_5_H__

#include <FreeRTOS.h>

#ifndef MMOSAL_TRACK_ALLOCATIONS
void *pvPortMalloc_impl(size_t xSize) PRIVILEGED_FUNCTION;

void *pvPortRealloc_impl(void *pv, size_t xWantedSize) PRIVILEGED_FUNCTION;

void vPortFree_impl(void *pv) PRIVILEGED_FUNCTION;

#define pvPortMalloc_(xSize) pvPortMalloc_impl(xSize)
#define pvPortRealloc_(pv, xSize) pvPortRealloc_impl((pv), (xSize))
#define vPortFree_(pv) vPortFree_impl(pv)
#else
void *pvPortMalloc_dbg(size_t xSize, const char *function, unsigned linenum) PRIVILEGED_FUNCTION;

void *pvPortRealloc_dbg(void *pv, size_t xWantedSize, const char *function,
                        unsigned linenum) PRIVILEGED_FUNCTION;

void vPortFree_dbg(void *pv) PRIVILEGED_FUNCTION;

#define pvPortMalloc_(xSize) pvPortMalloc_dbg((xSize), __func__, __LINE__)
#define pvPortRealloc_(pv, xSize) pvPortRealloc_dbg((pv), (xSize), __func__, __LINE__)
#define vPortFree_(pv) vPortFree_dbg(pv)
#endif

#endif
