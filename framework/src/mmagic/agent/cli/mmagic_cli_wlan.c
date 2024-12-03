/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmlog.h"
#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_wlan.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_wlan.h"

/* This should be included after all the header files */
#include "cli/autogen/mmagic_cli_wlan.def"

/* Duration to wait for a connection to be established with the AP */
#define MMAGIC_CLI_WLAN_CMD_TIMEOUT_MS 60000

void mmagic_cli_wlan_connect(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    embeddedCliPrint(cli, "Attempting to connect, may take up to 30 seconds");
    struct mmagic_core_wlan_connect_cmd_args cmd = {
        .timeout = MMAGIC_CLI_WLAN_CMD_TIMEOUT_MS
    };
    enum mmagic_status status = mmagic_core_wlan_connect(&ctx->core, &cmd);

    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Failed to connect to AP");
        return;
    }

    embeddedCliPrint(cli, "Successfully connected to AP");
}

void mmagic_cli_wlan_disconnect(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    mmagic_core_wlan_disconnect(&ctx->core);
    embeddedCliPrint(cli, "Disconnect");
}

void mmagic_cli_wlan_scan(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);

    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    struct mmagic_core_wlan_scan_cmd_args cmd = {
        .timeout = MMAGIC_CLI_WLAN_CMD_TIMEOUT_MS,
    };
    struct mmagic_core_wlan_scan_rsp_args rsp = { };
    embeddedCliPrint(cli, "Starting Scan");
    enum mmagic_status status = mmagic_core_wlan_scan(&ctx->core, &cmd, &rsp);

    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Scan Failed");
    }

    mmagic_cli_printf(cli, "%2s %-6s %-8s %s", "ID", "RSSI", "BSSID", "SSID");
    for (int i = 0; i < rsp.results.num; i++)
    {
        char bssid_str[31];
        char ssid_str[33];

        struct_mac_addr_to_string(&rsp.results.results[i].bssid, bssid_str, sizeof(bssid_str));
        struct_string_32_to_string(&rsp.results.results[i].ssid, ssid_str, sizeof(ssid_str));

        mmagic_cli_printf(cli, "%2d %ddBm %-8s %s",
                          i, rsp.results.results[i].rssi, bssid_str, ssid_str);
    }
}

void mmagic_cli_wlan_get_rssi(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);

    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    struct mmagic_core_wlan_get_rssi_rsp_args rsp = { };

    enum mmagic_status status = mmagic_core_wlan_get_rssi(&ctx->core, &rsp);

    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Unable to retrieve RSSI");
    }
    else
    {
        mmagic_cli_printf(cli, "RSSI: %lddBm", rsp.rssi);
    }
}

void mmagic_cli_wlan_get_mac_addr(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);

    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    struct mmagic_core_wlan_get_mac_addr_rsp_args rsp = { };
    enum mmagic_status status = mmagic_core_wlan_get_mac_addr(&ctx->core, &rsp);

    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Unable to retrieve MAC address");
    }

    mmagic_cli_printf(cli, MM_MAC_ADDR_FMT, MM_MAC_ADDR_VAL(rsp.mac_addr.addr));
}

#define MMAGIC_CLI_WLAN_WNM_SLEEP_HINT "wlan-wnm_sleep <enter|exit>"

void mmagic_cli_wlan_wnm_sleep(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(context);

    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 1)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        embeddedCliPrint(cli, MMAGIC_CLI_WLAN_WNM_SLEEP_HINT);
        return;
    }

    struct mmagic_core_wlan_wnm_sleep_cmd_args cmd = { };

    const char *argument = embeddedCliGetToken(args, 1);
    if (!strcmp("enter", argument))
    {
        cmd.wnm_sleep_enabled = true;
    }
    else if (!strcmp("exit", argument))
    {
        cmd.wnm_sleep_enabled = false;
    }
    else
    {
        embeddedCliPrint(cli, "Unrecognised argument");
        embeddedCliPrint(cli, MMAGIC_CLI_WLAN_WNM_SLEEP_HINT);
        return;
    }

    enum mmagic_status status = mmagic_core_wlan_wnm_sleep(&ctx->core, &cmd);

    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_printf(cli, "Failed to %s WNM sleep", argument);
    }
    else
    {
        embeddedCliPrint(cli, "Success");
    }
}

