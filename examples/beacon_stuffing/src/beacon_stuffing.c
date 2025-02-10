/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example app using the @ref MMWLAN_BEACON_VENDOR_IE_FILTER_API.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * @note We will only briefly touch on the AP side of this feature as it is out of scope for this
 * documentation.
 *
 * # Beacon Stuffing Overview
 * In a HaLow network the AP will send beacons out at a set interval. These can either be long
 * beacons or short beacons. Currently we can add additional Information Elements (IEs) to the long
 * beacons.
 *
 * Generally speaking a long beacon looks something like the following.
 * @code
 * +-------------------+------+------+       +------+
 * | S1G Beacon Header | IE_1 | IE_2 | ..... | IE_n |
 * +-------------------+------+------+       +------+
 *                     ^                            ^
 *                     |-------------IEs------------|
 * @endcode
 *
 * Beacon stuffing allows Vendor Specific IEs to be placed into the long beacons. This API allows
 * you to register a callback with a reference to the IEs when certain Vendor Specific IEs are
 * detected in the beacon.
 *
 * A Vendor Specific IE has the following structure
 * @code
 * |------------|--------|--------------|---------------------------|
 * | Element ID | Length |      OUI     |  Vendor specific content  |
 * |------------|--------|--------------|---------------------------|
 *        1         1            3                variable
 * @endcode
 *
 * @note Note that the API current only supports filtering on a 24-bit Organizational Unique
 * Identifier (OUI).
 *
 * # Application Overview
 * This application connects and then installs one OUI to filter on. It will then log how many times
 * it sees the Vendor Specific IE and the most recent data received.
 *
 * Additionally, if the length of the vendor specific content is greater than or equal to
 * @ref RGB_DATA_LENGTH_BYTES this application will treat it as intensity levels for the red, green
 * and blue LEDs. These will be set in the callback using @ref mmhal_set_led().
 *
 * To add a Vendor Specific IE using a Morse Micro AP the following command can be used.
 * @code
 * morse_cli vendor_ie -a 4D4D42FF00FF -b
 * @endcode
 *
 * This will add a Vendor Specific IE with
 * - OUI = @c 0x4D4D42
 * - Vendor Specific Content = @c 0xFF00FF
 *
 * To clear the Vendor Specific IEs.
 * @code
 * morse_cli vendor_ie -c -b
 * @endcode
 *
 * @note A beacon may contain multiple Vendor IEs with the same OUI. The handling of these is
 * application specific. In this example application, only the first matching Vendor IE is processed
 * and subsequent Vendor IEs with the same OUI, if present, are ignored.
 *
 * # Configuration
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional configuration
 * options for this application can be found in the config.hjson file.
 */

#include <string.h>
#include <endian.h>
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmutils.h"

#include "mm_app_common.h"

/* Application default configurations. */
/**
 * OUI to look for in the Vendor Specific IE.
 *
 * @note This is a made up OUI for the purposes of this application. Your final application will
 * likely need an OUI specific to your company.
 */
#define DEFAULT_OUI {0x4D, 0x4D, 0x42};
/** Length of the data to print when logging the stat struct. If the data is longer than this it
 * will be truncated. */
#define DEFAULT_LOG_LEN_BYTES 30
/** How often the contents for the stat struct is logged. */
#define DEFAULT_STATS_UPDATE_PERIOD_MS 1000


/** Filter index to install the OUI. */
#define FILTER_INDEX 0
/** Length of data required for updating the LED state. */
#define RGB_DATA_LENGTH_BYTES 3

/** Filter structure used when calling @ref mmwlan_update_beacon_vendor_ie_filter(). We declare it
 * here to ensure that the memory is always valid and not used for anything else. */
struct mmwlan_beacon_vendor_ie_filter filter;

/** Structure to store some basic statistics for the beacon Vendor Specific IE reception. */
struct beacon_vendor_ie_stat
{
    /** Number of times the Vendor Specific IE in the beacon was seen. */
    uint32_t occurrences;
    /** If not NULL, the most recent IE data received. This does not include the IE header. */
    uint8_t *ie_data;
    /** The length in bytes of ie_data. */
    uint32_t ie_len;
};

/**
 * Callback function that gets executed every time a beacon containing a matching Vendor Specific IE
 * is received.
 *
 * Current implementation will log the stats for the filter specified and update the state of the
 * LEDs on the board using @ref mmhal_set_led(). The LED update only occurs if there is at least
 * @ref RGB_DATA_LENGTH_BYTES worth of data after the OUI.
 *
 * @param ies       Reference to the list of information elements (little endian order) present in
 *                  the beacon that matched the filter.
 * @param ies_len   Length of the IE list in octets.
 * @param arg       Reference to the opaque argument provided with the filter.
 */
