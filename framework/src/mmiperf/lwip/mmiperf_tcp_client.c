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

/** Try to send more data on an iperf tcp session */
err_t iperf_tcp_client_send_more(struct iperf_state_tcp *conn)
{
    int send_more;
    err_t err;
    u16_t txlen;
    u16_t txlen_max;
    void *txptr;
    uint8_t apiflags;

    LWIP_ASSERT("conn invalid", (conn != NULL) && conn->base.tcp && (conn->base.server == 0));

    do {
        send_more = 0;
        if (conn->settings.amount & PP_HTONL(0x80000000))
        {
            /* this session is time-limited */
            uint32_t now = sys_now();
            uint32_t diff_ms = now - conn->base.time_started_ms;
            uint32_t time = (uint32_t)-(int32_t)lwip_htonl(conn->settings.amount);
            uint32_t time_ms = time * 10;

            if (diff_ms >= time_ms)
            {
                /* time specified by the client is over -> close the connection */
                iperf_tcp_close(conn, MMIPERF_TCP_DONE_CLIENT);
                return ERR_OK;
            }
        }
        else
        {
            /* this session is byte-limited */
            uint32_t amount_bytes = lwip_htonl(conn->settings.amount);
            /* @todo: this can send up to 1*MSS more than requested... */
            if (amount_bytes <= conn->base.report.bytes_transferred)
            {
                /* all requested bytes transferred -> close the connection */
                iperf_tcp_close(conn, MMIPERF_TCP_DONE_CLIENT);
                return ERR_OK;
            }
        }
        /* update block parameter after each block duration */
        if ((conn->bw_limit) && (conn->block_end_time < sys_now()))
        {
            conn->block_end_time += BLOCK_DURATION_MS;
            conn->block_remaining_txlen += conn->block_txlen;
        }

        if (conn->base.report.bytes_transferred < 24)
        {
            /* transmit the settings a first time */
            txptr = &((uint8_t *)&conn->settings)[conn->base.report.bytes_transferred];
            txlen_max = (u16_t)(24 - conn->base.report.bytes_transferred);
            apiflags = TCP_WRITE_FLAG_COPY;
        }
        else if (conn->base.report.bytes_transferred < 48)
        {
            /* transmit the settings a second time */
            txptr = &((uint8_t *)&conn->settings)[conn->base.report.bytes_transferred - 24];
            txlen_max = (u16_t)(48 - conn->base.report.bytes_transferred);
            apiflags = TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE;
            send_more = 1;
        }
        else
        {
            /* transmit data */
            /* @todo: every x bytes, transmit the settings again */
            txptr = LWIP_CONST_CAST(void *, iperf_get_data(conn->base.report.bytes_transferred));
            txlen_max = conn->mss;
            if (conn->base.report.bytes_transferred == 48)
            { /* @todo: fix this for intermediate settings, too */
                txlen_max = conn->mss - 24;
            }
            apiflags = 0; /* no copying needed */
            send_more = 1;
        }
        txlen = txlen_max;

        if (conn->conn_pcb->snd_buf >= (conn->mss / 2) &&
            conn->conn_pcb->snd_queuelen < LWIP_MIN(TCP_SND_QUEUELEN, TCP_SNDQUEUELEN_OVERFLOW))
        {
            if (txlen > conn->conn_pcb->snd_buf)
            {
                txlen = conn->conn_pcb->snd_buf;
            }
            err = tcp_write(conn->conn_pcb, txptr, txlen, apiflags);
        }
        else
        {
            err = ERR_MEM;
        }

        if (err == ERR_OK)
        {
            conn->base.report.bytes_transferred += txlen;
            conn->block_remaining_txlen -= txlen;
        }
        else
        {
            send_more = 0;
        }

        if ((conn->bw_limit) && (conn->block_remaining_txlen <= 0))
        {
            send_more = 0;
        }
    } while (send_more);

    tcp_output(conn->conn_pcb);
    return ERR_OK;
}

/** TCP sent callback, try to send more data */
static err_t iperf_tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)arg;
    /* @todo: check 'len' (e.g. to time ACK of all data)? for now, we just send more... */
    LWIP_ASSERT("invalid conn", conn->conn_pcb == tpcb);
    LWIP_UNUSED_ARG(tpcb);
    LWIP_UNUSED_ARG(len);

    conn->poll_count = 0;
    /* if block txlen is exceeded, sleep until block end time before sending more */
    while (conn->bw_limit && conn->block_remaining_txlen <= 0 && sys_now() < conn->block_end_time)
    {
        sys_msleep(1);
    }

    return iperf_tcp_client_send_more(conn);
}

/** TCP connected callback (active connection), send data now */
static err_t iperf_tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)arg;
    LWIP_ASSERT("invalid conn", conn->conn_pcb == tpcb);
    LWIP_UNUSED_ARG(tpcb);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Remote side aborted iperf test (%d)\n", MMIPERF_TCP_ABORTED_REMOTE));
        iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_REMOTE);
        return ERR_OK;
    }
    conn->poll_count = 0;
    conn->base.time_started_ms = sys_now();
    conn->block_end_time = sys_now() + BLOCK_DURATION_MS;

    iperf_tcp_init_report(conn, tpcb);

    return iperf_tcp_client_send_more(conn);
}

