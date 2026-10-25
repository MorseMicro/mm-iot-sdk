/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Morse Micro load configuration helper
 *
 * This file contains helper routines to load commonly used configuration settings
 * such as SSID, password, IP address settings and country code from the config store.
 * If a particular setting is not found, defaults are used.  It is safe to call these
 * functions if none of the settings are available in config store.
 */

#include "mmwlan.h"
#include "mmconfig.h"
#include "mmipal.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mm_app_loadconfig.h"
#include "mmregdb.h"

/* Default Country Code  */
#ifndef COUNTRY_CODE
/** Country code. (Do not quote; it will be stringified, as it can be passed at build time.) */
/* clang-format off */
#define COUNTRY_CODE ??
/* clang-format on */
#endif

/* Default SSID  */
#ifndef SSID
/** SSID of the AP to connect to. (Do not quote; it will be stringified.) */
#define SSID MorseMicro
#endif

#ifndef AP_SSID
/** SSID of the AP. (Do not quote; it will be stringified.) */
#define AP_SSID MorseMicro
#endif

/* Default passphrase  */
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

/* Configure the STA to use DHCP, this overrides any static configuration.
 * If the @c ip.dhcp_enabled is set in the config store that will take priority */
#ifndef ENABLE_DHCP
#define ENABLE_DHCP (1)
#endif

/* Static Network configuration */
#ifndef STATIC_LOCAL_IP
/** Statically configured IP address (if ENABLE_DHCP is not set). */
#define STATIC_LOCAL_IP "192.168.1.2"
#endif
#ifndef STATIC_GATEWAY
/** Statically configured gateway address (if ENABLE_DHCP is not set). */
#define STATIC_GATEWAY "192.168.1.1"
#endif
#ifndef STATIC_NETMASK
/** Statically configured netmask (if ENABLE_DHCP is not set). */
#define STATIC_NETMASK "255.255.255.0"
#endif

/* Static Network configuration */
#ifndef STATIC_LOCAL_IP6
/** Statically configured IP address (if ENABLE_AUTOCONFIG is not set). */
#define STATIC_LOCAL_IP6 "FE80::2"
#endif

#ifndef AP_STATIC_LOCAL_IP
/** Statically configured IP address. */
#define AP_STATIC_LOCAL_IP "192.168.1.1"
#endif

/** Maximum number of channels accepted in @c wlan.scan_selected_channels. */
#define SCAN_SELECTED_CHANNELS_MAX (10)

/** Selective scan attempts used when a channel list is set but no attempt count is. */
#define SCAN_SELECTIVE_ATTEMPTS_DEFAULT (1)

/*
 * --
 * Default AP channel configuration
 * --
 */

#ifndef OP_CLASS
/**
 * Operating Class to use for AP.
 * This together with S1G_CHANNEL must correspond to a channel in the regulatory database.
 */
#define OP_CLASS (71) /* US, 8 MHz */
#endif

#ifndef S1G_CHANNEL
/**
 * S1G Channel to use for AP.
 * This together with OP_CLASS must correspond to a channel in the regulatory database.
 */
#define S1G_CHANNEL (44) /* US, 8 MHz */
#endif

#ifndef PRIMARY_BW_MHZ
/**
 * Primary Bandwidth to use for AP.
 *
 * Valid values:
 * * 0 (auto)
 * * 1
 * * 2
 */
#define PRIMARY_BW_MHZ (0)
#endif

#ifndef PRIMARY_1MHZ_CHANNEL_INDEX
/** Primary 1 MHz Channel Index to use for AP */
#define PRIMARY_1MHZ_CHANNEL_INDEX (3)
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