static void beacon_vendor_ie_callback(const uint8_t *ies, uint32_t ies_len, void *arg)
{
    int offset;
    uint8_t length;
    struct beacon_vendor_ie_stat *stat = (struct beacon_vendor_ie_stat *)arg;

    offset = mm_find_vendor_specific_ie(ies, ies_len, filter.ouis[FILTER_INDEX], MMWLAN_OUI_SIZE);
    if (offset > 0)
    {
        stat->occurrences++;
        /* Note that we rely on mm_find_vendor_specific_ie() to validate that the IE does not
         * extend past the end of the given buffer. */
        length = ies[offset + 1];
        offset += 2;

        if (stat->ie_data && (stat->ie_len != length))
        {
            mmosal_free(stat->ie_data);
            stat->ie_data = NULL;
            stat->ie_len = 0;
        }

        if (!stat->ie_data)
        {
            stat->ie_data = (uint8_t *)mmosal_malloc(length);
            if (!stat->ie_data)
            {
                /* Generally we would want to avoid operations that might take an extended amount of
                 * time (i.e printf()). This has just been put in to try and make it as clear as
                 * possible what the error might be for the purposes of the example application. */
                printf("Failed to alloc memory for ie data.");
                return;
            }
            stat->ie_len = length;
        }

        memcpy(stat->ie_data, (ies + offset), length);

        if (stat->ie_len >= (RGB_DATA_LENGTH_BYTES + MMWLAN_OUI_SIZE))
        {
            const uint8_t *rgb_data = stat->ie_data + MMWLAN_OUI_SIZE;
            mmhal_set_led(LED_RED, rgb_data[0]);
            mmhal_set_led(LED_GREEN, rgb_data[1]);
            mmhal_set_led(LED_BLUE, rgb_data[2]);
        }
    }
}

/**
 * Print the contents of the given stat struct using @c printf().
 *
 * @param stat              Reference to the stat to print.
 * @param max_data_log_len  Maximum length in bytes to print for the data content in the stat
 *                          struct.
 */
static void beacon_vendor_ie_stat_log(struct beacon_vendor_ie_stat *stat, uint32_t max_data_log_len)
{
    uint32_t ii;
    printf("%11lu | %14lu | ", stat->occurrences, stat->ie_len);
    if (stat->ie_data)
    {
        for (ii = 0; (ii < stat->ie_len) && (ii < max_data_log_len); ii++)
        {
            printf("%02x", stat->ie_data[ii]);
        }

        printf("%s\n", (stat->ie_len >= max_data_log_len) ? "..." : "");
    }
    else
    {
        printf("N/A \n");
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    enum mmwlan_status status;
    struct beacon_vendor_ie_stat stat = {0};
    uint8_t mmconfig_oui[MMWLAN_OUI_SIZE];

    uint8_t oui[MMWLAN_OUI_SIZE] = DEFAULT_OUI;
    uint32_t log_length_bytes = DEFAULT_LOG_LEN_BYTES;
    uint32_t update_period_ms = DEFAULT_STATS_UPDATE_PERIOD_MS;

    /* Read out any params from configstore if they exist. If they don't the variables will retain
     * their current values. */
    mmconfig_read_uint32("beacon_stuffing.log_len_bytes", &log_length_bytes);
    mmconfig_read_uint32("beacon_stuffing.log_period_ms", &update_period_ms);

    int mmconfig_oui_len = mmconfig_read_bytes("beacon_stuffing.oui", &mmconfig_oui,
                                               MMWLAN_OUI_SIZE, 0);
    if (mmconfig_oui_len >= MMWLAN_OUI_SIZE)
    {
        memcpy(&oui, &mmconfig_oui, MMWLAN_OUI_SIZE);
    }

    printf("\n\nMorse Beacon Stuffing Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    memcpy(filter.ouis[FILTER_INDEX], &oui, MMWLAN_OUI_SIZE);
    filter.n_ouis = 1;
    filter.cb = beacon_vendor_ie_callback;
    filter.cb_arg = (void *)&stat;

    status = mmwlan_update_beacon_vendor_ie_filter(&filter);
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to register beacon vendor ie filter.\n");
        return;
    }

    printf("\nFiltering for Vendor Specific IE with OUI: 0x%02x%02x%02x\n", oui[0], oui[1], oui[2]);

    printf("Occurrences | Length (bytes) | Data (hex)\n");
    while (1)
    {
        mmosal_task_sleep(update_period_ms);
        beacon_vendor_ie_stat_log(&stat, log_length_bytes);
    }
}
