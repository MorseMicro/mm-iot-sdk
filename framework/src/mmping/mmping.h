/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 /**
  * @ingroup MMAPP
  * @defgroup MMPING Morse Micro Ping
  *
  * This is the ICMP echo (ping) request implementation.
  *
  * This implementation is designed to be similar to Unix ping, but differs from it in
  * some respects.
  *
  * The behavior of this ping implementation is as follows:
  *
  * * A ping request will be sent. The time for the next ping request is calculated as the
  *   current time plus the given ping interval.
  * * If the ping response is not received within the ping retry timeout (see below for details)
  *   then it the ping request will be retransmitted. This will be repeated up to
  *   @c MMPING_MAX_RETRIES times.
  * * If @c MMPING_MAX_RETRIES is exceeded or a valid ping reply is received then ping will
  *   wait until the next ping request time calculated above before proceeding to send the
  *   next ping request. If this time has already elapsed then the next ping request will
  *   be sent immediately.
  *
  * The ping retry timeout is calculated as the 2 times average RTT if one or more ping responses
  * have been received, otherwise it will be @c MMPING_INITIAL_RETRY_INTERVAL_MS.
  *
  * @{
  */


#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Default time interval in milliseconds between ping requests. */
#define MMPING_DEFAULT_PING_INTERVAL_MS                (1000)
/** Default max count for ping request to run forever. */
#define MMPING_DEFAULT_PING_COUNT                      (0)
/** Default ping data packet size in bytes, excluding ICMP header. */
#define MMPING_DEFAULT_DATA_SIZE                       (56)
/** Minimum data size of ping request packet excluding ICMP header */
#define MMPING_MIN_DATA_SIZE                           (8)
/** Maximum data size of ping request packet excluding ICMP header */
#define MMPING_MAX_DATA_SIZE                           (1500)
/** Maximum number of pings before terminating. */
#define MMPING_MAX_COUNT                               (0xffff)
/** Maximum number of times to retransmit a ping request before giving up and moving on. */
#define MMPING_MAX_RETRIES                             (2)
/** Rate at which to retry unacknowledged ping requests when the RTT is not known.`` */
#define MMPING_INITIAL_RETRY_INTERVAL_MS               (1000)

/** Maximum length of an IP address string including null-terminator. */
#define MMPING_IPADDR_MAXLEN                           (48)

/** Length of the ICMP echo header in octets. */
#define MMPING_ICMP_ECHO_HDR_LEN                       (8)

/**
 * Ping request arguments data structure.
 *
 * For forward compatibility this structure should be initialized using
 * @c MMPING_ARGS_DEFAULT. For example:
 *
 * @code{.c}
 * struct mmping_args args = MMPING_ARGS_DEFAULT;
 * @endcode
 */
struct mmping_args
{
    /** String representation of the local IP address */
    char ping_src[MMPING_IPADDR_MAXLEN];
    /** String representation of the IP address of the ping target */
    char ping_target[MMPING_IPADDR_MAXLEN];
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
};

/** Initializer for @ref mmping_args. */
#define MMPING_ARGS_DEFAULT                                                           \
    {                                                                                 \
        { 0 }, { 0 }, MMPING_DEFAULT_PING_INTERVAL_MS,                                \
        MMPING_DEFAULT_PING_COUNT, MMPING_DEFAULT_DATA_SIZE,                          \
    }

/**
 * Data structure to store ping results.
 */
struct mmping_stats
{
    /** String representation of the IP address of the ping receiver */
    char ping_receiver[MMPING_IPADDR_MAXLEN];
    /** Total number of requests sent */
    uint32_t ping_total_count;
    /** The number of ping responses received */
    uint32_t ping_recv_count;
    /** The minimum latency in ms between request sent and response received */
    uint32_t ping_min_time_ms;
    /** The average latency in ms between request sent and response received */
    uint32_t ping_avg_time_ms;
    /** The maximum latency in ms between request sent and response received */
    uint32_t ping_max_time_ms;
    /** Stores the ping running status */
    bool ping_is_running;
};

/**
 * Initialize ping parameters and start ping.
 *
 * @param args Ping request arguments
 *
 * @returns the ID for this ping on success, on failure returns 0.
 */
uint16_t mmping_start(const struct mmping_args* args);

/**
 * Stop any running ping request.
 */
void mmping_stop(void);

/**
 * Get Ping Statistics.
 *
 * @param stats Instance to receive ping statistics
 *
 * @note Calling @c mmping_start() will reset the ping statistics
 */
void mmping_stats(struct mmping_stats* stats);

#ifdef __cplusplus
}
#endif

/** @} */
