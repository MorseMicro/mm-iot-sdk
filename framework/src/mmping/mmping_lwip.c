/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lwip/mem.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/icmp6.h"
#include "lwip/inet.h"
#include "mmping.h"
#include "mmosal.h"
#include "lwip/tcpip.h"

#if !LWIP_RAW
#error "LWIP RAW support required"
#endif

#if !(LWIP_IPV4  || LWIP_IPV6)
#error "LWIP IPV4 or LWIP_IPV6 support required"
#endif


/** If ping interval is >= this threshold then per-packet log messages will be displayed. */
#define PING_DISPLAY_THRESHOLD_MS   (1000)

/** Magic number we put in the packet to make sure that the payload gets echoed. */
#define PING_MAGIC_NUMBER           (0xabcd0123)

/** Number of bits in our duplicate check bitmap. */
#define N_DUP_CHECK_BITS            (128)

/** Time to wait on failing to send a ping request. */
#define PING_ERROR_RETRY_INTERVAL_MS    (1000)

/** Network layer Pbuf IPv4 header length. */
#define PBUF_IP_HLEN_V4                                 (20)
/** Network layer Pbuf IPv6 header length. */
#define PBUF_IP_HLEN_V6                                 (40)

enum ping_state
{
    /** Ping is not currently running (may have stats left over from last run). */
    PING_IDLE,
    /** Last sent request has been acknowledged. Waiting for timeout to send next. */
    PING_AWAITING_SND,
    /** Last sent request has not yet been acknowledged. */
    PING_AWAITING_RSP,
};

struct ping_session
{
    /** Ping arguments. */
    struct {
        /** The local IP address, in binary format. */
        ip_addr_t ping_src;
        /** The IP address of the ping target, in binary format. */
        ip_addr_t ping_target;
        /** The time interval between ping requests (in milliseconds) */
        uint32_t ping_interval_ms;
        /**
         * This specifies the number of ping requests to send before terminating
         * the session. If this is zero or exceeds @ref MMPING_MAX_COUNT then it
         * it will be set to @ref MMPING_MAX_COUNT.
         */
        uint32_t ping_count;
        /** Specifies the data packet size in bytes excluding 8 bytes ICMP header */
        uint32_t ping_size;
    } args;

    /** This is where we record ping statistics. */
    volatile struct mmping_stats stats;
    /** Last received sequence number (excluding out-of-order packets). A negative number indicates
     *  no packets received yet. */
    volatile int32_t last_rx_seq_num;
    /** Bitmap used to check for duplicates. */
    volatile uint32_t dup_check_bitmap[N_DUP_CHECK_BITS/sizeof(uint32_t)];
    /** Sum of all RTTs for this session. */
    volatile uint32_t rtt_sum_ms;
    /** Absolute time for the next timeout event for this session. */
    volatile uint32_t timeout_time_ms;
    /** The time at which to send the next ping request with a new sequence number (i.e.,
     *  not including retries for the current sequence number). */
    volatile uint32_t next_seq_time_ms;
    /** ID of this session. This will be included in ping packets. */
    volatile uint16_t session_id;
    /** Number of retries of current request. */
    volatile uint16_t num_retries;
    /** Current ping state. */
    volatile enum ping_state state;
};

/** The current ping session. At present we only support a single session, but in future this
 *  may be an array of sessions. */
static struct ping_session ping_session;
/** Handle of the running ping task, or NULL if it is not running. */
static struct mmosal_task *ping_task_handle;
/** The next session ID to use. */
static uint16_t ping_next_session_id;

/**
 * Set the given bit in the duplicate checking bitmap for the given ping session.
 *
 * @warning This function does not perfom bounds checking. @p bit_num must be less than
 *          @ref N_DUP_CHECK_BITS.
 *
 * @param session   The ping session data structure.
 * @param bit_num   The offset of the bit to set. Must be less than @ref N_DUP_CHECK_BITS.
 */
static void dup_check_bitmap_set(volatile struct ping_session *session, uint16_t bit_num)
{
    size_t word_num = bit_num/sizeof(uint32_t);
    uint32_t mask = (1ul << bit_num % sizeof(uint32_t));

    session->dup_check_bitmap[word_num] |= mask;
}

/**
 * Clear the given bit in the duplicate checking bitmap for the given ping session.
 *
 * @warning This function does not perfom bounds checking. @p bit_num must be less than
 *          @ref N_DUP_CHECK_BITS.
 *
 * @param session   The ping session data structure.
 * @param bit_num   The offset of the bit to clear. Must be less than @ref N_DUP_CHECK_BITS.
 */
