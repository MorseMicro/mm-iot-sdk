/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @section LOADCONFIG Helper functions for loading configuration
 *
 * This API provides a set of helper functions for loading configuration from @ref MMCONFIG.
 * @{
 */

#include "mmipal.h"
#include "mmwlan.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Looks up country code and returns appropriate channel list.
 *
 * @returns A pointer to the channel list to load.
 */
const struct mmwlan_s1g_channel_list *load_channel_list(void);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmipal_init().
 *
 * @param args  A pointer to the @c mmipal_init_args to return the settings in.
 */
void load_mmipal_init_args(struct mmipal_init_args *args);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmipal_init().
 *
 * @note This is a AP specific implementation.
 *
 * @param args  A pointer to the @c mmipal_init_args to return the settings in.
 */
void load_ap_mmipal_init_args(struct mmipal_init_args *args);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmwlan_sta_enable().
 *
 * @param sta_config A pointer to the @c mmwlan_sta_args to return the settings in.
 */
void load_mmwlan_sta_args(struct mmwlan_sta_args *sta_config);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmwlan_relay_ap_enable().
 *
 * @param relay_ap_config A pointer to the @c mmwlan_relay_ap_args to return the settings in.
 */
void load_mmwlan_relay_ap_args(struct mmwlan_relay_ap_args *relay_ap_config);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmwlan_ap_enable().
 *
 * @param ap_config A pointer to the @c mmwlan_ap_args to return the settings in.
 */
void load_mmwlan_ap_args(struct mmwlan_ap_args *ap_config);

/**
 * Loads the provided structure with initialization parameters read from config store.
 * If a specific parameter is not found then default values are used. Use this function to load
 * defaults before calling @c mmwlan_relay_enable().
 *
 * @param relay_config A pointer to the @c mmwlan_relay_args to return the settings in.
 */
void load_mmwlan_relay_args(struct mmwlan_relay_args *relay_config);

/**
 * Loads various WLAN-specific STA settings from config store and applies them.
 */
void load_mmwlan_sta_settings(void);

/**
 * Loads various WLAN-specific STA settings from config store and applies them.
 *
 * @deprecated This function is deprecated and provided for backwards compatibility.
 *             @ref load_mmwlan_sta_settings should be used for new developments.
 */
static inline void load_mmwlan_settings(void)
{
    load_mmwlan_sta_settings();
}

/**
 * Loads various WLAN-specific relay settings from config store and applies them.
 */
void load_mmwlan_relay_settings(void);

/**
 * Loads various WLAN-specific AP settings from config store and applies them.
 */
void load_mmwlan_ap_settings(void);

/**
 * Check whether the provided country code is within the current
 * regulatory database
 *
 * @param code  Country code to check
 * @return True if country code found in regulatory database
 */
bool country_code_in_regulatory_domain(const char *code);

#ifdef __cplusplus
}
#endif

/** @} */
