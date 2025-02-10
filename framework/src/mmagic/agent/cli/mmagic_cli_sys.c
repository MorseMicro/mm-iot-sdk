/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_sys.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_sys.h"

/* This should be included after all the header files */
#include "cli/autogen/mmagic_cli_sys.def"

void mmagic_cli_sys_reset(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    mmagic_core_sys_reset(&ctx->core);
}

#define MMAGIC_CLI_SYS_DEEP_SLEEP_HINT "sys-deep_sleep <disabled|one_shot|hardware>"

void mmagic_cli_sys_deep_sleep(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 1)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        embeddedCliPrint(cli, MMAGIC_CLI_SYS_DEEP_SLEEP_HINT);
        return;
    }
    struct mmagic_core_sys_deep_sleep_cmd_args cmd_args = {};

    const char *argument = embeddedCliGetToken(args, 1);
    if (!strcmp("disabled", argument))
    {
        cmd_args.mode = MMAGIC_DEEP_SLEEP_MODE_DISABLED;
    }
    else if (!strcmp("one_shot", argument))
    {
        cmd_args.mode = MMAGIC_DEEP_SLEEP_MODE_ONE_SHOT;
    }
    else if (!strcmp("hardware", argument))
    {
        cmd_args.mode = MMAGIC_DEEP_SLEEP_MODE_HARDWARE;
    }
    else
    {
        embeddedCliPrint(cli, "Unrecognised argument");
        embeddedCliPrint(cli, MMAGIC_CLI_SYS_DEEP_SLEEP_HINT);
        return;
    }

    if (mmagic_core_sys_deep_sleep(&ctx->core, &cmd_args) != MMAGIC_STATUS_OK)
    {
        mmagic_cli_printf(cli, "Deep sleep mode '%s' not supported on this platform!", argument);
    }
}

void mmagic_cli_sys_get_version(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    struct mmagic_core_sys_get_version_rsp_args rsp = { };
    char str32[33];

    enum mmagic_status status = mmagic_core_sys_get_version(&ctx->core, &rsp);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Get version", status);
        return;
    }

    mmagic_struct_string_32_to_string(&rsp.results.application_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "Application Version: %s", str32);
    mmagic_struct_string_32_to_string(&rsp.results.bootloader_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "Bootloader Version: %s", str32);
    mmagic_struct_string_32_to_string(&rsp.results.user_hardware_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "User Hardware Version: %s", str32);
    mmagic_struct_string_32_to_string(&rsp.results.morse_firmware_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "Morse FW Version: %s", str32);
    mmagic_struct_string_32_to_string(&rsp.results.morselib_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "Morse SDK Version: %s", str32);
    mmagic_struct_string_32_to_string(&rsp.results.morse_hardware_version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "Morse HW Version: %s", str32);
}
