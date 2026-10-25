/*
 * Copyright 2026 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief Relay Mode Example Application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include "mmwlan.h"
#include "mmosal.h"
#include "mm_app_common.h"
#include "mm_app_loadconfig.h"
#include "mmlog.h"
#include "relay_mode_led.h"

/** Struct for passing to status update callback. */
struct relay_update
{
    /** Status of the relay device. */
    struct mmwlan_relay_status status;
    /** Semaphore to signal when the state changes. */
    struct mmosal_semb *semb;
};

/**
 * Handler for relay status callback.
 *
 * @param status Current status of the relay node when the cb was triggered.
 * @param arg    Opaque argument that was provided when the callback was registered.
 */
static void handle_relay_status_update(const struct mmwlan_relay_status *status, void *arg)
{
    struct relay_update *update = (struct relay_update *)arg;
    update->status = *status;
    mmosal_semb_give(update->semb);
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    enum mmwlan_status status;

    MMLOG_PRINTF("\n\nRelay Mode Example (Built " __DATE__ " " __TIME__ ")\n\n");

    struct mmwlan_relay_status last_status = {
        .state = MMWLAN_RELAY_STATE_DISABLED,
        .sta_state = MMWLAN_STA_DISABLED,
        .depth = MMWLAN_RELAY_DEPTH_UNKNOWN,
    };
    struct relay_update node_state = {
        .status = last_status,
        .semb = mmosal_semb_create("node_state"),
    };
    MMOSAL_ASSERT(node_state.semb != NULL);

    relay_led_init();

    /* Initialise the Wi-Fi system and boot the chip. */
    app_wlan_init();

    status = mmwlan_register_relay_status_cb(handle_relay_status_update, &node_state);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    /* Bring up the relay subsystem. */
    relay_led_set_connecting();
    app_wlan_start_relay(true);
    relay_led_set_connected();

    MMLOG_APP("Relay started successfully\n");

    while (mmosal_semb_wait(node_state.semb, UINT32_MAX))
    {
        last_status = node_state.status;
        relay_led_set_depth(last_status.depth);

        uint8_t parent_bssid[MMWLAN_MAC_ADDR_LEN] = { 0 };
        char parent_str[30] = { 0 };
        char *state_str = "UNKNOWN";
        switch (last_status.sta_state)
        {
            case MMWLAN_STA_DISABLED:
                state_str = "disabled";
                break;

            case MMWLAN_STA_CONNECTING:
                state_str = "connecting";
                break;

            case MMWLAN_STA_CONNECTED:
                state_str = "connected";
                mmwlan_get_bssid(parent_bssid);
                snprintf(parent_str,
                         sizeof(parent_str),
                         " to " MM_MAC_ADDR_FMT,
                         MM_MAC_ADDR_VAL(parent_bssid));
                break;
        }

        char *role_str = "UNKNOWN";
        switch (last_status.state)
        {
            case MMWLAN_RELAY_STATE_DISABLED:
                role_str = "disabled";
                break;

            case MMWLAN_RELAY_STATE_STA:
                role_str = "station";
                break;

            case MMWLAN_RELAY_STATE_REPEATER:
                role_str = "repeater";
                break;

            case MMWLAN_RELAY_STATE_ROOT:
                role_str = "root";
                MMOSAL_ASSERT(0);
                break;
        }

        char depth_num[4];
        char *depth_str = "detached";
        if (last_status.depth != MMWLAN_RELAY_DEPTH_UNKNOWN)
        {
            snprintf(depth_num, sizeof(depth_num), "%u", last_status.depth);
            depth_str = depth_num;
        }
        MMLOG_APP("UPDATE - Relay STA: %s%s, Relay role: %s, Depth: %s\n",
                  state_str,
                  parent_str,
                  role_str,
                  depth_str);
    }

    mmosal_semb_delete(node_state.semb);
}