void load_mmipal_init_args(struct mmipal_init_args *args)
{
    bool boolval;

    /* Load default static IP in case we don't find the key */
    (void)mmosal_safer_strcpy(args->ip_addr, STATIC_LOCAL_IP, sizeof(args->ip_addr));
    (void)mmconfig_read_string("ip.ip_addr", args->ip_addr, sizeof(args->ip_addr));

    /* Load default netmask in case we don't find the key */
    (void)mmosal_safer_strcpy(args->netmask, STATIC_NETMASK, sizeof(args->netmask));
    (void)mmconfig_read_string("ip.netmask", args->netmask, sizeof(args->netmask));

    /* Load default gateway in case we don't find the key */
    (void)mmosal_safer_strcpy(args->gateway_addr, STATIC_GATEWAY, sizeof(args->gateway_addr));
    (void)mmconfig_read_string("ip.gateway", args->gateway_addr, sizeof(args->gateway_addr));

#if ENABLE_DHCP
    args->mode = MMIPAL_DHCP;
#else
    args->mode = MMIPAL_STATIC;
#endif

    /* If the following setting is not found, we leave as is.
     * @note All mmconfig_read_xxx() functions leave the value untouched
     * if the key is not found.
     */
    if (mmconfig_read_bool("ip.dhcp_enabled", &boolval) == MMCONFIG_OK)
    {
        if (boolval)
        {
            /* DHCP mode */
            args->mode = MMIPAL_DHCP;
        }
        else
        {
            /* Static IP mode */
            args->mode = MMIPAL_STATIC;
        }
    }
    if (args->mode == MMIPAL_DHCP)
    {
        MMLOG_APP("Initialize IPv4 using DHCP...\n");
    }
    else
    {
        MMLOG_APP("Initialize IPv4 with static IP: %s...\n", args->ip_addr);
    }

#if defined(MMIPAL_IPV6_ENABLED) && MMIPAL_IPV6_ENABLED
    /* Load default static IPv6 in case we don't find the key */
    (void)mmosal_safer_strcpy(args->ip6_addr, STATIC_LOCAL_IP6, sizeof(args->ip6_addr));
    (void)mmconfig_read_string("ip6.ip_addr", args->ip6_addr, sizeof(args->ip6_addr));

    /* We set this as the by default IPv6 is set to disabled in @ref MMIPAL_INIT_ARGS_DEFAULT */
    args->ip6_mode = MMIPAL_IP6_AUTOCONFIG;

    /* If the following setting is not found, we default to autoconfig mode */
    if (mmconfig_read_bool("ip6.autoconfig", &boolval) == MMCONFIG_OK)
    {
        if (boolval)
        {
            /* Autoconfig mode */
            args->ip6_mode = MMIPAL_IP6_AUTOCONFIG;
        }
        else
        {
            args->ip6_mode = MMIPAL_IP6_STATIC;
        }
    }

    if (args->ip6_mode == MMIPAL_IP6_AUTOCONFIG)
    {
        MMLOG_APP("Initialize IPv6 using Autoconfig...\n");
    }
    else
    {
        MMLOG_APP("Initialize IPv6 with static IP %s\n", args->ip6_addr);
    }
#endif
}

void load_ap_mmipal_init_args(struct mmipal_init_args *args)
{
    /* DHCP not supported in AP mode. */
    args->mode = MMIPAL_STATIC;

    /* IPv6 not supported in AP mode. */
    args->ip6_mode = MMIPAL_IP6_DISABLED;

    /* Load default static IP in case we don't find the key */
    (void)mmosal_safer_strcpy(args->ip_addr, AP_STATIC_LOCAL_IP, sizeof(args->ip_addr));
    (void)mmconfig_read_string("ip.ip_addr", args->ip_addr, sizeof(args->ip_addr));

    /* Load default netmask in case we don't find the key */
    (void)mmosal_safer_strcpy(args->netmask, STATIC_NETMASK, sizeof(args->netmask));
    (void)mmconfig_read_string("ip.netmask", args->netmask, sizeof(args->netmask));

    /* Load default gateway in case we don't find the key */
    (void)mmosal_safer_strcpy(args->gateway_addr, STATIC_GATEWAY, sizeof(args->gateway_addr));
    (void)mmconfig_read_string("ip.gateway", args->gateway_addr, sizeof(args->gateway_addr));

    MMLOG_APP("Initialize IPv4 with static IP: %s...\n", args->ip_addr);
}

const struct mmwlan_s1g_channel_list *load_channel_list(void)
{
    char strval[16];
    const struct mmwlan_s1g_channel_list *channel_list;

    /* Set the default channel list in case country code is not found */
    (void)mmosal_safer_strcpy(strval, STRINGIFY(COUNTRY_CODE), sizeof(strval));
    (void)mmconfig_read_string("wlan.country_code", strval, sizeof(strval));
    channel_list = mmwlan_lookup_regulatory_domain(get_regulatory_db(), strval);

    if (channel_list == NULL)
    {
        MMLOG_ERR("Could not find specified regulatory domain matching country code %s\n", strval);
        MMLOG_ERR("Please set the configuration key wlan.country_code to a valid country code.\n");
        MMOSAL_ASSERT(false);
    }
    return channel_list;
}

