/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#ifdef MBEDTLS_THREADING_ALT
#include "threading_alt.h"
#endif

#include "mmosal.h"
#include "mmwlan.h"
#include "mmipal.h"
#include "mmconfig.h"
#include "mmhal_app.h"
#include "mmlog.h"
#include "mmutils.h"
#include "mm_app_loadconfig.h"
#include "mm_app_common.h"

/** Maximum number of DNS servers to attempt to retrieve from config store. */
#ifndef DNS_MAX_SERVERS
#define DNS_MAX_SERVERS 2
#endif

/** Binary semaphore used to start user_main() once the link comes up. */
static struct mmosal_semb *attempting_link = NULL;
static bool link_success = false;

/**
 * WLAN station status callback, invoked when WLAN STA state changes.
 *
 * @param sta_state  The new STA state.
 */
static void sta_status_callback(enum mmwlan_sta_state sta_state)
{
    switch (sta_state)
    {
        case MMWLAN_STA_DISABLED:
            MMLOG_APP("WLAN STA disabled\n");
            break;

        case MMWLAN_STA_CONNECTING:
            MMLOG_APP("WLAN STA connecting\n");
            break;

        case MMWLAN_STA_CONNECTED:
            MMLOG_APP("WLAN STA connected\n");
            break;
    }
}

/**
 * Function to convert @c mmwlan_ap_sta_state enumeration to a human readable string.
 *
 * @param state State enum to convert
 *
 * @return string representation of the enumeration.
 */
static char *ap_sta_state_to_str(enum mmwlan_ap_sta_state state)
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
    MM_UNUSED(arg);

    MMLOG_APP("AP STA " MM_MAC_ADDR_FMT " State: %s, AID: %u\n",
              MM_MAC_ADDR_VAL(sta_status->mac_addr),
              ap_sta_state_to_str(sta_status->state),
              sta_status->aid);
}

/**
 * Load DNS server IP addresses from mmconfig and update the mmipal configuration accordingly.
 *
 * @param config_key_format     Format string for the mmconfig keys. This must include a single
 *                              %u format specifier for the server number.
 *
 * @returns true if one or more DNS servers were successfully configured, else false.
 */
static bool set_dns_servers(const char *config_key_format)
{
    uint8_t ii;
    enum mmipal_status status;
    char addr_str[MMIPAL_IPADDR_STR_MAXLEN];
    char key[MMCONFIG_MAX_KEYLEN];
    bool dns_servers_set = false;

    /* Load DNS server addresses from mmconfig, if specified. This will override the servers
     * set by DHCP (if any).  */
    for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
    {
        snprintf(key, sizeof(key), config_key_format, ii);
        if (((mmconfig_read_string(key, addr_str, sizeof(addr_str))) > 0) && addr_str[0] != '\0')
        {
            (void)mmipal_set_dns_server(ii, addr_str);
        }

        status = mmipal_get_dns_server(ii, addr_str);
        if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
        {
            MMLOG_APP("DNS server %d: %s\n", ii, addr_str);
            dns_servers_set = true;
        }
    }

    return dns_servers_set;
}

/**
 * Link status callback
 *
 * @param link_status   Current link status info.
 */
static void link_status_callback(const struct mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        size_t ii;
        bool dns_servers_set = false;

        /* First try to load the DNS configuration from config keys `ip.dns_serverX`. If no
         * matching keys found then fall back to `dns.serverX`. Note `dns.serverX` is for
         * legacy support and should not be used in new designs. */
        dns_servers_set = set_dns_servers("ip.dns_server%u");
        if (!dns_servers_set)
        {
            dns_servers_set = set_dns_servers("dns.server%u");
        }

        MMLOG_PRINTF("Link is up. Time: %lu ms, ", time_ms);
        MMLOG_PRINTF("IP: %s, ", link_status->ip_addr);
        MMLOG_PRINTF("Netmask: %s, ", link_status->netmask);
        MMLOG_PRINTF("Gateway: %s", link_status->gateway);
        for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
        {
            enum mmipal_status status;
            char addr_str[MMIPAL_IPADDR_STR_MAXLEN];

            status = mmipal_get_dns_server(ii, addr_str);
            if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
            {
                MMLOG_PRINTF(", DNS server %u: %s\n", ii, addr_str);
            }
        }
        MMLOG_PRINTF("\n");
        /* Indicate to app_wlan_start that link up was successful */
        link_success = true;
    }
    else
    {
        MMLOG_PRINTF("Link is down. Time: %lu ms\n", time_ms);
        /* Indicate to app_wlan_start that link up was unsuccessful */
        link_success = false;
    }

    mmosal_semb_give(attempting_link);
}

