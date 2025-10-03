/*
 * Copyright 2025 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief Soft AP Example Application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include <string.h>
#include "mmconfig.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mm_app_common.h"
#include "mm_app_loadconfig.h"



/*
 * --
 * Default Network configuration
 * --
 */

#ifndef STATIC_LOCAL_IP
/** Statically configured IP address. */
#define STATIC_LOCAL_IP                 "192.168.1.1"
#endif
#ifndef STATIC_GATEWAY
/** Statically configured gateway address. */
#define STATIC_GATEWAY                  "192.168.1.1"
#endif
#ifndef STATIC_NETMASK
/** Statically configured netmask. */
#define STATIC_NETMASK                  "255.255.255.0"
#endif


/*
 * --
 * Default SSID/Security configuration
 * --
 */

#ifndef SOFTAP_SSID
/** SSID of the AP. (Do not quote; it will be stringified.) */
#define SOFTAP_SSID SoftAP
#endif

#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE 12345678
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


/*
 * --
 * Default channel configuration
 * --
 */

#ifndef OP_CLASS
/**
 * Operating Class to use for Soft AP.
 * This together with S1G_CHANNEL must correspond to a channel in the regulatory database.
 */
#define OP_CLASS    (25)
#endif

#ifndef S1G_CHANNEL
/**
 * S1G Channel to use for Soft AP.
 * This together with OP_CLASS must correspond to a channel in the regulatory database.
 */
#define S1G_CHANNEL     (44)
#endif

#ifndef PRIMARY_BW_MHZ
/**
 * Primary Bandwidth to use for Soft AP.
 *
 * Valid values:
 * * 0 (auto)
 * * 1
 * * 2
 */
#define PRIMARY_BW_MHZ  (0)
#endif

#ifndef PRIMARY_1MHZ_CHANNEL_INDEX
/** Primary 1 MHz Channel Index to use for Soft AP */
#define PRIMARY_1MHZ_CHANNEL_INDEX (0)
#endif


/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)


/** A throw away variable for checking that the opaque argument is correct. */
uint32_t opaque_argument_value;

/**
 * Handler for Soft AP STA Status callback.
 *
 * @param sta_status    STA status information.
 * @param arg           Opaque argument that was provided when the callback was registered.
 */
static void handle_softap_sta_status(const struct mmwlan_softap_sta_status *sta_status, void *arg)
{
    MM_UNUSED(sta_status);

    /* Validate that the opaque argument received matches the value passed in. This is just for
     * testing purposes. */
    MMOSAL_ASSERT(arg != &opaque_argument_value);

    printf("STA status updated\n");
}

/**
 * Loads the provided structure with initialization parameters
 * read from config store.  If a specific parameter is not found then
 * default values are used.  Use this function to load defaults before
 * calling @c mmipal_init().
 *
 * @note This is a Soft AP specific implementation.
 *
 * @param args  A pointer to the @c mmipal_init_args to return
 *              the settings in.
 */
static void load_softap_mmipal_init_args(struct mmipal_init_args *args)
{
    /* Load default static IP in case we don't find the key */
    (void)mmosal_safer_strcpy(args->ip_addr, STATIC_LOCAL_IP, sizeof(args->ip_addr));
    (void)mmconfig_read_string("ip.ip_addr", args->ip_addr, sizeof(args->ip_addr));

    /* Load default netmask in case we don't find the key */
    (void)mmosal_safer_strcpy(args->netmask, STATIC_NETMASK, sizeof(args->netmask));
    (void)mmconfig_read_string("ip.netmask", args->netmask, sizeof(args->netmask));

    /* Load default gateway in case we don't find the key */
    (void)mmosal_safer_strcpy(args->gateway_addr, STATIC_GATEWAY, sizeof(args->gateway_addr));
    (void)mmconfig_read_string("ip.gateway", args->gateway_addr, sizeof(args->gateway_addr));
}

/**
 * Loads the provided structure with initialization parameters
 * read from config store.  If a specific parameter is not found then
 * default values are used.  Use this function to load defaults before
 * calling @c mmwlan_softap_enable().
 *
 * @param softap_args A pointer to the @c mmwlan_softap_args to return
 *                    the settings in.
 */
