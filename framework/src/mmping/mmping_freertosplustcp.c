/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmping.h"
#include "mmhal.h"
#include "mmosal.h"
#include "mmipal.h"
#include "mmutils.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IPv4_Sockets.h"
#include "FreeRTOS_IPv6_Sockets.h"
#include "FreeRTOS_ND.h"

/** If ping interval is >= this threshold then per-packet log messages will be displayed. */
#define PING_DISPLAY_THRESHOLD_MS   (1000)

/** Time to wait on failing to send a ping request. */
#define PING_ERROR_RETRY_INTERVAL_MS    (1000)

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
        mmipal_ip_addr_t ping_src;
        /** The IP address of the ping target, in binary format. */
        mmipal_ip_addr_t ping_target;
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
    /** Sum of all RTTs for this session. */
    volatile uint32_t rtt_sum_ms;
    /** Ping request send time for calculating RTT for this session. */
    volatile uint32_t send_time_ms;
    /** Absolute time for the next timeout event for this session. */
    volatile uint32_t timeout_time_ms;
    /** The time at which to send the next ping request with a new sequence number. */
    volatile uint32_t next_seq_time_ms;
    /** ID of this session. This will be included in ping packets. */
    volatile uint16_t session_id;
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
 * Update session state on acknowledgement of the most recently sent ping request.
 */