static void fatal_error_handler(struct mmwlan_fatal_error_args *args)
{
    MM_UNUSED(args);
    /* This function is invoked when an MMWLAN error occurs that cannot be automatically recovered
     * from. This function is just a placeholder – application-specific handling of such errors
     * should be added here. For further information, see the documentation for
     * mmwlan_register_fatal_error_handler(). */
    MMOSAL_DEV_ASSERT(false);
    /* Indicate to app_wlan_start that link up was unsuccessful */
    link_success = false;
    mmosal_semb_give(attempting_link);
}

void app_print_version_info(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version = { 0 };
    struct mmwlan_bcf_metadata bcf_metadata = { 0 };
    char hw_version_string[32] = { 0 };

    bool ok = mmhal_get_hardware_version(hw_version_string, sizeof(hw_version_string));
    if (!ok)
    {
        snprintf(hw_version_string, sizeof(hw_version_string), "%s", "Unknown");
    }

    MMLOG_PRINTF("-----------------------------------\n");

    MMLOG_PRINTF("  HW Version:              %s\n", hw_version_string);
    status = mmwlan_get_bcf_metadata(&bcf_metadata);
    if (status == MMWLAN_SUCCESS)
    {
        MMLOG_PRINTF("  BCF API version:         %u.%u.%u\n",
                     bcf_metadata.version.major,
                     bcf_metadata.version.minor,
                     bcf_metadata.version.patch);
        if (bcf_metadata.build_version[0] != '\0')
        {
            MMLOG_PRINTF("  BCF build version:       %s\n", bcf_metadata.build_version);
        }
        if (bcf_metadata.board_desc[0] != '\0')
        {
            MMLOG_PRINTF("  BCF board description:   %s\n", bcf_metadata.board_desc);
        }
    }
    else
    {
        MMLOG_ERR("BCF metadata retrieval failed with code %d\n", status);
    }

    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        MMLOG_ERR("Error occured whilst retrieving version info (%d)\n", status);
    }
    MMLOG_PRINTF("  Morselib version:        %s\n", version.morselib_version);
    MMLOG_PRINTF("  Morse firmware version:  %s\n", version.morse_fw_version);
    MMLOG_PRINTF("  Morse chip ID:           0x%04lx\n", version.morse_chip_id);
    MMLOG_PRINTF("  Morse chip name:         %s\n", version.morse_chip_id_string);
    MMLOG_PRINTF("-----------------------------------\n");

    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
}

