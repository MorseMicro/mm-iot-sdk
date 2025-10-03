/*
 * Copyright 2023-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <errno.h>

#include "mmconfig.h"
#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_ip.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_ip.h"

static void print_ip_status(EmbeddedCli *cli, const struct struct_ip_status *ip_status)
{
    char buf[MMAGIC_CLI_PRINT_BUF_LEN] = {0};
    int len = MMAGIC_CLI_PRINT_BUF_LEN - 1;
    size_t ii;
    bool dns_servers_set = false;

    int written = 0;

    if (ip_status->link_state == MMAGIC_IP_LINK_STATE_UP)
    {
        written += snprintf(&buf[written], (len - written), "\nLink Up\n");

        written += snprintf(&buf[written], (len - written), "DHCP Enabled: ");
        written += mmagic_bool_to_string(ip_status->dhcp_enabled, &buf[written], (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "IP Addr: ");
        written += mmagic_struct_ip_addr_to_string(&ip_status->ip_addr, &buf[written],
                                                   (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Netmask: ");
        written += mmagic_struct_ip_addr_to_string(&ip_status->netmask, &buf[written],
                                                   (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Gateway: ");
        written += mmagic_struct_ip_addr_to_string(&ip_status->gateway, &buf[written],
                                                   (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Broadcast: ");
        written += mmagic_struct_ip_addr_to_string(&ip_status->broadcast, &buf[written],
                                                   (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        for (ii = 0; ii < MM_ARRAY_COUNT(ip_status->dns_servers); ii++)
        {
            if (ip_status->dns_servers[ii].addr[0] != '\0')
            {
                written += snprintf(&buf[written], (len - written), "DNS server %u: ", ii);
                written += mmagic_struct_ip_addr_to_string(&ip_status->dns_servers[ii],
                                                           &buf[written],
                                                           (len - written));
                written += snprintf(&buf[written], (len - written), "\n");
                dns_servers_set = true;
            }
        }

        if (!dns_servers_set)
        {
            written += snprintf(&buf[written], (len - written), "No DNS servers set\n");
        }
    }
    else
    {
        written += snprintf(&buf[written], (len - written), "\nLink Down\n");
    }
    embeddedCliPrint(cli, buf);
}

void mmagic_cli_ip_status(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    struct mmagic_core_ip_status_rsp_args ip_status;
    mmagic_core_ip_status(&ctx->core, &ip_status);

    print_ip_status(cli, &ip_status.status);
}

void mmagic_cli_ip_handle_event_link_status(
    struct mmagic_cli *ctx,
    const struct mmagic_core_event_ip_link_status_args *args)
{
    MM_UNUSED(args);

    print_ip_status(ctx->cli, &args->ip_link_status);
}

void mmagic_cli_ip_reload(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    enum mmagic_status status = mmagic_core_ip_reload(&ctx->core);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Set IP configuration", status);
        return;
    }

    embeddedCliPrint(cli, "Successfully set the IP configuration");
}

void mmagic_cli_ip_enable_tcp_keepalive_offload(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    enum { EXPECTED_ARGS = 3 };

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != EXPECTED_ARGS)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }

    long tokens[EXPECTED_ARGS] = { 0 }; // NOLINT(runtime/int)
    enum mmagic_status status = MMAGIC_STATUS_OK;
    for (int i = 0; i < EXPECTED_ARGS; ++i)
    {
        const char *arg_start = embeddedCliGetToken(args, 1);
        char *endptr = NULL;
        errno = 0;
        tokens[i] = strtol(arg_start, &endptr, 10); /* Tokens are in base 10 */
        /* Validate strtol operation */
        if (errno != 0 || endptr == arg_start)
        {
            /* Argument not valid. */
            status = MMAGIC_STATUS_INVALID_ARG;
            break;
        }
    }

    struct mmagic_core_ip_enable_tcp_keepalive_offload_cmd_args cmd_args =
    {
        .period_s = (uint16_t)(tokens[0]),
        .retry_count = (uint8_t)(tokens[1]),
        .retry_interval_s = (uint8_t)(tokens[2]),
    };

    /* Verify input arguments were valid and in range */
    if (status != MMAGIC_STATUS_OK ||
        cmd_args.period_s != tokens[0] ||
        cmd_args.retry_count != tokens[1] ||
        cmd_args.retry_interval_s != tokens[2])
    {
        embeddedCliPrint(cli, "Invalid argument/s provided");
        return;
    }

    status = mmagic_core_ip_enable_tcp_keepalive_offload(&ctx->core, &cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "TCP keepalive offload", status);
    }

    return;
}