static void dup_check_bitmap_clear(volatile struct ping_session *session, uint16_t bit_num)
{
    size_t word_num = bit_num/sizeof(uint32_t);
    uint32_t mask = (1ul << bit_num % sizeof(uint32_t));

    session->dup_check_bitmap[word_num] &= ~mask;
}

/**
 * Check whether the given bit is set in the duplicate checking bitmap for the given ping session.
 *
 * @warning This function does not perfom bounds checking. @p bit_num must be less than
 *          @ref N_DUP_CHECK_BITS.
 *
 * @param session   The ping session data structure.
 * @param bit_num   The offset of the bit to check. Must be less than @ref N_DUP_CHECK_BITS.
 *
 * @returns @c true if the bit is set, else @c false.
 */
static bool dup_check_bitmap_is_set(volatile struct ping_session *session, uint16_t bit_num)
{
    size_t word_num = bit_num/sizeof(uint32_t);
    uint32_t mask = (1ul << bit_num % sizeof(uint32_t));

    return (session->dup_check_bitmap[word_num] & mask) != 0;
}

/**
 * Update session state on acknowledgement of the most recently sent ping request.
 */
static void process_ping_acknowledgement(struct ping_session *session)
{
    /* If we are in the AWAITNG_RSP state, then check if all outstanding ping requests have now
     * been acknowledged, and if so transition into the IDLE state; otherwise update timeout and
     * return to the AWAITING_SND state in preparation to send the next request. */

    if (session->state == PING_AWAITING_RSP)
    {
        if (session->last_rx_seq_num >= ((int32_t)session->args.ping_count - 1))
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                        ("Final ping response received, transitioning to IDLE state\n"));
            session->state = PING_IDLE;
        }
        else
        {
            session->state = PING_AWAITING_SND;
            session->timeout_time_ms = mmosal_get_time_ms() + session->args.ping_interval_ms;
        }

        /* Wake up the ping task to handle the state change. */
        if (ping_task_handle != NULL)
        {
            mmosal_task_notify(ping_task_handle);
        }
    }
}

/**
 * Performs the bulk of the handling for a ping response.
 *
 * @note length of ping packet must be validated before entry to this function (taking into
 *       account the first 8 octets of payload that we put information into).
 */
static uint8_t process_ping_response(const uint8_t *hdr,
                                     size_t payload_size,
                                     const ip_addr_t *remote_ip_addr)
{
    LWIP_UNUSED_ARG(payload_size);
    struct ping_session *session = &ping_session;
    uint16_t ping_id = 0;
    uint16_t seq_num = 0;
#if LWIP_IPV4
    if (IP_IS_V4(remote_ip_addr))
    {
        ping_id = ntohs(((const struct icmp_echo_hdr *)hdr)->id);
        seq_num = ntohs(((const struct icmp_echo_hdr *)hdr)->seqno);
    }
#endif
#if LWIP_IPV6
    if (IP_IS_V6(remote_ip_addr))
    {
        ping_id = ntohs(((const struct icmp6_echo_hdr *)hdr)->id);
        seq_num = ntohs(((const struct icmp6_echo_hdr *)hdr)->seqno);
    }
#endif

    if (ping_id != session->session_id)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("ID mismatch (%u vs %u)\n", ping_id, session->session_id));
        return 0;
    }

    if (dup_check_bitmap_is_set(session, seq_num % N_DUP_CHECK_BITS))
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Duplicate ping response received: seq=%u\n", seq_num));
        return 1;
    }

    dup_check_bitmap_set(session, seq_num % N_DUP_CHECK_BITS);

    if ((int32_t)seq_num < session->last_rx_seq_num)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                    ("Ping response received out-of-order (got %u, expect %ld)\n",
                     seq_num, session->last_rx_seq_num));
    }

    /* Note: length of payload was validated before this function was invoked. */
    const uint8_t *ping_payload = NULL;
#if LWIP_IPV4
    if (IP_IS_V4(remote_ip_addr))
    {
        ping_payload = (const uint8_t *)((const struct icmp_echo_hdr *)hdr + 1);
    }
#endif
#if LWIP_IPV6
    if (IP_IS_V6(remote_ip_addr))
    {
        ping_payload = (const uint8_t *)((const struct icmp6_echo_hdr *)hdr + 1);
    }