void app_wlan_init(void)
{
    /* Ensure we don't call twice */
    MMOSAL_ASSERT(attempting_link == NULL);
    attempting_link = mmosal_semb_create("attempting_link");

    /* Initialize mbedTLS threading (required if MBEDTLS_THREADING_ALT is defined) */
#ifdef MBEDTLS_THREADING_ALT
    mbedtls_platform_threading_init();
#endif

    /* Initialize MMWLAN interface */
    mmwlan_init();
    struct mmwlan_fatal_error_args arg;
    mmwlan_register_fatal_error_handler(fatal_error_handler, &arg);
    mmwlan_set_channel_list(load_channel_list());

    /* Boot the WLAN interface so that we can retrieve the firmware version. */
    struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    (void)mmwlan_boot(&boot_args);
    app_print_version_info();

    /* Load IP stack settings from config store, or use defaults if no entry found in
     * config store. */
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    load_mmipal_init_args(&mmipal_init_args);

    /* Initialize IP stack. */
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        MMLOG_ERR("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_link_status_callback(link_status_callback);
}

void app_wlan_start(void)
{
    enum mmwlan_status status;

    /* Load Wi-Fi settings from config store */
    struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
    load_mmwlan_sta_args(&sta_args);
    load_mmwlan_sta_settings();

    MMLOG_PRINTF("Attempting to connect to %s ", sta_args.ssid);
    if (sta_args.security_type == MMWLAN_SAE)
    {
        MMLOG_PRINTF("with passphrase %s", sta_args.passphrase);
    }
    MMLOG_PRINTF("\n");
    MMLOG_PRINTF("This may take some time (~10 seconds)\n");

    status = mmwlan_sta_enable(&sta_args, sta_status_callback);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    /* Wait for link status callback.
     * Use a binary semaphore to block us until Link is up.
     */
    mmosal_semb_wait(attempting_link, UINT32_MAX);
    /* Wi-Fi link is now established, return to caller */
    MMOSAL_ASSERT(link_success);
}

bool app_wlan_start_relay(bool is_repeater)
{
    enum mmwlan_status status;

    /* Load Wi-Fi STA settings from config store */
    struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
    load_mmwlan_sta_args(&sta_args);
    load_mmwlan_sta_settings();

    /* Load Wi-Fi Relay settings from config store */
    struct mmwlan_relay_args relay_args = MMWLAN_RELAY_ARGS_INIT;
    relay_args.relay_sta_args = &sta_args;
    relay_args.sta_status_cb = sta_status_callback;
    load_mmwlan_relay_settings();

    struct mmwlan_relay_ap_args relay_ap_args = MMWLAN_RELAY_AP_ARGS_INIT;
    if (is_repeater)
    {
        /* Load Wi-Fi Relay AP settings from config store */
        load_mmwlan_relay_ap_args(&relay_ap_args);
        relay_ap_args.sta_status_cb = handle_ap_sta_status;
        relay_args.relay_ap_args = &relay_ap_args;
    }

    MMLOG_PRINTF("Attempting to connect to relay network %s ", sta_args.ssid);
    if (sta_args.security_type == MMWLAN_SAE)
    {
        MMLOG_PRINTF("with passphrase %s", sta_args.passphrase);
    }
    MMLOG_PRINTF("\n");
    MMLOG_PRINTF("This may take some time (~10 seconds)\n");

    status = mmwlan_relay_enable(&relay_args);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    /* Wait for link status callback.
     * Use a binary semaphore to block us until Link is up.
     */
    mmosal_semb_wait(attempting_link, UINT32_MAX);
    /* Wi-Fi link is now established, return to caller */
    MMOSAL_DEV_ASSERT(link_success);
    return link_success;
}

void app_wlan_stop(void)
{
    /* Shutdown wlan interface */
    mmwlan_shutdown();
}

static void hex_dump_morse_stats(struct mmwlan_morse_stats *stats, uint32_t core_num, bool reset)
{
    enum
    {
        BYTES_PER_LINE = 10,
    };

    static uint8_t call_count = 0;
    char leader[8];
    /* Fetch and set of call_count is not atomic, but the race window is very small and not likely
     * to ever occur. Similarly call_count will wrap, but this should not happen fast enough for
     * interleaving of two instances of same call_count to occur. */
    snprintf(leader, sizeof(leader), "MM#%02X", call_count++);

    MMLOG_APP("Morse stats dump %s (len: %u, core: %u, reset: %c)\n",
              leader,
              stats->len,
              core_num,
              reset ? 'y' : 'n');

    char hexbytes[BYTES_PER_LINE * 2 + 1];
    for (uint32_t ii = 0; ii < stats->len; ii++)
    {
        uint32_t pos = ii % BYTES_PER_LINE;
        snprintf(&hexbytes[pos * 2], sizeof("XX"), "%02X", stats->buf[ii]);

        if (pos == BYTES_PER_LINE - 1 || ii == stats->len - 1)
        {
            MMLOG_PRINTF("%s %s\n", leader, hexbytes);
        }
    }
}

bool app_chip_stats_dump(uint32_t core_num, bool reset)
{
    enum
    {
        MAX_CORE_STATS = 3,
    };
    struct mmwlan_morse_stats *stats_list[MAX_CORE_STATS] = {};

    if (core_num == UINT32_MAX)
    {
        bool some_stats = false;
        for (uint32_t core = 0; core < MAX_CORE_STATS; ++core)
        {
            stats_list[core] = mmwlan_get_morse_stats(core, reset);
            some_stats |= (stats_list[core] != NULL);
        }
        if (!some_stats)
        {
            return false;
        }
    }
    else if (core_num < MAX_CORE_STATS)
    {
        stats_list[core_num] = mmwlan_get_morse_stats(core_num, reset);
        if (stats_list[core_num] == NULL)
        {
            return false;
        }
    }
    else
    {
        MMLOG_ERR("Core %d invalid\n", core_num);
        return false;
    }

    /* Log all stats after fetch, so each core stats can be obtained as close as possible. */
    for (size_t core = 0; core < MAX_CORE_STATS; ++core)
    {
        struct mmwlan_morse_stats *stats = stats_list[core];
        if (stats)
        {
            hex_dump_morse_stats(stats, core, reset);
            mmwlan_free_morse_stats(stats);
        }
    }

    return true;
}
