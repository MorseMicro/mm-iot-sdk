/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>

#include "mmhal_core.h"
#include "mmosal.h"
#include "mmutils.h"

#include "main.h"

static volatile atomic_uint_fast32_t deep_sleep_vetos = 0;
static struct mmosal_mutex *rng_mutex = NULL;

void mmhal_random_init(void)
{
    MMOSAL_DEV_ASSERT(rng_mutex == NULL);
    if (rng_mutex != NULL)
    {
        return;
    }
    rng_mutex = mmosal_mutex_create("rng");
    MMOSAL_ASSERT(rng_mutex != NULL);
}

uint32_t mmhal_random_u32(uint32_t min, uint32_t max)
{
    mmosal_mutex_get(rng_mutex, UINT32_MAX);

    LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_CLK48);
    LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);

    /* Peripheral clock enable */
    LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_RNG);

    LL_RNG_Enable(RNG);

    /* Wait until valid random data is available */
    while (!LL_RNG_IsActiveFlag_DRDY(RNG))
    {
        /* Check if error occurs */
        if ((LL_RNG_IsActiveFlag_CECS(RNG)) || (LL_RNG_IsActiveFlag_SECS(RNG)))
        {
            LL_RNG_ClearFlag_CEIS(RNG);
        }
    }

    uint32_t rand = LL_RNG_ReadRandData32(RNG);

    LL_RNG_Disable(RNG);

    mmosal_mutex_release(rng_mutex);

    if (min != 0 || max != UINT32_MAX)
    {
        /* Warning: this does not guarantee uniformly distributed random numbers */
        rand = rand % (max - min) + min;
    }

    return rand;
}

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}