#endif
    MMOSAL_ASSERT(ping_payload != NULL);

    uint32_t sent_time =
        (ping_payload[0] << 24) | (ping_payload[1] << 16) |
        (ping_payload[2] << 8) | (ping_payload[3]);

    uint32_t magic_number =
        (ping_payload[4] << 24) | (ping_payload[5] << 16) |
        (ping_payload[6] << 8) | (ping_payload[7]);

    if (magic_number != PING_MAGIC_NUMBER)
    {
        /* If the magic number does not match then we cannot calculate RTT. */
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL, ("Ping response does not include payload\n"));
        return 1;
    }

    uint32_t ping_rtt = mmosal_get_time_ms() - sent_time;
    if ((int32_t)ping_rtt < 0)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                    ("Ping response received with negative RTT (%ld)\n", (int32_t)ping_rtt));
        return 1;
    }

    char ip_addr_str[IPADDR_STRLEN_MAX];
    ipaddr_ntoa_r(remote_ip_addr, ip_addr_str, sizeof(ip_addr_str));
    if (session->args.ping_interval_ms >= PING_DISPLAY_THRESHOLD_MS)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL, ("%10lu | %u bytes from %s: seq=%u time=%lu ms\n",
                    mmosal_get_time_ms(), payload_size, ip_addr_str, seq_num, ping_rtt));
    }

    if (ping_rtt < session->stats.ping_min_time_ms || session->stats.ping_recv_count == 0)
    {
        session->stats.ping_min_time_ms = ping_rtt;
    }

    if (ping_rtt > session->stats.ping_max_time_ms)
    {
        session->stats.ping_max_time_ms = ping_rtt;
    }

    session->rtt_sum_ms += ping_rtt;
    session->stats.ping_recv_count++;
    session->last_rx_seq_num = seq_num;

    /* If the seq_num matches the most recently sent request then process this as acknowledgement
     * of that request. */
    if (seq_num == (session->stats.ping_total_count - 1))
    {
        process_ping_acknowledgement(session);
    }
    return 1;
}

/**
 * ICMP receive packet handler.
 */
static uint8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(addr);
    LWIP_ASSERT("p != NULL", p != NULL);

    size_t ip_hdr_len = 0;
    size_t echo_hdr_size = 0;

#if LWIP_IPV4
    if (IP_IS_V4(addr))
    {
        ip_hdr_len = PBUF_IP_HLEN_V4;
        echo_hdr_size = sizeof(struct icmp_echo_hdr);
    }
#endif
#if LWIP_IPV6
    if (IP_IS_V6(addr))
    {
        ip_hdr_len = PBUF_IP_HLEN_V6;
        echo_hdr_size = sizeof(struct icmp6_echo_hdr);
    }
#endif

    if (p->tot_len < (ip_hdr_len + echo_hdr_size + MMPING_MIN_DATA_SIZE))
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Received ping packet too short\n"));
        return 0;
    }

    uint8_t ret = process_ping_response((uint8_t *)p->payload + ip_hdr_len,
                                        p->len - ip_hdr_len, addr);
    if (ret != 0)
    {
        pbuf_free(p);
    }

    return ret;
}

/**
 * Update the timeout time for a session after successfully sending a ping packet.
 *
 * The timeout time is 2 times the average RTT. If no packets have been sent then
 * @c MMPING_INITIAL_RETRY_INTERVAL will be used.
 */
static void ping_session_update_timeout_on_send(struct ping_session *session, bool success)
{
    uint32_t timeout_duration = PING_ERROR_RETRY_INTERVAL_MS;
    if (success)
    {
        if (session->stats.ping_recv_count > 0)
        {
            timeout_duration = 2 * session->rtt_sum_ms / session->stats.ping_recv_count;
        }
        else
        {
            timeout_duration = MMPING_INITIAL_RETRY_INTERVAL_MS;
        }
    }
    session->timeout_time_ms = mmosal_get_time_ms() + timeout_duration;
}

/**
 * Generate Ping payload.
 */
