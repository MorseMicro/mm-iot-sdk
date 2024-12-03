/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * This file provides the hardware interface for the LittleFS filesystem.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "mmekh08_hal_common.h"
#include "mmhal_flash.h"
#include "mmutils.h"

#include "lfs.h"

#define LFS_CACHE_SIZE          256
#define LFS_LOOKAHEAD_SIZE      32

struct mmhal_littlefs_context
{
    uint32_t partition_address;
    struct mmosal_mutex *lfs_mutex;
};

/**
 * Read a region in a block.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @param block  The block to read.
 * @param off    The offset within the block to read.
 * @param buffer The buffer to read into.
 * @param size   The number of bytes to read.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_read(const struct lfs_config *c, lfs_block_t block,
                        lfs_off_t off, void *buffer, lfs_size_t size)
{
    struct mmhal_littlefs_context *context = (struct mmhal_littlefs_context*) c->context;

    mmhal_flash_read(context->partition_address + (c->block_size * block) + off,
                     (uint8_t*) buffer, size);

    return LFS_ERR_OK;
}

/**
 * Program a region in a block. The block must have previously been erased.
 * May return LFS_ERR_CORRUPT if the block should be considered bad.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @param block  The block to write.
 * @param off    The offset within the block to write.
 * @param buffer The buffer to write from.
 * @param size   The number of bytes to write.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_write(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t off, const void *buffer, lfs_size_t size)
{
    struct mmhal_littlefs_context *context = (struct mmhal_littlefs_context*) c->context;

    mmhal_flash_write(context->partition_address + (c->block_size * block) + off,
                      (const uint8_t*) buffer, size);

    return LFS_ERR_OK;
}

/**
 * Erase a block. A block must be erased before being programmed.
 * The state of an erased block is undefined. May return LFS_ERR_CORRUPT
 * if the block should be considered bad.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @param block  The block to erase.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_erase(const struct lfs_config *c, lfs_block_t block)
{
    struct mmhal_littlefs_context *context = (struct mmhal_littlefs_context*) c->context;

    mmhal_flash_erase(context->partition_address + (c->block_size * block));

    return LFS_ERR_OK;
}

/**
 * Sync the state of the underlying block device.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_sync(const struct lfs_config *c)
{
    MM_UNUSED(c);

    return LFS_ERR_OK;
}

/**
 * Lock the underlying block device for mutual exclusion.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_lock(const struct lfs_config *c)
{
    struct mmhal_littlefs_context *context = (struct mmhal_littlefs_context*) c->context;

    mmosal_mutex_get(context->lfs_mutex, UINT32_MAX);

    return LFS_ERR_OK;
}

/**
 * Unlock the underlying block device for mutual exclusion.
 *
 * @param c      Pointer to the @c lfs_config structure.
 * @return       LFS_ERR_OK (0) on success, negative values on error.
 */
int mmhal_littlefs_unlock(const struct lfs_config *c)
{
    struct mmhal_littlefs_context *context = (struct mmhal_littlefs_context*) c->context;

    mmosal_mutex_release(context->lfs_mutex);

    return LFS_ERR_OK;
}

const struct lfs_config* mmhal_get_littlefs_config(void)
{
    /* These are static */
    static struct lfs_config lfs_conf  = { 0 };
    static struct mmhal_littlefs_context lfs_context = { 0 };
    /* Start of FILESYSTEM region in flash. */
    extern uint8_t filesystem_start;
    /* End of FILESYSTEM region in flash. */
    extern uint8_t filesystem_end;

    /* Set operating parameters */
    lfs_conf.block_size = mmhal_flash_getblocksize((uint32_t)&filesystem_start);
    MMOSAL_ASSERT(lfs_conf.block_size != 0);
    lfs_conf.block_count = (&filesystem_end - &filesystem_start) / lfs_conf.block_size;
    lfs_conf.read_size = 16;
    lfs_conf.prog_size = 16;
    lfs_conf.block_cycles = 100;

    /* Specify buffer sizes */
    lfs_conf.cache_size = LFS_CACHE_SIZE;
    lfs_conf.lookahead_size = LFS_LOOKAHEAD_SIZE;

    /* Setup context */
    lfs_context.partition_address = (uint32_t) &filesystem_start;
    lfs_context.lfs_mutex = mmosal_mutex_create("littlefs_mutex");
    lfs_conf.context = (void*) &lfs_context;

    /* Set HAL interface */
    lfs_conf.read   = mmhal_littlefs_read;
    lfs_conf.prog   = mmhal_littlefs_write;
    lfs_conf.erase  = mmhal_littlefs_erase;
    lfs_conf.sync   = mmhal_littlefs_sync;
    lfs_conf.lock   = mmhal_littlefs_lock;
    lfs_conf.unlock = mmhal_littlefs_unlock;

    return &lfs_conf;
}
