/*
 * Copyright 2023-2026 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmosal.h"
#include "mmutils.h"
#include "mmwlan_stats.h"

#include "core/autogen/mmagic_core_sys.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_sys.h"

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

static void display_version_string32(EmbeddedCli *cli,
                                     const char *label,
                                     const struct string32 *version)
{
    char str32[33];
    mmagic_string32_to_string(version, str32, sizeof(str32));
    mmagic_cli_printf(cli, "%s%s", label, str32[0] != '\0' ? str32 : "unavailable");
}

void mmagic_cli_sys_get_version(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    struct mmagic_core_sys_get_version_rsp_args rsp = {};

    enum mmagic_status status = mmagic_core_sys_get_version(&ctx->core, &rsp);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Get version", status);
        return;
    }

    display_version_string32(cli, "Application Version: ", &rsp.results.application_version);
    display_version_string32(cli, "Bootloader Version: ", &rsp.results.bootloader_version);
    display_version_string32(cli, "User Hardware Version: ", &rsp.results.user_hardware_version);
    display_version_string32(cli, "Morse FW Version: ", &rsp.results.morse_firmware_version);
    display_version_string32(cli, "Morse SDK Version: ", &rsp.results.morselib_version);
    display_version_string32(cli, "Morse HW Version: ", &rsp.results.morse_hardware_version);
}

#define MMAGIC_CLI_SYS_GET_STATS_HINT "sys-get_stats [format] [host|mac|phy|umac] [reset]"

void mmagic_cli_sys_get_stats(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(context);
    MM_UNUSED(args);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    struct mmagic_core_sys_get_stats_cmd_args cmd_args = { .subsystem = MMAGIC_SUBSYSTEM_ID_HOST,
                                                           .reset = false };
    struct mmagic_core_sys_get_stats_rsp_args rsp = { 0 };
    char printf_buffer[MMAGIC_CLI_PRINT_BUF_LEN + 1] = { 0 };
    int idx;
    bool sys_get_stats_format = false;
#define TLV_HEADER_SIZE 4

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens > 3)
    {
        embeddedCliPrint(cli, "Too many arguments");
        embeddedCliPrint(cli, MMAGIC_CLI_SYS_GET_STATS_HINT);
        return;
    }

    for (int arg_idx = 1; arg_idx <= num_tokens; arg_idx++)
    {
        const char *argument = embeddedCliGetToken(args, arg_idx);
        if (!strcmp("format", argument))
        {
            sys_get_stats_format = true;
        }
        else if (!strcmp("host", argument))
        {
            cmd_args.subsystem = MMAGIC_SUBSYSTEM_ID_HOST;
        }
        else if (!strcmp("mac", argument))
        {
            cmd_args.subsystem = MMAGIC_SUBSYSTEM_ID_MAC;
        }
        else if (!strcmp("phy", argument))
        {
            cmd_args.subsystem = MMAGIC_SUBSYSTEM_ID_PHY;
        }
        else if (!strcmp("umac", argument))
        {
            cmd_args.subsystem = MMAGIC_SUBSYSTEM_ID_UMAC;
        }
        else if (!strcmp("reset", argument))
        {
            cmd_args.reset = true;
        }
        else if (!strcmp("help", argument))
        {
            embeddedCliPrint(cli, MMAGIC_CLI_SYS_GET_STATS_HINT);
            return;
        }
        else
        {
            embeddedCliPrint(cli, "Unrecognised argument");
            embeddedCliPrint(cli, MMAGIC_CLI_SYS_GET_STATS_HINT);
            return;
        }
    }

    enum mmagic_status status = mmagic_core_sys_get_stats(&ctx->core, &cmd_args, &rsp);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Get stats", status);
        return;
    }

    if (!sys_get_stats_format)
    {
        /* Output as unformatted hex bytes (multi-line) */
        idx = 0;
        while (idx < rsp.buffer.len)
        {
            idx += mmagic_uint8_t_to_hexstring(&rsp.buffer.data[idx],
                                               rsp.buffer.len - idx,
                                               printf_buffer,
                                               MMAGIC_CLI_PRINT_BUF_LEN);
            embeddedCliPrint(cli, printf_buffer);
        }
    }
    else if (cmd_args.subsystem == MMAGIC_SUBSYSTEM_ID_UMAC)
    {
        /* Output lightly formatted umac stats */
        struct mmwlan_stats_umac_data stats_umac;
        struct mmwlan_stats_umac_data *data = &stats_umac;

        memcpy(&stats_umac, rsp.buffer.data, sizeof(stats_umac));
        mmagic_cli_printf(
            cli,
            "%lu %lu %lu [ %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu ] %d %u %u %u %u %u "
            "%lu %lu %lu %u %lu %lu %lu %lu %lu %lu %lu %lu",
            data->last_tx_time,
            data->datapath_rxq_frames_dropped,
            data->datapath_txq_frames_dropped,
            data->connect_timestamp[0],
            data->connect_timestamp[1],
            data->connect_timestamp[2],
            data->connect_timestamp[3],
            data->connect_timestamp[4],
            data->connect_timestamp[5],
            data->connect_timestamp[6],
            data->connect_timestamp[7],
            data->connect_timestamp[8],
            data->connect_timestamp[9],
            data->rssi,
            data->hw_restart_counter,
            data->num_scans_complete,
            data->datapath_rxq_high_water_mark,
            data->datapath_txq_high_water_mark,
            data->datapath_rx_mgmt_q_high_water_mark,
            data->datapath_rx_ccmp_failures,
            data->datapath_driver_rx_alloc_failures,
            data->datapath_driver_rx_read_failures,
            data->datapath_rx_reorder_list_high_water_mark,
            data->datapath_rx_reorder_overflow,
            data->datapath_rx_reorder_timedout,
            data->datapath_rx_reorder_outdated_drops,
            data->datapath_rx_reorder_retransmit_drops,
            data->datapath_rx_reorder_total,
            data->timeouts_fired,
            data->datapath_driver_tx_skbq_timeout,
            data->datapath_driver_tx_pending_status_timeout);
    }
    else
    {
        /* Output each TLV individually */
        idx = 0;
        while (idx < rsp.buffer.len)
        {
            uint16_t tlv_tag = rsp.buffer.data[idx + 0] | (rsp.buffer.data[idx + 1] << 8);
            uint16_t tlv_length = rsp.buffer.data[idx + 2] | (rsp.buffer.data[idx + 3] << 8);

            mmagic_cli_printf(cli,
                              "Index %u/%u (%#x/%#x) Tag: %#02x (%u) Length: %#02x (%u)",
                              idx,
                              rsp.buffer.len,
                              idx,
                              rsp.buffer.len,
                              tlv_tag,
                              tlv_tag,
                              tlv_length,
                              tlv_length);

            int max_length = (rsp.buffer.len - idx);
            if (max_length > tlv_length + TLV_HEADER_SIZE)
            {
                max_length = tlv_length + TLV_HEADER_SIZE;
            }

            int tlv_idx = 0;
            while (tlv_idx < max_length)
            {
                tlv_idx += mmagic_uint8_t_to_hexstring(&rsp.buffer.data[idx + tlv_idx],
                                                       max_length - tlv_idx,
                                                       printf_buffer,
                                                       MMAGIC_CLI_PRINT_BUF_LEN);
                embeddedCliPrint(cli, printf_buffer);
            }

            idx += tlv_idx;
        }
    }
}