static void generate_ping_payload(uint8_t *ping_payload, struct ping_session *session)
{
    uint32_t timestamp = mmosal_get_time_ms();
    /* First 4 octets of payload are the timestamp (big endian). */
    ping_payload[0] = (uint8_t)(timestamp >> 24);
    ping_payload[1] = (uint8_t)(timestamp >> 16);
    ping_payload[2] = (uint8_t)(timestamp >> 8);
    ping_payload[3] = (uint8_t)(timestamp);
    /* Next 4 octets of payload are the magic number (big endian). */
    ping_payload[4] = (uint8_t)(PING_MAGIC_NUMBER >> 24);
    ping_payload[5] = (uint8_t)(PING_MAGIC_NUMBER >> 16);
    ping_payload[6] = (uint8_t)(PING_MAGIC_NUMBER >> 8);
    ping_payload[7] = (uint8_t)(PING_MAGIC_NUMBER);

    /* Fill the additional data buffer with some data */
    uint32_t i;
    for (i = 8; i < session->args.ping_size; i++)
    {
        if ((i & 0x0f) <= 9)
        {
            ping_payload[i] = '0' + (i & 0x0f);
        }
        else
        {
            ping_payload[i] = 'a' - 0x0a + (i & 0x0f);
        }
    }
}

#if LWIP_IPV4
/**
 * Generate an ICMP echo request Pbuf.
 */
void ping_echo_pbuf(struct pbuf *p, struct ping_session *session,
                    uint16_t seq_num, size_t size)
{
    struct icmp_echo_hdr *hdr = (struct icmp_echo_hdr *)p->payload;
    memset(hdr, 0, sizeof(*hdr));

    ICMPH_TYPE_SET(hdr, ICMP_ECHO);

    hdr->chksum = 0;
    hdr->id     = lwip_htons(session->session_id);
    hdr->seqno  = lwip_htons(seq_num);

    uint8_t *ping_payload = (uint8_t *)p->payload + sizeof(*hdr);
    generate_ping_payload(ping_payload, session);

    /* Note: checksum needs to be calculated AFTER payload is filled out. */
    hdr->chksum = inet_chksum(hdr, size);
}
#endif

#if LWIP_IPV6
/**
 * Generate an ICMPv6 echo request Pbuf.
 */
void ping6_echo_pbuf(struct pbuf *p, struct ping_session *session, uint16_t seq_num)
{
    struct icmp6_echo_hdr *hdr = (struct icmp6_echo_hdr *)p->payload;
    memset(hdr, 0, sizeof(*hdr));

    ICMPH_TYPE_SET(hdr, ICMP6_TYPE_EREQ);

    hdr->chksum = 0;
    hdr->id     = lwip_htons(session->session_id);
    hdr->seqno  = lwip_htons(seq_num);

    uint8_t *ping_payload = (uint8_t *)p->payload + sizeof(*hdr);

    generate_ping_payload(ping_payload, session);

    /* Note: checksum needs to be calculated AFTER payload is filled out. */
    ip_addr_t *dst = &session->args.ping_target;
    ip_addr_t *src = &session->args.ping_src;
    hdr->chksum = ip6_chksum_pseudo(p, IP6_NEXTH_ICMP6, p->tot_len,
                                    ip_2_ip6(src), ip_2_ip6(dst));
}
#endif

/**
 * Generate and send an ICMP echo request. This will update the session state as appropriate.
 *
 * @note Must be invoked with TCPIP core locked.
 */
static void ping_send_req(struct raw_pcb *ping_pcb, struct ping_session *session,
                           uint16_t seq_num)
{
    err_t err = ERR_ABRT;
    struct pbuf *p;
    size_t size = sizeof(struct icmp6_echo_hdr) + session->args.ping_size;

    p = pbuf_alloc(PBUF_IP, (uint16_t)size, PBUF_RAM);
    if (p == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Failed to allocate ping\n"));
        err = ERR_MEM;
        goto finish;
    }

    if ((p->len != p->tot_len) || (p->next != NULL))
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Ping pbuf not contiguous\n"));
        pbuf_free(p);
        err = ERR_MEM;
        goto finish;
    }
#if LWIP_IPV4
    if (IP_IS_V4(&(session->args.ping_target)))
    {
        ping_echo_pbuf(p, session, seq_num, size);
    }
#endif
#if LWIP_IPV6
    if (IP_IS_V6(&(session->args.ping_target)))
    {
        ping6_echo_pbuf(p, session, seq_num);
    }
#endif

    err = raw_sendto(ping_pcb, p, (const ip_addr_t *)&(session->args.ping_target));

