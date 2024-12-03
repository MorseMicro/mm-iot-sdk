/*
 * Copyright 2021-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main.h"
#include "mmhal_wlan.h"
#include "mmconfig.h"
#include "mmosal.h"

/*
 * ---------------------------------------------------------------------------------------------
 *                                      BCF Retrieval
 * ---------------------------------------------------------------------------------------------
 */

/*
 * The following implementation reads the BCF File from the config store.
 */
void mmhal_wlan_read_bcf_file(uint32_t offset, uint32_t requested_len, struct mmhal_robuf *robuf)
{
#ifdef INCLUDE_BCF_FILE_IN_APPLICATION
    /** Points to the start of the BCF binary image. Defined as part of the Makefile */
    extern uint8_t bcf_binary_start;
    /** Points to the end of the BCF binary image. Defined as part of the Makefile */
    extern uint8_t bcf_binary_end;

    size_t bcf_len = &bcf_binary_end - &bcf_binary_start;

    /* Initialise robuf */
    robuf->buf = NULL;
    robuf->len = 0;
    robuf->free_arg = NULL;
    robuf->free_cb = NULL;

    /* Sanity check */
    if (bcf_len < offset)
    {
        printf("Detected an attempt to start reading off the end of the bcf file.\n");
        return;
    }

    robuf->buf = (uint8_t*)&bcf_binary_start + offset;
    robuf->len = bcf_len - offset;
    robuf->len = (robuf->len < requested_len) ? robuf->len : requested_len;
#else
    int length;

    /* Initialise robuf */
    robuf->buf = NULL;
    robuf->len = 0;
    robuf->free_arg = NULL;
    robuf->free_cb = NULL;

    /* Check actual length of buffer required */
    length = mmconfig_read_bytes("BCF_FILE", NULL, requested_len, offset);

    /* If data returned */
    if (length > 0)
    {
        if ((uint32_t) length > requested_len)
        {
            length = (int) requested_len;
        }
        /* Allocate buffer and free callbacks */
        void *buf = mmosal_malloc(length);
        if (buf)
        {
            robuf->buf = (uint8_t*) buf;
            robuf->free_arg = buf;
            robuf->len = length;
            robuf->free_cb = mmosal_free;

            /* Read data into allocated buffer */
            mmconfig_read_bytes("BCF_FILE", buf, length, offset);
        }
        else
        {
            printf("Failed to allocate memory while loading bcf file.\n");
            MMOSAL_ASSERT(false);
        }
    }
    else if (length == MMCONFIG_ERR_OUT_OF_BOUNDS)
    {
        printf("Detected an attempt to start reading off the end of the bcf file.\n");
    }
    else if (length == MMCONFIG_ERR_NOT_FOUND)
    {
        printf("\nUnable to find BCF_FILE entry in config store\n");
        printf("Please see the Troubleshooting section of the Getting Started Guide\n");
        printf("for more information.\n\n");
    }
#endif
}

/*
 * ---------------------------------------------------------------------------------------------
 *                                    Firmware Retrieval
 * ---------------------------------------------------------------------------------------------
 */
/** Points to the start of the firmware binary image. Defined as part of the Makefile */
extern uint8_t firmware_binary_start;
/** Points to the end of the firmware binary image. Defined as part of the Makefile */
extern uint8_t firmware_binary_end;

void mmhal_wlan_read_fw_file(uint32_t offset, uint32_t requested_len, struct mmhal_robuf *robuf)
{
    uint32_t firmware_len = &firmware_binary_end - &firmware_binary_start;
    if (offset > firmware_len)
    {
        printf("Detected an attempt to start read off the end of the firmware file.\n");
        robuf->buf = NULL;
        return;
    }

    robuf->buf = (&firmware_binary_start + offset);
    firmware_len -= offset;

    robuf->len = (firmware_len < requested_len) ? firmware_len : requested_len;
}
