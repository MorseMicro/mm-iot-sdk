/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>

#include "mmhal_core.h"
#include "mm_hal_common.h"
#include "mmosal.h"

#include "main.h"

static volatile atomic_uint_fast32_t deep_sleep_vetos = 0;
static struct mmosal_mutex *rng_mutex = NULL;

extern RNG_HandleTypeDef hrng;

void mmhal_random_init(void)
{
    MMOSAL_DEV_ASSERT(rng_mutex == NULL);
    if (rng_mutex != NULL)
    {
        return;
    }
    rng_mutex = mmosal_mutex_create("rng");
    MMOSAL_ASSERT(rng_mutex != NULL);
    MMOSAL_DEV_ASSERT(HAL_RNG_GetState(&hrng) == HAL_RNG_STATE_READY);
}

uint32_t mmhal_random_u32(uint32_t min, uint32_t max)
{
    MMOSAL_DEV_ASSERT(HAL_RNG_GetState(&hrng) > HAL_RNG_STATE_RESET);
    MMOSAL_DEV_ASSERT(rng_mutex);
#define RNG_MAX_GENERATE_ATTEMPTS   (100)
    uint32_t rndm;
    uint32_t ii;
    HAL_StatusTypeDef status = HAL_ERROR;
    for (ii = 0; ii < RNG_MAX_GENERATE_ATTEMPTS; ii++)
    {
        mmosal_mutex_get(rng_mutex, UINT32_MAX);
        status = HAL_RNG_GenerateRandomNumber(&hrng, &rndm);
        mmosal_mutex_release(rng_mutex);
        if (status == HAL_OK)
        {
            break;
        }
        printf("%lu HAL_RNG_GenerateRandomNumber failed with error code %lu; retrying...\n",
               ii, hrng.ErrorCode);
    }
    MMOSAL_ASSERT(status == HAL_OK);

    /* Caution: this does not guarantee uniformly distributed random numbers. */
    if (min == 0 && max == UINT32_MAX)
    {
        return rndm;
    }
    else
    {
        return rndm % (max-min+1) + min;
    }
}

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    MMOSAL_ASSERT(veto_id < 32);
    atomic_fetch_or(&deep_sleep_vetos, 1ul << veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    MMOSAL_ASSERT(veto_id < 32);
    atomic_fetch_and(&deep_sleep_vetos, ~(1ul << veto_id));
}

uint32_t mmhal_get_deep_sleep_veto(void)
{
    return deep_sleep_vetos;
}