finish:
    /* If the ping request was sent successfully then we stash the send time and update the total
     * count. */
    if (err == ERR_OK)
    {
        if (session->args.ping_interval_ms >= PING_DISPLAY_THRESHOLD_MS)
        {
            LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                        ("Ping req seq=%lu\n", session->stats.ping_total_count));
        }

        /* Ping request sent successfully, update timeout. */
        ping_session_update_timeout_on_send(session, true);
    }
    else
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL, ("Failed to send ping req %u (retry %u)\n",
                                         seq_num, session->num_retries));
        ping_session_update_timeout_on_send(session, false);
    }
    pbuf_free(p);
}

/**
 * Invoked when the timeout time is reached for a given session.
 *
 * @note Will be called with TCP/IP core locked.
 * @note This should update @c session->timeout_time_ms to the next timeout time.
 */
static void ping_session_timeout(struct ping_session *session, struct raw_pcb *ping_pcb)
{
    if (session->state == PING_AWAITING_SND)
    {
        session->num_retries = 0;
        uint16_t seq = session->stats.ping_total_count;
        /* Increment the count and update state first, since we might get the receive callback
         * before ping_send_req() returns. */
        session->stats.ping_total_count++;
        dup_check_bitmap_clear(session, session->stats.ping_total_count % N_DUP_CHECK_BITS);
        session->state = PING_AWAITING_RSP;
        session->next_seq_time_ms = mmosal_get_time_ms() + session->args.ping_interval_ms;
        ping_send_req(ping_pcb, session, seq);
    }
    else if (session->state == PING_AWAITING_RSP)
    {
        if (session->num_retries < MMPING_MAX_RETRIES)
        {
            session->num_retries++;
            ping_send_req(ping_pcb, session, session->stats.ping_total_count - 1);
        }
        else
        {
            /* Maximum number of retries received. Give up and move on if we have more to send.
             * Otherwise we are finished. */

            if (session->stats.ping_total_count < session->args.ping_count)
            {
                LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL, ("Maximum number of retries reached\n"));
                session->state = PING_AWAITING_SND;
                /* Set the timeout time to previously calculated next request time. Note that
                 * this may be in the past (and thus we will time out immediately) if the RTT
                 * was longer than the interval or if transmissions made us take longer than
                 * the interval. */
                session->timeout_time_ms = session->next_seq_time_ms;
            }
            else
            {
                LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                            ("Timeout waiting for final response, transitioning to IDLE state\n"));
                session->state = PING_IDLE;
            }
        }
    }
}

void ping_task(void *arg)
{
    struct raw_pcb *ping_pcb = NULL;
    struct ping_session *session = &ping_session;

    LWIP_UNUSED_ARG(arg);

    LOCK_TCPIP_CORE();
#if LWIP_IPV4
    if (IP_IS_V4(&(session->args.ping_target)))
    {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        LWIP_ASSERT("ping_pcb != NULL", ping_pcb != NULL);
        raw_recv(ping_pcb, ping_recv, NULL);
        raw_bind(ping_pcb, IP4_ADDR_ANY);
    }
#endif
#if LWIP_IPV6
    if (IP_IS_V6(&(session->args.ping_target)))
    {
        ping_pcb = raw_new(IP6_NEXTH_ICMP6);
        LWIP_ASSERT("ping_pcb != NULL", ping_pcb != NULL);
        raw_recv(ping_pcb, ping_recv, NULL);
        raw_bind(ping_pcb, IP6_ADDR_ANY);
    }
#endif

    while (session->state != PING_IDLE)
    {
        if (mmosal_time_le(session->timeout_time_ms, mmosal_get_time_ms()))
        {
            ping_session_timeout(session, ping_pcb);
        }

        uint32_t sleep_time = session->timeout_time_ms - mmosal_get_time_ms();
        UNLOCK_TCPIP_CORE();
        /* Sanity check that the current time hasn't passed the timeout time. */
        if ((int32_t)sleep_time > 0)
        {
            mmosal_task_wait_for_notification(sleep_time);
        }
        LOCK_TCPIP_CORE();
    }

    LWIP_DEBUGF(
        LWIP_DBG_LEVEL_ALL,
        ("Ping summary: %lu sent/%lu received (%lu%% loss) %lu/%lu/%lu min/avg/max RTT ms\n",
         session->stats.ping_total_count, session->stats.ping_recv_count,
         (session->stats.ping_total_count - session->stats.ping_recv_count) * 100 /
                        session->stats.ping_total_count,
         session->stats.ping_min_time_ms,
         session->rtt_sum_ms/session->stats.ping_recv_count,
         session->stats.ping_max_time_ms));

    raw_disconnect(ping_pcb);
    raw_remove(ping_pcb);
    UNLOCK_TCPIP_CORE();
    ping_task_handle = NULL;
}

