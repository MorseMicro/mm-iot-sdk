/**
 * Copyright 2024-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 */

#pragma once

#include "mmhal_core.h"

/** Example hardware version string used for @c mmhal_get_hardware_version() */
#define MMHAL_HARDWARE_VERSION "MM-MM6108-EKH05-SDIO"

/**
 * Deep sleep veto IDs used by mmhal. These must be in the range between
 *  MMHAL_VETO_ID_HAL_MIN and MMHAL_VETO_ID_HAL_MAX (inclusive).
 */
enum mmhal_deep_sleep_veto_id
{
    /** UART HAL deep sleep veto. */
    MMHAL_VETO_ID_HAL_UART = MMHAL_VETO_ID_HAL_MIN,
};

/**
 * Retrieve the current active deep sleep vetoes.
 *
 * @returns The current active deep sleep vetoes.
 */
uint32_t mmhal_get_deep_sleep_veto(void);

/**
 * Initialize mmhal resources for random number generator
 */
void mmhal_random_init(void);
