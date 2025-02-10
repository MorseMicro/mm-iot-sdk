/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_ping.h"
#include "core/autogen/mmagic_core_types.h"
#include "cli/autogen/mmagic_cli_internal.h"
#include "cli/autogen/mmagic_cli_ping.h"

/* This should be included after all the header files */
#include "cli/autogen/mmagic_cli_ping.def"

/** Interval (in milliseconds) at which to provide updates when the receive count has not
 *  changed. */
#define UPDATE_INTERVAL_MS  (5000)

void mmagic_cli_ping_run(EmbeddedCli *cli, char *args, void *context)
{
    MM_UNUSED(args);
    MM_UNUSED(context);
    unsigned last_ping_recv_count = 0;
    uint32_t next_update_time_ms;
    struct mmagic_cli *ctx = (struct mmagic_cli *)cli->appContext;

    /* Must zero befor use */
    struct mmagic_core_ping_run_rsp_args rsp = {};
    enum mmagic_status status = mmagic_core_ping_run(&ctx->core, &rsp);
    if (status != MMAGIC_STATUS_OK)
    {
        mmagic_cli_print_error(cli, "Start ping session", status);
        return;
    }

    next_update_time_ms = mmosal_get_time_ms() + UPDATE_INTERVAL_MS;
    while (rsp.status.session_id)
    {
        /* Print the stats every ping interval ms */
        mmagic_core_ping_run(&ctx->core, &rsp);
        if (rsp.status.recv_count != last_ping_recv_count ||
            mmosal_time_has_passed(next_update_time_ms))
        {
            mmagic_cli_printf(cli, "(%s) packets transmitted/received = %lu/%lu, "
                              "round-trip min/avg/max = %lu/%lu/%lu ms",
                              rsp.status.receiver_addr.addr, rsp.status.total_count,
                              rsp.status.recv_count, rsp.status.min_time_ms,
                              rsp.status.avg_time_ms, rsp.status.max_time_ms);
            next_update_time_ms = mmosal_get_time_ms() + UPDATE_INTERVAL_MS;
            last_ping_recv_count = rsp.status.recv_count;
        }
    }

    /* Once the ping session has completed print a summary of the results */
    uint32_t packet_loss = 0;
    if (rsp.status.total_count == 0)
    {
        packet_loss = 0;
    }
    else
    {
        packet_loss = (1000 * (rsp.status.total_count - rsp.status.recv_count) * 100 /
                       rsp.status.total_count);
    }
    mmagic_cli_printf(cli,
                      "\n--- %s ping statistics ---\n" \
                      "%lu packets transmitted, %lu packets received, ",
                      rsp.status.receiver_addr.addr, rsp.status.total_count,
                      rsp.status.recv_count);
    mmagic_cli_printf(cli, "%lu.%03lu%% packet loss\nround-trip min/avg/max = %lu/%lu/%lu ms\n",
                      packet_loss / 1000, packet_loss % 1000, rsp.status.min_time_ms,
                      rsp.status.avg_time_ms, rsp.status.max_time_ms);
}
