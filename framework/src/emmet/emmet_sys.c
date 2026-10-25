/*
 * Copyright 2021 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include "emmet_private.h"
#include "emmet-api.pb-c.h"

void emmet_pack_sysrsp(struct emmet_raw_rsp *raw_rsp, Emmet__SysRsp *sysrsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.sys = sysrsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_SYS;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void handle_echo(struct emmet_raw_rsp *raw_rsp, Emmet__SysEchoCmd *echo_cmd)
{
    Emmet__SysEchoRsp echo_rsp = EMMET__SYS_ECHO_RSP__INIT;
    Emmet__SysRsp sysrsp = EMMET__SYS_RSP__INIT;

    echo_rsp.payload = echo_cmd->payload;
    echo_rsp.intpayload = echo_cmd->intpayload;

    sysrsp.echo = &echo_rsp;
    sysrsp.rsp_case = EMMET__SYS_RSP__RSP_ECHO;
    emmet_pack_sysrsp(raw_rsp, &sysrsp);
}

static void trigger_failure_timer_cb(struct mmosal_timer *timer)
{
    uintptr_t failure_type = (uintptr_t)mmosal_timer_get_arg(timer);

    MMLOG_WRN("Failure timer cb %u\n", failure_type);

    switch (failure_type)
    {
        case EMMET__FAILURE_TYPE__ASSERTION:
            MMOSAL_ASSERT(false);
            break;

        case EMMET__FAILURE_TYPE__JUMP_TO_ZERO:
#ifndef SCAN_BUILD
        {
            void (*zero_fn)(void) = 0;
            zero_fn();
        }
#endif
        break;

        default:
            MMLOG_WRN("Invalid failure\n");
            break;
    }
}

static void handle_trigger_failure(struct emmet_raw_rsp *raw_rsp,
                                   Emmet__SysTriggerFailureCmd *trigger_failure_cmd)
{
    const char *failure_name = NULL;
    struct mmosal_timer *timer;

    switch (trigger_failure_cmd->failure_type)
    {
        case EMMET__FAILURE_TYPE__ASSERTION:
            failure_name = "assert";
            break;

        case EMMET__FAILURE_TYPE__JUMP_TO_ZERO:
            failure_name = "jump to zero";
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Unsupported failure type %u",
                                 trigger_failure_cmd->failure_type);
            return;
    }

    timer = mmosal_timer_create("failure",
                                1000,
                                false,
                                (void *)(uintptr_t)trigger_failure_cmd->failure_type,
                                trigger_failure_timer_cb);
    if (timer == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to create timer");
        return;
    }

    if (!mmosal_timer_start(timer))
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to start timer");
        return;
    }

    MMLOG_WRN("Triggering %s in 1 s\n", failure_name);

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_print_message(struct emmet_raw_rsp *raw_rsp,
                                 Emmet__SysPrintMessageCmd *print_message_cmd)
{
    mmosal_printf("%s\n", print_message_cmd->message);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_get_time(struct emmet_raw_rsp *raw_rsp, Emmet__SysGetTimeCmd *get_time_cmd)
{
    Emmet__SysGetTimeRsp get_time_rsp = EMMET__SYS_GET_TIME_RSP__INIT;
    Emmet__SysRsp sysrsp = EMMET__SYS_RSP__INIT;
    MM_UNUSED(get_time_cmd);

    get_time_rsp.time_ms = mmosal_get_time_ms();

    sysrsp.get_time = &get_time_rsp;
    sysrsp.rsp_case = EMMET__SYS_RSP__RSP_GET_TIME;
    emmet_pack_sysrsp(raw_rsp, &sysrsp);
}

void emmet_handle_syscmd(struct emmet_raw_rsp *raw_rsp, Emmet__SysCmd *syscmd)
{
    switch (syscmd->cmd_case)
    {
        case EMMET__SYS_CMD__CMD_ECHO:
            handle_echo(raw_rsp, syscmd->echo);
            break;

        case EMMET__SYS_CMD__CMD_TRIGGER_FAILURE:
            handle_trigger_failure(raw_rsp, syscmd->trigger_failure);
            break;

        case EMMET__SYS_CMD__CMD_PRINT_MESSAGE:
            handle_print_message(raw_rsp, syscmd->print_message);
            break;

        case EMMET__SYS_CMD__CMD_GET_TIME:
            handle_get_time(raw_rsp, syscmd->get_time);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
