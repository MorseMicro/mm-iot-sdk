/*
 * Copyright 2024 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include "emmet_private.h"
#include "mmhal_app.h"

static void emmet_pack_halrsp(struct emmet_raw_rsp *raw_rsp, Emmet__HalRsp *halrsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.hal = halrsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_HAL;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void handle_set_led(struct emmet_raw_rsp *raw_rsp, Emmet__HalSetLedCmd *cmd)
{
    emmet_hal_set_led(cmd->led_id, cmd->level);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_get_button(struct emmet_raw_rsp *raw_rsp, Emmet__HalGetButtonCmd *cmd)
{
    if (cmd->button_id != 0)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Unsupported button ID");
    }

    Emmet__HalRsp rsp = EMMET__HAL_RSP__INIT;
    Emmet__HalGetButtonRsp get_button_rsp = EMMET__HAL_GET_BUTTON_RSP__INIT;
    rsp.get_button = &get_button_rsp;
    rsp.rsp_case = EMMET__HAL_RSP__RSP_GET_BUTTON;

    enum emmet_button_state button_state = emmet_hal_get_button_state();
    if (button_state == EMMET_BUTTON_PRESSED)
    {
        rsp.get_button->button_state = EMMET__HAL_BUTTON_STATE__BUTTON_PRESSED;
    }
    else
    {
        rsp.get_button->button_state = EMMET__HAL_BUTTON_STATE__BUTTON_RELEASED;
    }

    emmet_pack_halrsp(raw_rsp, &rsp);
}

static void handle_set_button(struct emmet_raw_rsp *raw_rsp, Emmet__HalSetButtonCmd *cmd)
{
    if (cmd->button_id == 0)
    {
        if (cmd->button_state == EMMET__HAL_BUTTON_STATE__BUTTON_PRESSED)
        {
            emmet_hal_trigger_button_event(EMMET_BUTTON_PRESSED);
        }
        else
        {
            emmet_hal_trigger_button_event(EMMET_BUTTON_RELEASED);
        }
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Unsupported button ID");
    }
}

void emmet_handle_halcmd(struct emmet_raw_rsp *raw_rsp, Emmet__HalCmd *halcmd)
{
    switch (halcmd->cmd_case)
    {
        case EMMET__HAL_CMD__CMD_SET_LED:
            handle_set_led(raw_rsp, halcmd->set_led);
            break;

        case EMMET__HAL_CMD__CMD_GET_BUTTON:
            handle_get_button(raw_rsp, halcmd->get_button);
            break;

        case EMMET__HAL_CMD__CMD_SET_BUTTON:
            handle_set_button(raw_rsp, halcmd->set_button);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