/** Start TCP connection back to the client (either parallel or after the
 * receive test has finished.
 */
static err_t iperf_tx_start_impl(const struct mmiperf_client_args *args,
                                 struct iperf_settings *settings,
                                 struct iperf_state_tcp **new_conn)
{
    int result;
    err_t err;
    struct iperf_state_tcp *client_conn;
    struct tcp_pcb *newpcb;
    ip_addr_t remote_addr;
    uint16_t server_port;

    LWIP_ASSERT("remote_ip != NULL", settings != NULL);
    LWIP_ASSERT("new_conn != NULL", new_conn != NULL);
    *new_conn = NULL;

    if (args->server_port == 0)
    {
        server_port = MMIPERF_DEFAULT_PORT;
    }
    else
    {
        server_port = args->server_port;
    }

    LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                ("Starting TCP iperf client to %s:%u, amount %ld\n",
                 args->server_addr,
                 server_port,
                 (int32_t)ntohl(settings->amount)));

    client_conn = (struct iperf_state_tcp *)IPERF_ALLOC(struct iperf_state_tcp);
    if (client_conn == NULL)
    {
        return ERR_MEM;
    }

    result = ipaddr_aton(args->server_addr, &remote_addr);
    if (!result)
    {
        IPERF_FREE(struct iperf_state_tcp, client_conn);
        return ERR_ARG;
    }

    newpcb = tcp_new_ip_type(IP_GET_TYPE(&remote_addr));
    if (newpcb == NULL)
    {
        IPERF_FREE(struct iperf_state_tcp, client_conn);
        return ERR_MEM;
    }
    memset(client_conn, 0, sizeof(*client_conn));
    client_conn->base.tcp = 1;
    client_conn->conn_pcb = newpcb;
    client_conn->base.time_started_ms = sys_now();
    client_conn->base.report_fn = args->report_fn;
    client_conn->base.report_arg = args->report_arg;
    memcpy(&client_conn->settings, settings, sizeof(*settings));
    client_conn->have_settings_buf = 1;
    client_conn->mss = TCP_MSS;

#if LWIP_IPV6
    if (IP_IS_V6(&remote_addr))
    {
        client_conn->mss -= IPV6_HEADER_SIZE_DIFF;
    }
#endif

    /* set block parameter if bandwidth limit is set */
    if (args->target_bw == 0)
    {
        client_conn->bw_limit = false;
    }
    else
    {
        client_conn->bw_limit = true;
        client_conn->block_txlen = args->target_bw * BLOCK_DURATION_MS / 8;
        client_conn->block_remaining_txlen = client_conn->block_txlen;
        /* log error message if maximum segment size is too large for chosen bandwidth limit */
        if (client_conn->mss > client_conn->block_txlen)
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS, ("bandwidth limit too low\n"));
            iperf_tcp_close(client_conn, MMIPERF_TCP_ABORTED_LOCAL);
            return ERR_ARG;
        }
    }

    tcp_arg(newpcb, client_conn);
    tcp_sent(newpcb, iperf_tcp_client_sent);
    tcp_poll(newpcb, iperf_tcp_poll, 2U);
    tcp_err(newpcb, iperf_tcp_err);

    err = tcp_connect(newpcb, &remote_addr, server_port, iperf_tcp_client_connected);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Test aborted due to local error (%d)\n", MMIPERF_TCP_ABORTED_LOCAL));
        iperf_tcp_close(client_conn, MMIPERF_TCP_ABORTED_LOCAL);
        return err;
    }
    iperf_list_add(&client_conn->base);
    *new_conn = client_conn;
    return ERR_OK;
}

/** Control */
enum lwiperf_client_type
{
    /** Unidirectional tx only test */
    LWIPERF_CLIENT,
    /** Do a bidirectional test simultaneously */
    LWIPERF_DUAL,
    /** Do a bidirectional test individually */
    LWIPERF_TRADEOFF
};

mmiperf_handle_t mmiperf_start_tcp_client(const struct mmiperf_client_args *args)
{
    err_t ret;
    struct iperf_settings settings;
    struct iperf_state_tcp *state = NULL;
    mmiperf_handle_t result = NULL;

    /* Bidirectional/trade-off disabled until better tested and also until supported in UDP. */

    memset(&settings, 0, sizeof(settings));

    settings.amount = htonl(args->amount);
    settings.num_threads = htonl(1);
    settings.remote_port = htonl(MMIPERF_DEFAULT_PORT);

    LOCK_TCPIP_CORE();
    ret = iperf_tx_start_impl(args, &settings, &state);
    if (ret == ERR_OK)
    {
        LWIP_ASSERT("state != NULL", state != NULL);
        result = &(state->base);
    }
    UNLOCK_TCPIP_CORE();
    return result;
}

#endif /* LWIP_TCP && LWIP_CALLBACK_API */
