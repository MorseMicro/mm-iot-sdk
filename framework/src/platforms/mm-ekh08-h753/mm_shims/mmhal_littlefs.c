/*
 * Copyright 2024-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * This file provides the hardware interface for the LittleFS filesystem.
 */

#include "mmhal_flash.h"

const struct lfs_config *mmhal_get_littlefs_config(void)
{
    return NULL;
}
