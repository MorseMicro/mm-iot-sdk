/*
 * Copyright 2021-2026 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "lwip/err.h"
#include "mmosal.h"
#include "../common/mmiperf_private.h"

#include "lwip/debug.h"
#include "lwip/igmp.h"
#include "lwip/ip_addr.h"
#include "lwip/mld6.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

/* Currently, only UDP is implemented */
#if LWIP_UDP && LWIP_CALLBACK_API

/* State data fo a speicific iperf server session */
struct iperf_server_session_udp
{
    /* A negative value indicates there is no active session (but there may still be state
     * from a previous session) */
    int32_t next_packet_id;

    ip_addr_t client_addr;
    uint16_t client_port;
    struct timeval ipg_start;
};

/** Connection handle for a UDP iperf server */
struct iperf_server_state_udp
{
    struct mmiperf_state base;

    struct
    {
        ip_addr_t local_addr;
        uint16_t local_port;
        enum iperf_version version;
    } args;
    struct udp_pcb *pcb;
    struct iperf_server_session_udp session;
};

/* ip_addr_cmp_zoneless may not be defined if IPv6 is not enabled. In that case we don't have
 * zones so a normal compare is sufficient. */
#ifndef ip_addr_cmp_zoneless
#define ip_addr_cmp_zoneless(addr1, addr2) ip_addr_cmp(addr1, addr2)
#endif

static bool udp_server_session_has_timed_out(struct iperf_server_state_udp *server_state)
{
    return mmosal_time_has_passed(
        server_state->base.last_rx_time_ms + IPERF_UDP_SERVER_SESSION_TIMEOUT_MS);
}

static struct iperf_server_session_udp *udp_server_start_session(
    struct iperf_server_state_udp *server_state,
    const ip_addr_t *addr,
    uint16_t port)
{
    /* For now we only support a single session. */
    struct iperf_server_session_udp *session = &(server_state->session);
    if (session->next_packet_id >= 0 && !udp_server_session_has_timed_out(server_state))
    {
        return NULL;
    }

    memset(session, 0, sizeof(*session));
    session->client_addr = *addr;
    session->client_port = port;
    memset(&server_state->base.report, 0, sizeof(server_state->base.report));
    server_state->base.report.report_type = MMIPERF_INTERRIM_REPORT;
    server_state->base.time_started_ms = mmosal_get_time_ms();
    server_state->base.report.local_port = server_state->args.local_port;
    server_state->base.report.remote_port = port;

    const char *result;
    result = ipaddr_ntoa_r(&server_state->pcb->local_ip,
                           server_state->base.report.local_addr,
                           sizeof(server_state->base.report.local_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);
    result = ipaddr_ntoa_r(addr,
                           server_state->base.report.remote_addr,
                           sizeof(server_state->base.report.remote_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);

    return session;
}

static struct iperf_server_session_udp *get_session(struct iperf_server_state_udp *server_state,
                                                    const ip_addr_t *addr,
                                                    uint16_t port)
{
    /* We only support a single session. */
    struct iperf_server_session_udp *session = &(server_state->session);
    if (ip_addr_cmp_zoneless(&(session->client_addr), addr) &&
        session->client_port == port &&
        !udp_server_session_has_timed_out(server_state))
    {
        return session;
    }
    else
    {
        return udp_server_start_session(server_state, addr, port);
    }
}

/**
 * Get the difference in microseconds between two timevals.
 *
 * \note Delta must not exceed the range of an int32.
 */
static int32_t time_delta(const struct timeval *a, const struct timeval *b)
{
    int32_t delta = (a->tv_sec - b->tv_sec) * 1000000;
    delta += (a->tv_usec - b->tv_usec);
    return delta;
}

/* Construct and send server report after final packet if not a multicast address. */
static void iperf_udp_server_handle_final_packet(struct iperf_server_state_udp *server_state,
                                                 struct udp_pcb *pcb,
                                                 const ip_addr_t *addr,
                                                 uint16_t port,
                                                 struct pbuf *p)
{
    struct pbuf *report_pbuf = NULL;

    if (ip_addr_ismulticast(&(server_state->args.local_addr)))
    {
        return;
    }

    struct iperf_udp_server_report report;

    report_pbuf =
        pbuf_alloc(PBUF_TRANSPORT, sizeof(report) + sizeof(struct iperf_udp_header), PBUF_RAM);
    if (report_pbuf == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Pbuf alloc failure.\n"));
        goto report_cleanup;
    }
    err_t err = pbuf_copy_partial_pbuf(report_pbuf, p, sizeof(struct iperf_udp_header), 0);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Failed to retrieve header.\n"));
        goto report_cleanup;
    }

    iperf_populate_udp_server_report(&server_state->base, &report);

    err = pbuf_take_at(report_pbuf, &report, sizeof(report), sizeof(struct iperf_udp_header));

    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Failed to add report to pbuf.\n"));
        goto report_cleanup;
    }

    err = udp_sendto(pcb, report_pbuf, addr, port);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Failed to send Iperf Server Report.\n"));
    }

