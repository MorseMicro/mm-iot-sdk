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

#include <string.h>
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mm_app_common.h"
#include "mm_app_loadconfig.h"
#include "mmlog.h"
#include "relay_mode_led.h"

#ifndef MAX_STAS
/**
 * The maximum number of stations that can connect to the AP.
 * Must not be greater than @ref MMWLAN_AP_MAX_STAS_LIMIT.
 */
#define MAX_STAS MMWLAN_DEFAULT_AP_MAX_STAS
#endif

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

    printf("AP STA " MM_MAC_ADDR_FMT " State: %s, AID: %u, Time: %lu ms\n",
           MM_MAC_ADDR_VAL(sta_status->mac_addr),
           mmwlan_ap_sta_state_to_str(sta_status->state),
           sta_status->aid,
           mmosal_get_time_ms());
}

/**
 * Handler for relay depth change callback.
 *
 * @param new_depth The relay depth after the change.
 * @param arg       Opaque argument that was provided when the callback was registered.
 */
static void handle_relay_depth_change(uint8_t new_depth, void *arg)
{
    /* Validate that the opaque argument received matches the value passed in. This is just for
     * testing purposes. */
    MMOSAL_ASSERT(arg == &opaque_argument_value);

    if (new_depth == MMWLAN_RELAY_DEPTH_UNKNOWN)
    {
        MMLOG_APP("Relay depth unknown (no path to root)\n");
    }
    else
    {
        MMLOG_APP("Relay depth changed to %u\n", new_depth);
    }

    relay_led_set_depth(new_depth);
}

/**
 * Loads the provided structure, with the subset of AP initialization parameters that are
 * applicable to a relay, from config store.
 *
 * @param ap_args A pointer to the @c mmwlan_ap_args to return
 *                    the settings in.
 */
static void load_relay_mmwlan_ap_args(struct mmwlan_ap_args *ap_args)
{
    char strval[32];
    uint32_t uint32val;

    /* Load BSSID */
    if (mmconfig_read_string("wlan.ap_bssid", strval, sizeof(strval)) > 0)
    {
        int temp[6];
        int i;

        int ret = sscanf(strval,
                         "%x:%x:%x:%x:%x:%x",
                         &temp[0],
                         &temp[1],
                         &temp[2],
                         &temp[3],
                         &temp[4],
                         &temp[5]);
        if (ret == 6)
        {
            for (i = 0; i < 6; i++)
            {
                if (temp[i] > UINT8_MAX || temp[i] < 0)
                {
                    /* Invalid value, ignore and reset to default */
                    memset(ap_args->bssid, 0, sizeof(ap_args->bssid));
                    break;
                }

                ap_args->bssid[i] = (uint8_t)temp[i];
            }
        }
    }

    /* Load DTIM period */
    if (mmconfig_read_uint32("wlan.ap_dtim_period", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            ap_args->dtim_period = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.ap_dtim_period");
        }
    }

    /* Load Beacon Interval */
    if (mmconfig_read_uint32("wlan.ap_beacon_interval_tus", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            ap_args->beacon_interval_tus = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.ap_beacon_interval_tus");
        }
    }
}

/**
 * Apply any relay mode specific settings from the config store.
 */
static void load_mmwlan_relay_settings(void)
{
    /* Optionally override the relay depth. Only valid while STA and AP are both disabled, so it
     * must be applied before connecting to the network. */
    uint32_t relay_depth_override;
    if (mmconfig_read_uint32("wlan.relay_depth_override", &relay_depth_override) == MMCONFIG_OK)
    {
        if (relay_depth_override <= MMWLAN_RELAY_MAX_DEPTH)
        {
            enum mmwlan_status status =
                mmwlan_relay_set_depth_override((uint8_t)relay_depth_override);
            if (status != MMWLAN_SUCCESS)
            {
                MMLOG_ERR("Failed to set relay depth override (status %d)\n", status);
            }
        }
        else
        {
            printf("%s out of range\n", "wlan.relay_depth_override");
        }
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    enum mmwlan_status status;

    MMLOG_PRINTF("\n\nRelay Mode Example (Built " __DATE__ " " __TIME__ ")\n\n");

    relay_led_init();

    /* Initialise the Wi-Fi system and boot the chip. */
    app_wlan_init();

    load_mmwlan_relay_settings();

    /* Bring up the relay subsystem. */
    struct mmwlan_relay_args relay_args = MMWLAN_RELAY_ARGS_INIT;
    status = mmwlan_relay_enable(&relay_args);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_ERR("Failed to start relay (status %d)\n", status);
        relay_led_set_failed();
        MMOSAL_ASSERT(0);
    }

    status = mmwlan_register_depth_change_cb(handle_relay_depth_change, &opaque_argument_value);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_ERR("Failed to register depth change callback (status %d)\n", status);
    }

    /* Connect to Wi-Fi network, block till connected. */
    relay_led_set_connecting();
    app_wlan_start();
    if (mmwlan_get_sta_state() == MMWLAN_STA_CONNECTED)
    {
        relay_led_set_connected();
    }
    else
    {
        relay_led_set_failed();
    }
    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);

    /* Load the sta args shared by the whole relay tree. */
    struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
    load_mmwlan_sta_args(&sta_args);

    /* Bring up the AP interface. */
    struct mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
    load_relay_mmwlan_ap_args(&ap_args);

    /* The AP re-advertises the STA credentials so the whole tree shares one SSID. */
    memcpy(ap_args.ssid, sta_args.ssid, sizeof(ap_args.ssid));
    ap_args.ssid_len = sta_args.ssid_len;
    ap_args.security_type = sta_args.security_type;
    memcpy(ap_args.passphrase, sta_args.passphrase, sizeof(ap_args.passphrase));
    ap_args.passphrase_len = sta_args.passphrase_len;
    ap_args.pmf_mode = sta_args.pmf_mode;

    ap_args.sta_status_cb = handle_ap_sta_status;
    ap_args.sta_status_cb_arg = &opaque_argument_value;

    ap_args.max_stas = MAX_STAS;

    status = mmwlan_ap_enable(&ap_args);
    if (status == MMWLAN_SUCCESS)
    {
        MMLOG_PRINTF("AP started with SSID: %s ", ap_args.ssid);
        if (ap_args.security_type == MMWLAN_SAE)
        {
            MMLOG_PRINTF("with passphrase %s", ap_args.passphrase);
        }
        MMLOG_PRINTF("\n");
    }
    else
    {
        MMLOG_PRINTF("Failed to start AP (status %d)\n", status);
        relay_led_set_failed();
    }
}