static void process_ping_acknowledgement(struct ping_session *session)
{
    /* If we are in the AWAITNG_RSP state, then check if all outstanding ping requests have now
     * been acknowledged, and if so transition into the IDLE state; otherwise update timeout and
     * return to the AWAITING_SND state in preparation to send the next request. */
    if (session->state == PING_AWAITING_RSP)
    {
        if (session->last_rx_seq_num >= (int32_t)session->args.ping_count)
        {
            FreeRTOS_debug_printf(("Final ping response received, transitioning to IDLE state\n"));
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
 * Performs the bulk of the handling for a successfull ping response.
 */
static void process_ping_response(uint16_t ping_id)
{
    struct ping_session *session = &ping_session;
    uint32_t ping_rtt = mmosal_get_time_ms() - session->send_time_ms;

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
    session->last_rx_seq_num = ping_id;

    /* If the seq_num matches the most recently sent request then process this as acknowledgement
     * of that request. */
    if (ping_id == session->stats.ping_total_count)
    {
        process_ping_acknowledgement(session);
    }
}

void vApplicationPingReplyHook(ePingReplyStatus_t eStatus,
                               uint16_t usIdentifier)
{
    if (eStatus == eSuccess)
    {
        FreeRTOS_debug_printf(("Ping reply (%u)\n", usIdentifier));
        process_ping_response(usIdentifier);
    }
    else
    {
        FreeRTOS_debug_printf(("Ping reply (%u) received with eStatus = %d\n",
                              usIdentifier, eStatus));
    }
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
 * Generate and send an ICMP echo request. This will update the session state as appropriate.
 *
 * @note Must be invoked with TCPIP core locked.
 */
static void ping_send(struct ping_session *session)
{
    BaseType_t ret = pdFAIL;
    uint32_t target_ip_address = 0;
    IPv6_Address_t target_ipv6_address;
    bool is_ipv6 = false;

#if ipconfigUSE_IPv4
    ret = FreeRTOS_inet_pton4(session->args.ping_target, (void *)&target_ip_address);
#endif
#if ipconfigUSE_IPv6
    if (ret != pdPASS)
    {
        ret = FreeRTOS_inet_pton6(session->args.ping_target, target_ipv6_address.ucBytes);
        if (ret == pdPASS)
        {
            is_ipv6 = true;
        }
    }
#endif
    if (ret == pdFAIL)
    {
        FreeRTOS_debug_printf(("Incorrect ping target = %s\n", session->args.ping_target));
        return;
    }

#if ipconfigUSE_IPv6
    if (is_ipv6)
    {
        session->send_time_ms = mmosal_get_time_ms();
        /* First ping maybe missed while this does a Neighbor Solicitation. */
        ret = FreeRTOS_SendPingRequestIPv6(
            &target_ipv6_address,
            session->args.ping_size,
            session->args.ping_interval_ms * 1000/mmosal_ticks_per_second());
    }
    else
#else
    MM_UNUSED(is_ipv6);
    MM_UNUSED(target_ipv6_address);
#endif
    {
#if ipconfigUSE_IPv4
        if (session->stats.ping_total_count == 0)
        {
            /* Wait for IP address resolution before we send the first ping request. */
            xARPWaitResolution(target_ip_address, mmosal_ticks_per_second());
        }
#endif

        session->send_time_ms = mmosal_get_time_ms();
        ret = FreeRTOS_SendPingRequest(
            target_ip_address,
            session->args.ping_size,
            session->args.ping_interval_ms * 1000/mmosal_ticks_per_second());
    }
    if (ret == pdFAIL)
    {
        FreeRTOS_debug_printf(("Failed to send ping req\n"));
        ping_session_update_timeout_on_send(session, false);
        return;
    }
    else
    {
        session->stats.ping_total_count++;

        if (session->args.ping_interval_ms >= PING_DISPLAY_THRESHOLD_MS)
        {
            FreeRTOS_debug_printf(("Ping req seq=%lu\n", session->stats.ping_total_count));
        }

        /* Ping request sent successfully, update timeout. */
        ping_session_update_timeout_on_send(session, true);
    }
}

/**
 * Invoked when the timeout time is reached for a given session.
 *
 * @note Will be called with TCP/IP core locked.
 * @note This should update @c session->timeout_time_ms to the next timeout time.
 */
static void ping_session_timeout(struct ping_session *session)
{
    if (session->state == PING_AWAITING_SND)
    {
        /* Update state first, since we might get the receive callback before ping_send()
         * returns. */
        session->state = PING_AWAITING_RSP;
        session->next_seq_time_ms = mmosal_get_time_ms() + session->args.ping_interval_ms;
        ping_send(session);
    }
    else if (session->state == PING_AWAITING_RSP)
    {
        if (session->stats.ping_total_count < session->args.ping_count)
        {
            session->state = PING_AWAITING_SND;
            /* Set the timeout time to previously calculated next request time. Note that
             * this may be in the past (and thus we will time out immediately) if the RTT
             * was longer than the interval or if transmissions made us take longer than
             * the interval. */
            session->timeout_time_ms = session->next_seq_time_ms;
        }
        else
        {
            FreeRTOS_debug_printf(
                ("Timeout waiting for final response, transitioning to IDLE state\n"));
            session->state = PING_IDLE;
        }
    }
}

void ping_task(void *arg)
{
    MM_UNUSED(arg);
    struct ping_session *session = &ping_session;

    while (session->state != PING_IDLE)
    {
        if (mmosal_time_le(session->timeout_time_ms, mmosal_get_time_ms()))
        {
            ping_session_timeout(session);
        }

        uint32_t sleep_time = session->timeout_time_ms - mmosal_get_time_ms();
        /* Sanity check that the current time hasn't passed the timeout time. */
        if ((int32_t)sleep_time > 0)
        {
            mmosal_task_wait_for_notification(sleep_time);
        }
    }

    FreeRTOS_debug_printf((
        "Ping summary: %lu sent/%lu received (%lu%% loss) %lu/%lu/%lu min/avg/max RTT ms\n",
         session->stats.ping_total_count, session->stats.ping_recv_count,
         (session->stats.ping_total_count - session->stats.ping_recv_count) * 100 /
                        session->stats.ping_total_count,
         session->stats.ping_min_time_ms,
         session->rtt_sum_ms/session->stats.ping_recv_count,
         session->stats.ping_max_time_ms));

    ping_task_handle = NULL;
}

void mmping_stats(struct mmping_stats* stats)
{
    struct ping_session *session = &ping_session;
    *stats = session->stats;
    stats->ping_is_running = (session->state != PING_IDLE);
    stats->ping_avg_time_ms = session->rtt_sum_ms / session->stats.ping_recv_count;
    if (stats->ping_receiver[0] == '\0')
    {
        mmosal_safer_strcpy(stats->ping_receiver, session->args.ping_target, MMPING_IPADDR_MAXLEN);
    }
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
        FreeRTOS_debug_printf(("Invalid ping size %lu (valid range %d-%d)\n",
                               args->ping_size, MMPING_MIN_DATA_SIZE, MMPING_MAX_DATA_SIZE));
        return 0;
    }

    struct ping_session *session = ping_get_empty_session();
    if (session == NULL)
    {
        FreeRTOS_debug_printf(("Unable to start ping: session already in progress\n"));
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

    FreeRTOS_debug_printf(
        ("Starting ping session. Target=%s Interval=%lums Count=%lu Size=%lu\n",
         args->ping_target, args->ping_interval_ms, args->ping_count, args->ping_size));

    mmosal_safer_strcpy(session->args.ping_src, args->ping_src, MMIPAL_IPADDR_STR_MAXLEN);
    mmosal_safer_strcpy(session->args.ping_target, args->ping_target, MMIPAL_IPADDR_STR_MAXLEN);

    session->args.ping_interval_ms = args->ping_interval_ms;
    session->args.ping_count = args->ping_count;
    session->args.ping_size = args->ping_size;

    session->state = PING_AWAITING_SND;
    session->timeout_time_ms = mmosal_get_time_ms();
    session->last_rx_seq_num = -1;

    if (session->args.ping_interval_ms < PING_DISPLAY_THRESHOLD_MS)
    {
        FreeRTOS_debug_printf(
            ("Interval is less than %d ms threshold for showing individual pings\n",
             PING_DISPLAY_THRESHOLD_MS));
    }

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
        FreeRTOS_debug_printf(("Ping task already running\n"));
    }

    return session->session_id;
}
