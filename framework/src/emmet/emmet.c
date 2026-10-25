/*
 * Copyright 2021-2024 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include <stdarg.h>
#include "emmet_private.h"
#include "mmhal_core.h"

static void *emmet_alloc(void *allocator_data, size_t size)
{
    MM_UNUSED(allocator_data);
    return mmosal_malloc(size);
}

static void emmet_free(void *allocator_data, void *pointer)
{
    MM_UNUSED(allocator_data);
    mmosal_free(pointer);
}

static struct ProtobufCAllocator emmet_protoc_allocator = {
    .alloc = emmet_alloc,
    .free = emmet_free,
};

void emmet_pack_rsp(struct emmet_raw_rsp *raw_rsp, const Emmet__DutRsp *dutrsp)
{
    size_t len = emmet__dut_rsp__get_packed_size(dutrsp);
    MMOSAL_ASSERT(len > 0);

    if (len > raw_rsp->buf_size)
    {
        MMLOG_WRN("Emmet response too long (%u > %u)\n", len, raw_rsp->buf_size);
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__RESPONSE_TOO_LONG,
                             "(%u > %u)",
                             len,
                             raw_rsp->buf_size);
        return;
    }
    raw_rsp->rsp_len = emmet__dut_rsp__pack(dutrsp, raw_rsp->rsp_buf);
}

void emmet_pack_basic_rsp(struct emmet_raw_rsp *raw_rsp,
                          Emmet__StatusCode status_code,
                          const char *fmt,
                          ...)
{
    char buf[100];
    if (fmt != NULL)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
    }
    else
    {
        buf[0] = '\0';
    }

    Emmet__BasicRsp basic = EMMET__BASIC_RSP__INIT;
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    basic.code = status_code;
    basic.message = buf;
    dutrsp.basic = &basic;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_BASIC;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

void emmet_rx(struct emmet_raw_rsp *raw_rsp, const uint8_t *cmd, size_t cmd_len)
{
    Emmet__DutCmd *dut_cmd = emmet__dut_cmd__unpack(&emmet_protoc_allocator, cmd_len, cmd);
    if (dut_cmd != NULL)
    {
        switch (dut_cmd->cmdgrp_case)
        {
            case EMMET__DUT_CMD__CMDGRP_SYS:
                emmet_handle_syscmd(raw_rsp, dut_cmd->sys);
                break;

            case EMMET__DUT_CMD__CMDGRP_LWIP:
                emmet_handle_lwipcmd(raw_rsp, dut_cmd->lwip);
                break;

            case EMMET__DUT_CMD__CMDGRP_WLAN:
                emmet_handle_wlancmd(raw_rsp, dut_cmd->wlan);
                break;

            case EMMET__DUT_CMD__CMDGRP_CAPI:
                emmet_handle_capicmd(raw_rsp, dut_cmd->capi);
                break;

            case EMMET__DUT_CMD__CMDGRP_CONFIG:
                emmet_handle_cfgcmd(raw_rsp, dut_cmd->config);
                break;

            case EMMET__DUT_CMD__CMDGRP_FILE:
                emmet_handle_filecmd(raw_rsp, dut_cmd->file);
                break;

            case EMMET__DUT_CMD__CMDGRP_HAL:
                emmet_handle_halcmd(raw_rsp, dut_cmd->hal);
                break;

            default:
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__INVALID_COMMAND,
                                     "Invalid command");
                break;
        }
        emmet__dut_cmd__free_unpacked(dut_cmd, &emmet_protoc_allocator);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__MALFORMED_PACKET, "Malformed command");
    }
}

void emmet_init(void)
{
    MMLOG_INF("Initialising Emmet...\n");
    emmet_capi_init();
    emmet_lwip_init();
    emmet_wlan_init();
    emmet_transport_init(emmet_rx);
}

void emmet_start(void)
{
    emmet_wlan_start();
    mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_EMMET);
    emmet_transport_start();
}
