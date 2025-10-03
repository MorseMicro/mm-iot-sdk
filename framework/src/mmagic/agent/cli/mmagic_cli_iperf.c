/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmosal.h"
#include "mmipal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_iperf.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_iperf.h"

/** Array of power of 10 unit specifiers. */
static const char units[] = {' ', 'K', 'M', 'G', 'T'};

/**
 * Function to format a given number of bytes into an appropriate SI base. I.e if you give it 1400
 * it will return 1 with unit_index set to 1 for Kilo.
 *
 * @warning This uses power of 10 units (kilo, mega, giga, etc). Not to be confused with power of 2
 *          units (kibi, mebi, gibi, etc).
 *
 * @param[in]  bytes      Original number of bytes
 * @param[out] unit_index Index into the @ref units array. Must not be NULL
 *
 * @return                Number of bytes formatted to the appropriate unit given by the unit index.
 */
static uint32_t format_bytes(uint64_t bytes, uint8_t *unit_index)
{
    MMOSAL_ASSERT(unit_index != NULL);
    *unit_index = 0;

    while (bytes >= 1000 && *unit_index < 4)
    {
        bytes /= 1000;
        (*unit_index)++;
    }

    return bytes;
}

void mmagic_cli_iperf_run(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;
    struct mmagic_data *mmagicd = (struct mmagic_data *)context;
    struct mmagic_iperf_data *data = mmagic_data_get_iperf(mmagicd);

    struct mmipal_ip_config ip_config;
    struct mmipal_ip6_config ip6_config;
    enum mmipal_status ip_status = mmipal_get_ip_config(&ip_config);
    enum mmipal_status ip6_status = mmipal_get_ip6_config(&ip6_config);

    switch (data->config.mode)
    {
    case MMAGIC_IPERF_MODE_UDP_SERVER:
        embeddedCliPrint(cli, "\nStarting Iperf UDP server...\n");
        if (ip_status == MMIPAL_SUCCESS)
        {
            mmagic_cli_printf(cli, "Execute cmd on AP 'iperf -c %s -p %u -i 1 -u -b 20M' for IPv4",
                              ip_config.ip_addr, data->config.port);
        }
        if (ip6_status == MMIPAL_SUCCESS)
        {
            mmagic_cli_printf(cli, "Execute cmd on AP " \
                              "'iperf -c %s%%wlan0 -p %u -i 1 -V -u -b 20M' for IPv6\n",
                              ip6_config.ip6_addr[0], data->config.port);
        }
        embeddedCliPrint(cli, "waiting for client to connect...\n");
        break;

    case MMAGIC_IPERF_MODE_TCP_SERVER:
        embeddedCliPrint(cli, "\nStarting Iperf TCP server...\n");
        if (ip_status == MMIPAL_SUCCESS)
        {
            mmagic_cli_printf(cli, "Execute cmd on AP 'iperf -c %s -p %u -i 1' for IPv4",
                              ip_config.ip_addr, data->config.port);
        }
        if (ip6_status == MMIPAL_SUCCESS)
        {
            mmagic_cli_printf(cli, "Execute cmd on AP " \
                              "'iperf -c %s%%wlan0 -p %u -i 1 -V' for IPv6\n",
                              ip6_config.ip6_addr[0], data->config.port);
        }
        embeddedCliPrint(cli, "waiting for client to connect...\n");
        break;

    case MMAGIC_IPERF_MODE_UDP_CLIENT:
        embeddedCliPrint(cli, "\nStarting Iperf UDP client, wait for completion...\n");
        break;

    case MMAGIC_IPERF_MODE_TCP_CLIENT:
        embeddedCliPrint(cli, "\nStarting Iperf TCP client, wait for completion...\n");
        break;
    }

    /* Must zero before use */
    struct mmagic_core_iperf_run_rsp_args rsp = { };
    enum mmagic_status status = mmagic_core_iperf_run(&ctx->core, &rsp);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Iperf session", status);
    }
    else
    {
        uint8_t bytes_transferred_unit_index = 0;
        uint32_t bytes_transferred_formatted = format_bytes(rsp.status.bytes_transferred,
                                                            &bytes_transferred_unit_index);

        mmagic_cli_printf(cli, "Iperf Report");
        mmagic_cli_printf(cli, "  Remote Address: %s:%d",
                          rsp.status.remote_addr.addr, rsp.status.remote_port);
        mmagic_cli_printf(cli, "  Local Address:  %s:%d",
                          rsp.status.local_addr.addr, rsp.status.local_port);
        mmagic_cli_printf(cli,
                          "  Transferred: %lu %cBytes, duration: %lu ms, bandwidth: %lu kbps\n",
                          bytes_transferred_formatted, units[bytes_transferred_unit_index],
                          rsp.status.duration_ms, rsp.status.bandwidth_kbitpsec);
    }
}
