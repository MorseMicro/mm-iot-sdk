/*
 * Copyright 2025 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief AP Example Application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include "mmosal.h"
#include "mm_app_common.h"
#include "mm_app_loadconfig.h"
#include "mmlog.h"

/** A throwaway variable for checking that the opaque argument is correct. */
uint32_t opaque_argument_value;

/**
 * Function to convert @c mmwlan_ap_sta_state enumeration to a human readable string.
 *
 * @param state State enum to convert
 *
 * @return string representation of the enumeration.
 */
static char *mmwlan_ap_sta_state_to_str(enum mmwlan_ap_sta_state state)
{
    switch (state)
    {
        case MMWLAN_AP_STA_UNKNOWN:
            return "Unknown (disconnected)";

        case MMWLAN_AP_STA_AUTHENTICATED:
            return "Authenticated";

        case MMWLAN_AP_STA_ASSOCIATED:
            return "Associated";

        case MMWLAN_AP_STA_AUTHORIZED:
            return "Authorized";
    }

    return "Unrecognized";
}

/**
 * Handler for AP STA Status callback.
 *
 * @param sta_status    STA status information.
 * @param arg           Opaque argument that was provided when the callback was registered.
 */
static void handle_ap_sta_status(const struct mmwlan_ap_sta_status *sta_status, void *arg)
{
    /* Validate that the opaque argument received matches the value passed in. This is just for
     * testing purposes. */
    MMOSAL_ASSERT(arg == &opaque_argument_value);

    MMLOG_APP("AP STA " MM_MAC_ADDR_FMT " State: %s, AID: %u\n",
              MM_MAC_ADDR_VAL(sta_status->mac_addr),
              mmwlan_ap_sta_state_to_str(sta_status->state),
              sta_status->aid);
}

/**
 * Link status callback
 *
 * @param link_status   Current link status
 */
static void link_status_callback(const struct mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        MMLOG_PRINTF("Link is up. Time: %lu ms", time_ms);
        MMLOG_PRINTF(", IP: %s", link_status->ip_addr);
        MMLOG_PRINTF(", Netmask: %s", link_status->netmask);
        MMLOG_PRINTF(", Gateway: %s\n", link_status->gateway);
    }
    else
    {
        MMLOG_PRINTF("Link is down. Time: %lu ms\n", time_ms);
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    MMLOG_PRINTF("\n\nAP Mode Example (Built " __DATE__ " " __TIME__ ")\n\n");
    mmwlan_init();
    mmwlan_set_channel_list(load_channel_list());
    mmwlan_boot(NULL);

    app_print_version_info();

    /* Load IP stack settings from config store, or use defaults if no entry found in
     * config store. */
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    load_ap_mmipal_init_args(&mmipal_init_args);

    /* Initialize IP stack. */
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        MMLOG_APP("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_link_status_callback(link_status_callback);

    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    load_mmwlan_ap_settings();

    struct mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
    load_mmwlan_ap_args(&ap_args);

    ap_args.sta_status_cb = handle_ap_sta_status;
    ap_args.sta_status_cb_arg = &opaque_argument_value;

    MMLOG_APP("Starting AP mode...\n");
    enum mmwlan_status status = mmwlan_ap_enable(&ap_args);
    if (status == MMWLAN_SUCCESS)
    {
        MMLOG_APP("AP started successfully\n");
    }
    else
    {
        MMLOG_APP("Failed to start AP (status %d)\n", status);
    }
    // do led flashing for num stas connected?
}
