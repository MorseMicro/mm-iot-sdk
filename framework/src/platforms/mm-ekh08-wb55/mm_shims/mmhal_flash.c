/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 * This File implements BSP specific shims for accessing the flash.
 */

#include <string.h>
#include "mmhal.h"
#include "main.h"
#include "stm32wbxx_hal_flash.h"
#include "stm32wbxx_hal_flash_ex.h"

uint32_t mmhal_flash_getblocksize(uint32_t block_address)
{
    if ((block_address >= FLASH_BASE) && (block_address < FLASH_BASE + FLASH_SIZE))
    {
        return FLASH_PAGE_SIZE;
    }
    else
    {
        return 0;
    }
}

int mmhal_flash_erase(uint32_t block_address)
{
    int retval = 0;
    HAL_FLASH_Unlock();
    if ((block_address >= FLASH_BASE) && (block_address < FLASH_BASE + FLASH_SIZE))
    {
        FLASH_EraseInitTypeDef erase_params;
        uint32_t erase_status;

        erase_params.TypeErase = FLASH_TYPEERASE_PAGES;
        erase_params.Page = (block_address - FLASH_BASE) / FLASH_PAGE_SIZE;
        erase_params.NbPages = 1;

        if (HAL_FLASHEx_Erase(&erase_params, &erase_status) != HAL_OK)
        {
            retval = -1;
        }
    }
    HAL_FLASH_Lock();
    return retval;
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

    if ((write_address >= FLASH_BASE) && (write_address + size < FLASH_BASE + FLASH_SIZE))
    {
        /* WB55 supports writing only DWords (8 bytes), so if we are not DWord aligned... */
        if (write_address & 0x7)
        {
            /* Calculate the qword-aligned address */
            uint32_t dword_address = write_address & 0xFFFFFFF8;
            uint8_t dword_data[8];
            uint32_t byte_offset = write_address & 0x07;

            /* Read current qword */
            memcpy(dword_data, (void*) dword_address, 8);

            /* Merge existing flash qword with incomming offset data */
            for (int i = byte_offset; i < 8 && size > 0; i++)
            {
                dword_data[i] = *data++;            /* Replace specific bytes to be programmed */
                size--;
                write_address++;
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dword_address,
                              *((uint64_t*)dword_data));
        }

        /* Now write remaining DWords */
        while (size >= 8)
        {
            uint64_t *dwordptr = (uint64_t*) data;
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_address, *dwordptr);
            write_address += 8;
            data += 8;
            size -= 8;
        }

        /* Check that we are not ending on a non-qword address */
        if (size != 0)
        {
            /* perform final unaligned write */
            uint8_t dword_data[8];

            /* Read current qword */
            memcpy(dword_data, (void*) write_address, 8);

            /* Merge existing flash qword with incomming offset data */
            size_t i;
            for (i = 0; i < size; i++)
            {
                dword_data[i] = *data++;            /* Replace specific bytes to be programmed */
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, write_address,
                              *((uint64_t*)dword_data));
        }

        retval = 0;
    }

    HAL_FLASH_Lock();

    return retval;
}
