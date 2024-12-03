/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example utilizing WNM sleep to conserve power in between periodic transmissions.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates an example of how WNM sleep can be utilized using the Morse Micro WLAN
 * API. See @ref MMWLAN_WNM for more details.
 *
 * The application is based of the ping example application. The difference here is that we will
 * enter WNM sleep after transmitting. We then sleep for a set amount of time before repeating. This
 * can be used to conserve power if you have some data that only needs to be transferred
 * periodically.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 */

#include <string.h>
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmping.h"
#include "mmconfig.h"

#include "mmipal.h"

#include "mm_app_common.h"

/* Default Application configurations. These are used if the required parameters cannot be found in
 * the configstore*/
#ifndef DEFAULT_PING_REMOTE_IP
/** IP address of the remote host to ping. */
#define DEFAULT_PING_REMOTE_IP                  "192.168.1.1"
#endif
#ifndef DEFAULT_PING_COUNT
/** Number of ping requests to send. Set to 0 to continue indefinitely. */
#define DEFAULT_PING_COUNT                      10
#endif
#ifndef DEFAULT_PING_DATA_SIZE
/** Size of the ping request data, excluding 8-byte ICMP header. */
#define DEFAULT_PING_DATA_SIZE                  56
#endif
#ifndef DEFAULT_PING_INTERVAL_MS
/** Interval between successive ping requests. */
#define DEFAULT_PING_INTERVAL_MS                1000
#endif
#ifndef DEFAULT_WNM_SLEEP_DURATION_MS
/** Duration to remain in wnm sleep between transmissions. */
#define DEFAULT_WNM_SLEEP_DURATION_MS           20000
#endif
#ifndef POST_PING_DELAY_MS
/** Delay in ms to wait before terminating connection on completion of ping. */
#define POST_PING_DELAY_MS                10000
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
    DEBUG_STATE_INIT                         = 0x00,
    /** Indicates we are connecting to the AP. */
    DEBUG_STATE_CONNECTING                   = 0x01,
    /** Indicates we are connected to the AP. */
    DEBUG_STATE_CONNECTED                    = 0x03,
    /** Indicates that the ping is in progress. */
    DEBUG_STATE_PINGING_0                    = 0x02,
    /** Indicates that the ping has completed. */
    DEBUG_STATE_PING_0_DONE                  = 0x00,
    /** Indicates that WNM sleep is in progress. */
    DEBUG_STATE_WNM_SLEEP                    = 0x01,
    /** Indicates that we are exiting WNM sleep. */
    DEBUG_STATE_EXITING_WNM_SLEEP            = 0x03,
    /** Indicates that WNM sleep has completed. */
    DEBUG_STATE_WNM_SLEEP_DONE               = 0x02,
    /** Indicates that the ping is in progress. */
    DEBUG_STATE_PINGING_1                    = 0x00,
    /** Indicates that the ping has completed. */
    DEBUG_STATE_PING_1_DONE                  = 0x01,
    /** Indicates that WNM sleep is in progress with chip powered down. */
    DEBUG_STATE_WNM_SLEEP_POWER_DOWN         = 0x03,
    /** Indicates that we are exiting WNM sleep with chip powered down. */
    DEBUG_STATE_EXITING_WNM_SLEEP_POWER_DOWN = 0x02,
    /** Indicates that WNM sleep with chip powered down has completed. */
    DEBUG_STATE_WNM_SLEEP_POWER_DOWN_DONE    = 0x00,
    /** Indicates that we are disconnecting from the AP. */
    DEBUG_STATE_TERMINATING                  = 0x01,
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
 * Function to execute ping request. This function will block until the ping operation has
 * completed.
 *
 * @param iteration Specifies which iteration (0 or 1) of ping this is so that we can select the
 *                  correct debug state.
 */
static void execute_ping_request(int iteration)
{
    struct mmping_args args = MMPING_ARGS_DEFAULT;

    set_debug_state(iteration == 0 ? DEBUG_STATE_PINGING_0 : DEBUG_STATE_PINGING_1);

    /* Get the target IP */
    strncpy(args.ping_target, DEFAULT_PING_REMOTE_IP, sizeof(args.ping_target));
    mmconfig_read_string("ping.target", args.ping_target, sizeof(args.ping_target));

    enum mmipal_status status = mmipal_get_local_addr(args.ping_src, args.ping_target);
    if (status != MMIPAL_SUCCESS)
    {
        printf("failed to get local address for PING\n");
    }
    args.ping_count = DEFAULT_PING_COUNT;
    mmconfig_read_uint32("ping.count", &args.ping_count);

    args.ping_size = DEFAULT_PING_DATA_SIZE;
    mmconfig_read_uint32("ping.target", &args.ping_size);

    args.ping_interval_ms = DEFAULT_PING_INTERVAL_MS;
    mmconfig_read_uint32("ping.interval", &args.ping_interval_ms);

    mmping_start(&args);
    printf("\nPing %s %lu(%lu) bytes of data.\n", args.ping_target, args.ping_size,
           MMPING_ICMP_ECHO_HDR_LEN + args.ping_size);
    mmosal_task_sleep(args.ping_interval_ms);

    struct mmping_stats stats;
    mmping_stats(&stats);
    while (stats.ping_is_running)
    {
        mmosal_task_sleep(args.ping_interval_ms);
        mmping_stats(&stats);
        printf("(%s) packets transmitted/received = %lu/%lu, "
               "round-trip min/avg/max = %lu/%lu/%lu ms\n",
               stats.ping_receiver, stats.ping_total_count, stats.ping_recv_count,
               stats.ping_min_time_ms, stats.ping_avg_time_ms, stats.ping_max_time_ms);
    }
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

    set_debug_state(iteration == 0 ? DEBUG_STATE_PING_0_DONE : DEBUG_STATE_PING_1_DONE);

    printf("\n--- %s ping statistics ---\n%lu packets transmitted, %lu packets received, ",
            stats.ping_receiver, stats.ping_total_count, stats.ping_recv_count);
    printf("%lu.%03lu%% packet loss\nround-trip min/avg/max = %lu/%lu/%lu ms\n",
            loss/1000, loss%1000, stats.ping_min_time_ms, stats.ping_avg_time_ms,
            stats.ping_max_time_ms);
}

