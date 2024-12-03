/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_ip.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_ip.h"

/* This should be included after all the header files */
#include "cli/autogen/mmagic_cli_ip.def"

void mmagic_cli_ip_status(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    struct mmagic_core_ip_status_rsp_args rsp;
    if (mmagic_core_ip_status(&ctx->core, &rsp) != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Error retrieving IP status");
        return;
    }

    char buf[MMAGIC_CLI_PRINT_BUF_LEN] = {0};
    int len = MMAGIC_CLI_PRINT_BUF_LEN - 1;
    size_t ii;
    bool dns_servers_set = false;

    int written = 0;

    written += snprintf(&buf[written], (len - written), "DHCP Enabled: ");
    written += bool_to_string(&rsp.status.dhcp_enabled, &buf[written], (len - written));
    written += snprintf(&buf[written], (len - written), "\n");

    written += snprintf(&buf[written], (len - written), "IP Addr: ");
    written += struct_ip_addr_to_string(&rsp.status.ip_addr, &buf[written], (len - written));
    written += snprintf(&buf[written], (len - written), "\n");

    written += snprintf(&buf[written], (len - written), "Netmask: ");
    written += struct_ip_addr_to_string(&rsp.status.netmask, &buf[written], (len - written));
    written += snprintf(&buf[written], (len - written), "\n");

    written += snprintf(&buf[written], (len - written), "Gateway: ");
    written += struct_ip_addr_to_string(&rsp.status.gateway, &buf[written], (len - written));
    written += snprintf(&buf[written], (len - written), "\n");

    written += snprintf(&buf[written], (len - written), "Broadcast: ");
    written += struct_ip_addr_to_string(&rsp.status.broadcast, &buf[written], (len - written));
    written += snprintf(&buf[written], (len - written), "\n");

    for (ii = 0; ii < MM_ARRAY_COUNT(rsp.status.dns_servers); ii++)
    {
        if (rsp.status.dns_servers[ii].addr[0] != '\0')
        {
            written += snprintf(&buf[written], (len - written), "DNS server %u: ", ii);
            written += struct_ip_addr_to_string(&rsp.status.dns_servers[ii], &buf[written],
                                                (len - written));
            written += snprintf(&buf[written], (len - written), "\n");
            dns_servers_set = true;
        }
    }

    if (!dns_servers_set)
    {
        written += snprintf(&buf[written], (len - written), "No DNS servers set\n");
    }

    embeddedCliPrint(cli, buf);
}

void mmagic_cli_ip_reload(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    if (mmagic_core_ip_reload(&ctx->core) != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Failed to set the IP configuration");
        return;
    }

    embeddedCliPrint(cli, "Successfully set the IP configuration");
}

void mmagic_cli_ip_enable_tcp_keepalive_offload(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(context);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    uint16_t num_tokens = embeddedCliGetTokenCount(args);
    if (num_tokens != 3)
    {
        embeddedCliPrint(cli, "Invalid number of arguments");
        return;
    }
    struct mmagic_core_ip_enable_tcp_keepalive_offload_cmd_args cmd_args = {};

    cmd_args.period_s = atoi(embeddedCliGetToken(args, 1));
    cmd_args.retry_count = atoi(embeddedCliGetToken(args, 2));
    cmd_args.retry_interval_s = atoi(embeddedCliGetToken(args, 3));

    if (mmagic_core_ip_enable_tcp_keepalive_offload(&ctx->core, &cmd_args) != MMAGIC_STATUS_OK)
    {
        embeddedCliPrint(cli, "Error executing command");
    }
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
        if (string_to_struct_ip_addr(&cmd_args.src_ip, embeddedCliGetToken(args, 1)) < 0)
        {
            embeddedCliPrint(cli, "Invalid source IP");
            return;
        }
    }

    if (num_tokens >= 2)
    {
        if (string_to_struct_ip_addr(&cmd_args.netmask, embeddedCliGetToken(args, 2)) < 0)
        {
            embeddedCliPrint(cli, "Invalid netmask");
            return;
        }
    }
    else
    {
        (void)string_to_struct_ip_addr(&cmd_args.netmask, "0.0.0.0");
    }

    if (num_tokens >= 3)
    {
        if (string_to_struct_ip_addr(&cmd_args.dest_ip, embeddedCliGetToken(args, 3)) < 0)
        {
            embeddedCliPrint(cli, "Invalid destination IP");
            return;
        }
    }
    else
    {
        (void)string_to_struct_ip_addr(&cmd_args.dest_ip, "0.0.0.0");
    }

    if (num_tokens >= 4)
    {
        if (string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 4)) < 0)
        {
            embeddedCliPrint(cli, "Invalid source port");
            return;
        }
        cmd_args.src_port = uint16val;
    }

    if (num_tokens >= 5)
    {
        if (string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 5)) < 0)
        {
            embeddedCliPrint(cli, "Invalid destination port");
            return;
        }
        cmd_args.dest_port = uint16val;
    }

    if (num_tokens >= 6)
    {
        if (string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 6)) < 0)
        {
            embeddedCliPrint(cli, "Invalid IPv4 protocol");
            return;
        }
        cmd_args.ip_protocol = uint16val;
    }

    if (num_tokens >= 7)
    {
        if (string_to_uint16_t(&uint16val, embeddedCliGetToken(args, 7)) < 0)
        {
            embeddedCliPrint(cli, "Invalid LLC protocol");
            return;
        }
        cmd_args.llc_protocol = uint16val;
    }

    mmagic_core_ip_set_whitelist_filter(&ctx->core, &cmd_args);
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

    mmagic_core_ip_clear_whitelist_filter(&ctx->core);
}
