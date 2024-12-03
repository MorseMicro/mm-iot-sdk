/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Simple ping demonstration.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to ping a target using the Morse Micro WLAN API.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 */

#include <string.h>
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmping.h"

#include "mmipal.h"

#include "mm_app_common.h"

/* Ping configurations. */
#ifndef PING_REMOTE_IP
/** IP address of the remote host to ping. */
#define PING_REMOTE_IP                  "192.168.1.1"
#endif
#ifndef PING_COUNT
/** Number of ping requests to send. Set to 0 to continue indefinitely. */
#define PING_COUNT                      10
#endif
#ifndef PING_DATA_SIZE
/** Size of the ping request data, excluding 8-byte ICMP header. */
#define PING_DATA_SIZE                  56
#endif
#ifndef PING_INTERVAL_MS
/** Interval between successive ping requests. */
#define PING_INTERVAL_MS                1000
#endif
#ifndef POST_PING_DELAY_MS
/** Delay in ms to wait before terminating connection on completion of ping. */
#define POST_PING_DELAY_MS                10000
#endif
#ifndef UPDATE_INTERVAL_MS
/** Interval (in milliseconds) at which to provide updates when the receive count has not
 *  changed. */
#define UPDATE_INTERVAL_MS  (5000)
#endif

/**
 * Enumeration of debug states that will be reflected on debug pins. Note that due to limited
 * availability of pins, the values are mapped to 2-bit codes and so are not unique. The code
 * sequence has been chosen to be gray code like in that only one bit changes at a time, but
 * it does not return to zero so a zero code can be used to identify the first state.
 */
enum debug_state
{
    /** Initial state at startup. */
    DEBUG_STATE_INIT           = 0x00,
    /** Indicates that we are booting the MM chip (note that this will also include the
     *  host MCU startup time. */
    DEBUG_STATE_BOOTING_CHIP   = 0x01,
    /** Indicates we are connecting to the AP. */
    DEBUG_STATE_CONNECTING     = 0x03,
    /** Indicates we are connected to the AP. */
    DEBUG_STATE_CONNECTED      = 0x02,
    /** Indicates that we have connected to the AP, but have not started the ping yet. */
    DEBUG_STATE_CONNECTED_IDLE = 0x00,
    /** Indicates that the ping is in progress. */
    DEBUG_STATE_PINGING        = 0x02,
    /** Indicates that the ping has completed. */
    DEBUG_STATE_PING_DONE      = 0x03,
    /** Indicates that we are idling with WLAN still on. */
    DEBUG_STATE_IDLE           = 0x01,
    /** Indicates that we are disconnecting from the AP. */
    DEBUG_STATE_TERMINATING    = 0x00,
};

/**
 * Perform necessary operation (i.e., setting GPIO pins) upon entering the given debug state.
 *
 * @param state     The debug state to enter. See @ref debug_state.
 */
static void set_debug_state(enum debug_state state)
{
    mmhal_set_debug_pins(MMHAL_ALL_DEBUG_PINS, state);
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    /** Executes the ping. */
    struct mmping_args args = MMPING_ARGS_DEFAULT;

    set_debug_state(DEBUG_STATE_BOOTING_CHIP);

    printf("\n\nMorse Ping Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();

    set_debug_state(DEBUG_STATE_CONNECTING);

    app_wlan_start();

    set_debug_state(DEBUG_STATE_CONNECTED);

    /* Delay to allow communications to settle so we measure only idle current */
    mmosal_task_sleep(150);

    set_debug_state(DEBUG_STATE_CONNECTED_IDLE);

    mmosal_task_sleep(1000);

    set_debug_state(DEBUG_STATE_PINGING);

    /* Get the target IP */
    strncpy(args.ping_target, PING_REMOTE_IP, sizeof(args.ping_target));
    mmconfig_read_string("ping.target", args.ping_target, sizeof(args.ping_target));

    enum mmipal_status status = mmipal_get_local_addr(args.ping_src, args.ping_target);
    if (status != MMIPAL_SUCCESS)
    {
        printf("Failed to get local address for PING\n");
    }
    args.ping_count = PING_COUNT;
    mmconfig_read_uint32("ping.count", &args.ping_count);

    args.ping_size = PING_DATA_SIZE;
    mmconfig_read_uint32("ping.size", &args.ping_size);

    args.ping_interval_ms = PING_INTERVAL_MS;
    mmconfig_read_uint32("ping.interval", &args.ping_interval_ms);

    mmping_start(&args);
    printf("\nPing %s %lu(%lu) bytes of data.\n", args.ping_target, args.ping_size,
            (MMPING_ICMP_ECHO_HDR_LEN + args.ping_size));
    mmosal_task_sleep(args.ping_interval_ms);

    struct mmping_stats stats;
    uint32_t next_update_time_ms = mmosal_get_time_ms() + UPDATE_INTERVAL_MS;
    unsigned last_ping_recv_count = 0;
    mmping_stats(&stats);
    while (stats.ping_is_running)
    {
        mmosal_task_sleep(10);
        mmping_stats(&stats);
        if (stats.ping_recv_count != last_ping_recv_count ||
            mmosal_time_has_passed(next_update_time_ms))
        {
            printf("(%s) packets transmitted/received = %lu/%lu, "
                   "round-trip min/avg/max = %lu/%lu/%lu ms\n",
                   stats.ping_receiver, stats.ping_total_count, stats.ping_recv_count,
                   stats.ping_min_time_ms, stats.ping_avg_time_ms, stats.ping_max_time_ms);
            last_ping_recv_count = stats.ping_recv_count;
            next_update_time_ms = mmosal_get_time_ms() + UPDATE_INTERVAL_MS;
        }
    }

    set_debug_state(DEBUG_STATE_PING_DONE);

    uint32_t loss = 0;
    if (stats.ping_total_count == 0)
    {
        loss = 0;
    }
    else
    {
        loss = (1000 * (stats.ping_total_count - stats.ping_recv_count) * 100 /
                stats.ping_total_count);
    }
    printf("\n--- %s ping statistics ---\n%lu packets transmitted, %lu packets received, ",
            stats.ping_receiver, stats.ping_total_count, stats.ping_recv_count);
    printf("%lu.%03lu%% packet loss\nround-trip min/avg/max = %lu/%lu/%lu ms\n",
            loss/1000, loss%1000, stats.ping_min_time_ms, stats.ping_avg_time_ms,
            stats.ping_max_time_ms);

    /* Delay to allow communications to settle so we measure only idle current */
    mmosal_task_sleep(150);

    set_debug_state(DEBUG_STATE_IDLE);

    uint32_t post_ping_delay_ms = POST_PING_DELAY_MS;
    mmconfig_read_uint32("ping.post_ping_delay_ms", &post_ping_delay_ms);
    mmosal_task_sleep(post_ping_delay_ms);

    set_debug_state(DEBUG_STATE_TERMINATING);

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}
