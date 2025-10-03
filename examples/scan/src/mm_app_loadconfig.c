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
#include "mmosal.h"
#include "mm_app_loadconfig.h"
#include "mm_app_regdb.h"

#ifndef COUNTRY_CODE
#define COUNTRY_CODE "??"
#endif

/* Default SSID  */
#ifndef SSID
/** SSID of the AP to connect to. (Do not quote; it will be stringified.) */
#define SSID                            MorseMicro
#endif

/* Default passphrase  */
#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE                  12345678
#endif

/* Default security type  */
#ifndef SECURITY_TYPE
/** Security type (@see mmwlan_security_type). */
#define SECURITY_TYPE                   MMWLAN_SAE
#endif

/* Default PMF mode */
#ifndef PMF_MODE
/** Protected Management Frames (PMF) mode (@see mmwlan_pmf_mode). */
#define PMF_MODE                        MMWLAN_PMF_REQUIRED
#endif

/* Configure the STA to use DHCP, this overrides any static configuration.
 * If the @c ip.dhcp_enabled is set in the config store that will take priority */
#ifndef ENABLE_DHCP
#define ENABLE_DHCP                     (1)
#endif

/* Static Network configuration */
#ifndef STATIC_LOCAL_IP
/** Statically configured IP address (if ENABLE_DHCP is not set). */
#define STATIC_LOCAL_IP                 "192.168.1.2"
#endif
#ifndef STATIC_GATEWAY
/** Statically configured gateway address (if ENABLE_DHCP is not set). */
#define STATIC_GATEWAY                  "192.168.1.1"
#endif
#ifndef STATIC_NETMASK
/** Statically configured netmask (if ENABLE_DHCP is not set). */
#define STATIC_NETMASK                  "255.255.255.0"
#endif

/* Static Network configuration */
#ifndef STATIC_LOCAL_IP6
/** Statically configured IP address (if ENABLE_AUTOCONFIG is not set). */
#define STATIC_LOCAL_IP6                 "FE80::2"
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
            boolval = false;
            (void) mmconfig_read_bool("ip.dhcp_offload", &boolval);
            if (boolval)
            {
                /* DHCP offload mode */
                args->mode = MMIPAL_DHCP_OFFLOAD;
            }
            else
            {
            /* DHCP mode */
            args->mode = MMIPAL_DHCP;
            }
        }
        else
        {
            /* Static IP mode */
            args->mode = MMIPAL_STATIC;
        }
    }
    if (args->mode == MMIPAL_DHCP)
    {
        printf("Initialize IPv4 using DHCP...\n");
    }
    else if (args->mode == MMIPAL_DHCP_OFFLOAD)
    {
        printf("Initialize IPv4 using DHCP offload...\n");
    }
    else
    {
        printf("Initialize IPv4 with static IP: %s...\n", args->ip_addr);
    }

    /* Check if any offload features are enabled */
    (void)mmconfig_read_bool("wlan.offload_arp_response", &args->offload_arp_response);
    (void)mmconfig_read_uint32("wlan.offload_arp_refresh_s", &args->offload_arp_refresh_s);

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
        printf("Initialize IPv6 using Autoconfig...\n");
    }
    else
    {
        printf("Initialize IPv6 with static IP %s\n", args->ip6_addr);
    }
}

const struct mmwlan_s1g_channel_list* load_channel_list(void)
{
    char strval[16];
    const struct mmwlan_s1g_channel_list *channel_list;

    /* Set the default channel list in case country code is not found */
    (void)mmosal_safer_strcpy(strval, STRINGIFY(COUNTRY_CODE), sizeof(strval));
    (void)mmconfig_read_string("wlan.country_code", strval, sizeof(strval));
    channel_list = mmwlan_lookup_regulatory_domain(get_regulatory_db(), strval);

    if (channel_list == NULL)
    {
        printf("Could not find specified regulatory domain matching country code %s\n", strval);
        printf("Please set the configuration key wlan.country_code to the correct country code.\n");
        MMOSAL_ASSERT(false);
    }
    return channel_list;
}

void load_mmwlan_sta_args(struct mmwlan_sta_args *sta_config)
{
    char strval[32];
    int intval;
    bool boolval;

    /* Load SSID */
    (void)mmosal_safer_strcpy((char*)sta_config->ssid, STRINGIFY(SSID), sizeof(sta_config->ssid));
    (void)mmconfig_read_string("wlan.ssid", (char*) sta_config->ssid, sizeof(sta_config->ssid));
    sta_config->ssid_len = strlen((char*)sta_config->ssid);

    /* Load password */
    (void)mmosal_safer_strcpy(sta_config->passphrase, STRINGIFY(SAE_PASSPHRASE),
                              sizeof(sta_config->passphrase));
    (void)mmconfig_read_string("wlan.password", sta_config->passphrase,
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
    if (mmconfig_read_string("wlan.bssid", strval, sizeof(strval)) > 0)
    {
        /* Need to provide an array of ints to sscanf otherwise it will overflow */
        int temp[6];
        int i;

        int ret = sscanf(strval, "%x:%x:%x:%x:%x:%x",
                         &temp[0], &temp[1], &temp[2],
                         &temp[3], &temp[4], &temp[5]);
        if (ret == 6)
        {
            for (i = 0; i < 6; i++)
            {
                if (temp[i] > UINT8_MAX || temp[i] < 0)
                {
                    /* Invalid value, ignore and reset to default */
                    memset(sta_config->bssid, 0, sizeof(sta_config->bssid));
                    break;
                }

                sta_config->bssid[i] = (uint8_t)temp[i];
            }
        }
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
        sta_config->raw_sta_priority = (int16_t) intval;
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

void load_mmwlan_settings(void)
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
    (void)mmconfig_read_uint32("wlan.sta_scan_dwell_time_ms", &scan_config.dwell_time_ms);
    (void)mmconfig_read_bool("wlan.ndp_probe_enabled", &scan_config.ndp_probe_enabled);
    (void)mmconfig_read_uint32("wlan.home_chan_dwell_time_ms",
                               &scan_config.home_channel_dwell_time_ms);
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

bool country_code_in_regulatory_domain(const char * code)
{
    if (mmwlan_lookup_regulatory_domain(get_regulatory_db(), code) == NULL)
    {
        return false;
    }
    return true;
}
