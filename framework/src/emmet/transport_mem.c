/*
 * Copyright 2021 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include "emmet_private.h"

#define COMMAND_BUFFER_SIZE  (1600)
#define RESPONSE_BUFFER_SIZE (1600)
#define MAGIC_NUMBER         (0xa5defa3d)


struct emmet_hostif_table
{
    uint32_t magic_number;
    uint32_t command_buffer_address;
    uint32_t command_buffer_length;
    uint32_t command_length;
    uint32_t response_buffer_address;
    uint32_t response_length;
};

static uint8_t command_buffer[COMMAND_BUFFER_SIZE];
static uint8_t response_buffer[RESPONSE_BUFFER_SIZE];

__attribute__((
    section(".emmet_hostif_table"))) volatile struct emmet_hostif_table emmet_hostif_table = { 0 };

static emmet_transport_rx_fn_t emmet_rx_handler = NULL;

static struct mmosal_task *emmet_task_handle;

enum emmet_hostif_state
{
    HOSTIF_IDLE,
    HOSTIF_PROCESSING,
    HOSTIF_RSP_READY,
    HOSTIF_RSP_ACKED,
};

static enum emmet_hostif_state current_hostif_state(void)
{
    if (emmet_hostif_table.command_length == 0)
    {
        if (emmet_hostif_table.response_length == 0)
        {
            return HOSTIF_IDLE;
        }
        else
        {
            return HOSTIF_RSP_ACKED;
        }
    }
    else
    {
        if (emmet_hostif_table.response_length == 0)
        {
            return HOSTIF_PROCESSING;
        }
        else
        {
            return HOSTIF_RSP_READY;
        }
    }
}

static void emmet_task(void *arg)
{
    MM_UNUSED(arg);
    MMOSAL_ASSERT(emmet_rx_handler != NULL);

    emmet_hostif_table.magic_number = MAGIC_NUMBER;

    while (true)
    {
        uint32_t command_length = 0;
        while ((command_length = emmet_hostif_table.command_length) == 0)
        {
            mmosal_task_sleep(100);
        }

        struct emmet_raw_rsp raw_rsp = { .rsp_buf = response_buffer,
                                         .buf_size = sizeof(response_buffer),
                                         .rsp_len = 0 };
        emmet_rx_handler(&raw_rsp, command_buffer, command_length);
        MMOSAL_ASSERT(raw_rsp.rsp_len != 0);

        emmet_hostif_table.response_length = raw_rsp.rsp_len;

        while (emmet_hostif_table.command_length != 0)
        {
            mmosal_task_sleep(10);
        }

        MMOSAL_ASSERT(current_hostif_state() == HOSTIF_RSP_ACKED);

        emmet_hostif_table.response_length = 0;
    }
}

void emmet_transport_init(emmet_transport_rx_fn_t rx_handler)
{
    emmet_hostif_table.magic_number = 0;
    emmet_hostif_table.command_buffer_address = (uint32_t)(uintptr_t)(command_buffer);
    emmet_hostif_table.command_buffer_length = sizeof(command_buffer);
    emmet_hostif_table.command_length = 0;
    emmet_hostif_table.response_buffer_address = (uint32_t)(uintptr_t)response_buffer;
    emmet_hostif_table.response_length = 0;
    emmet_rx_handler = rx_handler;
}

void emmet_transport_start(void)
{
    emmet_task_handle = mmosal_task_create(emmet_task, 0, MMOSAL_TASK_PRI_LOW, 1024, "emmet");
    MMOSAL_ASSERT(emmet_task_handle != NULL);
}
