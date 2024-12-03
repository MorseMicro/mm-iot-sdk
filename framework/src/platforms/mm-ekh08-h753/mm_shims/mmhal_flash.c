/**
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 * @file
 * This File implements BSP specific shims for accessing the flash.
 */

#include <string.h>
#include "mmhal.h"
#include "main.h"
#include "stm32h7xx_hal_flash.h"
#include "stm32h7xx_hal_flash_ex.h"

uint32_t mmhal_flash_getblocksize(uint32_t block_address)
{
    if ((block_address >= FLASH_BASE) && (block_address < FLASH_BASE + FLASH_SIZE))
    {
        return FLASH_SECTOR_SIZE;
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

        erase_params.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase_params.Banks = 0;
        erase_params.Sector = (block_address - FLASH_BASE) / FLASH_SECTOR_SIZE;
        erase_params.NbSectors = 1;
        erase_params.VoltageRange = FLASH_VOLTAGE_RANGE_4;

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
        /* H723 flash words are 32 bytes, expecting a word aligned source */
        if (write_address & 0x1F)
        {
            /* Calculate the flash word aligned address */
            uint32_t fw_address = write_address & 0xFFFFFFE0;
            uint8_t fw_data[32];
            uint32_t byte_offset = write_address & 0x1F;

            /* Read current flashword */
            memcpy(fw_data, (void*) fw_address, 32);

            /* Merge existing flash qword with incomming offset data */
            for (int i = byte_offset; i < 32 && size > 0; i++)
            {
                fw_data[i] = *data++;            /* Replace specific bytes to be programmed */
                size--;
                write_address++;
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, fw_address,
                              (uint32_t)fw_data);
        }

        /* Now write remaining DWords */
        while (size >= 32)
        {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, write_address, (uint32_t) data);
            write_address += 32;
            data += 32;
            size -= 32;
        }

        /* Check that we are not ending on a non-fword address */
        if (size != 0)
        {
            /* perform final unaligned write */
            uint8_t fw_data[32];

            /* Read current flash word */
            memcpy(fw_data, (void*) write_address, 32);

            /* Merge existing flash word with incomming offset data */
            size_t i;
            for (i = 0; i < size; i++)
            {
                fw_data[i] = *data++;            /* Replace specific bytes to be programmed */
            }
            /* perform aligned writes */
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, write_address,
                              *((uint64_t*)fw_data));
        }

        retval = 0;
    }

    HAL_FLASH_Lock();

    return retval;
}
