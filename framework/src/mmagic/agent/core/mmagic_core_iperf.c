/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmipal.h"
#include "mmiperf.h"
#include "mmutils.h"
#include "mmwlan.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_iperf.h"
#include "mmagic.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_iperf.def"

static struct mmagic_iperf_config default_config =
{
    .mode = MMAGIC_IPERF_MODE_UDP_SERVER,
    .server = {.addr = "192.168.1.1"},
    .port = 5001,
    .amount = -10,
};

void mmagic_core_iperf_init(struct mmagic_data *core)
{
    struct mmagic_iperf_data *data = mmagic_data_get_iperf(core);
    memcpy(&data->config, &default_config, sizeof(data->config));
}

void mmagic_core_iperf_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

/********* MMAGIC Core Iperf ops **********/

struct mmagic_core_iperf_args
{
    struct mmosal_semb *iperf_complete_semb;
    struct struct_iperf_status *results;
    enum mmagic_iperf_state udp_server_state;
    enum mmagic_iperf_state tcp_server_state;
};

static struct mmagic_core_iperf_args cb_args;

static void mmagic_core_iperf_report_handler(const struct mmiperf_report *report, void *arg,
                                             mmiperf_handle_t handle)
{
    (void)handle;

    struct mmagic_core_iperf_args *cb_args = (struct mmagic_core_iperf_args *)arg;

    cb_args->results->bandwidth_kbitpsec = report->bandwidth_kbitpsec;
    cb_args->results->bytes_transferred = report->bytes_transferred;
    cb_args->results->duration_ms = report->duration_ms;
    cb_args->results->local_port = report->local_port;
    cb_args->results->remote_port = report->remote_port;

    (void)mmosal_safer_strcpy(cb_args->results->remote_addr.addr, report->remote_addr,
                              sizeof(cb_args->results->remote_addr.addr));
    (void)mmosal_safer_strcpy(cb_args->results->local_addr.addr, report->local_addr,
                              sizeof(cb_args->results->local_addr.addr));

    if (cb_args->iperf_complete_semb != NULL)
    {
        mmosal_semb_give(cb_args->iperf_complete_semb);
    }
}

enum mmagic_status mmagic_core_iperf_run(struct mmagic_data *core,
                                         struct mmagic_core_iperf_run_rsp_args *rsp_args)
{
    mmiperf_handle_t iperf_handle = NULL;
    struct mmagic_iperf_data *data = mmagic_data_get_iperf(core);
    struct mmiperf_client_args iperf_client_args = MMIPERF_CLIENT_ARGS_DEFAULT;
    struct mmiperf_server_args iperf_server_args = MMIPERF_SERVER_ARGS_DEFAULT;

    memset(&rsp_args->status, 0, sizeof(rsp_args->status));

    cb_args.iperf_complete_semb = mmosal_semb_create("iperf_complete");
    cb_args.results = &rsp_args->status;
    if (cb_args.iperf_complete_semb == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    switch (data->config.mode)
    {
    case MMAGIC_IPERF_MODE_UDP_SERVER:
        iperf_server_args.local_port = data->config.port;
        iperf_server_args.report_arg = (void *)&cb_args;
        iperf_server_args.report_fn = mmagic_core_iperf_report_handler;

        iperf_handle = mmiperf_start_udp_server(&iperf_server_args);
        if ((iperf_handle == NULL) && (cb_args.udp_server_state != MMAGIC_IPERF_STATE_RUNNING))
        {
            mmosal_semb_delete(cb_args.iperf_complete_semb);
            cb_args.iperf_complete_semb = NULL;
            return MMAGIC_STATUS_UNAVAILABLE;
        }
        cb_args.udp_server_state = MMAGIC_IPERF_STATE_RUNNING;
        break;

    case MMAGIC_IPERF_MODE_TCP_SERVER:
        iperf_server_args.local_port = data->config.port;
        iperf_server_args.report_arg = (void *)&cb_args;
        iperf_server_args.report_fn = mmagic_core_iperf_report_handler;

        iperf_handle = mmiperf_start_tcp_server(&iperf_server_args);
        if ((iperf_handle == NULL) && (cb_args.tcp_server_state != MMAGIC_IPERF_STATE_RUNNING))
        {
            mmosal_semb_delete(cb_args.iperf_complete_semb);
            cb_args.iperf_complete_semb = NULL;
            return MMAGIC_STATUS_UNAVAILABLE;
        }
        cb_args.tcp_server_state = MMAGIC_IPERF_STATE_RUNNING;
        break;

    case MMAGIC_IPERF_MODE_UDP_CLIENT:
        (void)mmosal_safer_strcpy(iperf_client_args.server_addr, data->config.server.addr,
                                  (sizeof(data->config.server.addr) /
                                   sizeof(data->config.server.addr[1])));
        iperf_client_args.server_port = data->config.port;
        iperf_client_args.amount = data->config.amount;
        if (iperf_client_args.amount < 0)
        {
            iperf_client_args.amount *= 100;
        }
        iperf_client_args.report_arg = (void *)&cb_args;
        iperf_client_args.report_fn = mmagic_core_iperf_report_handler;

        iperf_handle = mmiperf_start_udp_client(&iperf_client_args);
        if (iperf_handle == NULL)
        {
            mmosal_semb_delete(cb_args.iperf_complete_semb);
            cb_args.iperf_complete_semb = NULL;
            return MMAGIC_STATUS_UNAVAILABLE;
        }
        break;

    case MMAGIC_IPERF_MODE_TCP_CLIENT:
        (void)mmosal_safer_strcpy(iperf_client_args.server_addr, data->config.server.addr,
                                  (sizeof(data->config.server.addr) /
                                   sizeof(data->config.server.addr[1])));
        iperf_client_args.server_port = data->config.port;
        iperf_client_args.amount = data->config.amount;
        if (iperf_client_args.amount < 0)
        {
            iperf_client_args.amount *= 100;
        }
        iperf_client_args.report_arg = (void *)&cb_args;
        iperf_client_args.report_fn = mmagic_core_iperf_report_handler;

        iperf_handle = mmiperf_start_tcp_client(&iperf_client_args);
        if (iperf_handle == NULL)
        {
            mmosal_semb_delete(cb_args.iperf_complete_semb);
            cb_args.iperf_complete_semb = NULL;
            return MMAGIC_STATUS_UNAVAILABLE;
        }
        break;
    }

    bool ok = mmosal_semb_wait(cb_args.iperf_complete_semb, UINT32_MAX);
    mmosal_semb_delete(cb_args.iperf_complete_semb);
    cb_args.iperf_complete_semb = NULL;
    if (!ok)
    {
        return MMAGIC_STATUS_ERROR;
    }

    return MMAGIC_STATUS_OK;
}