report_cleanup:
    if (report_pbuf != NULL)
    {
        pbuf_free(report_pbuf);
    }
}

static void iperf_udp_server_recv(void *arg,
                                  struct udp_pcb *pcb,
                                  struct pbuf *p,
                                  const ip_addr_t *addr,
                                  uint16_t port)
{
    struct iperf_server_state_udp *server_state = (struct iperf_server_state_udp *)arg;

    LWIP_ASSERT("NULL packet", p != NULL);

    struct iperf_udp_header hdr = { 0 };

    uint16_t copy_len = pbuf_copy_partial(p, &hdr, sizeof(hdr), 0);
    if (copy_len != sizeof(hdr))
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Dropping too short packet\n"));
        goto cleanup;
    }

    struct iperf_settings settings = { 0 };
    copy_len = pbuf_copy_partial(p, &settings, sizeof(settings), sizeof(hdr));
    if (copy_len != sizeof(settings))
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Dropping too short packet\n"));
        goto cleanup;
    }

    struct timeval packet_time = {
        .tv_sec = ntohl(hdr.tv_sec),
        .tv_usec = ntohl(hdr.tv_usec),
    };
    int64_t packet_id = 0;
    bool final_packet = false;
    struct iperf_server_session_udp *session = &server_state->session;

    if (server_state->args.version == IPERF_VERSION_2_0_9)
    {
        packet_id = (int64_t)((int32_t)ntohl(hdr.id_lo));
    }
    else
    {
        packet_id = (int64_t)(((uint64_t)ntohl(hdr.id_hi) << 32) | (uint64_t)ntohl(hdr.id_lo));
    }

    /* A negative packet ID indicates that this is the final packet. */
    if (packet_id < 0)
    {
        final_packet = true;
        packet_id = -packet_id;
    }

    session = get_session(server_state, addr, port);
    if (session == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Another session already in progress\n"));
        goto cleanup;
    }

    /* next_packet_id < 0 indicates that we have already received the final frame from the client
     * so we should not update our session state. However, we can still send responses. */
    if (session->next_packet_id >= 0)
    {
        server_state->base.last_rx_time_ms = mmosal_get_time_ms();
        server_state->base.report.bytes_transferred += p->tot_len;
        server_state->base.report.rx_frames++;
        server_state->base.report.ipg_count++;
        server_state->base.report.ipg_sum_ms += time_delta(&packet_time, &(session->ipg_start));
        session->ipg_start = packet_time;

        if (packet_id < session->next_packet_id)
        {
            server_state->base.report.out_of_sequence_frames++;
        }
        else if (packet_id > session->next_packet_id)
        {
            server_state->base.report.error_count += packet_id - session->next_packet_id;
        }

        if (packet_id >= session->next_packet_id)
        {
            session->next_packet_id = packet_id + 1;
        }
    }

    if (final_packet)
    {
        /* Handle the local report if this is the first time receiving the final packet. */
        if (session->next_packet_id >= 0)
        {
            uint32_t duration_ms =
                server_state->base.last_rx_time_ms - server_state->base.time_started_ms;
            iperf_finalize_report_and_invoke_callback(&server_state->base,
                                                      duration_ms,
                                                      MMIPERF_UDP_DONE_SERVER);
            session->next_packet_id = -1;
        }

        iperf_udp_server_handle_final_packet(server_state, pcb, addr, port, p);
    }

cleanup:
    if (p != NULL)
    {
        pbuf_free(p);
    }
}