void load_mmwlan_sta_args(struct mmwlan_sta_args *sta_config)
{
    char strval[32];
    uint8_t macval[MMWLAN_MAC_ADDR_LEN];
    int intval;
    bool boolval;

    /* Load SSID */
    (void)mmosal_safer_strcpy((char *)sta_config->ssid, STRINGIFY(SSID), sizeof(sta_config->ssid));
    (void)mmconfig_read_string("wlan.ssid", (char *)sta_config->ssid, sizeof(sta_config->ssid));
    sta_config->ssid_len = strlen((char *)sta_config->ssid);

    /* Load password */
    (void)mmosal_safer_strcpy(sta_config->passphrase,
                              STRINGIFY(SAE_PASSPHRASE),
                              sizeof(sta_config->passphrase));
    (void)mmconfig_read_string("wlan.password",
                               sta_config->passphrase,
                               sizeof(sta_config->passphrase));
    sta_config->passphrase_len = strlen(sta_config->passphrase);

    /* Load security type */
    sta_config->security_type = SECURITY_TYPE;
    if (mmconfig_read_string("wlan.security", strval, sizeof(strval)) > 0)
    {
        if (strncmp("sae", strval, sizeof(strval)) == 0)
        {
            sta_config->security_type = MMWLAN_SAE;
        }
        else if (strncmp("owe", strval, sizeof(strval)) == 0)
        {
            sta_config->security_type = MMWLAN_OWE;
        }
        else if (strncmp("open", strval, sizeof(strval)) == 0)
        {
            sta_config->security_type = MMWLAN_OPEN;
        }
    }

    /* Load PMF mode */
    sta_config->pmf_mode = PMF_MODE;
    if (mmconfig_read_string("wlan.pmf_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            sta_config->pmf_mode = MMWLAN_PMF_DISABLED;
        }
        else if (strncmp("required", strval, sizeof(strval)) == 0)
        {
            sta_config->pmf_mode = MMWLAN_PMF_REQUIRED;
        }
    }

    /* Load BSSID */
    if (mmconfig_read_mac_addr("wlan.bssid", macval) == MMCONFIG_OK)
    {
        memcpy(sta_config->bssid, macval, sizeof(macval));
    }

    /* Load STA type */
    if (mmconfig_read_string("wlan.station_type", strval, sizeof(strval)) > 0)
    {
        if (strncmp("non_sensor", strval, sizeof(strval)) == 0)
        {
            sta_config->sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
        }
        else if (strncmp("sensor", strval, sizeof(strval)) == 0)
        {
            sta_config->sta_type = MMWLAN_STA_TYPE_SENSOR;
        }
    }

    /* Load CAC if specified */
    if (mmconfig_read_bool("wlan.cac_enabled", &boolval) == MMCONFIG_OK)
    {
        sta_config->cac_mode = boolval ? MMWLAN_CAC_ENABLED : MMWLAN_CAC_DISABLED;
    }

    /* Load raw priority if specified */
    if (mmconfig_read_int("wlan.raw_priority", &intval) == MMCONFIG_OK)
    {
        sta_config->raw_sta_priority = (int16_t)intval;
    }

    /* Load scan interval parameters, if specified */
    if (mmconfig_read_int("wlan.sta_scan_interval_base_s", &intval) == MMCONFIG_OK)
    {
        sta_config->scan_interval_base_s = (int16_t)intval;
    }
    if (mmconfig_read_int("wlan.sta_scan_interval_limit_s", &intval) == MMCONFIG_OK)
    {
        sta_config->scan_interval_limit_s = (int16_t)intval;
    }
}

/**
 * Parses a comma separated list of S1G channel numbers, such as "33,37,41".
 *
 * @param  str      Null terminated string to parse.
 * @param  channels Buffer to receive the parsed channel numbers.
 * @param  max      Capacity of @p channels.
 *
 * @returns the number of channels parsed, or zero if @p str was malformed or empty.
 */
