/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmipal.h"
#include "mmping.h"
#include "mmutils.h"
#include "mmwlan.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_ping.h"
#include "mmagic.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_ping.def"

static struct mmagic_ping_config default_config =
{
    .target = {.addr = "192.168.1.1"},
    .interval = 1000,
    .count = 10
};

void mmagic_core_ping_init(struct mmagic_data *core)
{
    struct mmagic_ping_data *data = mmagic_data_get_ping(core);
    memcpy(&data->config, &default_config, sizeof(data->config));
}

void mmagic_core_ping_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

/********* MMAGIC Core Ping ops **********/
enum mmagic_status mmagic_core_ping_run(struct mmagic_data *core,
                                        struct mmagic_core_ping_run_rsp_args *rsp_args)
{
    struct mmagic_ping_data *data = mmagic_data_get_ping(core);
    struct mmping_args ping_args = MMPING_ARGS_DEFAULT;
    enum mmipal_status ret;
    struct mmping_stats stats;

    if (!rsp_args->status.session_id)
    {
        ping_args.ping_count = data->config.count;
        ping_args.ping_interval_ms = data->config.interval;
        (void)mmosal_safer_strcpy(ping_args.ping_target, data->config.target.addr,
                                  (sizeof(data->config.target.addr) /
                                   sizeof(data->config.target.addr[1])));

        ret = mmipal_get_local_addr(ping_args.ping_src, ping_args.ping_target);
        if (ret != MMIPAL_SUCCESS)
        {
            printf("Failed to get local address for PING\n");
            return MMAGIC_STATUS_ERROR;
        }

        rsp_args->status.session_id = mmping_start(&ping_args);
        if (!rsp_args->status.session_id)
        {
            printf("Failed to start ping session\n");
            return MMAGIC_STATUS_ERROR;
        }
    }

    mmosal_task_sleep(10);

    mmping_stats(&stats);

    (void)mmosal_safer_strcpy(rsp_args->status.receiver_addr.addr, stats.ping_receiver,
                              (sizeof(rsp_args->status.receiver_addr.addr) /
                               sizeof(rsp_args->status.receiver_addr.addr[1])));
    rsp_args->status.total_count = stats.ping_total_count;
    rsp_args->status.recv_count = stats.ping_recv_count;
    rsp_args->status.min_time_ms = stats.ping_min_time_ms;
    rsp_args->status.avg_time_ms = stats.ping_avg_time_ms;
    rsp_args->status.max_time_ms = stats.ping_max_time_ms;

    if (!stats.ping_is_running)
    {
        rsp_args->status.session_id = 0;
    }

    return MMAGIC_STATUS_OK;
}