void mmagic_cli_ip_disable_tcp_keepalive_offload(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    mmagic_core_ip_disable_tcp_keepalive_offload(&ctx->core);
}

void mmagic_cli_ip_set_whitelist_filter(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_core_ip_set_whitelist_filter_cmd_args cmd_args = { 0 };
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    uint16_t uint16val;

    MM_UNUSED(context);

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens < 1)
    {
        embeddedCliPrint(cli, "At least source IP must be specified");
        return;
    }
    if (num_tokens > 7)
    {
        embeddedCliPrint(cli, "Invalid number of arguments specified");
        return;
    }

    if (num_tokens >= 1)
    {
        if (mmagic_string_to_struct_ip_addr(&cmd_args.src_ip, embeddedCliGetToken(args, 1)) < 0)
        {
            embeddedCliPrint(cli, "Invalid source IP");
            return;
        }
    }

    if (num_tokens >= 2)
    {
        if (mmagic_string_to_struct_ip_addr(&cmd_args.netmask, embeddedCliGetToken(args, 2)) < 0)
        {
            embeddedCliPrint(cli, "Invalid netmask");
            return;
        }
    }
    else
    {
        (void)mmagic_string_to_struct_ip_addr(&cmd_args.netmask, "0.0.0.0");
    }

    if (num_tokens >= 3)
    {
        if (mmagic_string_to_struct_ip_addr(&cmd_args.dest_ip, embeddedCliGetToken(args, 3)) < 0)
        {
            embeddedCliPrint(cli, "Invalid destination IP");
            return;
        }
    }
    else
    {
        (void)mmagic_string_to_struct_ip_addr(&cmd_args.dest_ip, "0.0.0.0");
    }

    if (num_tokens >= 4)
    {
        if (mmagic_string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 4)) < 0)
        {
            embeddedCliPrint(cli, "Invalid source port");
            return;
        }
        cmd_args.src_port = uint16val;
    }

    if (num_tokens >= 5)
    {
        if (mmagic_string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 5)) < 0)
        {
            embeddedCliPrint(cli, "Invalid destination port");
            return;
        }
        cmd_args.dest_port = uint16val;
    }

    if (num_tokens >= 6)
    {
        if (mmagic_string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 6)) < 0)
        {
            embeddedCliPrint(cli, "Invalid IPv4 protocol");
            return;
        }
        cmd_args.ip_protocol = uint16val;
    }

    if (num_tokens >= 7)
    {
        if (mmagic_string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 7)) < 0)
        {
            embeddedCliPrint(cli, "Invalid LLC protocol");
            return;
        }
        cmd_args.llc_protocol = uint16val;
    }

    enum mmagic_status status = mmagic_core_ip_set_whitelist_filter(&ctx->core, &cmd_args);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Set IP whitelist filter", status);
    }
}

void mmagic_cli_ip_clear_whitelist_filter(EmbeddedCli *cli, char *args, void *context)
{
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    MM_UNUSED(context);

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 0)
    {
        embeddedCliPrint(cli, "Invalid number of arguments specified");
        return;
    }

    enum mmagic_status status = mmagic_core_ip_clear_whitelist_filter(&ctx->core);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Clear IP whitelist filter", status);
    }
}
