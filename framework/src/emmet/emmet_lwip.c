/*
 * Copyright 2021-2026 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include <endian.h>

#include "emmet_private.h"
#include "mmipal.h"
#include "mmiperf.h"
#include "mmping.h"

#include "lwip/sockets.h"

struct mmiperf_instance
{
    mmiperf_handle_t handle;
    Emmet__IperfState state;
    volatile bool server_report_pending;
    struct mmiperf_report report;
};

static struct
{
    struct mmiperf_instance server_udp;
    struct mmiperf_instance server_tcp;
    struct mmiperf_instance client_udp;
    struct mmiperf_instance client_tcp;
} emmet_mmiperf_state;

static bool bytes_to_ip_addr_str(char *ip_addr, const ProtobufCBinaryData *bytes)
{
    MMOSAL_ASSERT(ip_addr != NULL);
    ip_addr[0] = '\0';

    if (bytes == NULL)
    {
        return true;
    }

    if (bytes->len == 0)
    {
        return true;
    }

    if (bytes->len == 4)
    {
        snprintf(ip_addr,
                 MMIPAL_IPADDR_STR_MAXLEN,
                 "%d.%d.%d.%d",
                 bytes->data[0],
                 bytes->data[1],
                 bytes->data[2],
                 bytes->data[3]);
        return true;
    }

    if (bytes->len == 16)
    {
        unsigned offset = 0;
        int i;
        for (i = 0; i < 16; i += 2)
        {
            if (i > 0)
            {
                ip_addr[offset++] = ':';
            }
            int ret = snprintf(ip_addr + offset,
                               MMIPAL_IPADDR_STR_MAXLEN - offset,
                               "%02x%02x",
                               bytes->data[i],
                               bytes->data[i + 1]);
            if (ret <= 0)
            {
                return false;
            }
            offset += ret;
        }
        return true;
    }
    return false;
}

static bool ipv4_addr_str_to_bytes(ProtobufCBinaryData *bytes, const char *ip_addr)
{
    unsigned int a, b, c, d;
    int ret;

    bytes->len = 4;
    bytes->data = (uint8_t *)mmosal_malloc(bytes->len);
    if (bytes->data == NULL)
    {
        return false;
    }

    if (ip_addr[0] == '\0')
    {
        memset(bytes->data, 0, bytes->len);
        return true;
    }

    ret = sscanf(ip_addr, "%u.%u.%u.%u", &a, &b, &c, &d);
    if (ret != 4)
    {
        memset(bytes->data, 0, bytes->len);
        return false;
    }

    MMLOG_DBG("Parse IPv4 addr: %s -> %u.%u.%u.%u\n", ip_addr, a, b, c, d);

    bytes->data[0] = a;
    bytes->data[1] = b;
    bytes->data[2] = c;
    bytes->data[3] = d;

    return true;
}

static bool ipv6_addr_str_to_bytes(ProtobufCBinaryData *bytes, const char *ip_addr)
{
    int ret;
    uint32_t *addr;

    bytes->len = 16;
    addr = (uint32_t *)mmosal_malloc(bytes->len);
    if (addr == NULL)
    {
        return false;
    }
    bytes->data = (uint8_t *)addr;

    ret = emmet_ip6addr_aton(ip_addr, addr);
    if (ret == 0)
    {
        return false;
    }

    MMLOG_DBG("Parse IPv6 addr: %s -> "
              "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
              ip_addr,
              bytes->data[0],
              bytes->data[1],
              bytes->data[2],
              bytes->data[3],
              bytes->data[4],
              bytes->data[5],
              bytes->data[6],
              bytes->data[7],
              bytes->data[8],
              bytes->data[9],
              bytes->data[10],
              bytes->data[11],
              bytes->data[12],
              bytes->data[13],
              bytes->data[14],
              bytes->data[15]);

    return true;
}

static bool ip_addr_str_to_bytes(ProtobufCBinaryData *bytes, const char *ip_addr)
{
    unsigned ii;

    for (ii = 0; ii < 5; ii++)
    {
        if (ip_addr[ii] == '\0' || ip_addr[ii] == '.')
        {
            return ipv4_addr_str_to_bytes(bytes, ip_addr);
        }
        else if (ip_addr[ii] == ':')
        {
            return ipv6_addr_str_to_bytes(bytes, ip_addr);
        }
    }

    MMLOG_WRN("IP address %s not IPv4 or IPv6\n", ip_addr);
    return false;
}

void emmet_pack_lwiprsp(struct emmet_raw_rsp *raw_rsp, Emmet__LwipRsp *lwiprsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.lwip = lwiprsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_LWIP;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void emmet_mmiperf_report_handler(const struct mmiperf_report *report,
                                         void *arg,
                                         mmiperf_handle_t handle)
{
    MM_UNUSED(handle);
    struct mmiperf_instance *inst = (struct mmiperf_instance *)arg;

    if (inst->state != EMMET__IPERF_STATE__RUNNING)
    {
        MMLOG_WRN("Received mmiperf report in unexpected state\n");
        return;
    }

    memcpy(&inst->report, report, sizeof(inst->report));
    bool is_server = (inst == &emmet_mmiperf_state.server_udp) ||
                     (inst == &emmet_mmiperf_state.server_tcp);

    const char *report_type_str = "Unspecified";
    switch (report->report_type)
    {
        case MMIPERF_TCP_DONE_SERVER:
            report_type_str = "TCP Server";
            break;

        case MMIPERF_TCP_DONE_CLIENT:
            report_type_str = "TCP Client";
            break;

        case MMIPERF_UDP_DONE_SERVER:
            report_type_str = "UDP Server";
            break;

        case MMIPERF_UDP_DONE_CLIENT:
            report_type_str = "UDP Client";
            break;

        case MMIPERF_TCP_ABORTED_LOCAL:
        case MMIPERF_TCP_ABORTED_LOCAL_DATAERROR:
        case MMIPERF_TCP_ABORTED_LOCAL_TXERROR:
        case MMIPERF_TCP_ABORTED_REMOTE:
            report_type_str = "TCP Aborted";
            break;

        case MMIPERF_STOPPED:
            report_type_str = "Stopped";
            break;

        default:
            break;
    }

    if ((report->report_type == MMIPERF_TCP_DONE_CLIENT) ||
        (report->report_type == MMIPERF_UDP_DONE_CLIENT) ||
        (report->report_type == MMIPERF_STOPPED))
    {
        inst->state = EMMET__IPERF_STATE__FINISHED;
    }
    else if ((report->report_type != MMIPERF_TCP_DONE_SERVER) &&
             (report->report_type != MMIPERF_UDP_DONE_SERVER))
    {
        inst->state = EMMET__IPERF_STATE__ABORTED;
        char *dir = is_server ? "server" : "client";
        MMLOG_WRN("TCP %s aborted\n", dir);
    }

    if (!is_server)
    {
        inst->handle = NULL;
    }
    else
    {
        inst->server_report_pending = true;
        if (report->report_type == MMIPERF_STOPPED)
        {
            inst->handle = NULL;
        }
    }

    MM_UNUSED(report_type_str);
    MMLOG_INF("Iperf %s report (%u): 0x" MM_X64_FMT " octets in %lu ms (%lu kbps)\n",
              report_type_str,
              report->report_type,
              MM_X64_VAL(report->bytes_transferred),
              report->duration_ms,
              report->bandwidth_kbitpsec);
}

static void handle_ping_request(struct emmet_raw_rsp *raw_rsp, Emmet__PingRequestCmd *cmd)
{
    struct mmping_stats stats;
    struct mmping_args args = MMPING_ARGS_DEFAULT;
    mmping_stats(&stats);
    if (stats.ping_is_running == true)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Ping already running");
        return;
    }

    bool ok = bytes_to_ip_addr_str(args.ping_target, &cmd->ip_address);
    if (!ok)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid IP addr");
        return;
    }

    MMLOG_INF("PING %s : %lu data bytes\n", args.ping_target, cmd->data_size);

    if (cmd->data_size != 0)
    {
        if (cmd->data_size < MMPING_MIN_DATA_SIZE || cmd->data_size > MMPING_MAX_DATA_SIZE)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "invalid data size %lu (valid range %d-%d)",
                                 cmd->data_size,
                                 MMPING_MIN_DATA_SIZE,
                                 MMPING_MAX_DATA_SIZE);
            return;
        }
        else
        {
            args.ping_size = cmd->data_size;
        }
    }

    enum mmipal_status status = mmipal_get_local_addr(args.ping_src, args.ping_target);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "failed to get local address");
        return;
    }
    args.ping_interval_ms = cmd->interval;
    args.ping_count = cmd->max_count;
    mmping_start(&args);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_ping_stats(struct emmet_raw_rsp *raw_rsp, Emmet__PingStatsCmd *cmd)
{
    bool ok;
    struct mmping_stats stats;
    Emmet__PingStatsRsp stats_rsp = EMMET__PING_STATS_RSP__INIT;

    MM_UNUSED(cmd);

    mmping_stats(&stats);

    ok = ip_addr_str_to_bytes(&stats_rsp.ip_address, stats.ping_receiver);
    if (!ok)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "IP addr conversion failed");
        return;
    }

    stats_rsp.num_sent = stats.ping_total_count;
    stats_rsp.num_received = stats.ping_recv_count;
    stats_rsp.min_latency = stats.ping_min_time_ms;
    stats_rsp.avg_latency = stats.ping_avg_time_ms;
    stats_rsp.max_latency = stats.ping_max_time_ms;
    stats_rsp.active = stats.ping_is_running;

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    rsp.ping_stats = &stats_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_PING_STATS;
    emmet_pack_lwiprsp(raw_rsp, &rsp);
    mmosal_free(stats_rsp.ip_address.data);
}

static void handle_ping_stop(struct emmet_raw_rsp *raw_rsp, Emmet__PingStopCmd *cmd)
{
    MM_UNUSED(cmd);

    mmping_stop();

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_iperf_client_start(struct emmet_raw_rsp *raw_rsp,
                                      Emmet__IperfClientStartCmd *cmd)
{
    struct mmiperf_client_args args = MMIPERF_CLIENT_ARGS_DEFAULT;

    bool ok = bytes_to_ip_addr_str(args.server_addr, &cmd->remote_addr);
    if (!ok)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid IP addr");
        return;
    }

    if (cmd->remote_port > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid port");
        return;
    }
    uint16_t remote_port = (uint16_t)(cmd->remote_port);

    if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
    {
        struct mmiperf_instance *inst = &(emmet_mmiperf_state.client_tcp);
        if (!(inst->state == EMMET__IPERF_STATE__NOT_STARTED ||
              inst->state == EMMET__IPERF_STATE__FINISHED))
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf in progress");
            return;
        }

        if (inst->handle != NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf internal error");
            return;
        }

        args.server_port = remote_port;
        args.target_bw = cmd->target_bw;
        args.amount = cmd->amount;
        args.report_fn = emmet_mmiperf_report_handler;
        args.report_arg = inst;

        MMLOG_INF("Starting TCP iperf client\n");

        inst->state = EMMET__IPERF_STATE__RUNNING;
        inst->handle = mmiperf_start_tcp_client(&args);

        if (inst->handle == NULL)
        {
            MMLOG_WRN("Failed to start TCP iperf client\n");
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf failed to start");
            inst->state = EMMET__IPERF_STATE__NOT_STARTED;
            return;
        }
    }
    else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
    {
        struct mmiperf_instance *inst = &(emmet_mmiperf_state.client_udp);
        if (inst->state == EMMET__IPERF_STATE__RUNNING)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf in progress");
            return;
        }

        if (inst->handle != NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf internal error");
            return;
        }

        args.server_port = remote_port;
        args.target_bw = cmd->target_bw;
        args.packet_size = cmd->packet_size;
        args.amount = cmd->amount;
        args.report_fn = emmet_mmiperf_report_handler;
        args.report_arg = inst;

        MMLOG_INF("Starting UDP iperf client\n");

        inst->state = EMMET__IPERF_STATE__RUNNING;
        inst->handle = mmiperf_start_udp_client(&args);

        if (inst->handle == NULL)
        {
            MMLOG_WRN("Failed to start UDP iperf client\n");
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf failed to start");
            inst->state = EMMET__IPERF_STATE__NOT_STARTED;
            return;
        }
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid port type");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_iperf_server_start(struct emmet_raw_rsp *raw_rsp,
                                      Emmet__IperfServerStartCmd *cmd)
{
    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    Emmet__IperfServerStartRsp iperf_rsp = EMMET__IPERF_SERVER_START_RSP__INIT;
    rsp.iperf_server_start = &iperf_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_IPERF_SERVER_START;

    struct mmiperf_server_args args = MMIPERF_SERVER_ARGS_DEFAULT;
    bool ok = bytes_to_ip_addr_str(args.local_addr, &cmd->local_addr);
    if (!ok && cmd->local_addr.len != 0)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid multicast addr to bind");
        return;
    }

    if (cmd->port > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid port");
        return;
    }

    uint16_t port = (uint16_t)(cmd->port);

    if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
    {
        struct mmiperf_instance *inst = &(emmet_mmiperf_state.server_tcp);
        if (inst->state != EMMET__IPERF_STATE__NOT_STARTED)
        {
            iperf_rsp.server_state = inst->state;
            emmet_pack_lwiprsp(raw_rsp, &rsp);
            return;
        }

        if (inst->handle != NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf internal error");
            return;
        }

        args.local_port = port;
        args.report_fn = emmet_mmiperf_report_handler;
        args.report_arg = inst;

        MMLOG_INF("Starting TCP iperf server\n");

        inst->state = EMMET__IPERF_STATE__RUNNING;
        inst->handle = mmiperf_start_tcp_server(&args);

        if (inst->handle == NULL)
        {
            MMLOG_WRN("Failed to start TCP iperf server\n");
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf failed to start");
            inst->state = EMMET__IPERF_STATE__NOT_STARTED;
            return;
        }
    }
    else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
    {
        struct mmiperf_instance *inst = &(emmet_mmiperf_state.server_udp);
        if (inst->state != EMMET__IPERF_STATE__NOT_STARTED)
        {
            iperf_rsp.server_state = inst->state;
            emmet_pack_lwiprsp(raw_rsp, &rsp);
            return;
        }

        if (inst->handle != NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf internal error");
            return;
        }

        args.local_port = port;
        args.report_fn = emmet_mmiperf_report_handler;
        args.report_arg = inst;

        MMLOG_INF("Starting UDP iperf server\n");

        inst->state = EMMET__IPERF_STATE__RUNNING;
        inst->handle = mmiperf_start_udp_server(&args);

        if (inst->handle == NULL)
        {
            MMLOG_WRN("Failed to start UDP iperf server\n");
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf failed to start");
            inst->state = EMMET__IPERF_STATE__NOT_STARTED;
            return;
        }
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid port type");
        return;
    }

    emmet_pack_lwiprsp(raw_rsp, &rsp);
}

static void handle_iperf_stop(struct emmet_raw_rsp *raw_rsp, Emmet__IperfStopCmd *cmd)
{
    struct mmiperf_instance *inst = NULL;

    if (cmd->dir == EMMET__IPERF_DIR__CLIENT)
    {
        if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
        {
            inst = &(emmet_mmiperf_state.client_tcp);
        }
        else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
        {
            inst = &(emmet_mmiperf_state.client_udp);
        }
    }
    else if (cmd->dir == EMMET__IPERF_DIR__SERVER)
    {
        if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
        {
            inst = &(emmet_mmiperf_state.server_tcp);
        }
        else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
        {
            inst = &(emmet_mmiperf_state.server_udp);
        }
    }

    if (inst == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "invalid port type or dir");
        return;
    }

    if (inst->state != EMMET__IPERF_STATE__RUNNING || inst->handle == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf not running");
        return;
    }

    bool ok = mmiperf_stop(inst->handle);
    if (ok)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "iperf internal error");
    }
}

static void handle_iperf_get_stats(struct emmet_raw_rsp *raw_rsp, Emmet__IperfGetStatsCmd *cmd)
{
    struct mmiperf_report report_buf = {};
    struct mmiperf_report *report = &report_buf;

    struct mmiperf_instance *inst = NULL;
    Emmet__IperfState reset_state = EMMET__IPERF_STATE__NOT_STARTED;
    if (cmd->dir == EMMET__IPERF_DIR__CLIENT)
    {
        if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
        {
            inst = &(emmet_mmiperf_state.client_tcp);
        }
        else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
        {
            inst = &(emmet_mmiperf_state.client_udp);
        }
    }
    else if (cmd->dir == EMMET__IPERF_DIR__SERVER)
    {
        if (cmd->port_type == EMMET__LWIP_PORT_TYPE__TCP)
        {
            inst = &(emmet_mmiperf_state.server_tcp);
        }
        else if (cmd->port_type == EMMET__LWIP_PORT_TYPE__UDP)
        {
            inst = &(emmet_mmiperf_state.server_udp);
        }
        if ((inst == NULL) || (inst->handle == NULL))
        {
            reset_state = EMMET__IPERF_STATE__NOT_STARTED;
        }
        else
        {
            reset_state = EMMET__IPERF_STATE__RUNNING;
        }
    }

    if (inst == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "invalid port type or dir");
        return;
    }

    Emmet__IperfGetStatsRsp stats_rsp = EMMET__IPERF_GET_STATS_RSP__INIT;
    stats_rsp.state = inst->state;

    if ((inst->state == EMMET__IPERF_STATE__FINISHED) ||
        (inst->state == EMMET__IPERF_STATE__ABORTED) ||
        inst->server_report_pending)
    {
        if (inst->server_report_pending)
        {
            inst->server_report_pending = false;
            stats_rsp.state = EMMET__IPERF_STATE__FINISHED;
        }
        report = &inst->report;

        inst->state = reset_state;
    }
    else if (inst->handle != NULL)
    {
        mmiperf_get_interim_report(inst->handle, report);
    }

    ip_addr_str_to_bytes(&stats_rsp.local_addr, report->local_addr);
    stats_rsp.local_port = report->local_port;
    ip_addr_str_to_bytes(&stats_rsp.remote_addr, report->remote_addr);
    stats_rsp.remote_port = report->remote_port;
    stats_rsp.bytes_transferred = report->bytes_transferred;
    stats_rsp.ms_duration = report->duration_ms;
    stats_rsp.bandwidth_kbitpsec = report->bandwidth_kbitpsec;
    stats_rsp.tx_frames = report->tx_frames;
    stats_rsp.rx_frames = report->rx_frames;
    stats_rsp.out_of_sequence_frames = report->out_of_sequence_frames;
    stats_rsp.error_count = report->error_count;

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    rsp.iperf_get_stats = &stats_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_IPERF_GET_STATS;
    emmet_pack_lwiprsp(raw_rsp, &rsp);

    mmosal_free(stats_rsp.local_addr.data);
    mmosal_free(stats_rsp.remote_addr.data);

    if (cmd->dir == EMMET__IPERF_DIR__SERVER && inst->state == EMMET__IPERF_STATE__RUNNING)
    {
        memset(&inst->report, 0, sizeof(inst->report));
    }
}

static void handle_get_ip_config(struct emmet_raw_rsp *raw_rsp, Emmet__GetIpConfigCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmipal_ip_config ip_config;
    enum mmipal_status status;
    Emmet__GetIpConfigRsp cfg_rsp = EMMET__GET_IP_CONFIG_RSP__INIT;

    status = mmipal_get_ip_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed with status %u",
                             status);
        return;
    }

    if (ip_config.mode == MMIPAL_DISABLED)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__NOT_SUPPORTED, "IPv4 is disabled");
        return;
    }

    cfg_rsp.dhcp_enabled = ip_config.mode == MMIPAL_DHCP;
    ip_addr_str_to_bytes(&cfg_rsp.ip_addr, ip_config.ip_addr);
    ip_addr_str_to_bytes(&cfg_rsp.netmask, ip_config.netmask);
    ip_addr_str_to_bytes(&cfg_rsp.gateway_addr, ip_config.gateway_addr);

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    rsp.get_ip_config = &cfg_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_GET_IP_CONFIG;
    emmet_pack_lwiprsp(raw_rsp, &rsp);

    mmosal_free(cfg_rsp.ip_addr.data);
    mmosal_free(cfg_rsp.netmask.data);
    mmosal_free(cfg_rsp.gateway_addr.data);
}

static void handle_set_ip_config(struct emmet_raw_rsp *raw_rsp, Emmet__SetIpConfigCmd *cmd)
{
    struct mmipal_ip_config ip_config = MMIPAL_IP_CONFIG_DEFAULT;
    enum mmipal_status status;
    bool ok = true;

    ip_config.mode = cmd->dhcp_enabled ? MMIPAL_DHCP : MMIPAL_STATIC;
    ok = ok && bytes_to_ip_addr_str(ip_config.ip_addr, &cmd->ip_addr);
    ok = ok && bytes_to_ip_addr_str(ip_config.netmask, &cmd->netmask);
    ok = ok && bytes_to_ip_addr_str(ip_config.gateway_addr, &cmd->gateway_addr);

    if (!ok)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid addr len");
        return;
    }

    status = mmipal_set_ip_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed with status %u",
                             status);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_get_ip6_config(struct emmet_raw_rsp *raw_rsp, Emmet__GetIp6ConfigCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmipal_ip6_config ip_config;
    enum mmipal_status status;
    Emmet__GetIp6ConfigRsp cfg_rsp = EMMET__GET_IP6_CONFIG_RSP__INIT;
    int i;

    status = mmipal_get_ip6_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed with status %u",
                             status);
        return;
    }

    if (ip_config.ip6_mode == MMIPAL_IP6_DISABLED)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__NOT_SUPPORTED, "IPv6 is disabled");
        return;
    }

    switch (ip_config.ip6_mode)
    {
        case MMIPAL_IP6_STATIC:
            cfg_rsp.mode = EMMET__IP6_ADDR_MODE__STATIC;
            break;

        case MMIPAL_IP6_AUTOCONFIG:
            cfg_rsp.mode = EMMET__IP6_ADDR_MODE__AUTOCONFIG;
            break;

        case MMIPAL_IP6_DHCP6_STATELESS:
            cfg_rsp.mode = EMMET__IP6_ADDR_MODE__DHCP6_STATELESS;
            break;

        default:
            break;
    }

    cfg_rsp.n_addrs = MMIPAL_MAX_IPV6_ADDRESSES;
    cfg_rsp.addrs = (ProtobufCBinaryData *)mmosal_malloc(sizeof(*cfg_rsp.addrs) * cfg_rsp.n_addrs);
    if (cfg_rsp.addrs == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE,
                             "memory allcation for IPv6 address failed");
        return;
    }

    for (i = 0; i < MMIPAL_MAX_IPV6_ADDRESSES; i++)
    {
        ip_addr_str_to_bytes(&cfg_rsp.addrs[i], ip_config.ip6_addr[i]);
    }

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    rsp.get_ip6_config = &cfg_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_GET_IP6_CONFIG;
    emmet_pack_lwiprsp(raw_rsp, &rsp);
    for (i = 0; i < MMIPAL_MAX_IPV6_ADDRESSES; i++)
    {
        mmosal_free(cfg_rsp.addrs[i].data);
    }
    mmosal_free(cfg_rsp.addrs);
}

static void handle_set_ip6_config(struct emmet_raw_rsp *raw_rsp, Emmet__SetIp6ConfigCmd *cmd)
{
    struct mmipal_ip6_config ip_config;
    enum mmipal_status status;
    int i;

    switch (cmd->mode)
    {
        case EMMET__IP6_ADDR_MODE__STATIC:
            ip_config.ip6_mode = MMIPAL_IP6_STATIC;
            break;

        case EMMET__IP6_ADDR_MODE__AUTOCONFIG:
            ip_config.ip6_mode = MMIPAL_IP6_AUTOCONFIG;
            break;

        case EMMET__IP6_ADDR_MODE__DHCP6_STATELESS:
            ip_config.ip6_mode = MMIPAL_IP6_DHCP6_STATELESS;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid IPv6 mode %u",
                                 cmd->mode);
            return;
    }

    if (cmd->n_addrs > MMIPAL_MAX_IPV6_ADDRESSES)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "too many input address");
        return;
    }

    for (i = 0; i < MMIPAL_MAX_IPV6_ADDRESSES; i++)
    {
        if (i >= (int)cmd->n_addrs)
        {
            strncpy(ip_config.ip6_addr[i], "::0", sizeof(ip_config.ip6_addr[i]));
        }
        else
        {
            bool ok = bytes_to_ip_addr_str(ip_config.ip6_addr[i], &cmd->addrs[i]);
            if (!ok)
            {
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                     "invalid addr len");
                return;
            }
        }
    }

    status = mmipal_set_ip6_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed with status %u",
                             status);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_static_arp(struct emmet_raw_rsp *raw_rsp, Emmet__SetStaticArpCmd *cmd)
{
    struct mmipal_arp_config arp_config = { 0 };
    enum mmipal_status status;

    if (!bytes_to_ip_addr_str(arp_config.ip_addr, &cmd->ip_addr))
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "invalid addr len");
        return;
    }

    if (cmd->remove)
    {
        status = mmipal_remove_static_arp_entry(&arp_config);
    }
    else
    {
        if (cmd->mac_addr.len != MMWLAN_MAC_ADDR_LEN)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid MAC address length (%u)",
                                 cmd->mac_addr.len);
            return;
        }

        memcpy(&arp_config.mac_addr, cmd->mac_addr.data, MMWLAN_MAC_ADDR_LEN);

        status = mmipal_add_static_arp_entry(&arp_config);
    }

    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed with status %u",
                             status);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_tx_qos_tid(struct emmet_raw_rsp *raw_rsp, Emmet__SetTxQosTidCmd *cmd)
{
    if (cmd->qos_tid > MMWLAN_MAX_QOS_TID)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "qos_tid too large");
        return;
    }
    mmipal_set_tx_qos_tid(cmd->qos_tid);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_get_link_state(struct emmet_raw_rsp *raw_rsp, Emmet__GetLinkStateCmd *cmd)
{
    MM_UNUSED(cmd);

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    Emmet__GetLinkStateRsp get_link_state_rsp = EMMET__GET_LINK_STATE_RSP__INIT;
    rsp.get_link_state = &get_link_state_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_GET_LINK_STATE;

    switch (mmipal_get_link_state())
    {
        case MMIPAL_LINK_DOWN:
            rsp.get_link_state->state = EMMET__LINK_STATE__LINK_DOWN;
            break;

        case MMIPAL_LINK_UP:
            rsp.get_link_state->state = EMMET__LINK_STATE__LINK_UP;
            break;
    }

    emmet_pack_lwiprsp(raw_rsp, &rsp);
}

static void handle_tcp_connect(struct emmet_raw_rsp *raw_rsp, Emmet__TcpConnectCmd *cmd)
{
#if LWIP_IPV4
    struct sockaddr_in sockaddr_in = { .sin_len = sizeof(sockaddr_in),
                                       .sin_family = AF_INET,
                                       .sin_port = htobe16(cmd->port),
                                       .sin_addr = {
                                           .s_addr = htobe32(cmd->ipv4_addr),
                                       } };
    int ret;

    Emmet__LwipRsp rsp = EMMET__LWIP_RSP__INIT;
    Emmet__TcpConnectRsp tcp_connect_rsp = EMMET__TCP_CONNECT_RSP__INIT;
    rsp.tcp_connect = &tcp_connect_rsp;
    rsp.rsp_case = EMMET__LWIP_RSP__RSP_TCP_CONNECT;

    ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ret < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SOCKET_ERROR, "socket() failed");
        return;
    }

    tcp_connect_rsp.socket_id = ret;

    ret = connect(ret, (struct sockaddr *)&sockaddr_in, sizeof(sockaddr_in));
    if (ret < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SOCKET_ERROR, "connect() failed");
        return;
    }

    emmet_pack_lwiprsp(raw_rsp, &rsp);
#else
    MM_UNUSED(cmd);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SOCKET_ERROR, "IPv4 not enabled");
#endif
}

static void handle_tcp_send(struct emmet_raw_rsp *raw_rsp, Emmet__TcpSendCmd *cmd)
{
    ssize_t ret = send(cmd->socket_id, cmd->data.data, cmd->data.len, 0);

    if (ret == (ssize_t)cmd->data.len)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else if (ret < 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SOCKET_ERROR, "send() failed %d", ret);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__SOCKET_ERROR,
                             "send() too short %d (expect %u)",
                             ret,
                             cmd->data.len);
    }
}

void emmet_handle_lwipcmd(struct emmet_raw_rsp *raw_rsp, Emmet__LwipCmd *lwipcmd)
{
    switch (lwipcmd->cmd_case)
    {
        case EMMET__LWIP_CMD__CMD_IPERF_CLIENT_START:
            handle_iperf_client_start(raw_rsp, lwipcmd->iperf_client_start);
            break;

        case EMMET__LWIP_CMD__CMD_IPERF_SERVER_START:
            handle_iperf_server_start(raw_rsp, lwipcmd->iperf_server_start);
            break;

        case EMMET__LWIP_CMD__CMD_IPERF_GET_STATS:
            handle_iperf_get_stats(raw_rsp, lwipcmd->iperf_get_stats);
            break;

        case EMMET__LWIP_CMD__CMD_IPERF_STOP:
            handle_iperf_stop(raw_rsp, lwipcmd->iperf_stop);
            break;

        case EMMET__LWIP_CMD__CMD_PING_REQUEST:
            handle_ping_request(raw_rsp, lwipcmd->ping_request);
            break;

        case EMMET__LWIP_CMD__CMD_PING_STATS:
            handle_ping_stats(raw_rsp, lwipcmd->ping_stats);
            break;

        case EMMET__LWIP_CMD__CMD_PING_STOP:
            handle_ping_stop(raw_rsp, lwipcmd->ping_stop);
            break;

        case EMMET__LWIP_CMD__CMD_GET_IP_CONFIG:
            handle_get_ip_config(raw_rsp, lwipcmd->get_ip_config);
            break;

        case EMMET__LWIP_CMD__CMD_SET_IP_CONFIG:
            handle_set_ip_config(raw_rsp, lwipcmd->set_ip_config);
            break;

        case EMMET__LWIP_CMD__CMD_GET_IP6_CONFIG:
            handle_get_ip6_config(raw_rsp, lwipcmd->get_ip6_config);
            break;

        case EMMET__LWIP_CMD__CMD_SET_IP6_CONFIG:
            handle_set_ip6_config(raw_rsp, lwipcmd->set_ip6_config);
            break;

        case EMMET__LWIP_CMD__CMD_SET_STATIC_ARP:
            handle_set_static_arp(raw_rsp, lwipcmd->set_static_arp);
            break;

        case EMMET__LWIP_CMD__CMD_SET_TX_QOS_TID:
            handle_set_tx_qos_tid(raw_rsp, lwipcmd->set_tx_qos_tid);
            break;

        case EMMET__LWIP_CMD__CMD_GET_LINK_STATE:
            handle_get_link_state(raw_rsp, lwipcmd->get_link_state);
            break;

        case EMMET__LWIP_CMD__CMD_TCP_CONNECT:
            handle_tcp_connect(raw_rsp, lwipcmd->tcp_connect);
            break;

        case EMMET__LWIP_CMD__CMD_TCP_SEND:
            handle_tcp_send(raw_rsp, lwipcmd->tcp_send);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}

void emmet_lwip_init(void)
{
    memset(&emmet_mmiperf_state, 0, sizeof(emmet_mmiperf_state));
}


MM_WEAK int socket(int domain, int type, int protocol)
{
    MM_UNUSED(domain);
    MM_UNUSED(type);
    MM_UNUSED(protocol);

    return -1;
}

MM_WEAK int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    MM_UNUSED(s);
    MM_UNUSED(name);
    MM_UNUSED(namelen);

    return -1;
}

MM_WEAK ssize_t send(int s, const void *data, size_t size, int flags)
{
    MM_UNUSED(s);
    MM_UNUSED(data);
    MM_UNUSED(size);
    MM_UNUSED(flags);

    return -1;
}
