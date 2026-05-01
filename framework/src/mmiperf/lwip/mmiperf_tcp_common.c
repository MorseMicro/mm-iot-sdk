/*
 * Copyright (c) 2014 Simon Goldschmidt
 * Copyright 2021-2026 Morse Micro
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
#include "mmiperf_tcp_private.h"

#include "lwip/tcpip.h"
#include "lwip/arch.h"
#include "lwip/debug.h"

/* Currently, only TCP is implemented */
#if LWIP_TCP && LWIP_CALLBACK_API

/** Close an iperf tcp session */
void iperf_tcp_close(struct iperf_state_tcp *conn, enum mmiperf_report_type report_type)
{
    err_t err;

    MMOSAL_ASSERT(conn != NULL);

    iperf_finalize_report_and_invoke_callback(&conn->base,
                                              mmosal_get_time_ms() - conn->base.time_started_ms,
                                              report_type);
    if (conn->conn_pcb != NULL)
    {
        tcp_arg(conn->conn_pcb, NULL);
        tcp_poll(conn->conn_pcb, NULL, 0);
        tcp_sent(conn->conn_pcb, NULL);
        tcp_recv(conn->conn_pcb, NULL);
        tcp_err(conn->conn_pcb, NULL);
        err = tcp_close(conn->conn_pcb);
        if (err != ERR_OK)
        {
            /* don't want to wait for free memory here... */
            tcp_abort(conn->conn_pcb);
        }
        conn->conn_pcb = NULL;
    }

    if (conn->server_pcb != NULL && report_type == MMIPERF_STOPPED)
    {
        /* no conn pcb, this is the listener pcb */
        err = tcp_close(conn->server_pcb);
        LWIP_ASSERT("error", err == ERR_OK);
        conn->server_pcb = NULL;
    }

    if (conn->conn_pcb == NULL && conn->server_pcb == NULL)
    {
        iperf_list_remove(&conn->base);
        IPERF_FREE(struct iperf_state_tcp, conn);
    }
}

/** Error callback, iperf tcp session aborted */
void iperf_tcp_err(void *arg, err_t err)
{
    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)arg;
    LWIP_UNUSED_ARG(err);

    /* pcb is already deallocated, prevent double-free */
    conn->conn_pcb = NULL;
    conn->server_pcb = NULL;

    LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                ("Remote side aborted iperf test (%d)\n", MMIPERF_TCP_ABORTED_REMOTE));
    iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_REMOTE);
}

/** TCP poll callback, try to send more data */
err_t iperf_tcp_poll(void *arg, struct tcp_pcb *tpcb)
{
    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)arg;
    LWIP_ASSERT("pcb mismatch", conn->conn_pcb == tpcb);
    LWIP_UNUSED_ARG(tpcb);
    if (++conn->poll_count >= IPERF_TCP_MAX_IDLE_S)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Test aborted due to local error (%d)\n", MMIPERF_TCP_ABORTED_LOCAL));
        iperf_tcp_close(conn, MMIPERF_TCP_ABORTED_LOCAL);
        return ERR_OK; /* iperf_tcp_close frees conn */
    }

    if (!conn->base.server)
    {
        iperf_tcp_client_send_more(conn);
    }

    return ERR_OK;
}

void iperf_tcp_init_report(struct iperf_state_tcp *conn, struct tcp_pcb *pcb)
{
    char *result;
    struct mmiperf_report *report = &conn->base.report;

    report->report_type = MMIPERF_INTERRIM_REPORT;

    if (pcb != NULL)
    {
        result = ipaddr_ntoa_r(&pcb->local_ip, report->local_addr, sizeof(report->local_addr));
        LWIP_ASSERT("IP buf too short", result != NULL);
        report->local_port = pcb->local_port;
    }

    if (pcb != NULL)
    {
        result = ipaddr_ntoa_r(&pcb->remote_ip, report->remote_addr, sizeof(report->remote_addr));
        LWIP_ASSERT("IP buf too short", result != NULL);
        report->remote_port = pcb->remote_port;
    }
}

bool iperf_tcp_stop(mmiperf_handle_t handle)
{
    struct mmiperf_state *state = iperf_list_get(handle);
    if (state == NULL || !state->tcp)
    {
        return false;
    }

    struct iperf_state_tcp *conn = (struct iperf_state_tcp *)state;
    LOCK_TCPIP_CORE();
    iperf_tcp_close(conn, MMIPERF_STOPPED);
    UNLOCK_TCPIP_CORE();
    return true;
}

#endif /* LWIP_TCP && LWIP_CALLBACK_API */
