/*
 * Copyright 2022-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example application to demonstrate using the MMWLAN scan subsystem.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include <string.h>
#include "mmosal.h"
#include "mmutils.h"
#include "mmwlan.h"
#include "mm_app_loadconfig.h"

/*
 * If ASNI_ESCAPE_ENABLED is non-zero (the default) then ANSI escape characters will be used to
 *  format the log output.
 */
#if !(defined(ASNI_ESCAPE_ENABLED) && ASNI_ESCAPE_ENABLED == 0)
/** ANSI escape sequence for bold text. */
#define ANSI_BOLD  "\x1b[1m"
/** ANSI escape sequence to reset font. */
#define ANSI_RESET "\x1b[0m"
#else
/** ANSI escape sequence for bold text (disabled so no-op). */
#define ANSI_BOLD  ""
/** ANSI escape sequence to reset font (disabled so no-op). */
#define ANSI_RESET ""
#endif

/** Length of string representation of a MAC address (i.e., "XX:XX:XX:XX:XX:XX")
 * including null terminator. */
#define MAC_ADDR_STR_LEN    (18)

/** Number of results found. */
static int num_scan_results;

/**
 * Scan rx callback.
 *
 * @param result        Pointer to the scan result.
 * @param arg           Opaque argument.
 */
static void scan_rx_callback(const struct mmwlan_scan_result *result, void *arg)
{
    (void)(arg);
    char bssid_str[MAC_ADDR_STR_LEN];
    char ssid_str[MMWLAN_SSID_MAXLEN];
    int ret;
    struct mm_rsn_information rsn_info;

    num_scan_results++;
    snprintf(bssid_str, MAC_ADDR_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3],
             result->bssid[4], result->bssid[5]);
    snprintf(ssid_str, (result->ssid_len+1), "%s", result->ssid);

    printf(ANSI_BOLD "%2d. %s" ANSI_RESET "\n", num_scan_results, ssid_str);
    printf("    Operating BW: %u MHz\n",  result->op_bw_mhz);
    printf("    BSSID: %s\n", bssid_str);
    printf("    RSSI: %3d\n", result->rssi);
    printf("    Beacon Interval(TUs): %u\n", result->beacon_interval);
    printf("    Capability Info: 0x%04x\n", result->capability_info);

    ret = mm_parse_rsn_information(result->ies, result->ies_len, &rsn_info);
    if (ret == 0)
    {
        unsigned ii;
        printf("    Security:");
        for (ii = 0; ii < rsn_info.num_akm_suites; ii++)
        {
            printf(" %s", mm_akm_suite_to_string(rsn_info.akm_suites[ii]));
        }
        printf("\n");
    }
    else if (ret == -1 || rsn_info.num_akm_suites == 0)
    {
        printf("    Security: None\n");
    }
    else
    {
        printf("    Invalid RSN IE in probe response\n");
    }
}

/**
 * Scan complete callback.
 *
 * @param state         Scan complete status.
 * @param arg           Opaque argument.
 */
static void scan_complete_callback(enum mmwlan_scan_state state, void *arg)
{
    (void)(state);
    (void)(arg);
    printf("Scanning completed.\n");
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version;

    printf("\n\nMorse Scan Demo (Built "__DATE__ " " __TIME__ ")\n\n");

    mmwlan_init();

    const struct mmwlan_s1g_channel_list* channel_list = load_channel_list();
    status = mmwlan_set_channel_list(channel_list);
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to set country code %s\n", channel_list->country_code);
        MMOSAL_ASSERT(false);
    }

    struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    status = mmwlan_boot(&boot_args);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    status = mmwlan_get_version(&version);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
    printf("Morse firmware version %s, morselib version %s, Morse chip ID 0x%lx\n\n",
           version.morse_fw_version, version.morselib_version, version.morse_chip_id);

    num_scan_results = 0;
    struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_rx_cb = scan_rx_callback;
    scan_req.scan_complete_cb = scan_complete_callback;
    status = mmwlan_scan_request(&scan_req);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
    printf("Scan started on %s channels, Waiting for results...\n", channel_list->country_code);
}
