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

#pragma once

#include "mmiperf.h"
#include "../common/mmiperf_private.h"
#include "mmosal.h"

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"

/** Connection handle for a TCP iperf session */
struct iperf_state_tcp
{
    struct mmiperf_state base;
    struct tcp_pcb *server_pcb;
    struct tcp_pcb *conn_pcb;
    uint8_t poll_count;
    uint32_t mss;
    /* 1=start server when client is closed */
    uint8_t client_tradeoff_mode;
    struct iperf_settings settings;
    uint8_t have_settings_buf;
    ip_addr_t remote_addr;
    /* block parameter */
    bool bw_limit;
    uint32_t block_end_time;
    uint32_t block_txlen;
    int32_t block_remaining_txlen;
};

/** Close an iperf tcp session */
void iperf_tcp_close(struct iperf_state_tcp *conn, enum mmiperf_report_type report_type);

/** Stop an iperf tcp session (user-requested). */
bool iperf_tcp_stop(mmiperf_handle_t handle);

/** TCP poll callback, try to send more data */
err_t iperf_tcp_poll(void *arg, struct tcp_pcb *tpcb);

/** Error callback, iperf tcp session aborted */
void iperf_tcp_err(void *arg, err_t err);

/** TCP client send callback implementation */
err_t iperf_tcp_client_send_more(struct iperf_state_tcp *conn);

/** Initialize report fields for a TCP session */
void iperf_tcp_init_report(struct iperf_state_tcp *conn, struct tcp_pcb *pcb);