void mmping_stats(struct mmping_stats* stats)
{
    LOCK_TCPIP_CORE();
    struct ping_session *session = &ping_session;
    *stats = session->stats;
    stats->ping_is_running = (session->state != PING_IDLE);
    stats->ping_avg_time_ms = session->rtt_sum_ms / session->stats.ping_recv_count;
    if (stats->ping_receiver[0] == '\0')
    {
        ipaddr_ntoa_r(&session->args.ping_target,
                      stats->ping_receiver, sizeof(stats->ping_receiver));
    }
    UNLOCK_TCPIP_CORE();
}

void mmping_stop(void)
{
    struct ping_session *session = &ping_session;
    session->state = PING_IDLE;
    if (ping_task_handle != NULL)
    {
        mmosal_task_notify(ping_task_handle);
    }
}

static struct ping_session *ping_get_empty_session(void)
{
    struct ping_session *session = &ping_session;

    if (session->state == PING_IDLE)
    {
        return session;
    }
    else
    {
        return NULL;
    }
}

uint16_t mmping_start(const struct mmping_args* args)
{
    if (args->ping_size < MMPING_MIN_DATA_SIZE || args->ping_size > MMPING_MAX_DATA_SIZE)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING, ("Invalid ping size %lu (valid range %d-%d)\n",
                    args->ping_size, MMPING_MIN_DATA_SIZE, MMPING_MAX_DATA_SIZE));
        return 0;
    }

    struct ping_session *session = ping_get_empty_session();
    if (session == NULL)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_WARNING,
                    ("Unable to start ping: session already in progress\n"));
        return 0;
    }

    /* We can safely cast away the volatile in this case, becasue the session is inactive so there
     * won't be multiple threads accessing the data structure. */
    memset((void *)session, 0, sizeof(*session));

    /* Start at a random session identifier */
    if (ping_next_session_id == 0)
    {
        ping_next_session_id = mmhal_random_u32(1, 0xffff);
    }

    session->session_id = ping_next_session_id;

    /* Increment next session identifier, but skip zero since we reserve it. */
    if (++ping_next_session_id == 0)
    {
        ping_next_session_id = 1;
    }

    LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                ("Starting ping session. Target=%s Interval=%lums Count=%lu Size=%lu\n",
                 args->ping_target, args->ping_interval_ms, args->ping_count, args->ping_size));
    if (session->args.ping_interval_ms < PING_DISPLAY_THRESHOLD_MS)
    {
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL,
                    ("Interval is less than %d ms threshold for showing individual pings\n",
                     PING_DISPLAY_THRESHOLD_MS));
    }

    int ok = ipaddr_aton(args->ping_src, &session->args.ping_src);
    if (!ok)
    {
        return 0;
    }
    ok = ipaddr_aton(args->ping_target, &session->args.ping_target);
    if (!ok)
    {
        return 0;
    }

    session->args.ping_interval_ms = args->ping_interval_ms;
    session->args.ping_count = args->ping_count;
    session->args.ping_size = args->ping_size;

    session->state = PING_AWAITING_SND;
    session->timeout_time_ms = mmosal_get_time_ms();
    session->last_rx_seq_num = -1;

    if (args->ping_count == 0 || args->ping_count > MMPING_MAX_COUNT)
    {
        session->args.ping_count = MMPING_MAX_COUNT;
    }

    /* There will be an unlikely race condition where the ping thread could be terminating
     * but still be non-NULL. Then the session will not run. But it is very unlikely bordering
     * impossible. */
    if (ping_task_handle == NULL)
    {
        ping_task_handle = mmosal_task_create(ping_task, NULL, MMOSAL_TASK_PRI_LOW,
                                              512, "ping_request");
        if (ping_task_handle == NULL)
        {
            session->stats.ping_is_running = false;
            return 0;
        }
    }
    else
    {
        /* Ping task is running, but probably asleep. Wake it up so we can send immediately. */
        mmosal_task_notify(ping_task_handle);
        LWIP_DEBUGF(LWIP_DBG_LEVEL_ALL, ("Ping task already running\n"));
    }

    return session->session_id;
}
