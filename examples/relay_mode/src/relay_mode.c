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

/*
 * --
 * Default SSID/Security configuration
 * --
 */

#ifndef AP_SSID
/** SSID of the AP. (Do not quote; it will be stringified.) */
#define AP_SSID MorseMicroIoT
#endif

#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE 12345678
#endif

/* Default security type  */
#ifndef SECURITY_TYPE
/** Security type (@see mmwlan_security_type). */
#define SECURITY_TYPE MMWLAN_SAE
#endif

/* Default PMF mode */
#ifndef PMF_MODE
/** Protected Management Frames (PMF) mode (@see mmwlan_pmf_mode). */
#define PMF_MODE MMWLAN_PMF_REQUIRED
#endif

#ifndef MAX_STAS
/**
 * The maximum number of stations that can connect to the AP.
 * Must not be greater than @ref MMWLAN_AP_MAX_STAS_LIMIT.
 */
#define MAX_STAS MMWLAN_DEFAULT_AP_MAX_STAS
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

/** A throw away variable for checking that the opaque argument is correct. */
uint32_t opaque_argument_value;

/**
 * Function to convert @c mmwlan_ap_sta_state enumeration to a human readable string.
 *
 * @param state State enum to convert
 *
 * @return sting representation of the enumeration.
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
 * Loads the provided structure with initialization parameters
 * read from config store.  If a specific parameter is not found then
 * default values are used.  Use this function to load defaults before
 * calling @c mmwlan_ap_enable().
 *
 * @param ap_args A pointer to the @c mmwlan_ap_args to return
 *                    the settings in.
 */
void load_mmwlan_ap_args(struct mmwlan_ap_args *ap_args)
{
    char strval[32];

    /* Load SSID */
    (void)mmosal_safer_strcpy((char *)ap_args->ssid, STRINGIFY(AP_SSID), sizeof(ap_args->ssid));
    (void)mmconfig_read_string("wlan.ap_ssid", (char *)ap_args->ssid, sizeof(ap_args->ssid));
    ap_args->ssid_len = strlen((char *)ap_args->ssid);

    /* Load password */
    (void)mmosal_safer_strcpy(ap_args->passphrase,
                              STRINGIFY(SAE_PASSPHRASE),
                              sizeof(ap_args->passphrase));
    (void)mmconfig_read_string("wlan.ap_password",
                               ap_args->passphrase,
                               sizeof(ap_args->passphrase));
    ap_args->passphrase_len = strlen(ap_args->passphrase);

    /* Load security type */
    ap_args->security_type = SECURITY_TYPE;
    if (mmconfig_read_string("wlan.ap_security", strval, sizeof(strval)) > 0)
    {
        if (strncmp("sae", strval, sizeof(strval)) == 0)
        {
            ap_args->security_type = MMWLAN_SAE;
        }
        else if (strncmp("owe", strval, sizeof(strval)) == 0)
        {
            ap_args->security_type = MMWLAN_OWE;
        }
        else if (strncmp("open", strval, sizeof(strval)) == 0)
        {
            ap_args->security_type = MMWLAN_OPEN;
        }
        else
        {
            printf("Invalid value of %s read from config store: %s\n", "wlan.ap_security", strval);
        }
    }

    /* Load PMF mode */
    ap_args->pmf_mode = PMF_MODE;
    if (mmconfig_read_string("wlan.ap_pmf_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            printf("PMF disabled\n");
            ap_args->pmf_mode = MMWLAN_PMF_DISABLED;
        }
        else if (strncmp("required", strval, sizeof(strval)) == 0)
        {
            ap_args->pmf_mode = MMWLAN_PMF_REQUIRED;
        }
        else
        {
            printf("Invalid value of %s read from config store: %s\n", "wlan.ap_pmf_mode", strval);
        }
    }

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
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    MMLOG_PRINTF("\n\nRelay Mode Example (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    /* Query STA channel info to configure the AP interface. */
    struct mmwlan_vif_channel_info channel_info = {};
    enum mmwlan_status status = mmwlan_get_vif_channel_info(MMWLAN_VIF_STA, &channel_info);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_ERR("Failed to retrieve STA channel info\n", status);
        MMOSAL_ASSERT(0);
    }

    /* Bring up the relay subsystem. */
    struct mmwlan_relay_args args = MMWLAN_RELAY_ARGS_INIT;
    status = mmwlan_relay_enable(&args);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_ERR("Failed to start relay (status %d)\n", status);
        MMOSAL_ASSERT(0);
    }

    /* Configure the AP args. */
    struct mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
    load_mmwlan_ap_args(&ap_args);
    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);

    ap_args.op_class = channel_info.op_class;
    ap_args.s1g_chan_num = channel_info.s1g_chan_num;
    ap_args.pri_bw_mhz = channel_info.pri_bw_mhz;
    ap_args.pri_1mhz_chan_idx = channel_info.pri_1mhz_chan_idx;

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
    }
}
