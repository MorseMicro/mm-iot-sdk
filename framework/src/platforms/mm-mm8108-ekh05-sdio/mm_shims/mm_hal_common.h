/**
 * Copyright 2024-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 */

#pragma once

#include "mmhal.h"
#include "mmosal.h"
#include "main.h"

/** Example hardware version string used for @c mmhal_get_hardware_version() */
#define MMHAL_HARDWARE_VERSION "MM-MM8108-EKH05-SDIO"

/**
 * Deep sleep veto IDs used by mmhal. These must be in the range between
 *  MMHAL_VETO_ID_HAL_MIN and MMHAL_VETO_ID_HAL_MAX (inclusive).
 */
enum mmhal_deep_sleep_veto_id
{
    /** UART HAL deep sleep veto. */
    MMHAL_VETO_ID_HAL_UART = MMHAL_VETO_ID_HAL_MIN,
};
