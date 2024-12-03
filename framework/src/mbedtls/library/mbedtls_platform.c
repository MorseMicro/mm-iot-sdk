/*
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file mbedtls_platform.c
 * @brief Implements mbed TLS platform functions for MMOSAL.
 */

/* mbed TLS includes. */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include "threading_alt.h"
#include "mbedtls/entropy.h"
#include "mmhal.h"
#include "mmosal.h"


#ifdef MBEDTLS_THREADING_ALT

/**
 * @brief Creates a mutex.
 *
 * @param[in, out] pMutex mbedtls mutex handle.
 */
static void mbedtls_platform_mutex_init(mbedtls_threading_mutex_t * pMutex)
{
    MMOSAL_ASSERT(pMutex != NULL);

    /* Create a mutex. The name is ignored by the API below. */
    pMutex->mutexHandle = mmosal_mutex_create("mbedtls");
    MMOSAL_ASSERT(pMutex->mutexHandle != NULL);
}

/**
 * @brief Frees a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @note This function is an empty stub as nothing needs to be done to free
 * a statically allocated FreeRTOS mutex.
 */
static void mbedtls_platform_mutex_free(mbedtls_threading_mutex_t * pMutex)
{
    /* Delete the mutex */
    mmosal_mutex_delete(pMutex->mutexHandle);
}

/**
 * @brief Function to lock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 (success) is always returned as any other failure is asserted.
 */
static int mbedtls_platform_mutex_lock(mbedtls_threading_mutex_t * pMutex)
{
    bool mutexStatus = false;

    MMOSAL_ASSERT(pMutex != NULL);

    /* mutexStatus is not used if asserts are disabled. */
    ( void ) mutexStatus;

    /* This function should never fail if the mutex is initialized. */
    mutexStatus = mmosal_mutex_get(pMutex->mutexHandle, UINT32_MAX);
    MMOSAL_ASSERT(mutexStatus);

    return 0;
}

/**
 * @brief Function to unlock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 is always returned as any other failure is asserted.
 */
static int mbedtls_platform_mutex_unlock( mbedtls_threading_mutex_t * pMutex )
{
    bool mutexStatus = false;

    MMOSAL_ASSERT(pMutex != NULL);
    /* mutexStatus is not used if asserts are disabled. */
    ( void ) mutexStatus;

    /* This function should never fail if the mutex is initialized. */
    mutexStatus = mmosal_mutex_release(pMutex->mutexHandle);
    MMOSAL_ASSERT(mutexStatus);

    return 0;
}

void mbedtls_platform_threading_init(void)
{
    /* Set the mutex functions for mbed TLS thread safety. */
    mbedtls_threading_set_alt(mbedtls_platform_mutex_init,
                              mbedtls_platform_mutex_free,
                              mbedtls_platform_mutex_lock,
                              mbedtls_platform_mutex_unlock);
}

void mbedtls_platform_threading_deinit(void)
{
    mbedtls_threading_free_alt();
}

#else

void mbedtls_platform_threading_init(void)
{
}

void mbedtls_platform_threading_deinit(void)
{
}

#endif

/**
 * @brief Function to generate a random number based on a hardware poll.
 *
 * Calls the Random number generator via MMHAL
 *
 * @param[in] data Callback context.
 * @param[out] output The address of the buffer that receives the random number.
 * @param[in] len Maximum size of the random number to be generated.
 * @param[out] olen The size, in bytes, of the #output buffer.
 *
 * @return 0 if no critical failures occurred,
 */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    uint32_t random = 0;
    size_t i;
    ((void) data);

    for (i = 0; i < len; i ++)
    {
        if ((i & 0x03) == 0)
        {
            random = mmhal_random_u32(0, UINT32_MAX);
        }
        output[i] = random;
        random >>= 8;
    }
    *olen = len;
    return 0;
}

#ifdef MBEDTLS_PLATFORM_MS_TIME_ALT
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return mmosal_get_time_ms();
}
#endif