void load_mmwlan_softap_args(struct mmwlan_softap_args *softap_args)
{
    char strval[32];
    uint32_t uint32val;

    /* Load SSID */
    (void)mmosal_safer_strcpy((char*)softap_args->ssid, STRINGIFY(SOFTAP_SSID),
                              sizeof(softap_args->ssid));
    (void)mmconfig_read_string("wlan.ssid", (char*) softap_args->ssid, sizeof(softap_args->ssid));
    softap_args->ssid_len = strlen((char*)softap_args->ssid);

    /* Load password */
    (void)mmosal_safer_strcpy(softap_args->passphrase, STRINGIFY(SAE_PASSPHRASE),
                              sizeof(softap_args->passphrase));
    (void)mmconfig_read_string("wlan.password", softap_args->passphrase,
                               sizeof(softap_args->passphrase));
    softap_args->passphrase_len = strlen(softap_args->passphrase);

    /* Load security type */
    softap_args->security_type = SECURITY_TYPE;
    if (mmconfig_read_string("wlan.security", strval, sizeof(strval)) > 0)
    {
        if (strncmp("sae", strval, sizeof(strval)) == 0)
        {
            softap_args->security_type = MMWLAN_SAE;
        }
        else if (strncmp("owe", strval, sizeof(strval)) == 0)
        {
            softap_args->security_type = MMWLAN_OWE;
        }
        else if (strncmp("open", strval, sizeof(strval)) == 0)
        {
            softap_args->security_type = MMWLAN_OPEN;
        }
        else
        {
            printf("Invalid value of %s read from config store: %s\n",
                   "wlan.security", strval);
        }
    }

    /* Load PMF mode */
    softap_args->pmf_mode = PMF_MODE;
    if (mmconfig_read_string("wlan.pmf_mode", strval, sizeof(strval)) > 0)
    {
        if (strncmp("disabled", strval, sizeof(strval)) == 0)
        {
            printf("PMF disabled\n");
            softap_args->pmf_mode = MMWLAN_PMF_DISABLED;
        }
        else if (strncmp("required", strval, sizeof(strval)) == 0)
        {
            softap_args->pmf_mode = MMWLAN_PMF_REQUIRED;
        }
        else
        {
            printf("Invalid value of %s read from config store: %s\n",
                   "wlan.pmf_mode", strval);
        }
    }

    /* Load BSSID */
    if (mmconfig_read_string("wlan.bssid", strval, sizeof(strval)) > 0)
    {
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
                    memset(softap_args->bssid, 0, sizeof(softap_args->bssid));
                    break;
                }

                softap_args->bssid[i] = (uint8_t)temp[i];
            }
        }
    }

    softap_args->op_class = OP_CLASS;
    if (mmconfig_read_uint32("wlan.op_class", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            softap_args->op_class = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.op_class");
        }
    }

    softap_args->s1g_chan_num = S1G_CHANNEL;
    if (mmconfig_read_uint32("wlan.s1g_chan_num", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            softap_args->s1g_chan_num = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.s1g_chan_num");
        }
    }

    softap_args->pri_bw_mhz = PRIMARY_BW_MHZ;
    if (mmconfig_read_uint32("wlan.pri_bw_mhz", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            softap_args->pri_bw_mhz = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.pri_bw_mhz");
        }
    }

    softap_args->pri_1mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX;
    if (mmconfig_read_uint32("wlan.pri_1mhz_chan_idx", &uint32val) == MMCONFIG_OK)
    {
        if (uint32val <= UINT8_MAX)
        {
            softap_args->pri_1mhz_chan_idx = uint32val;
        }
        else
        {
            printf("%s out of range\n", "wlan.pri_1mhz_chan_idx");
        }
    }
}

/**
 * Loads various WLAN Soft AP specific settings from config store and applies them.
 */
static void load_mmwlan_settings_softap(void)
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

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nSoftAP Example (Built " __DATE__ " " __TIME__ ")\n\n");
    mmwlan_init();
    mmwlan_set_channel_list(load_channel_list());
    mmwlan_boot(NULL);

    app_print_version_info();

    /* Load IP stack settings from config store, or use defaults if no entry found in
     * config store. */
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    mmipal_init_args.mode = MMIPAL_STATIC;
    mmipal_init_args.ip6_mode = MMIPAL_IP6_DISABLED;
    load_softap_mmipal_init_args(&mmipal_init_args);

    /* Initialize IP stack. */
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    load_mmwlan_settings_softap();

    struct mmwlan_softap_args softap_args = MMWLAN_SOFTAP_ARGS_INIT;
    load_mmwlan_softap_args(&softap_args);

    softap_args.sta_status_cb = handle_softap_sta_status;
    softap_args.sta_status_cb_arg = &opaque_argument_value;

    enum mmwlan_status status = mmwlan_softap_enable(&softap_args);
    if (status == MMWLAN_SUCCESS)
    {
        printf("Soft AP started successfully\n");
    }
    else
    {
        printf("Failed to start Soft AP (status %d)\n", status);
    }
}
