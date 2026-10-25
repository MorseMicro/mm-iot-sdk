/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @ingroup MMHAL
 * @defgroup MMHAL_FS Morse Micro File System Hardware Abstraction Layer API
 *
 * This API provides abstraction from the underlying file system storage, allowing platforms to
 * supply a LittleFS configuration to the file I/O shim layer.
 *
 * @note This API is not used by morselib.
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Get LittleFS configuration.
 *
 * LittleFS initialization is done by @c littlefs_init() in @c mmosal_shim_fileio.c.
 * which in turn calls this function to fetch the hardware configuration for LittleFS
 * from the HAL layer. The LittleFS configuration will vary from platform to platform.
 * If LittleFS is not supported by the platform then we just return NULL. This function
 * returns a static pointer to @c struct @c lfs_config which is defined in @c lfs.h.
 *
 * See @c mmhal_littlefs.c for the full HAL layer implementation for your platform.
 * See @c mmosal_shim_fileio.c for the @c libc shims for LittleFS.
 * See @c README.md in the @c src/littlefs folder for detailed information on LittleFS.
 *
 * @return A static pointer to the LittleFS config structure, or NULL if not supported.
 */
const struct lfs_config *mmhal_get_littlefs_config(void);

#ifdef __cplusplus
}
#endif

/** @} */
