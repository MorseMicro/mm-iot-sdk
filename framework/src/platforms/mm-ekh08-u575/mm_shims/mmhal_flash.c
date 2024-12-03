/**
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 * This File implements BSP specific shims for accessing the flash.
 */

#include <string.h>

#include "mmekh08_hal_common.h"


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

    /* ST Requires ICACHE be disabled before programming Flash on the U5 */
    MMOSAL_ASSERT(HAL_ICACHE_Disable() == HAL_OK);

    HAL_FLASH_Unlock();

    if ((block_address >= FLASH_BASE) && (block_address < FLASH_BASE + FLASH_SIZE))
    {
        FLASH_EraseInitTypeDef erase_params;
        uint32_t erase_status;

        if (block_address < FLASH_BASE + FLASH_BANK_SIZE)
        {
            erase_params.Page = (block_address - FLASH_BASE) / FLASH_PAGE_SIZE;
            erase_params.Banks = FLASH_BANK_1;
        }
        else
        {
            erase_params.Page = (block_address - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
            erase_params.Banks = FLASH_BANK_2;
        }

        erase_params.TypeErase = FLASH_TYPEERASE_PAGES;
        erase_params.NbPages = 1;

        if (HAL_FLASHEx_Erase(&erase_params, &erase_status) != HAL_OK)
        {
            retval = -1;
        }
    }

    HAL_FLASH_Lock();

    /* Reenable ICACHE */
    MMOSAL_ASSERT(HAL_ICACHE_Enable() == HAL_OK);

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

    /* ST Requires ICACHE be disabled before programming Flash on the U5 */
    MMOSAL_ASSERT(HAL_ICACHE_Disable() == HAL_OK);

    HAL_FLASH_Unlock();

    if ((write_address >= FLASH_BASE) && (write_address + size < FLASH_BASE + FLASH_SIZE))
    {
        /* U575 supports writing only QWords (16 bytes), so if we are not QWord aligned... */
        if (write_address & 0xF)
        {
            /* Calculate the qword-aligned address */
            uint32_t qword_address = write_address & 0xFFFFFFF0;
            uint8_t qword_data[16];
            uint32_t byte_offset = write_address & 0x0F;

            /* Fill qword buffer with erase values - this will retain existing flash contents */
            memset(qword_data, 0xFF, 16);

            /* Merge existing flash qword with incomming offset data */
            for (int i = byte_offset; i < 16 && size > 0; i++)
            {
                qword_data[i] = *data++;            /* Replace specific bytes to be programmed */
                size--;
                write_address++;
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, qword_address, (uint32_t)qword_data);
        }

        /* Now write remaining QWords upto 128 byte boundary*/
        while ((size >= 16) && (write_address & 0xFFFFFF80))
        {
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_address, (uint32_t)data);
            write_address += 16;
            data += 16;
            size -= 16;
        }

        /* If we have more than 128 bytes, try Turbo-Boost*/
        while (size >= 128)
        {
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_BURST, write_address, (uint32_t)data);
            write_address += 128;
            data += 128;
            size -= 128;
        }

        /* Now write remaining QWords */
        while (size >= 16)
        {
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_address, (uint32_t)data);
            write_address += 16;
            data += 16;
            size -= 16;
        }

        /* Check that we are not ending on a non-qword address */
        if (size != 0)
        {
            /* perform final unaligned write */
            uint8_t qword_data[16];

            /* Fill qword buffer with erase values - this will retain existing flash contents */
            memset(qword_data, 0xFF, 16);

            /* Merge existing flash qword with incomming offset data */
            size_t i;
            for (i = 0; i < size; i++)
            {
                qword_data[i] = *data++;            /* Replace specific bytes to be programmed */
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_address, (uint32_t)qword_data);
        }

        retval = 0;
    }

    HAL_FLASH_Lock();

    /* Reenable ICACHE */
    MMOSAL_ASSERT(HAL_ICACHE_Enable() == HAL_OK);

    return retval;
}
