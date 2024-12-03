/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 * This File implements BSP specific shims for accessing the flash.
 */

#include <string.h>
#include "mmhal.h"
#include "bsp.h"

uint32_t mmhal_flash_getblocksize(uint32_t block_address)
{
    if ((block_address >= FLASH_BASE) && (block_address <= FLASH_END))
    {
        if (block_address < FLASH_BASE + 0x10000)
        {
            /* First 4 blocks are 16K */
            return 0x4000;
        }
        else if (block_address < FLASH_BASE + 0x20000)
        {
            /* Next block is 64K */
            return 0x10000;
        }
        else if (block_address < FLASH_BASE + 0x100000)
        {
            /* Remaining 7 blocks are 128K */
            return 0x20000;
        }
        else if (block_address < FLASH_BASE + 0x110000)
        {
            /* Next 4 blocks are 16K */
            return 0x4000;
        }
        else if (block_address < FLASH_BASE + 0x120000)
        {
            /* Next block is 64K */
            return 0x10000;
        }
        else
        {
            /* Remaining 7 blocks are 128K */
            return 0x20000;
        }
    }
    else
    {
        return 0;
    }
}

int mmhal_flash_erase(uint32_t block_address)
{
    HAL_FLASH_Unlock();
    if ((block_address >= FLASH_BASE) && (block_address <= FLASH_END))
    {
        FLASH_EraseInitTypeDef erase_params;
        uint32_t erase_status;

        /* Calculate flash Bank */
        if (block_address < FLASH_BASE + 0x100000)
        {
            erase_params.Banks = FLASH_BANK_1;
        }
        else
        {
            erase_params.Banks = FLASH_BANK_2;
        }

        /* Calculate flash Sector */
        if (block_address < FLASH_BASE + 0x4000)
        {
            erase_params.Sector = FLASH_SECTOR_0;
        }
        else if (block_address < FLASH_BASE + 0x8000)
        {
            erase_params.Sector = FLASH_SECTOR_1;
        }
        else if (block_address < FLASH_BASE + 0xC000)
        {
            erase_params.Sector = FLASH_SECTOR_2;
        }
        else if (block_address < FLASH_BASE + 0x10000)
        {
            erase_params.Sector = FLASH_SECTOR_3;
        }
        else if (block_address < FLASH_BASE + 0x20000)
        {
            erase_params.Sector = FLASH_SECTOR_4;
        }
        else if (block_address < FLASH_BASE + 0x40000)
        {
            erase_params.Sector = FLASH_SECTOR_5;
        }
        else if (block_address < FLASH_BASE + 0x60000)
        {
            erase_params.Sector = FLASH_SECTOR_6;
        }
        else if (block_address < FLASH_BASE + 0x80000)
        {
            erase_params.Sector = FLASH_SECTOR_7;
        }
        else if (block_address < FLASH_BASE + 0xA0000)
        {
            erase_params.Sector = FLASH_SECTOR_8;
        }
        else if (block_address < FLASH_BASE + 0xC0000)
        {
            erase_params.Sector = FLASH_SECTOR_9;
        }
        else if (block_address < FLASH_BASE + 0xE0000)
        {
            erase_params.Sector = FLASH_SECTOR_10;
        }
        else if (block_address < FLASH_BASE + 0x100000)
        {
            erase_params.Sector = FLASH_SECTOR_11;
        }
        else if (block_address < FLASH_BASE + 0x104000)
        {
            erase_params.Sector = FLASH_SECTOR_12;
        }
        else if (block_address < FLASH_BASE + 0x108000)
        {
            erase_params.Sector = FLASH_SECTOR_13;
        }
        else if (block_address < FLASH_BASE + 0x10C000)
        {
            erase_params.Sector = FLASH_SECTOR_14;
        }
        else if (block_address < FLASH_BASE + 0x110000)
        {
            erase_params.Sector = FLASH_SECTOR_15;
        }
        else if (block_address < FLASH_BASE + 0x120000)
        {
            erase_params.Sector = FLASH_SECTOR_16;
        }
        else if (block_address < FLASH_BASE + 0x140000)
        {
            erase_params.Sector = FLASH_SECTOR_17;
        }
        else if (block_address < FLASH_BASE + 0x160000)
        {
            erase_params.Sector = FLASH_SECTOR_18;
        }
        else if (block_address < FLASH_BASE + 0x180000)
        {
            erase_params.Sector = FLASH_SECTOR_19;
        }
        else if (block_address < FLASH_BASE + 0x1A0000)
        {
            erase_params.Sector = FLASH_SECTOR_20;
        }
        else if (block_address < FLASH_BASE + 0x1C0000)
        {
            erase_params.Sector = FLASH_SECTOR_21;
        }
        else if (block_address < FLASH_BASE + 0x1E0000)
        {
            erase_params.Sector = FLASH_SECTOR_22;
        }
        else
        {
            erase_params.Sector = FLASH_SECTOR_23;
        }

        erase_params.NbSectors = 1;
        erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        HAL_FLASHEx_Erase(&erase_params, &erase_status);
    }
    HAL_FLASH_Lock();
    return 0;
}

int mmhal_flash_read(uint32_t read_address, uint8_t *buf, size_t size)
{
    /* Stub for memory mapped flash */
    memcpy(buf, (void*) read_address, size);
    return 0;
}

int mmhal_flash_write(uint32_t write_address, const uint8_t *data, size_t size)
{
    int retval = -1;

    HAL_FLASH_Unlock();

    if ((write_address >= FLASH_BASE) && (write_address <= FLASH_END))
    {
        /* F429 supports byte writes */
        while ((write_address & 0x3) && (size > 0))
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, write_address++, (uint64_t)(*data++));
            size--;
        }

        /* Now write remaining DWords */
        while (size >= 4)
        {
            /* perform aligned writes */
            uint32_t *wordptr = (uint32_t*) data;
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_address, (uint64_t)*wordptr);
            write_address += 4;
            data += 4;
            size -= 4;
        }

        /* Now write remaining bytes */
        while (size > 0)
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, write_address++, (uint64_t)(*data++));
            size--;
        }

        retval = 0;
    }

    HAL_FLASH_Lock();

    return retval;
}
