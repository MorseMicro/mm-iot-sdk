/*
 * Copyright 2023 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include "emmet_private.h"
#include "emmet-api.pb-c.h"
#include "mmconfig.h"

void emmet_pack_cfgrsp(struct emmet_raw_rsp *raw_rsp, Emmet__CfgRsp *cfgrsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.config = cfgrsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_CONFIG;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void handle_read(struct emmet_raw_rsp *raw_rsp, Emmet__CfgReadCmd *read_cmd)
{
    Emmet__CfgReadRsp read_rsp = EMMET__CFG_READ_RSP__INIT;
    Emmet__CfgRsp cfgrsp = EMMET__CFG_RSP__INIT;

    void *buffer;
    read_rsp.len = mmconfig_alloc_and_load(read_cmd->key, &buffer);
    if (read_rsp.len >= 0)
    {
        read_rsp.payload.len = read_rsp.len;
        read_rsp.payload.data = (uint8_t *)buffer;
    }
    else
    {
        read_rsp.payload.len = 0;
        read_rsp.payload.data = NULL;
    }

    cfgrsp.read = &read_rsp;
    cfgrsp.rsp_case = EMMET__CFG_RSP__RSP_READ;
    emmet_pack_cfgrsp(raw_rsp, &cfgrsp);

    if (buffer)
    {
        mmosal_free(buffer);
    }
}

static void handle_write(struct emmet_raw_rsp *raw_rsp, Emmet__CfgWriteCmd *write_cmd)
{
    if (mmconfig_write_data(write_cmd->key,
                            (const void *)write_cmd->payload.data,
                            write_cmd->payload.len) >= 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
    }
}

static void handle_del(struct emmet_raw_rsp *raw_rsp, Emmet__CfgDelCmd *del_cmd)
{
    if (mmconfig_write_data(del_cmd->key, NULL, 0) >= 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
    }
}

static void handle_erase(struct emmet_raw_rsp *raw_rsp, Emmet__CfgEraseCmd *erase_cmd)
{
    MM_UNUSED(erase_cmd);

    if (mmconfig_eraseall() == 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
    }
}

void emmet_handle_cfgcmd(struct emmet_raw_rsp *raw_rsp, Emmet__CfgCmd *cfgcmd)
{
    switch (cfgcmd->cmd_case)
    {
        case EMMET__CFG_CMD__CMD_READ:
            handle_read(raw_rsp, cfgcmd->read);
            break;

        case EMMET__CFG_CMD__CMD_WRITE:
            handle_write(raw_rsp, cfgcmd->write);
            break;

        case EMMET__CFG_CMD__CMD_DEL:
            handle_del(raw_rsp, cfgcmd->del);
            break;

        case EMMET__CFG_CMD__CMD_ERASE:
            handle_erase(raw_rsp, cfgcmd->erase);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
