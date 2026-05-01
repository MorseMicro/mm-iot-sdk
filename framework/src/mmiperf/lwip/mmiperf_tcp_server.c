/*
 * Copyright (c) 2014 Simon Goldschmidt
 * Copyright 2021-2024 Morse Micro
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Authors: Simon Goldschmidt, Morse Micro
 */
#include <string.h>

#include "mmiperf_tcp_private.h"

#include "lwip/tcpip.h"
#include "lwip/arch.h"
#include "lwip/debug.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"

/* Currently, only TCP is implemented */
#if LWIP_TCP && LWIP_CALLBACK_API

static err_t iperf_start_tcp_server_impl(const struct mmiperf_server_args *args,
                                         struct iperf_state_tcp **state);

/** Receive data on an iperf tcp session */
static err_t iperf_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    uint8_t tmp;
    u16_t tot_len;
    uint32_t packet_idx;
    struct pbuf *q;
    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)arg;

    LWIP_ASSERT("pcb mismatch", conn->conn_pcb == tpcb);
    LWIP_UNUSED_ARG(tpcb);

    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Remote side aborted iperf test (%d)\n", MMIPERF_TCP_ABORTED_REMOTE));
        iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_REMOTE);
        return ERR_OK;
    }
    if (p == NULL)
    {
        iperf_tcp_close(conn, MMIPERF_TCP_DONE_SERVER);
        return ERR_OK;
    }
    tot_len = p->tot_len;

    conn->poll_count = 0;

    if ((!conn->have_settings_buf) ||
        ((conn->base.report.bytes_transferred - 24) % (1024 * 128) == 0))
    {
        /* wait for 24-byte header */
        if (p->tot_len < sizeof(conn->settings))
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                        ("test aborted due to data check error  (%d)\n",
                         MMIPERF_TCP_ABORTED_LOCAL_DATAERROR));
            iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_LOCAL_DATAERROR);
            pbuf_free(p);
            return ERR_OK;
        }
        if (!conn->have_settings_buf)
        {
            if (pbuf_copy_partial(p, &conn->settings, sizeof(conn->settings), 0) !=
                sizeof(conn->settings))
            {
                LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                            ("Test aborted due to local error (%d)\n", MMIPERF_TCP_ABORTED_LOCAL));
                iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_LOCAL);
                pbuf_free(p);
                return ERR_OK;
            }
            conn->have_settings_buf = 1;
        }
        conn->base.report.bytes_transferred += sizeof(conn->settings);
        if (conn->base.report.bytes_transferred <= 24)
        {
            conn->base.time_started_ms = sys_now();
            tcp_recved(tpcb, p->tot_len);
            pbuf_free(p);
            return ERR_OK;
        }
        tmp = pbuf_remove_header(p, 24);
        LWIP_ASSERT("pbuf_remove_header failed", tmp == 0);
        LWIP_UNUSED_ARG(tmp); /* for LWIP_NOASSERT */
    }

    packet_idx = 0;
    for (q = p; q != NULL; q = q->next)
    {
        packet_idx += q->len;
    }
    LWIP_ASSERT("count mismatch", packet_idx == p->tot_len);
    conn->base.report.bytes_transferred += packet_idx;
    tcp_recved(tpcb, tot_len);
    pbuf_free(p);
    return ERR_OK;
}

/** This is called when a new client connects for an iperf tcp session */
static err_t iperf_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    struct iperf_state_tcp *conn;
    if ((err != ERR_OK) || (newpcb == NULL) || (arg == NULL))
    {
        return ERR_VAL;
    }

    conn = (struct iperf_state_tcp *)arg;
    LWIP_ASSERT("invalid session", conn->base.server);
    LWIP_ASSERT("invalid listen pcb", conn->server_pcb != NULL);

    if (conn->conn_pcb != NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("TCP session already in progress\n"));
        return ERR_ALREADY;
    }

    memset(&conn->base.report, 0, sizeof(conn->base.report));
    memset(&conn->settings, 0, sizeof(conn->settings));
    conn->have_settings_buf = false;

    /* setup the tcp rx connection */
    conn->conn_pcb = newpcb;
    tcp_arg(newpcb, conn);
    tcp_recv(newpcb, iperf_tcp_recv);
    tcp_poll(newpcb, iperf_tcp_poll, 2U);
    tcp_err(conn->conn_pcb, iperf_tcp_err);

    iperf_tcp_init_report(conn, newpcb);

    return ERR_OK;
}

mmiperf_handle_t mmiperf_start_tcp_server(const struct mmiperf_server_args *args)
{
    err_t err;
    struct iperf_state_tcp *state = NULL;

    LOCK_TCPIP_CORE();
    err = iperf_start_tcp_server_impl(args, &state);
    UNLOCK_TCPIP_CORE();
    if (err == ERR_OK)
    {
        return &(state->base);
    }
    return NULL;
}

static err_t iperf_start_tcp_server_impl(const struct mmiperf_server_args *args,
                                         struct iperf_state_tcp **state)
{
    err_t err = ERR_MEM;
    struct tcp_pcb *pcb = NULL;
    struct iperf_state_tcp *s = NULL;
    uint16_t local_port;
    ip_addr_t local_addr = *(IP_ADDR_ANY);

    if (args->local_addr[0] != '\0')
    {
        int result = ipaddr_aton(args->local_addr, &local_addr);
        if (!result)
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS,
                        ("Unable to parse local_addr as IP address (%s)\n", args->local_addr));
            err = ERR_ARG;
            goto exit;
        }
    }

    LWIP_ASSERT_CORE_LOCKED();

    LWIP_ASSERT("state != NULL", state != NULL);

    s = (struct iperf_state_tcp *)IPERF_ALLOC(struct iperf_state_tcp);
    if (s == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS, ("Failed to allocate state data\n"));
        err = ERR_MEM;
        goto exit;
    }
    memset(s, 0, sizeof(struct iperf_state_tcp));
    s->base.tcp = 1;
    s->base.server = 1;
    s->base.report_fn = args->report_fn;
    s->base.report_arg = args->report_arg;

    pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS, ("Failed to create pcb for iperf server\n"));
        err = ERR_MEM;
        goto exit;
    }

    local_port = args->local_port ? args->local_port : MMIPERF_DEFAULT_PORT;

    err = tcp_bind(pcb, &local_addr, local_port);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS, ("Failed to bind to TCP port %d\n", local_port));
        goto exit;
    }
    pcb = tcp_listen_with_backlog_and_err(pcb, 1, &err);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS, ("Failed to listen on TCP port\n"));
        goto exit;
    }
    s->server_pcb = pcb;
    tcp_arg(s->server_pcb, s);
    tcp_accept(s->server_pcb, iperf_tcp_accept);

    iperf_list_add(&s->base);
    *state = s;
    s = NULL;
    return ERR_OK;

exit:
    if (pcb != NULL)
    {
        tcp_close(pcb);
        pcb = NULL;
    }
    if (s != NULL)
    {
        IPERF_FREE(struct iperf_state_tcp, s);
        s = NULL;
    }
    return err;
}

#endif /* LWIP_TCP && LWIP_CALLBACK_API */
