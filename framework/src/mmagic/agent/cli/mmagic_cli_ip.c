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
    char buf[MMAGIC_CLI_PRINT_BUF_LEN] = { 0 };
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
        written +=
            mmagic_struct_ip_addr_to_string(&ip_status->ip_addr, &buf[written], (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Netmask: ");
        written +=
            mmagic_struct_ip_addr_to_string(&ip_status->netmask, &buf[written], (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Gateway: ");
        written +=
            mmagic_struct_ip_addr_to_string(&ip_status->gateway, &buf[written], (len - written));
        written += snprintf(&buf[written], (len - written), "\n");

        written += snprintf(&buf[written], (len - written), "Broadcast: ");
        written +=
            mmagic_struct_ip_addr_to_string(&ip_status->broadcast, &buf[written], (len - written));
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