/**
 * Function to execute WNM sleep.
 *
 * @param wnm_sleep_duration_ms     Duration in milliseconds in WNM sleep.
 */
static void execute_wnm_sleep(uint32_t wnm_sleep_duration_ms)
{
    enum mmwlan_status status = MMWLAN_ERROR;

    printf("\nEntering WNM sleep with chip power down disabled\n");
    printf("Expected sleep time %lums.\n", wnm_sleep_duration_ms);
    /* Delay to allow printf's to settle so we measure only idle current */
    mmosal_task_sleep(100);

    set_debug_state(DEBUG_STATE_WNM_SLEEP);

    uint32_t timestamp = mmosal_get_time_ms();
    status = mmwlan_set_wnm_sleep_enabled(true);
    uint32_t wnm_sleep_enable_duration_ms = mmosal_get_time_ms() - timestamp;
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to enable WNM sleep\n");
        return;
    }

    mmosal_task_sleep(wnm_sleep_duration_ms);

    timestamp = mmosal_get_time_ms();

    set_debug_state(DEBUG_STATE_EXITING_WNM_SLEEP);
    status = mmwlan_set_wnm_sleep_enabled(false);
    set_debug_state(DEBUG_STATE_WNM_SLEEP_DONE);

    if (status == MMWLAN_SUCCESS)
    {
        printf("\nEnter WNM sleep took %lu ms.\n", wnm_sleep_enable_duration_ms);
        printf("\nExit WNM sleep took %lu ms.\n", (mmosal_get_time_ms() - timestamp));
    }
    else
    {
        printf("Failed to disable WNM sleep\n");
    }
}

/**
 * Function to enter WNM Sleep and power off the MM chip while asleep.
 * This will achieve lower power consumption during the sleep at the expense of a longer wake-up
 * time.
 *
 * @param wnm_sleep_duration_ms     Duration in milliseconds in WNM sleep.
 */
static void execute_wnm_sleep_ext(uint32_t wnm_sleep_duration_ms)
{
    enum mmwlan_status status = MMWLAN_ERROR;
    struct mmwlan_set_wnm_sleep_enabled_args wnm_sleep_args =
        MMWLAN_SET_WNM_SLEEP_ENABLED_ARGS_INIT;

    printf("\nEntering WNM sleep with chip power down enabled.\n");
    printf("Expected sleep time %lums.\n", wnm_sleep_duration_ms);
    /* Delay to allow printf's to settle so we measure only idle current */
    mmosal_task_sleep(100);

    set_debug_state(DEBUG_STATE_WNM_SLEEP_POWER_DOWN);

    uint32_t timestamp = mmosal_get_time_ms();
    wnm_sleep_args.wnm_sleep_enabled = true;
    wnm_sleep_args.chip_powerdown_enabled = true;
    status = mmwlan_set_wnm_sleep_enabled_ext(&wnm_sleep_args);
    uint32_t wnm_sleep_enable_duration_ms = mmosal_get_time_ms() - timestamp;
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to enable WNM sleep\n");
        return;
    }

    mmosal_task_sleep(wnm_sleep_duration_ms);

    timestamp = mmosal_get_time_ms();
    wnm_sleep_args.wnm_sleep_enabled = false;

    set_debug_state(DEBUG_STATE_EXITING_WNM_SLEEP_POWER_DOWN);
    status = mmwlan_set_wnm_sleep_enabled_ext(&wnm_sleep_args);
    set_debug_state(DEBUG_STATE_WNM_SLEEP_POWER_DOWN_DONE);

    if (status == MMWLAN_SUCCESS)
    {
        printf("\nEnter WNM sleep took %lu ms.\n", wnm_sleep_enable_duration_ms);
        printf("\nExit WNM sleep took %lu ms.\n", (mmosal_get_time_ms() - timestamp));
    }
    else
    {
        printf("Failed to disable WNM sleep\n");
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nMorse WNM Sleep Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();

    set_debug_state(DEBUG_STATE_CONNECTING);

    app_wlan_start();

    set_debug_state(DEBUG_STATE_CONNECTED);

    /* Delay to allow communications to settle so we measure only idle current */
    mmosal_task_sleep(150);

    uint32_t wnm_sleep_duration_ms = DEFAULT_WNM_SLEEP_DURATION_MS;
    mmconfig_read_uint32("wlan.wnm_sleep_duration_ms", &wnm_sleep_duration_ms);

    execute_ping_request(0);

    /* The following demonstrates basic WNM Sleep functionality. The MM chip remains powered,
        * but does not wake up to receive beacons. */
    execute_wnm_sleep(wnm_sleep_duration_ms);

    execute_ping_request(1);

    /* The following demonstrates extended WNM Sleep API that allows the chip to be shutdown
        * while in WNM Sleep. */
    execute_wnm_sleep_ext(wnm_sleep_duration_ms);

    /* Delay to allow communications to settle so we measure only idle current */
    mmosal_task_sleep(150);

    set_debug_state(DEBUG_STATE_TERMINATING);

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}