static uint8_t parse_scan_channel_list(const char *str, uint8_t *channels, uint8_t max)
{
    uint8_t count = 0;

    while (count < max)
    {
        char *end;
        uint32_t value = strtoul(str, &end, 10);

        if (end == str || value == 0 || value > UINT8_MAX)
        {
            return 0;
        }

        channels[count++] = (uint8_t)value;

        if (*end == '\0')
        {
            return count;
        }
        if (*end != ',')
        {
            return 0;
        }
        str = end + 1;
    }

    return 0;
}

void load_mmwlan_sta_settings(void)
{
    int intval;
    uint32_t uintval;
    bool boolval;
    char strval[32];

    /* Load power save mode */
    if (mmconfig_read_string("wlan.power_save_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("enabled", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_power_save_mode(MMWLAN_PS_ENABLED);
        }
        else if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
        }
    }

    /* Apply subbands enabled if specified */
    if (mmconfig_read_bool("wlan.subbands_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_subbands_enabled(boolval);
    }

    /* Apply sgi enabled if specified */
    if (mmconfig_read_bool("wlan.sgi_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_sgi_enabled(boolval);
    }

    /* Apply ampdu enabled if specified */
    if (mmconfig_read_bool("wlan.ampdu_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_ampdu_enabled(boolval);
    }

    /* Apply fragment threshold if specified */
    if (mmconfig_read_int("wlan.fragment_threshold", &intval) == MMCONFIG_OK)
    {
        mmwlan_set_fragment_threshold(intval);
    }

    /* Apply rts threshold if specified */
    if (mmconfig_read_int("wlan.rts_threshold", &intval) == MMCONFIG_OK)
    {
        mmwlan_set_rts_threshold(intval);
    }

    /* Apply Health check intervals if specified */
    if (mmconfig_read_uint32("wlan.max_health_check_intvl_ms", &uintval) == MMCONFIG_OK)
    {
        /* If not specified, the minimum is 0 */
        uint32_t health_check_min = 0;
        mmconfig_read_uint32("wlan.min_health_check_intvl_ms", &health_check_min);
        mmwlan_set_health_check_interval(health_check_min, uintval);
    }
    else if (mmconfig_read_uint32("wlan.min_health_check_intvl_ms", &uintval) == MMCONFIG_OK)
    {
        /* If only minimum is specified, then treat the maximum as unbounded */
        mmwlan_set_health_check_interval(uintval, UINT32_MAX);
    }

    struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
    uint8_t selected_channels[SCAN_SELECTED_CHANNELS_MAX];
    (void)mmconfig_read_uint32("wlan.sta_scan_dwell_time_ms", &scan_config.dwell_time_ms);
    (void)mmconfig_read_bool("wlan.ndp_probe_enabled", &scan_config.ndp_probe_enabled);
    (void)mmconfig_read_uint32("wlan.home_chan_dwell_time_ms",
                               &scan_config.home_channel_dwell_time_ms);
    if (mmconfig_read_string("wlan.scan_selected_channels", strval, sizeof(strval)) > 0)
    {
        uint8_t num_channels =
            parse_scan_channel_list(strval, selected_channels, SCAN_SELECTED_CHANNELS_MAX);
        if (num_channels == 0)
        {
            MMLOG_ERR("Ignoring malformed wlan.scan_selected_channels: %s\n", strval);
        }
        else
        {
            scan_config.selected_channels = selected_channels;
            scan_config.selected_channels_len = num_channels;
            scan_config.selective_scan_attempts = SCAN_SELECTIVE_ATTEMPTS_DEFAULT;
            if (mmconfig_read_uint32("wlan.selective_scan_attempts", &uintval) == MMCONFIG_OK)
            {
                if (uintval <= UINT8_MAX)
                {
                    scan_config.selective_scan_attempts = uintval;
                }
                else
                {
                    MMLOG_ERR("%s out of range\n", "wlan.selective_scan_attempts");
                }
            }
        }
    }
    mmwlan_set_scan_config(&scan_config);

    /* Apply MCS10 mode if specified */
    if (mmconfig_read_string("wlan.mcs10_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_mcs10_mode(MMWLAN_MCS10_MODE_DISABLED);
        }
        else if (strncmp("forced", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_mcs10_mode(MMWLAN_MCS10_MODE_FORCED);
        }
        else if (strncmp("auto", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_mcs10_mode(MMWLAN_MCS10_MODE_AUTO);
        }
    }

    /* Apply duty cycle mode if specified */
    if (mmconfig_read_string("wlan.duty_cycle_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("spread", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_duty_cycle_mode(MMWLAN_DUTY_CYCLE_MODE_SPREAD);
        }
        else if (strncmp("burst", strval, sizeof(strval)) == 0)
        {
            mmwlan_set_duty_cycle_mode(MMWLAN_DUTY_CYCLE_MODE_BURST);
        }
    }
}

void load_mmwlan_relay_ap_args(struct mmwlan_relay_ap_args *relay_ap_config)
{
    uint8_t macval[MMWLAN_MAC_ADDR_LEN];
    uint32_t uint32val;

    /* Load BSSID */
    if (mmconfig_read_mac_addr("wlan.ap_bssid", macval) == MMCONFIG_OK)
    {
        memcpy(relay_ap_config->bssid, macval, sizeof(macval));
    }

    /* Load DTIM period */
    if (mmconfig_read_uint32("wlan.ap_dtim_period", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            relay_ap_config->dtim_period = uint32val;
        }
        else
        {
            MMLOG_ERR("%s out of range\n", "wlan.ap_dtim_period");
        }
    }

    /* Load Beacon Interval */
    if (mmconfig_read_uint32("wlan.ap_beacon_interval_tus", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            relay_ap_config->beacon_interval_tus = uint32val;
        }
        else
        {
            MMLOG_ERR("%s out of range\n", "wlan.ap_beacon_interval_tus");
        }
    }

    relay_ap_config->max_stas = MAX_STAS;
    if (mmconfig_read_uint32("wlan.max_stas", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            relay_ap_config->max_stas = uint32val;
        }
        else
        {
            MMLOG_ERR("%s out of range\n", "wlan.max_stas");
        }
    }
}

void load_mmwlan_ap_args(struct mmwlan_ap_args *ap_config)
{
    char strval[32];
    uint8_t macval[MMWLAN_MAC_ADDR_LEN];
    uint32_t uint32val;

    /* Load SSID */
    (void)mmosal_safer_strcpy((char *)ap_config->ssid, STRINGIFY(AP_SSID), sizeof(ap_config->ssid));
    (void)mmconfig_read_string("wlan.ap_ssid", (char *)ap_config->ssid, sizeof(ap_config->ssid));
    ap_config->ssid_len = strlen((char *)ap_config->ssid);

    /* Load password */
    (void)mmosal_safer_strcpy(ap_config->passphrase,
                              STRINGIFY(SAE_PASSPHRASE),
                              sizeof(ap_config->passphrase));
    (void)mmconfig_read_string("wlan.ap_password",
                               ap_config->passphrase,
                               sizeof(ap_config->passphrase));
    ap_config->passphrase_len = strlen(ap_config->passphrase);

    /* Load security type */
    ap_config->security_type = SECURITY_TYPE;
    if (mmconfig_read_string("wlan.ap_security", strval, sizeof(strval)) > 0)
    {
        if (strncmp("sae", strval, sizeof(strval)) == 0)
        {
            ap_config->security_type = MMWLAN_SAE;
        }
        else if (strncmp("owe", strval, sizeof(strval)) == 0)
        {
            ap_config->security_type = MMWLAN_OWE;
        }
        else if (strncmp("open", strval, sizeof(strval)) == 0)
        {
            ap_config->security_type = MMWLAN_OPEN;
        }
        else
        {
            MMLOG_APP("Invalid value of %s read from config store: %s\n",
                      "wlan.ap_security",
                      strval);
        }
    }

    /* Load PMF mode */
    ap_config->pmf_mode = PMF_MODE;
    if (mmconfig_read_string("wlan.ap_pmf_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            MMLOG_APP("PMF disabled\n");
            ap_config->pmf_mode = MMWLAN_PMF_DISABLED;
        }
        else if (strncmp("required", strval, sizeof(strval)) == 0)
        {
            ap_config->pmf_mode = MMWLAN_PMF_REQUIRED;
        }
        else
        {
            MMLOG_APP("Invalid value of %s read from config store: %s\n",
                      "wlan.ap_pmf_mode",
                      strval);
        }
    }

    /* Load BSSID */
    if (mmconfig_read_mac_addr("wlan.ap_bssid", macval) == MMCONFIG_OK)
    {
        memcpy(ap_config->bssid, macval, sizeof(macval));
    }

    ap_config->op_class = OP_CLASS;
    if (mmconfig_read_uint32("wlan.ap_op_class", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            ap_config->op_class = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_op_class");
        }
    }

    ap_config->s1g_chan_num = S1G_CHANNEL;
    if (mmconfig_read_uint32("wlan.ap_s1g_chan_num", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            ap_config->s1g_chan_num = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_s1g_chan_num");
        }
    }

    ap_config->pri_bw_mhz = PRIMARY_BW_MHZ;
    if (mmconfig_read_uint32("wlan.ap_pri_bw_mhz", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            ap_config->pri_bw_mhz = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_pri_bw_mhz");
        }
    }

    ap_config->pri_1mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX;
    if (mmconfig_read_uint32("wlan.ap_pri_1mhz_chan_idx", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            ap_config->pri_1mhz_chan_idx = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_pri_1mhz_chan_idx");
        }
    }

    /* Load DTIM period */
    if (mmconfig_read_uint32("wlan.ap_dtim_period", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            ap_config->dtim_period = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_dtim_period");
        }
    }

    /* Load Beacon Interval */
    if (mmconfig_read_uint32("wlan.ap_beacon_interval_tus", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT16_MAX)
        {
            ap_config->beacon_interval_tus = uint32val;
        }
        else
        {
            MMLOG_APP("%s out of range\n", "wlan.ap_beacon_interval_tus");
        }
    }

    ap_config->max_stas = MAX_STAS;
    if (mmconfig_read_uint32("wlan.max_stas", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            ap_config->max_stas = uint32val;
        }
        else
        {
            MMLOG_ERR("%s out of range\n", "wlan.max_stas");
        }
    }
}

void load_mmwlan_ap_settings(void)
{
    int intval;
    uint32_t uintval;
    bool boolval;

    /* Apply subbands enabled if specified */
    if (mmconfig_read_bool("wlan.subbands_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_subbands_enabled(boolval);
    }

    /* Apply sgi enabled if specified */
    if (mmconfig_read_bool("wlan.sgi_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_sgi_enabled(boolval);
    }

    /* Apply ampdu enabled if specified */
    if (mmconfig_read_bool("wlan.ampdu_enabled", &boolval) == MMCONFIG_OK)
    {
        mmwlan_set_ampdu_enabled(boolval);
    }

    /* Apply fragment threshold if specified */
    if (mmconfig_read_int("wlan.fragment_threshold", &intval) == MMCONFIG_OK)
    {
        mmwlan_set_fragment_threshold(intval);
    }

    /* Apply rts threshold if specified */
    if (mmconfig_read_int("wlan.rts_threshold", &intval) == MMCONFIG_OK)
    {
        mmwlan_set_rts_threshold(intval);
    }

    /* Apply Health check intervals if specified */
    if (mmconfig_read_uint32("wlan.max_health_check_intvl_ms", &uintval) == MMCONFIG_OK)
    {
        /* If not specified, the minimum is 0 */
        uint32_t health_check_min = 0;
        mmconfig_read_uint32("wlan.min_health_check_intvl_ms", &health_check_min);
        mmwlan_set_health_check_interval(health_check_min, uintval);
    }
    else if (mmconfig_read_uint32("wlan.min_health_check_intvl_ms", &uintval) == MMCONFIG_OK)
    {
        /* If only minimum is specified, then treat the maximum as unbounded */
        mmwlan_set_health_check_interval(uintval, UINT32_MAX);
    }
}

void load_mmwlan_relay_settings(void)
{
    /* Optionally override the relay depth. May only be configured while relay mode is disabled. */
    uint32_t relay_depth_override;
    if (mmconfig_read_uint32("wlan.relay_depth_override", &relay_depth_override) == MMCONFIG_OK)
    {
        if (relay_depth_override <= MMWLAN_RELAY_MAX_DEPTH)
        {
            enum mmwlan_status status =
                mmwlan_relay_ate_set_depth_override((uint8_t)relay_depth_override);
            if (status != MMWLAN_SUCCESS)
            {
                MMLOG_ERR("Failed to set relay depth override (status %d)\n", status);
            }
        }
        else
        {
            MMLOG_ERR("%s out of range\n", "wlan.relay_depth_override");
        }
    }
}

bool country_code_in_regulatory_domain(const char *code)
{
    if (mmwlan_lookup_regulatory_domain(get_regulatory_db(), code) == NULL)
    {
        return false;
    }
    return true;
}