void mmagic_cli_wlan_beacon_monitor_enable(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    struct mmagic_core_wlan_beacon_monitor_enable_cmd_args cmd_args = {};
    enum mmagic_status status;
    uint16_t num_tokens;
    uint16_t ii;
    const char *argument;

    MM_UNUSED(context);

    num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens > MM_ARRAY_COUNT(cmd_args.oui_filter.ouis))
    {
        embeddedCliPrint(cli, "Too many OUIs specified");
        return;
    }

    for (ii = 0; ii < num_tokens; ii++)
    {
        size_t argument_len;

        argument = embeddedCliGetToken(args, ii + 1);
        MMOSAL_ASSERT(argument != NULL);
        argument_len = strlen(argument);
        if (argument_len != 6)
        {
            embeddedCliPrint(cli, "Invalid OUI specified");
            return;
        }

        {
            const char *inbuf = argument;
            uint8_t *outbuf = cmd_args.oui_filter.ouis[ii].oui;
            size_t jj;
            for (jj = 0; jj < 3; jj++)
            {
                if (*inbuf >= '0' && *inbuf <= '9')
                {
                    *outbuf = *inbuf - '0';
                }
                else if (*inbuf >= 'a' && *inbuf <= 'f')
                {
                    *outbuf = *inbuf - 'f' + 0x0f;
                }
                else if (*inbuf >= 'A' && *inbuf <= 'F')
                {
                    *outbuf = *inbuf - 'F' + 0x0f;
                }
                else
                {
                    embeddedCliPrint(cli, "Invalid OUI specified");
                    return;
                }
                inbuf++;
                *outbuf <<= 4;
                if (*inbuf >= '0' && *inbuf <= '9')
                {
                    *outbuf |= *inbuf - '0';
                }
                else if (*inbuf >= 'a' && *inbuf <= 'f')
                {
                    *outbuf |= *inbuf - 'f' + 0x0f;
                }
                else if (*inbuf >= 'A' && *inbuf <= 'F')
                {
                    *outbuf |= *inbuf - 'F' + 0x0f;
                }
                else
                {
                    embeddedCliPrint(cli, "Invalid OUI specified");
                    return;
                }
                inbuf++;
                outbuf++;
            }
        }
    }

    cmd_args.oui_filter.count = num_tokens;

    status = mmagic_core_wlan_beacon_monitor_enable(&ctx->core, &cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Failed to configure beacon monitoring");
        return;
    }
}

void mmagic_cli_wlan_beacon_monitor_disable(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);

    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    enum mmagic_status status = mmagic_core_wlan_beacon_monitor_disable(&ctx->core);
    if (status != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Failed to disable beacon monitoring");
        return;
    }
}

static char display_buf[100];

void mmagic_cli_wlan_handle_event_beacon_rx(
    struct mmagic_cli *ctx,
    const struct mmagic_core_event_wlan_beacon_rx_args *args)
{
    uint32_t offset = 0;
    int display_buf_offset;
    embeddedCliPrint(ctx->cli, "Beacon received...");

    while (offset < args->vendor_ies.len)
    {
        uint8_t ie_type;
        uint8_t ie_len;
        uint32_t ii;

        if (offset + 2 > args->vendor_ies.len)
        {
            embeddedCliPrint(ctx->cli, "Beacon IEs malformed");
            break;
        }

        ie_type = args->vendor_ies.data[offset];
        ie_len = args->vendor_ies.data[offset + 1];

        offset += 2;

        if (offset + ie_len > args->vendor_ies.len)
        {
            embeddedCliPrint(ctx->cli, "Beacon IEs malformed");
            break;
        }

        display_buf_offset = snprintf(display_buf, sizeof(display_buf),
                                      "    IE type 0x%02x, IE len 0x%02x, Contents: ",
                                      ie_type, ie_len);
        if (display_buf_offset < 0)
        {
            break;
        }

        for (ii = 0; ii < ie_len && display_buf_offset + 3 < (int)sizeof(display_buf); ii++)
        {
            uint8_t x = args->vendor_ies.data[offset++];
            display_buf[display_buf_offset++] = mm_nibble_to_hex_char(x >> 4);
            display_buf[display_buf_offset++] = mm_nibble_to_hex_char(x);
        }

        MMOSAL_ASSERT(display_buf_offset < (int)sizeof(display_buf));

        display_buf[display_buf_offset] = '\0';
        embeddedCliPrint(ctx->cli, display_buf);
    }
}

void mmagic_cli_wlan_handle_event_standby_exit(
    struct mmagic_cli *ctx,
    const struct mmagic_core_event_wlan_standby_exit_args *args)
{
    char buf[MMAGIC_CLI_PRINT_BUF_LEN] = {0};
    snprintf(buf, MMAGIC_CLI_PRINT_BUF_LEN, "Standby exit reason = %d", args->reason);
    embeddedCliPrint(ctx->cli, buf);
}

void mmagic_cli_wlan_standby_enter(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    MM_UNUSED(args);
    MM_UNUSED(context);

    mmagic_core_wlan_standby_enter(&ctx->core);
}

void mmagic_cli_wlan_standby_exit(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    MM_UNUSED(args);
    MM_UNUSED(context);

    mmagic_core_wlan_standby_exit(&ctx->core);
}