bool iperf_udp_server_stop(mmiperf_handle_t handle)
{
    struct mmiperf_state *state = iperf_list_get(handle);
    if (state == NULL || state->tcp || !state->server)
    {
        return false;
    }

    struct iperf_server_state_udp *server_state = (struct iperf_server_state_udp *)state;

    LOCK_TCPIP_CORE();

    if (server_state->pcb == NULL)
    {
        UNLOCK_TCPIP_CORE();
        return false;
    }

    /* If there is an active session, send report to client. */
    if (server_state->session.next_packet_id >= 0 &&
        !ip_addr_ismulticast(&(server_state->args.local_addr)))
    {
        struct pbuf *report_pbuf =
            pbuf_alloc(PBUF_TRANSPORT,
                       sizeof(struct iperf_udp_server_report) + sizeof(struct iperf_udp_header),
                       PBUF_RAM);
        if (report_pbuf != NULL)
        {
            struct iperf_udp_header hdr = { 0 };
            struct iperf_udp_server_report report;

            iperf_populate_udp_server_report(&server_state->base, &report);
            memcpy(report_pbuf->payload, &hdr, sizeof(hdr));
            memcpy((uint8_t *)report_pbuf->payload + sizeof(hdr), &report, sizeof(report));

            udp_sendto(server_state->pcb,
                       report_pbuf,
                       &server_state->session.client_addr,
                       server_state->session.client_port);
            pbuf_free(report_pbuf);
        }
    }

    udp_recv(server_state->pcb, NULL, NULL);
    udp_remove(server_state->pcb);
    server_state->pcb = NULL;

    uint32_t duration_ms = 0;
    if (server_state->session.next_packet_id >= 0)
    {
        duration_ms = server_state->base.last_rx_time_ms - server_state->base.time_started_ms;
    }
    iperf_list_remove(&server_state->base);
    iperf_finalize_report_and_invoke_callback(&server_state->base, duration_ms, MMIPERF_STOPPED);
    IPERF_FREE(struct iperf_server_state_udp, server_state);

    UNLOCK_TCPIP_CORE();
    return true;
}

mmiperf_handle_t mmiperf_start_udp_server(const struct mmiperf_server_args *args)
{
    LOCK_TCPIP_CORE();

    err_t err;
    struct udp_pcb *pcb;
    struct iperf_server_state_udp *s;
    mmiperf_handle_t result = NULL;

    LWIP_ASSERT_CORE_LOCKED();

    s = (struct iperf_server_state_udp *)IPERF_ALLOC(struct iperf_server_state_udp);
    if (s == NULL)
    {
        goto exit;
    }
    memset(s, 0, sizeof(*s));
    s->base.tcp = 0;
    s->base.server = 1;
    s->base.report_fn = args->report_fn;
    s->base.report_arg = args->report_arg;
    s->args.local_port = args->local_port;
    s->args.version = args->version;
    /* Set next_packet_id to -1 to show that there is no session active. We will start a new
     * session with the first packet we receive from a client. */
    s->session.next_packet_id = -1;
    s->base.report.report_type = MMIPERF_INTERRIM_REPORT;

    s->args.local_addr = *(IP_ADDR_ANY);
    if (args->local_addr[0] != '\0')
    {
        int ok = ipaddr_aton(args->local_addr, &s->args.local_addr);
        if (!ok)
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_SERIOUS,
                        ("Unable to parse local_addr as IP address (%s)\n", args->local_addr));
            goto exit;
        }
    }

    pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == NULL)
    {
        goto exit;
    }

    err = udp_bind(pcb, &s->args.local_addr, args->local_port);
    if (err != ERR_OK)
    {
        goto exit;
    }
#if LWIP_IPV4 && LWIP_IGMP
    if (IP_IS_V4(&s->args.local_addr) && ip_addr_ismulticast(&s->args.local_addr))
    {
        const ip_addr_t ifaddr = IPADDR4_INIT(IPADDR_ANY);
        igmp_joingroup(ip_2_ip4(&ifaddr), ip_2_ip4(&s->args.local_addr));
    }
#endif
#if LWIP_IPV6 && LWIP_IGMP
    if (IP_IS_V6(&s->args.local_addr) && ip_addr_ismulticast(&s->args.local_addr))
    {
        mld6_joingroup(IP6_ADDR_ANY6, ip_2_ip6(&s->args.local_addr));
    }
#endif

    udp_recv(pcb, iperf_udp_server_recv, s);

    s->pcb = pcb;

    iperf_list_add(&s->base);
    result = &(s->base);
    s = NULL;

exit:
    if (s != NULL)
    {
        IPERF_FREE(struct iperf_server_state_udp, s);
    }
    UNLOCK_TCPIP_CORE();
    return result;
}

#endif
