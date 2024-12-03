/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Demonstration of TWT setup
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to configure TWT using the Morse Micro WLAN API. The device
 * then connects to an AP using the given TWT configuration. Once connected, it is possible
 * to observe the effect of TWT by pinging the device from the AP. The device will only
 * respond to the ping during its service period. To get a better understanding of the effect
 * of TWT, it is recommended to capture and review a sniffer trace to see the over-the-air
 * behavior.
 *
 * For more details about the theory and operation of TWT see the Morse Micro
 * Application Note `APPNOTE-03`.
 */

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"


#include "mm_app_common.h"


/**
 * Add a TWT configuration requesting a periodic service period with the given arguments.
 *
 * @note Please see _APPNOTE-03 Target Wake Time (TWT)_ for more detail regarding the
 *       TWT configuration arguments.
 *
 * @param wake_interval_us      The wake interval in microseconds.
 * @param min_wake_duration_us  The minimum wake duration in microseconds.
 *
 * @return the status code returned by @ref mmwlan_twt_add_configuration().
 */
static enum mmwlan_status add_twt_configuration(uint32_t wake_interval_us,
                                                uint32_t min_wake_duration_us)
{
    enum mmwlan_status status;
    struct mmwlan_twt_config_args twt_config = MMWLAN_TWT_CONFIG_ARGS_INIT;

    twt_config.twt_mode = MMWLAN_TWT_REQUESTER;
    twt_config.twt_setup_command = MMWLAN_TWT_SETUP_REQUEST;
    twt_config.twt_wake_interval_us = wake_interval_us;
    twt_config.twt_min_wake_duration_us = min_wake_duration_us;

    status = mmwlan_twt_add_configuration(&twt_config);
    if (status == MMWLAN_SUCCESS)
    {
        printf("Successfully added TWT configuration\n");
    }
    else
    {
        printf("Failed to set TWT configuration\n");
    }
    return status;
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    uint32_t twt_wake_interval_us;
    uint32_t twt_min_wake_duration_us;

    printf("\n\nMorse TWT Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi network. Blocks till connected. */
    app_wlan_init();

    /* Default to an interval of 5 minutes (approx) with each service period lasting 65 ms. */
    twt_wake_interval_us = 300000000;
    twt_min_wake_duration_us = 65280;
    mmconfig_read_uint32("twt.wake_interval_us", &twt_wake_interval_us);
    mmconfig_read_uint32("twt.min_wake_duration_us", &twt_min_wake_duration_us);
    add_twt_configuration(twt_wake_interval_us, twt_min_wake_duration_us);

    app_wlan_start();
}