static int hex_to_bytes(const char *hexstring, uint8_t *bytes, size_t bytes_size)
{
    const char *pos = hexstring;
    uint32_t i;
    uint32_t len = strlen(hexstring);

    if (len & 1)
    {
        /* Hex string length must be even number */
        return -1;
    }

    if (bytes_size < len / 2)
    {
        /* Byte array not large enough */
        return -1;
    }

    for (i = 0; i < len / 2; i++)
    {
        uint8_t byte = 0;
        if ((*pos >= '0') && (*pos <= '9'))
        {
            byte = (*pos - '0') << 4;
        }
        else if ((*pos >= 'a') && (*pos <= 'f'))
        {
            byte = (*pos + 10 - 'a') << 4;
        }
        else if ((*pos >= 'A') && (*pos <= 'F'))
        {
            byte = (*pos + 10 - 'A') << 4;
        }
        else
        {
            /* Invalid hex string */
            return -1;
        }
        pos++;

        if ((*pos >= '0') && (*pos <= '9'))
        {
            byte += *pos - '0';
        }
        else if ((*pos >= 'a') && (*pos <= 'f'))
        {
            byte += *pos + 10 - 'a';
        }
        else if ((*pos >= 'A') && (*pos <= 'F'))
        {
            byte += *pos + 10 - 'A';
        }
        else
        {
            /* Invalid hex string */
            return -1;
        }
        pos++;

        bytes[i] = byte;
    }

    return len / 2;
}

void mmagic_cli_wlan_standby_set_status_payload(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_core_wlan_standby_set_status_payload_cmd_args cmd_args = {};
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    MM_UNUSED(context);

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 1)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }

    int len = hex_to_bytes(embeddedCliGetToken(args, 1),
                           cmd_args.payload.buffer,
                           sizeof(cmd_args.payload.buffer));
    if (len <= 0)
    {
        embeddedCliPrint(cli, "Invalid hex string");
        return;
    }

    cmd_args.payload.len = len;

    mmagic_core_wlan_standby_set_status_payload(&ctx->core, &cmd_args);
}

void mmagic_cli_wlan_standby_set_wake_filter(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_core_wlan_standby_set_wake_filter_cmd_args cmd_args = {};
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    uint32_t uint32val;

    MM_UNUSED(context);

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if ((num_tokens != 1) && (num_tokens != 2))
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }

    int len = hex_to_bytes(embeddedCliGetToken(args, 1),
                           cmd_args.filter.buffer,
                           sizeof(cmd_args.filter.buffer));
    if (len <= 0)
    {
        embeddedCliPrint(cli, "Invalid hex string");
        return;
    }

    cmd_args.filter.len = len;
    cmd_args.offset = 0;

    if (num_tokens == 2)
    {
        (void)string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 2));
        cmd_args.offset = uint32val;
    }

    mmagic_core_wlan_standby_set_wake_filter(&ctx->core, &cmd_args);
}

void mmagic_cli_wlan_standby_set_config(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_core_wlan_standby_set_config_cmd_args cmd_args = {};
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    uint32_t uint32val;
    uint16_t uint16val;

    MM_UNUSED(context);

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 8)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }

    if (string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 1)) < 0)
    {
        embeddedCliPrint(cli, "Invalid notify period");
        return;
    }
    cmd_args.notify_period_s = uint32val;

    if (string_to_struct_ip_addr(&cmd_args.src_ip, embeddedCliGetToken(args, 2)) < 0)
    {
        embeddedCliPrint(cli, "Invalid source IP");
        return;
    }

    if (string_to_struct_ip_addr(&cmd_args.dst_ip, embeddedCliGetToken(args, 3)) < 0)
    {
        embeddedCliPrint(cli, "Invalid destination IP");
        return;
    }

    if (string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 4)) < 0)
    {
        embeddedCliPrint(cli, "Invalid destination port");
        return;
    }
    cmd_args.dst_port = uint16val;

    if (string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 5)) < 0)
    {
        embeddedCliPrint(cli, "Invalid BSS inactivity");
        return;
    }
    cmd_args.bss_inactivity_s = uint32val;

    if (string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 6)) < 0)
    {
        embeddedCliPrint(cli, "Invalid snooze interval");
        return;
    }
    cmd_args.snooze_period_s = uint32val;

    if (string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 7)) < 0)
    {
        embeddedCliPrint(cli, "Invalid snooze increment");
        return;
    }
    cmd_args.snooze_increment_s = uint32val;

    if (string_to_uint32_t(&uint32val, embeddedCliGetToken(args, 8)) < 0)
    {
        embeddedCliPrint(cli, "Invalid snooze max interval");
        return;
    }
    cmd_args.snooze_max_s = uint32val;

    mmagic_core_wlan_standby_set_config(&ctx->core, &cmd_args);
}
