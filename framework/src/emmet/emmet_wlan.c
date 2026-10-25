/*
 * Copyright 2021-2024 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include <string.h>

#include "emmet-api.pb-c.h"
#include "emmet_private.h"
#include "mmwlan.h"
#include "mmutils.h"
#include "mmosal.h"
#include "mmipal.h"
#include "common/morse_commands.h"
#include "common/morse_command_utils.h"

void mmwlan_trigger_core_assert(uint32_t core_id);

static const struct mmwlan_regulatory_db *emmet_reg_db;

#define MAX_SCAN_RESULTS 9

#define TWT_WAKE_DURATION_UNIT_US (256)

struct scan_result
{
    struct scan_result *next;
    Emmet__StaScanResult sta_scan_result;
    uint8_t bssid[MMWLAN_MAC_ADDR_LEN];
    uint8_t ssid[MMWLAN_SSID_MAXLEN];
    uint8_t ies[];
};

struct scan_data
{
    struct scan_result *results;
    enum mmwlan_scan_state scan_state;
    bool scan_state_valid;
};

static struct scan_data scan_data;

static struct
{
    struct mmwlan_beacon_vendor_ie_filter filter;
    Emmet__BeaconVendorIeFilterStat stats[MMWLAN_BEACON_VENDOR_IE_MAX_OUI_FILTERS];
} emmet_beacon_vendor_ie_filter_data;

void emmet_beacon_vendor_ie_handler(const uint8_t *ies, uint32_t ies_len, void *arg)
{
    int ii;
    int offset;
    uint8_t length;
    Emmet__BeaconVendorIeFilterStat *current_stat;
    MM_UNUSED(arg);

    for (ii = 0; ii < emmet_beacon_vendor_ie_filter_data.filter.n_ouis; ii++)
    {
        offset = mm_find_vendor_specific_ie(ies,
                                            ies_len,
                                            emmet_beacon_vendor_ie_filter_data.filter.ouis[ii],
                                            MMWLAN_OUI_SIZE);
        if (offset > 0)
        {
            current_stat = &emmet_beacon_vendor_ie_filter_data.stats[ii];
            current_stat->occurrences++;
            length = ies[offset + 1];
            offset += 2;

            if (current_stat->ie.data && (current_stat->ie.len != length))
            {
                mmosal_free(current_stat->ie.data);
                current_stat->ie.data = NULL;
                current_stat->ie.len = 0;
            }

            if (!current_stat->ie.data)
            {
                current_stat->ie.data = (uint8_t *)mmosal_malloc(length);
                if (!current_stat->ie.data)
                {
                    MMLOG_ERR("Failed to alloc memory for OUI.");
                    continue;
                }
                current_stat->ie.len = length;
            }

            memcpy(current_stat->ie.data, (ies + offset), length);
        }
    }
}

static struct
{
    uint32_t paused_cb_count;
    uint32_t ready_cb_count;
} emmet_wlan_tx_flow_control_stats;

#define TX_FC_ARG ((void *)0x0badc0de)

static void emmet_wlan_tx_flow_control_handler(enum mmwlan_tx_flow_control_state state, void *arg)
{
    MMOSAL_DEV_ASSERT(arg == TX_FC_ARG);
    if (state == MMWLAN_TX_READY)
    {
        emmet_wlan_tx_flow_control_stats.ready_cb_count++;
    }
    else if (state == MMWLAN_TX_PAUSED)
    {
        emmet_wlan_tx_flow_control_stats.paused_cb_count++;
    }
    else
    {
        MMOSAL_ASSERT(false);
    }
}

void emmet_wlan_init(void)
{
    int ii;

    emmet_reg_db = NULL;

    emmet_beacon_vendor_ie_filter_data.filter.cb = emmet_beacon_vendor_ie_handler;
    for (ii = 0; ii < MMWLAN_BEACON_VENDOR_IE_MAX_OUI_FILTERS; ii++)
    {
        emmet__beacon_vendor_ie_filter_stat__init(&emmet_beacon_vendor_ie_filter_data.stats[ii]);
        emmet_beacon_vendor_ie_filter_data.stats[ii].oui.data =
            emmet_beacon_vendor_ie_filter_data.filter.ouis[ii];
        emmet_beacon_vendor_ie_filter_data.stats[ii].oui.len = MMWLAN_OUI_SIZE;
    }
}

void emmet_wlan_start(void)
{
    mmwlan_register_tx_flow_control_cb(emmet_wlan_tx_flow_control_handler, TX_FC_ARG);
}

extern const struct mmwlan_s1g_channel_list s1g_channel_list_au_80211_2020;
extern const struct mmwlan_s1g_channel_list s1g_channel_list_au_80211_2024;
extern const struct mmwlan_s1g_channel_list s1g_channel_list_au_80211_revmf;

enum mmwlan_status emmet_set_country_code(const char *country_code)
{
    const struct mmwlan_s1g_channel_list *channel_list = NULL;

    if (strnlen(country_code, MMWLAN_COUNTRY_CODE_LEN) < MMWLAN_COUNTRY_CODE_LEN)
    {
        channel_list = mmwlan_lookup_regulatory_domain(emmet_reg_db, country_code);
    }
    else
    {
        if (strcasecmp(country_code, "au_80211_2020") == 0)
        {
            channel_list = &s1g_channel_list_au_80211_2020;
        }
        else if (strcasecmp(country_code, "au_80211_2024") == 0)
        {
            channel_list = &s1g_channel_list_au_80211_2024;
        }
        else if (strcasecmp(country_code, "au_80211_revmf") == 0)
        {
            channel_list = &s1g_channel_list_au_80211_revmf;
        }
    }

    if (channel_list == NULL)
    {
        MMLOG_ERR("No regulatory domain matching code %s\n", country_code);
        return MMWLAN_INVALID_ARGUMENT;
    }
    return mmwlan_set_channel_list(channel_list);
}

enum mmwlan_status emmet_set_reg_db(const struct mmwlan_regulatory_db *reg_db)
{
    static enum mmwlan_status status = MMWLAN_SUCCESS;

    emmet_reg_db = reg_db;

    return status;
}

static void emmet_pack_wlanrsp(struct emmet_raw_rsp *raw_rsp, Emmet__WlanRsp *wlanrsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.wlan = wlanrsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_WLAN;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static Emmet__StatusCode convert_mmwlan_status_to_emmet_status_code(enum mmwlan_status status)
{
    switch (status)
    {
        case MMWLAN_SUCCESS:
            return EMMET__STATUS_CODE__SUCCESS;

        case MMWLAN_ERROR:
            return EMMET__STATUS_CODE__OTHER_ERROR;

        case MMWLAN_INVALID_ARGUMENT:
            return EMMET__STATUS_CODE__INVALID_ARGUMENT;

        case MMWLAN_UNAVAILABLE:
            return EMMET__STATUS_CODE__UNAVAILABLE;

        case MMWLAN_CHANNEL_LIST_NOT_SET:
            return EMMET__STATUS_CODE__CHANNEL_LIST_NOT_SET;

        case MMWLAN_NO_MEM:
            return EMMET__STATUS_CODE__NO_MEM;

        case MMWLAN_TIMED_OUT:
            return EMMET__STATUS_CODE__TIMED_OUT;

        case MMWLAN_SHUTDOWN_BLOCKED:
            return EMMET__STATUS_CODE__SHUTDOWN_BLOCKED;

        case MMWLAN_CHANNEL_INVALID:
            return EMMET__STATUS_CODE__CHANNEL_INVALID;

        case MMWLAN_NOT_FOUND:
            return EMMET__STATUS_CODE__NOT_FOUND;

        case MMWLAN_NOT_RUNNING:
            return EMMET__STATUS_CODE__NOT_RUNNING;

        case MMWLAN_NOT_INITIALIZED:
            return EMMET__STATUS_CODE__NOT_INITIALIZED;

        case MMWLAN_VIF_ERROR:
            return EMMET__STATUS_CODE__VIF_ERROR;

        case MMWLAN_NOT_SUPPORTED:
            return EMMET__STATUS_CODE__NOT_SUPPORTED;

        case MMWLAN_COMMAND_ERROR:
            return EMMET__STATUS_CODE__COMMAND_ERROR;

    }

    return EMMET__STATUS_CODE__OTHER_ERROR;
}

static void handle_set_mcs_override(struct emmet_raw_rsp *raw_rsp, Emmet__SetMcsOverrideCmd *cmd)
{
    enum mmwlan_status result = MMWLAN_INVALID_ARGUMENT;
    if ((cmd->mcs >= MMWLAN_MCS_NONE) && (cmd->mcs <= MMWLAN_MCS_MAX))
    {
        result = mmwlan_ate_override_rate_control((enum mmwlan_mcs)cmd->mcs,
                                                  MMWLAN_BW_NONE,
                                                  MMWLAN_GI_NONE);
    }

    if (result == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp,
                         EMMET__STATUS_CODE__INVALID_ARGUMENT,
                         "Selected MCS out of range");
}

static void handle_set_rts_threshold(struct emmet_raw_rsp *raw_rsp, Emmet__SetRtsThresholdCmd *cmd)
{
    enum mmwlan_status result = MMWLAN_ERROR;
    result = mmwlan_set_rts_threshold(cmd->threshold);

    if (result == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else if (result == MMWLAN_INVALID_ARGUMENT)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Selected RTS threshold out of range (min value %d)",
                             MMWLAN_MINIMUM_FRAGMENT_THRESHOLD);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_rts_threshold() failed");
    }
}

static void handle_set_country_code(struct emmet_raw_rsp *raw_rsp, Emmet__SetCountryCodeCmd *cmd)
{
    enum mmwlan_status result = emmet_set_country_code(cmd->country_code);
    switch (result)
    {
        case MMWLAN_SUCCESS:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
            break;

        case MMWLAN_INVALID_ARGUMENT:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Unsupported country code");
            break;

        case MMWLAN_UNAVAILABLE:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__UNAVAILABLE,
                                 "Not supported in current state");
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Unknown error");
            break;
    }
}

static void handle_set_tx_power_override(struct emmet_raw_rsp *raw_rsp,
                                         Emmet__SetTxPowerOverrideCmd *cmd)
{
    if (cmd->tx_power_override > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Unsupported tx power");
        return;
    }

    enum mmwlan_status result = mmwlan_override_max_tx_power(cmd->tx_power_override);
    switch (result)
    {
        case MMWLAN_SUCCESS:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
            break;

        case MMWLAN_INVALID_ARGUMENT:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Unsupported tx power");
            break;

        case MMWLAN_UNAVAILABLE:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__UNAVAILABLE,
                                 "Not supported in current state");
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Unknown error");
            break;
    }
}

static void handle_set_sgi_enabled(struct emmet_raw_rsp *raw_rsp, Emmet__SetSgiEnabledCmd *cmd)
{
    enum mmwlan_status status = mmwlan_set_sgi_enabled(cmd->sgi_enabled);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_set_sgi_enabled() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_subbands_enabled(struct emmet_raw_rsp *raw_rsp,
                                        Emmet__SetSubbandsEnabledCmd *cmd)
{
    enum mmwlan_status status = mmwlan_set_subbands_enabled(cmd->subbands_enabled);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_set_subbands_enabled() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_power_save_mode(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__SetPowerSaveModeCmd *cmd)
{
    enum mmwlan_ps_mode mode;

    switch (cmd->power_save_mode)
    {
        case EMMET__PWR_SAVE_MODE__PWR_SAVE_DISABLED:
            mode = MMWLAN_PS_DISABLED;
            break;

        case EMMET__PWR_SAVE_MODE__PWR_SAVE_ENABLED:
            mode = MMWLAN_PS_ENABLED;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid Power Save Mode");
            return;
    }

    enum mmwlan_status status = mmwlan_set_power_save_mode(mode);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_set_power_save_mode() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_ampdu_enabled(struct emmet_raw_rsp *raw_rsp, Emmet__SetAmpduEnabledCmd *cmd)
{
    enum mmwlan_status status = mmwlan_set_ampdu_enabled(cmd->ampdu_enabled);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_set_ampdu_enabled() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_wnm_sleep_enabled(struct emmet_raw_rsp *raw_rsp,
                                         Emmet__SetWNMSleepEnabledCmd *cmd)
{
    struct mmwlan_set_wnm_sleep_enabled_args wnm_sleep_args =
        MMWLAN_SET_WNM_SLEEP_ENABLED_ARGS_INIT;
    wnm_sleep_args.wnm_sleep_enabled = cmd->wnm_sleep_enabled;
    wnm_sleep_args.chip_powerdown_enabled = cmd->chip_power_down_enabled;
    enum mmwlan_status status = mmwlan_set_wnm_sleep_enabled_ext(&wnm_sleep_args);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_set_wnm_sleep_enabled_ext() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_fragment_threshold(struct emmet_raw_rsp *raw_rsp,
                                          Emmet__SetFragmentThresholdCmd *cmd)
{
    enum mmwlan_status result = MMWLAN_ERROR;
    result = mmwlan_set_fragment_threshold(cmd->threshold);

    if (result == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp,
                         EMMET__STATUS_CODE__INVALID_ARGUMENT,
                         "Selected Fragment threshold out of range");
}

static void handle_set_scan_config(struct emmet_raw_rsp *raw_rsp, Emmet__SetScanConfigCmd *cmd)
{
    enum mmwlan_status result;

    struct mmwlan_scan_config config = {
        .dwell_time_ms = cmd->dwell_time_ms,
        .ndp_probe_enabled = cmd->ndp_probe_enabled,
    };

    result = mmwlan_set_scan_config(&config);

    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_scan_config() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_health_check_interval(struct emmet_raw_rsp *raw_rsp,
                                             Emmet__SetHealthCheckIntervalCmd *cmd)
{
    enum mmwlan_status result;

    result = mmwlan_set_health_check_interval(cmd->min_health_check_intvl_ms,
                                              cmd->max_health_check_intvl_ms);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_health_check_interval() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_default_qos_queue_params(struct emmet_raw_rsp *raw_rsp,
                                                Emmet__SetDefaultQosQueueParamsCmd *cmd)
{
    enum mmwlan_status result;

    struct mmwlan_qos_queue_params params;
    params.aci = cmd->aci;
    params.aifs = cmd->aifs;
    params.cw_min = cmd->cw_min;
    params.cw_max = cmd->cw_max;
    params.txop_max_us = cmd->txop_max_us;

    result = mmwlan_set_default_qos_queue_params(&params, 1);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_default_qos_queue_params() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_mcs10_mode(struct emmet_raw_rsp *raw_rsp, Emmet__SetMCS10ModeCmd *cmd)
{
    enum mmwlan_status result;

    enum mmwlan_mcs10_mode mode = (enum mmwlan_mcs10_mode)cmd->mode;

    result = mmwlan_set_mcs10_mode(mode);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_mcs10_mode() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_duty_cycle_mode(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__SetDutyCycleModeCmd *cmd)
{
    enum mmwlan_status result;

    enum mmwlan_duty_cycle_mode mode;
    switch (cmd->mode)
    {
        case EMMET__DUTY_CYCLE_MODE__DUTY_CYCLE_MODE_SPREAD:
            mode = MMWLAN_DUTY_CYCLE_MODE_SPREAD;
            break;

        case EMMET__DUTY_CYCLE_MODE__DUTY_CYCLE_MODE_BURST:
            mode = MMWLAN_DUTY_CYCLE_MODE_BURST;
            break;

        default:
            result = MMWLAN_INVALID_ARGUMENT;
            goto exit;
    }

    result = mmwlan_set_duty_cycle_mode(mode);

exit:
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_duty_cycle_mode() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_set_non_tim_mode_enabled(struct emmet_raw_rsp *raw_rsp,
                                            Emmet__SetNonTimModeEnabledCmd *cmd)
{
    enum mmwlan_status result;

    result = mmwlan_set_non_tim_mode_enabled(cmd->non_tim_mode_enabled);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(result),
                             "mmwlan_set_non_tim_mode_enabled() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_get_duty_cycle_stats(struct emmet_raw_rsp *raw_rsp,
                                        Emmet__GetDutyCycleStatsCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_duty_cycle_stats stats;
    enum mmwlan_status status = mmwlan_get_duty_cycle_stats(&stats);
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetDutyCycleStatsRsp get_duty_cycle_stats = EMMET__GET_DUTY_CYCLE_STATS_RSP__INIT;

    if (status != MMWLAN_SUCCESS)
    {
        goto error;
    }

    get_duty_cycle_stats.duty_cycle = stats.duty_cycle;
    switch (stats.mode)
    {
        case MMWLAN_DUTY_CYCLE_MODE_SPREAD:
            get_duty_cycle_stats.mode = EMMET__DUTY_CYCLE_MODE__DUTY_CYCLE_MODE_SPREAD;
            break;

        case MMWLAN_DUTY_CYCLE_MODE_BURST:
            get_duty_cycle_stats.mode = EMMET__DUTY_CYCLE_MODE__DUTY_CYCLE_MODE_BURST;
            break;

        default:
            status = MMWLAN_ERROR;
            goto error;
    }
    get_duty_cycle_stats.burst_airtime_remaining_us = stats.burst_airtime_remaining_us;
    get_duty_cycle_stats.burst_window_duration_us = stats.burst_window_duration_us;
    rsp.get_duty_cycle_stats = &get_duty_cycle_stats;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_DUTY_CYCLE_STATS;
    emmet_pack_wlanrsp(raw_rsp, &rsp);
    return;

error:
    emmet_pack_basic_rsp(raw_rsp,
                         convert_mmwlan_status_to_emmet_status_code(status),
                         " mmwlan_get_duty_cycle_stats() failed");
}

static void emmet_sta_status_callback(enum mmwlan_sta_state sta_state)
{
    switch (sta_state)
    {
        case MMWLAN_STA_DISABLED:
            MMLOG_APP("WLAN STA disabled\n");
            emmet_hal_set_led(EMMET_LED_RED, 255);
            emmet_hal_set_led(EMMET_LED_GREEN, 0);
            emmet_hal_set_led(EMMET_LED_BLUE, 0);
            break;

        case MMWLAN_STA_CONNECTING:
            MMLOG_APP("WLAN STA connecting\n");
            emmet_hal_set_led(EMMET_LED_RED, 0);
            emmet_hal_set_led(EMMET_LED_GREEN, 0);
            emmet_hal_set_led(EMMET_LED_BLUE, 255);
            break;

        case MMWLAN_STA_CONNECTED:
            MMLOG_APP("WLAN STA connected\n");
            emmet_hal_set_led(EMMET_LED_RED, 0);
            emmet_hal_set_led(EMMET_LED_GREEN, 255);
            emmet_hal_set_led(EMMET_LED_BLUE, 0);
            break;
    }
}

static bool prepare_sta_args(struct emmet_raw_rsp *raw_rsp,
                             struct mmwlan_sta_args *args,
                             const Emmet__StaEnableCmd *cmd)
{
    enum mmwlan_status status = MMWLAN_ERROR;

    if (cmd->ssid.len < 1 || cmd->ssid.len >= MMWLAN_SSID_MAXLEN)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid SSID length");
        return false;
    }

    if (cmd->raw_sta_priority < -1 || cmd->raw_sta_priority > MMWLAN_RAW_MAX_PRIORITY)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid RAW Priority");
        return false;
    }

    memcpy(args->ssid, cmd->ssid.data, cmd->ssid.len);
    args->ssid_len = cmd->ssid.len;
    memcpy(args->passphrase, cmd->passphrase.data, cmd->passphrase.len);
    args->passphrase_len = cmd->passphrase.len;
    args->raw_sta_priority = cmd->raw_sta_priority;
    args->scan_interval_base_s = cmd->scan_interval_base_s;
    args->scan_interval_limit_s = cmd->scan_interval_limit_s;
    args->use_4addr = cmd->use_4addr ? MMWLAN_4ADDR_MODE_ENABLED : MMWLAN_4ADDR_MODE_DISABLED;

    if (cmd->bssid.len == MMWLAN_MAC_ADDR_LEN)
    {
        memcpy(args->bssid, cmd->bssid.data, MMWLAN_MAC_ADDR_LEN);
    }
    else if (cmd->bssid.len != 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid BSSID length");
        return false;
    }

    switch (cmd->sta_type)
    {
        case 0:
            args->sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
            break;

        case 1:
            args->sta_type = MMWLAN_STA_TYPE_SENSOR;
            break;

        case 2:
            args->sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid sta type %lu",
                                 cmd->sta_type);
            return false;
    }

    switch (cmd->security)
    {
        case EMMET__WLAN_SECURITY_TYPE__NO_SECURITY:
            args->security_type = MMWLAN_OPEN;
            break;

        case EMMET__WLAN_SECURITY_TYPE__OWE_SECURITY:
            args->security_type = MMWLAN_OWE;
            break;

        case EMMET__WLAN_SECURITY_TYPE__SAE_SECURITY:
            args->security_type = MMWLAN_SAE;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid security type");
            return false;
    }

    switch (cmd->pmf_mode)
    {
        case EMMET__WLAN_PMF_MODE__PMF_REQUIRED:
            args->pmf_mode = MMWLAN_PMF_REQUIRED;
            break;

        case EMMET__WLAN_PMF_MODE__PMF_DISABLED:
            args->pmf_mode = MMWLAN_PMF_DISABLED;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid PMF mode");
            break;
    }

    if (cmd->n_sae_owe_ec_groups)
    {
        if (cmd->n_sae_owe_ec_groups >= MMWLAN_MAX_EC_GROUPS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "n_group_ids exceeds MMWLAN_MAX_EC_GROUPS");
            return false;
        }

        for (uint16_t i = 0; i < cmd->n_sae_owe_ec_groups; i++)
        {
            args->sae_owe_ec_groups[i] = cmd->sae_owe_ec_groups[i];
        }
    }

    args->cac_mode = cmd->cac_enabled ? MMWLAN_CAC_ENABLED : MMWLAN_CAC_DISABLED;

    if (cmd->bgscan_short_interval_s == 0 || cmd->bgscan_long_interval_s == 0)
    {
        args->bgscan_short_interval_s = 0;
        args->bgscan_signal_threshold_dbm = 0;
        args->bgscan_long_interval_s = 0;
        MMLOG_DBG("Background scanning disabled\n");
    }
    else
    {
        args->bgscan_short_interval_s = cmd->bgscan_short_interval_s;
        args->bgscan_signal_threshold_dbm = cmd->bgscan_signal_threshold_dbm;
        args->bgscan_long_interval_s = cmd->bgscan_long_interval_s;
    }

    if (cmd->twt_enabled)
    {
        struct mmwlan_twt_config_args twt_config_args = MMWLAN_TWT_CONFIG_ARGS_INIT;

        twt_config_args.twt_wake_interval_us = cmd->twt_wake_interval_us;
        if (cmd->twt_min_wake_duration_us > UINT8_MAX * TWT_WAKE_DURATION_UNIT_US)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid TWT wake duration");
            return false;
        }
        twt_config_args.twt_min_wake_duration_us = cmd->twt_min_wake_duration_us;
        switch (cmd->twt_setup_command)
        {
            case EMMET__TWT_SETUP_COMMAND__TWT_REQUEST:
                twt_config_args.twt_setup_command = MMWLAN_TWT_SETUP_REQUEST;
                break;

            case EMMET__TWT_SETUP_COMMAND__TWT_SUGGEST:
                twt_config_args.twt_setup_command = MMWLAN_TWT_SETUP_SUGGEST;
                break;

            case EMMET__TWT_SETUP_COMMAND__TWT_DEMAND:
                twt_config_args.twt_setup_command = MMWLAN_TWT_SETUP_DEMAND;
                break;

            default:
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                     "Invalid TWT setup command");
                return false;
        }
        twt_config_args.twt_mode = MMWLAN_TWT_REQUESTER;

        MMLOG_INF("TWT configurations: twt_enabled=%u, twt_wake_interval_us=0x" MM_X64_FMT ", "
                  "twt_min_wake_duration_us=%lu, twt_setup_command=%lu\n",
                  twt_config_args.twt_mode,
                  MM_X64_VAL(twt_config_args.twt_wake_interval_us),
                  twt_config_args.twt_min_wake_duration_us,
                  twt_config_args.twt_setup_command);

        status = mmwlan_twt_add_configuration(&twt_config_args);
        if (status != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 convert_mmwlan_status_to_emmet_status_code(status),
                                 "mmwlan_twt_add_configuration() failed");
            return false;
        }
    }

    if (cmd->selected_channels.len)
    {
        if (cmd->selected_channels.len > UINT8_MAX || cmd->selective_scan_attempts > UINT8_MAX)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid selective scan arguments");
            return false;
        }

        struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
        scan_config.selected_channels = cmd->selected_channels.data;
        scan_config.selected_channels_len = (uint8_t)cmd->selected_channels.len;
        scan_config.selective_scan_attempts = (uint8_t)cmd->selective_scan_attempts;

        status = mmwlan_set_scan_config(&scan_config);
        if (status != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 convert_mmwlan_status_to_emmet_status_code(status),
                                 "mmwlan_set_scan_config() failed");
            return false;
        }
    }

    MMLOG_INF("Enable STA (SSID=%s, security=%u, passphrase=%s, pmf=%u, raw_sta_priority=%d), "
              "cac_enabled=%u, bgscan_short_interval_s=%u, bgscan_signal_threshold_dbm=%d, "
              "bgscan_long_interval_s=%u\n",
              args->ssid,
              args->security_type,
              args->passphrase,
              args->pmf_mode,
              args->raw_sta_priority,
              args->cac_mode,
              args->bgscan_short_interval_s,
              args->bgscan_signal_threshold_dbm,
              args->bgscan_long_interval_s);

    return true;
}

static void handle_sta_enable(struct emmet_raw_rsp *raw_rsp, Emmet__StaEnableCmd *cmd)
{
    struct mmwlan_sta_args args = MMWLAN_STA_ARGS_INIT;
    if (!prepare_sta_args(raw_rsp, &args, cmd))
    {
        return;
    }

    emmet_hal_set_led(EMMET_LED_RED, 255);
    emmet_hal_set_led(EMMET_LED_BLUE, 255);

    enum mmwlan_status status = mmwlan_sta_enable(&args, emmet_sta_status_callback);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_sta_enable() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_disable(struct emmet_raw_rsp *raw_rsp, Emmet__StaDisableCmd *cmd)
{
    MM_UNUSED(cmd);
    mmwlan_sta_disable();
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_get_state(struct emmet_raw_rsp *raw_rsp, Emmet__StaGetStateCmd *cmd)
{
    MM_UNUSED(cmd);

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__StaGetStateRsp sta_get_state_rsp = EMMET__STA_GET_STATE_RSP__INIT;
    rsp.sta_get_state = &sta_get_state_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_STA_GET_STATE;

    switch (mmwlan_get_sta_state())
    {
        case MMWLAN_STA_DISABLED:
            rsp.sta_get_state->state = EMMET__STA_STATE__STA_DISABLED;
            break;

        case MMWLAN_STA_CONNECTING:
            rsp.sta_get_state->state = EMMET__STA_STATE__STA_CONNECTING;
            break;

        case MMWLAN_STA_CONNECTED:
            rsp.sta_get_state->state = EMMET__STA_STATE__STA_CONNECTED;
            break;
    }

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_get_mac_address(struct emmet_raw_rsp *raw_rsp, Emmet__GetMacAddressCmd *cmd)
{
    MM_UNUSED(cmd);
    uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN];

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetMacAddressRsp get_mac_address_rsp = EMMET__GET_MAC_ADDRESS_RSP__INIT;
    rsp.get_mac_address = &get_mac_address_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_MAC_ADDRESS;

    enum mmwlan_status status = MMWLAN_INVALID_ARGUMENT;

    switch (cmd->vif)
    {
        case EMMET__WLAN_VIF__VIF_UNSPECIFIED:
            status = mmwlan_get_mac_addr(mac_addr);
            break;

        case EMMET__WLAN_VIF__VIF_STA:
            status = mmwlan_get_vif_mac_addr(MMWLAN_VIF_STA, mac_addr);
            break;

        case EMMET__WLAN_VIF__VIF_AP:
            status = mmwlan_get_vif_mac_addr(MMWLAN_VIF_AP, mac_addr);
            break;

        default:
            break;
    }

    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_get_mac_addr() failed");
        return;
    }

    rsp.get_mac_address->mac_address.data = mac_addr;
    rsp.get_mac_address->mac_address.len = MMWLAN_MAC_ADDR_LEN;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_get_bssid(struct emmet_raw_rsp *raw_rsp, Emmet__GetBssidCmd *cmd)
{
    MM_UNUSED(cmd);
    uint8_t bssid[MMWLAN_MAC_ADDR_LEN];

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetBssidRsp get_bssid_rsp = EMMET__GET_BSSID_RSP__INIT;
    rsp.get_bssid = &get_bssid_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_BSSID;

    enum mmwlan_status status = mmwlan_get_bssid(bssid);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_get_bssid() failed");
        return;
    }

    rsp.get_bssid->bssid.data = bssid;
    rsp.get_bssid->bssid.len = MMWLAN_MAC_ADDR_LEN;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_get_vif_channel_info(struct emmet_raw_rsp *raw_rsp,
                                        Emmet__GetVifChannelInfoCmd *cmd)
{
    struct mmwlan_vif_channel_info info = { 0 };
    enum mmwlan_vif vif = MMWLAN_VIF_STA;

    if (cmd != NULL)
    {
        switch (cmd->vif)
        {
            case EMMET__WLAN_VIF__VIF_UNSPECIFIED:
            case EMMET__WLAN_VIF__VIF_STA:
                vif = MMWLAN_VIF_STA;
                break;

            case EMMET__WLAN_VIF__VIF_AP:
                vif = MMWLAN_VIF_AP;
                break;

            default:
                emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid VIF");
                return;
        }
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetVifChannelInfoRsp get_vif_channel_info_rsp = EMMET__GET_VIF_CHANNEL_INFO_RSP__INIT;
    rsp.get_vif_channel_info = &get_vif_channel_info_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_VIF_CHANNEL_INFO;

    enum mmwlan_status status = mmwlan_get_vif_channel_info(vif, &info);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_get_vif_channel_info() failed");
        return;
    }

    rsp.get_vif_channel_info->op_class = info.op_class;
    rsp.get_vif_channel_info->s1g_chan_num = info.s1g_chan_num;
    rsp.get_vif_channel_info->pri_bw_mhz = info.pri_bw_mhz;
    rsp.get_vif_channel_info->pri_1mhz_chan_idx = info.pri_1mhz_chan_idx;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_boot(struct emmet_raw_rsp *raw_rsp, Emmet__BootCmd *cmd)
{
    MM_UNUSED(cmd);

    enum mmwlan_status status = mmwlan_boot(NULL);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_boot() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, "Successful");
}

static void handle_shutdown(struct emmet_raw_rsp *raw_rsp, Emmet__ShutdownCmd *cmd)
{
    MM_UNUSED(cmd);

    enum mmwlan_status status = mmwlan_shutdown();
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_shutdown() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, "Successful");
}

static void handle_get_version(struct emmet_raw_rsp *raw_rsp, Emmet__GetVersionCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_version version;
    enum mmwlan_status status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_get_version() failed");
        return;
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetVersionRsp get_version_rsp = EMMET__GET_VERSION_RSP__INIT;
    rsp.get_version = &get_version_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_VERSION;

    rsp.get_version->morse_fw_version = version.morse_fw_version;
    rsp.get_version->morselib_version = version.morselib_version;
    rsp.get_version->morse_chip_id = version.morse_chip_id;
    rsp.get_version->morse_chip_id_string = version.morse_chip_id_string;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_get_bcf_metadata(struct emmet_raw_rsp *raw_rsp, Emmet__GetBcfMetadataCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_bcf_metadata bcf_metadata;

    enum mmwlan_status status = mmwlan_get_bcf_metadata(&bcf_metadata);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_get_bcf_metadata() failed");
        return;
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetBcfMetadataRsp get_bcf_metadata_rsp = EMMET__GET_BCF_METADATA_RSP__INIT;
    rsp.get_bcf_metadata = &get_bcf_metadata_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_BCF_METADATA;

    rsp.get_bcf_metadata->version_major = bcf_metadata.version.major;
    rsp.get_bcf_metadata->version_minor = bcf_metadata.version.minor;
    rsp.get_bcf_metadata->version_patch = bcf_metadata.version.patch;
    rsp.get_bcf_metadata->board_desc = bcf_metadata.board_desc;
    rsp.get_bcf_metadata->build_version = bcf_metadata.build_version;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static int get_num_scan_results(void)
{
    int num = 0;
    struct scan_result *ptr = scan_data.results;

    while (ptr != NULL)
    {
        num++;
        ptr = ptr->next;
    }
    return num;
}

static int emmet_alloc_and_fill_scan_result(struct scan_result *scan_result_ptr,
                                            const struct mmwlan_scan_result *result)
{
    scan_result_ptr =
        (struct scan_result *)mmosal_malloc((sizeof(struct scan_result) + result->ies_len));
    if (scan_result_ptr == NULL)
    {
        MMLOG_WRN("Failed to allocate buffer for scan data\n");
        return -1;
    }

    emmet__sta_scan_result__init(&scan_result_ptr->sta_scan_result);
    scan_result_ptr->next = NULL;
    scan_result_ptr->sta_scan_result.bssid.data = scan_result_ptr->bssid;
    scan_result_ptr->sta_scan_result.ssid.data = scan_result_ptr->ssid;
    scan_result_ptr->sta_scan_result.ies.data = scan_result_ptr->ies;

    scan_result_ptr->sta_scan_result.rssi = result->rssi;
    scan_result_ptr->sta_scan_result.bssid.len = MMWLAN_MAC_ADDR_LEN;
    memcpy(scan_result_ptr->sta_scan_result.bssid.data, result->bssid, MMWLAN_MAC_ADDR_LEN);
    scan_result_ptr->sta_scan_result.beacon_interval = result->beacon_interval;
    scan_result_ptr->sta_scan_result.capability_info = result->capability_info;
    scan_result_ptr->sta_scan_result.ssid.len = result->ssid_len;
    memcpy(scan_result_ptr->sta_scan_result.ssid.data, result->ssid, result->ssid_len);
    scan_result_ptr->sta_scan_result.ies.len = result->ies_len;
    scan_result_ptr->sta_scan_result.noise_dbm = result->noise_dbm;
    scan_result_ptr->sta_scan_result.channel_freq_hz = result->channel_freq_hz;
    scan_result_ptr->sta_scan_result.bw_mhz = result->bw_mhz;
    scan_result_ptr->sta_scan_result.op_bw_mhz = result->op_bw_mhz;
    if (result->ies_len)
    {
        memcpy(scan_result_ptr->ies, result->ies, result->ies_len);
    }

    if (scan_data.results == NULL)
    {
        scan_data.results = scan_result_ptr;
    }
    else
    {
        struct scan_result *ptr = scan_data.results;
        while (ptr->next != NULL)
        {
            ptr = ptr->next;
        }
        ptr->next = scan_result_ptr;
    }
    return 0;
}

static void emmet_clear_scan_results(void)
{
    struct scan_result *scan_result_ptr = scan_data.results;
    struct scan_result *ptr;

    while (scan_result_ptr != NULL)
    {
        ptr = scan_result_ptr->next;
        mmosal_free(scan_result_ptr);
        scan_result_ptr = ptr;
    }
    scan_data.results = NULL;
    scan_data.scan_state_valid = false;
}

static void emmet_scan_rx_callback(const struct mmwlan_scan_result *result, void *arg)
{
    MM_UNUSED(arg);
    char bssid[MM_MAC_ADDR_STR_LEN];
    mm_mac_addr_to_str((uint8_t *)result->bssid, bssid);
    MMLOG_INF("SSID: %s, BSSID: %s, RSSI: %d\n", result->ssid, bssid, result->rssi);

    int ret = 0;
    struct scan_result *scan_result_ptr = scan_data.results;

    int num_scan_results = get_num_scan_results();
    if (num_scan_results >= MAX_SCAN_RESULTS)
    {
        MMLOG_WRN("Cannot store more than %d scan results\n", MAX_SCAN_RESULTS);
    }
    else
    {
        for (int i = 0; i < num_scan_results; i++)
        {
            scan_result_ptr = scan_result_ptr->next;
        }
        ret = emmet_alloc_and_fill_scan_result(scan_result_ptr, result);
        if (ret != 0)
        {
            return;
        }
    }
}

static void emmet_scan_complete_callback(enum mmwlan_scan_state state, void *arg)
{
    MM_UNUSED(arg);
    scan_data.scan_state = state;
    scan_data.scan_state_valid = true;
    MMLOG_INF("Scan complete: scan_state = %u\n", scan_data.scan_state);
}

static void handle_sta_scan_request(struct emmet_raw_rsp *raw_rsp, Emmet__StaScanRequestCmd *cmd)
{
    if (scan_data.scan_state_valid && scan_data.scan_state == MMWLAN_SCAN_RUNNING)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, NULL);
        return;
    }

    emmet_clear_scan_results();

    struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_rx_cb = emmet_scan_rx_callback;
    scan_req.scan_complete_cb = emmet_scan_complete_callback;

    if (cmd->ssid.len >= MMWLAN_SSID_MAXLEN)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid SSID length");
        return;
    }

    if (cmd->ssid.len)
    {
        memcpy(scan_req.args.ssid, cmd->ssid.data, cmd->ssid.len);
        scan_req.args.ssid_len = cmd->ssid.len;
    }

    if (cmd->dwell_time_ms)
    {
        scan_req.args.dwell_time_ms = cmd->dwell_time_ms;
    }

    scan_req.args.dwell_on_home_ms = cmd->dwell_on_home_ms;

    if (cmd->extra_ies.len)
    {
        scan_req.args.extra_ies = (uint8_t *)mmosal_malloc(cmd->extra_ies.len);
        if (scan_req.args.extra_ies == NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE, NULL);
            return;
        }
        memcpy(scan_req.args.extra_ies, cmd->extra_ies.data, cmd->extra_ies.len);
        scan_req.args.extra_ies_len = cmd->extra_ies.len;
    }

    if (cmd->selected_channels.len)
    {
        if (cmd->selected_channels.len > UINT8_MAX)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid selected channels length");
            goto exit;
        }

        scan_req.args.selected_channels = cmd->selected_channels.data;
        scan_req.args.selected_channels_len = (uint8_t)cmd->selected_channels.len;
    }

    enum mmwlan_status status = mmwlan_scan_request(&scan_req);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_scan_request() failed");
        goto exit;
    }

    scan_data.scan_state = MMWLAN_SCAN_RUNNING;
    scan_data.scan_state_valid = true;
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);

exit:
    mmosal_free(scan_req.args.extra_ies);
}

static void handle_sta_scan_abort(struct emmet_raw_rsp *raw_rsp, Emmet__StaScanAbortCmd *cmd)
{
    MM_UNUSED(cmd);

    if (!(scan_data.scan_state_valid && scan_data.scan_state == MMWLAN_SCAN_RUNNING))
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, NULL);
        return;
    }

    enum mmwlan_status status = mmwlan_scan_abort();
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "mmwlan_scan_abort() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_scan_results(struct emmet_raw_rsp *raw_rsp, Emmet__StaScanResultsCmd *cmd)
{
    MM_UNUSED(cmd);
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__StaScanResultsRsp sta_scan_results_rsp = EMMET__STA_SCAN_RESULTS_RSP__INIT;
    rsp.sta_scan_results = &sta_scan_results_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_STA_SCAN_RESULTS;

    if (!scan_data.scan_state_valid)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, NULL);
        return;
    }

    switch (scan_data.scan_state)
    {
        case MMWLAN_SCAN_SUCCESSFUL:
            sta_scan_results_rsp.state = EMMET__STA_SCAN_STATE__SCAN_COMPLETED;
            break;
        case MMWLAN_SCAN_TERMINATED:
            sta_scan_results_rsp.state = EMMET__STA_SCAN_STATE__SCAN_TERMINATED;
            break;
        case MMWLAN_SCAN_RUNNING:
            sta_scan_results_rsp.state = EMMET__STA_SCAN_STATE__SCAN_RUNNING;
            break;
        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, NULL);
            return;
    }

    if (scan_data.scan_state == MMWLAN_SCAN_RUNNING)
    {
        emmet_pack_wlanrsp(raw_rsp, &rsp);
        return;
    }

    int num_scan_results = get_num_scan_results();
    if (num_scan_results)
    {
        sta_scan_results_rsp.n_scan_results = num_scan_results;

        Emmet__StaScanResult **res_table =
            (Emmet__StaScanResult **)mmosal_malloc(sizeof(*res_table) * num_scan_results);
        if (res_table == NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE, NULL);
            return;
        }
        struct scan_result *scan_result_ptr = scan_data.results;
        for (int i = 0; i < num_scan_results; i++)
        {
            res_table[i] = &scan_result_ptr->sta_scan_result;
            scan_result_ptr = scan_result_ptr->next;
        }
        sta_scan_results_rsp.scan_results = res_table;
        emmet_pack_wlanrsp(raw_rsp, &rsp);
        mmosal_free(res_table);
    }
    else
    {
        emmet_pack_wlanrsp(raw_rsp, &rsp);
    }

    emmet_clear_scan_results();
}

static void handle_get_rssi(struct emmet_raw_rsp *raw_rsp, Emmet__GetRssiCmd *cmd)
{
    MM_UNUSED(cmd);

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetRssiRsp get_rssi_rsp = EMMET__GET_RSSI_RSP__INIT;
    rsp.get_rssi = &get_rssi_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_RSSI;

    rsp.get_rssi->rssi = mmwlan_get_rssi();

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_get_rc_stats(struct emmet_raw_rsp *raw_rsp, Emmet__GetRcStatsCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_rc_stats *stats = mmwlan_get_rc_stats();
    if (stats == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Unable to retrieve RC stats.");
        return;
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetRcStatsRsp get_rc_stats_rsp = EMMET__GET_RC_STATS_RSP__INIT;
    rsp.get_rc_stats = &get_rc_stats_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_RC_STATS;

    get_rc_stats_rsp.n_rate_info = stats->n_entries;
    get_rc_stats_rsp.rate_info = stats->rate_info;

    get_rc_stats_rsp.n_total_sent = stats->n_entries;
    get_rc_stats_rsp.total_sent = stats->total_sent;

    get_rc_stats_rsp.n_total_success = stats->n_entries;
    get_rc_stats_rsp.total_success = stats->total_success;

    emmet_pack_wlanrsp(raw_rsp, &rsp);

    mmwlan_free_rc_stats(stats);
}

static void handle_get_morse_stats(struct emmet_raw_rsp *raw_rsp, Emmet__GetMorseStatsCmd *cmd)
{
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetMorseStatsRsp get_morse_stats_rsp = EMMET__GET_MORSE_STATS_RSP__INIT;
    rsp.get_morse_stats = &get_morse_stats_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_MORSE_STATS;

    struct mmwlan_morse_stats *stats = mmwlan_get_morse_stats(cmd->core_num, cmd->reset);
    if (stats == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Unable to retrieve Morse stats.");
        return;
    }

    get_morse_stats_rsp.stats.data = stats->buf;
    get_morse_stats_rsp.stats.len = stats->len;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
    mmwlan_free_morse_stats(stats);
}

static void handle_trigger_core_assert(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__TriggerCoreAssertCmd *cmd)
{
    switch (cmd->core_num)
    {
        case 0:
            MMLOG_INF("Asserting APP_CORE\n");
            break;

        case 1:
            MMLOG_INF("Asserting MAC_CORE\n");
            break;

        case 2:
            MMLOG_INF("Asserting UPHY_CORE\n");
            break;

        case 3:
            MMLOG_INF("Asserting LPHY_CORE\n");
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid Core");
            return;
    }

    mmwlan_trigger_core_assert(cmd->core_num);

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

#define TEST_COMMAND_RESPONSE_BUFFER_LEN (512)

static void handle_execute_test_command(struct emmet_raw_rsp *raw_rsp,
                                        Emmet__ExecuteTestCommandCmd *cmd)
{
    uint8_t *response_buffer = (uint8_t *)mmosal_malloc(TEST_COMMAND_RESPONSE_BUFFER_LEN);
    if (response_buffer == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE,
                             "Unable to allocate for response");
        return;
    }

    memset(response_buffer, 0, TEST_COMMAND_RESPONSE_BUFFER_LEN);

    uint32_t response_len = TEST_COMMAND_RESPONSE_BUFFER_LEN;
    MMLOG_DUMP_INF("Execute cmd:", cmd->command.data, cmd->command.len);
    enum mmwlan_status status = mmwlan_ate_execute_command(cmd->command.data,
                                                           cmd->command.len,
                                                           response_buffer,
                                                           &response_len);

    if ((status != MMWLAN_SUCCESS) && (status != MMWLAN_COMMAND_ERROR))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Failed to execute command");
        MMLOG_INF("Command failed\n");
    }
    else
    {
        Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
        Emmet__ExecuteTestCommandRsp exec_rsp = EMMET__EXECUTE_TEST_COMMAND_RSP__INIT;
        rsp.execute_test_command = &exec_rsp;
        rsp.rsp_case = EMMET__WLAN_RSP__RSP_EXECUTE_TEST_COMMAND;
        exec_rsp.response.data = response_buffer;
        exec_rsp.response.len = response_len;
        emmet_pack_wlanrsp(raw_rsp, &rsp);
        MMLOG_DUMP_INF("Execute rsp:", response_buffer, response_len);
    }

    mmosal_free(response_buffer);
}

static void handle_get_keys(struct emmet_raw_rsp *raw_rsp)
{
    struct mmwlan_key_info key_info[MMWLAN_MAX_KEYS] = {};
    uint32_t num_keys = MMWLAN_MAX_KEYS;
    enum mmwlan_status status = MMWLAN_SUCCESS;
    uint32_t ii;

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetKeysRsp get_keys_rsp = EMMET__GET_KEYS_RSP__INIT;
    rsp.get_keys = &get_keys_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_KEYS;

    status = mmwlan_ate_get_key_info(key_info, &num_keys);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Failed to execute command");
        return;
    }

    if (num_keys)
    {
        get_keys_rsp.n_keys = num_keys;

        Emmet__Key **keys = (Emmet__Key **)mmosal_malloc(sizeof(*keys) * num_keys);
        if (keys == NULL)
        {
            goto cleanup;
        }
        memset(keys, 0, sizeof(*keys));
        get_keys_rsp.keys = keys;

        for (ii = 0; ii < num_keys; ii++)
        {
            keys[ii] = (Emmet__Key *)mmosal_malloc(sizeof(*keys[ii]));
            if (keys[ii] == NULL)
            {
                goto cleanup;
            }

            emmet__key__init(keys[ii]);

            keys[ii]->id = key_info[ii].key_id;
            switch (key_info[ii].key_type)
            {
                case MMWLAN_KEY_TYPE_PAIRWISE:
                    keys[ii]->type = EMMET__KEY_TYPE__PAIRWISE;
                    break;

                case MMWLAN_KEY_TYPE_GROUP:
                    keys[ii]->type = EMMET__KEY_TYPE__GROUP;
                    break;

                case MMWLAN_KEY_TYPE_IGTK:
                    keys[ii]->type = EMMET__KEY_TYPE__IGTK;
                    break;
            }

            keys[ii]->data.len = key_info[ii].key_len;
            keys[ii]->data.data = key_info[ii].key_data;
        }
    }

cleanup:
    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_wlanrsp(raw_rsp, &rsp);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE, NULL);
    }

    if (get_keys_rsp.keys != NULL)
    {
        for (ii = 0; ii < num_keys; ii++)
        {
            mmosal_free(get_keys_rsp.keys[ii]);
        }
        mmosal_free(get_keys_rsp.keys);
    }
}

static void handle_get_umac_stats(struct emmet_raw_rsp *raw_rsp)
{
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetUmacStatsRsp stats_rsp = EMMET__GET_UMAC_STATS_RSP__INIT;

    rsp.get_umac_stats = &stats_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_UMAC_STATS;

    size_t buf_len = 1024;
    stats_rsp.stats.data = (uint8_t *)mmosal_calloc(1, buf_len);
    if (stats_rsp.stats.data == NULL)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__NO_MEM, "Memory allocation failure");
        return;
    }

    struct morse_cmd_req command =
        MORSE_COMMAND_INIT(command, MORSE_CMD_ID_IOT_READ_STATS, MMDRV_VIF_ID_INVALID);

    uint32_t response_size = sizeof(struct morse_cmd_resp) + buf_len;
    struct morse_cmd_resp *cmd_rsp = (struct morse_cmd_resp *)mmosal_malloc(response_size);
    if (cmd_rsp == NULL)
    {
        mmosal_free(stats_rsp.stats.data);
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__NO_MEM, "Memory allocation failure");
        return;
    }

    enum mmwlan_status status = mmwlan_ate_execute_command((uint8_t *)&command,
                                                           sizeof(command),
                                                           (uint8_t *)cmd_rsp,
                                                           &response_size);
    if (status != MMWLAN_SUCCESS)
    {
        mmosal_free(cmd_rsp);
        mmosal_free(stats_rsp.stats.data);
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to retrieve stats");
        return;
    }

    size_t stats_len = response_size - sizeof(struct morse_cmd_resp);
    memcpy(stats_rsp.stats.data, cmd_rsp->data, stats_len);
    mmosal_free(cmd_rsp);
    stats_rsp.stats.len = stats_len;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
    mmosal_free(stats_rsp.stats.data);
}

static void handle_get_tx_fc_stats(struct emmet_raw_rsp *raw_rsp)
{
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__GetTxFCStatsRsp stats_rsp = EMMET__GET_TX_FCSTATS_RSP__INIT;

    rsp.get_tx_fc_stats = &stats_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_GET_TX_FC_STATS;

    stats_rsp.paused_cb_count = emmet_wlan_tx_flow_control_stats.paused_cb_count;
    stats_rsp.ready_cb_count = emmet_wlan_tx_flow_control_stats.ready_cb_count;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_beacon_vendor_ie_filter_add(struct emmet_raw_rsp *raw_rsp,
                                               Emmet__BeaconVendorIeFilterAddCmd *cmd)
{
    int ii;
    uint8_t install_idx;
    enum mmwlan_status status;

    if (cmd->oui.len != MMWLAN_OUI_SIZE)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, NULL);
    }

    if (emmet_beacon_vendor_ie_filter_data.filter.n_ouis >= MMWLAN_BEACON_VENDOR_IE_MAX_OUI_FILTERS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__UNAVAILABLE, "Filter list full");
        return;
    }

    for (ii = 0; ii < emmet_beacon_vendor_ie_filter_data.filter.n_ouis; ii++)
    {
        if (!memcmp(cmd->oui.data,
                    emmet_beacon_vendor_ie_filter_data.filter.ouis[ii],
                    MMWLAN_OUI_SIZE))
        {
            MMLOG_DBG("already installed\n");
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
            return;
        }
    }

    status = mmwlan_update_beacon_vendor_ie_filter(NULL);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
        return;
    }

    install_idx = emmet_beacon_vendor_ie_filter_data.filter.n_ouis;
    memcpy(emmet_beacon_vendor_ie_filter_data.filter.ouis[install_idx],
           cmd->oui.data,
           MMWLAN_OUI_SIZE);

    emmet_beacon_vendor_ie_filter_data.filter.n_ouis++;
    status = mmwlan_update_beacon_vendor_ie_filter(&emmet_beacon_vendor_ie_filter_data.filter);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_beacon_vendor_ie_filter_data.filter.n_ouis--;
    }

    emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
}

static void handle_beacon_vendor_ie_filter_clear(struct emmet_raw_rsp *raw_rsp)
{
    int ii;
    Emmet__BeaconVendorIeFilterStat *current_stat;
    enum mmwlan_status status = mmwlan_update_beacon_vendor_ie_filter(NULL);

    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
        return;
    }

    for (ii = 0; ii < emmet_beacon_vendor_ie_filter_data.filter.n_ouis; ii++)
    {
        current_stat = &emmet_beacon_vendor_ie_filter_data.stats[ii];
        if (current_stat->ie.data)
        {
            mmosal_free(current_stat->ie.data);
            current_stat->ie.data = NULL;
            current_stat->ie.len = 0;
            current_stat->occurrences = 0;
        }
    }
    emmet_beacon_vendor_ie_filter_data.filter.n_ouis = 0;

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_beacon_vendor_ie_filter_get_stats(struct emmet_raw_rsp *raw_rsp)
{
    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__BeaconVendorIeFilterGetStatsRsp stats_rsp =
        EMMET__BEACON_VENDOR_IE_FILTER_GET_STATS_RSP__INIT;

    rsp.beacon_vendor_ie_filter_get_stats = &stats_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_BEACON_VENDOR_IE_FILTER_GET_STATS;

    int n_filters = emmet_beacon_vendor_ie_filter_data.filter.n_ouis;
    if (n_filters)
    {
        stats_rsp.n_stats = n_filters;

        Emmet__BeaconVendorIeFilterStat **stats =
            (Emmet__BeaconVendorIeFilterStat **)mmosal_malloc(sizeof(*stats) * n_filters);
        if (stats == NULL)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__MEMORY_ALLOCATION_FAILURE, NULL);
            return;
        }

        for (int ii = 0; ii < n_filters; ii++)
        {
            stats[ii] = &emmet_beacon_vendor_ie_filter_data.stats[ii];
        }

        stats_rsp.stats = stats;
        emmet_pack_wlanrsp(raw_rsp, &rsp);
        mmosal_free(stats_rsp.stats);
        return;
    }

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static uint8_t mgmt_types_to_mask(const Emmet__VendorIeMgmtType *types, size_t n)
{
    uint8_t mask = 0;
    for (size_t i = 0; i < n; i++)
    {
        switch (types[i])
        {
            case EMMET__VENDOR_IE_MGMT_TYPE__BEACON:
                mask |= MMWLAN_VENDOR_IE_MGMT_BEACON;
                break;
            case EMMET__VENDOR_IE_MGMT_TYPE__PROBE_RESP:
                mask |= MMWLAN_VENDOR_IE_MGMT_PROBE_RESP;
                break;
            case EMMET__VENDOR_IE_MGMT_TYPE__ALL:
                mask |= MMWLAN_VENDOR_IE_MGMT_ALL;
                break;
            case _EMMET__VENDOR_IE_MGMT_TYPE_IS_INT_SIZE:
                break;
        }
    }
    return mask;
}

static void handle_vendor_ie_add_ie(struct emmet_raw_rsp *raw_rsp, Emmet__VendorIeAddIeCmd *cmd)
{
    if (cmd->ie.len > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, NULL);
        return;
    }

    struct mmwlan_vendor_ie ie = {
        .data = cmd->ie.data,
        .data_len = (uint8_t)cmd->ie.len,
        .mgmt_type_mask = mgmt_types_to_mask(cmd->mgmt_types, cmd->n_mgmt_types),
    };

    enum mmwlan_status status = mmwlan_add_vendor_ie(&ie);
    emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
}

static void handle_vendor_ie_clear_ies(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__VendorIeClearIeCmd *cmd)
{
    uint8_t mask = mgmt_types_to_mask(cmd->mgmt_types, cmd->n_mgmt_types);
    enum mmwlan_status status = mmwlan_clear_vendor_ies(mask);
    emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
}

static void handle_set_listen_interval(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__SetListenIntervalCmd *wlancmd)
{
    enum mmwlan_status status = mmwlan_set_listen_interval(wlancmd->interval);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error calling handle_set_listen_interval()");
    }
}

static void handle_set_beacon_loss_threshold(struct emmet_raw_rsp *raw_rsp,
                                             Emmet__SetBeaconLossThresholdCmd *cmd)
{
    if (cmd->threshold > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, NULL);
        return;
    }

    enum mmwlan_status status = mmwlan_set_beacon_loss_threshold((uint8_t)cmd->threshold);
    emmet_pack_basic_rsp(raw_rsp, convert_mmwlan_status_to_emmet_status_code(status), NULL);
}

static void emmet_ap_sta_status(const struct mmwlan_ap_sta_status *sta_status, void *arg)
{
    MM_UNUSED(arg);

    const char *state_str;
    switch (sta_status->state)
    {
        case MMWLAN_AP_STA_UNKNOWN:
            state_str = "Unknown (disconnected)";
            break;

        case MMWLAN_AP_STA_AUTHENTICATED:
            state_str = "Authenticated";
            break;

        case MMWLAN_AP_STA_ASSOCIATED:
            state_str = "Associated";
            break;

        case MMWLAN_AP_STA_AUTHORIZED:
            state_str = "Authorized";
            break;

        default:
            state_str = "?";
            break;
    }
    MMLOG_APP("AP STA " MM_MAC_ADDR_FMT " State: %s, AID: %u, Time: %lu ms\n",
              MM_MAC_ADDR_VAL(sta_status->mac_addr),
              state_str,
              sta_status->aid,
              mmosal_get_time_ms());
}

static bool prepare_ap_args(struct emmet_raw_rsp *raw_rsp,
                            struct mmwlan_ap_args *args,
                            const Emmet__ApEnableCmd *cmd)
{
    memcpy(args->ssid, cmd->ssid.data, cmd->ssid.len);
    args->ssid_len = cmd->ssid.len;
    memcpy(args->passphrase, cmd->passphrase.data, cmd->passphrase.len);
    args->passphrase_len = cmd->passphrase.len;
    args->sta_status_cb = emmet_ap_sta_status;

    if (cmd->bssid.len == MMWLAN_MAC_ADDR_LEN)
    {
        memcpy(args->bssid, cmd->bssid.data, MMWLAN_MAC_ADDR_LEN);
    }
    else if (cmd->bssid.len != 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid BSSID length");
        return false;
    }

    switch (cmd->security)
    {
        case EMMET__WLAN_SECURITY_TYPE__NO_SECURITY:
            args->security_type = MMWLAN_OPEN;
            break;

        case EMMET__WLAN_SECURITY_TYPE__OWE_SECURITY:
            args->security_type = MMWLAN_OWE;
            break;

        case EMMET__WLAN_SECURITY_TYPE__SAE_SECURITY:
            args->security_type = MMWLAN_SAE;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid security type");
            return false;
    }

    switch (cmd->pmf_mode)
    {
        case EMMET__WLAN_PMF_MODE__PMF_REQUIRED:
            args->pmf_mode = MMWLAN_PMF_REQUIRED;
            break;

        case EMMET__WLAN_PMF_MODE__PMF_DISABLED:
            args->pmf_mode = MMWLAN_PMF_DISABLED;
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid PMF mode");
            break;
    }

    if (cmd->n_sae_owe_ec_groups)
    {
        if (cmd->n_sae_owe_ec_groups >= MMWLAN_MAX_EC_GROUPS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "n_group_ids exceeds MMWLAN_MAX_EC_GROUPS");
            return false;
        }

        for (uint16_t i = 0; i < cmd->n_sae_owe_ec_groups; i++)
        {
            args->sae_owe_ec_groups[i] = cmd->sae_owe_ec_groups[i];
        }
    }

    if (cmd->op_class > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "op_class exceeds UINT16_MAX");
        return false;
    }
    if (cmd->op_class != 0)
    {
        args->op_class = cmd->op_class;
    }

    if (cmd->s1g_chan_num > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "s1g_chan_num exceeds UINT16_MAX");
        return false;
    }
    if (cmd->s1g_chan_num != 0)
    {
        args->s1g_chan_num = cmd->s1g_chan_num;
    }

    if (cmd->beacon_interval_tus > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "beacon_interval_tus exceeds UINT16_MAX");
        return false;
    }
    if (cmd->beacon_interval_tus != 0)
    {
        args->beacon_interval_tus = cmd->beacon_interval_tus;
    }

    if (cmd->dtim_period > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "dtim_period exceeds UINT16_MAX");
        return false;
    }
    if (cmd->dtim_period != 0)
    {
        args->dtim_period = cmd->dtim_period;
    }

    if (cmd->pri_bw_mhz > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "pri_bw_mhz exceeds UINT8_MAX");
        return false;
    }
    if (cmd->pri_bw_mhz != 0)
    {
        args->pri_bw_mhz = cmd->pri_bw_mhz;
    }

    if (cmd->pri_1mhz_chan_idx > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "pri_1mhz_chan_idx exceeds UINT8_MAX");
        return false;
    }
    if (cmd->pri_1mhz_chan_idx != 0)
    {
        args->pri_1mhz_chan_idx = cmd->pri_1mhz_chan_idx;
    }

    if (cmd->max_stas > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "max_connected_stas exceeds UINT8_MAX");
        return false;
    }
    if (cmd->max_stas != 0)
    {
        args->max_stas = cmd->max_stas;
    }

    return true;
}

static void handle_ap_enable(struct emmet_raw_rsp *raw_rsp, Emmet__ApEnableCmd *cmd)
{
    struct mmwlan_ap_args args = MMWLAN_AP_ARGS_INIT;
    if (!prepare_ap_args(raw_rsp, &args, cmd))
    {
        return;
    }

    enum mmwlan_status status = mmwlan_ap_enable(&args);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error enabling AP");
    }
}

static void handle_ap_disable(struct emmet_raw_rsp *raw_rsp)
{
    enum mmwlan_status status = mmwlan_ap_disable();

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error disabling AP");
    }
}

static Emmet__ApStaState convert_mmwlan_ap_sta_state_to_emmet(enum mmwlan_ap_sta_state state)
{
    switch (state)
    {
        case MMWLAN_AP_STA_AUTHENTICATED:
            return EMMET__AP_STA_STATE__AUTHENTICATED;
        case MMWLAN_AP_STA_UNKNOWN:
            return EMMET__AP_STA_STATE__UNKNOWN;
        case MMWLAN_AP_STA_ASSOCIATED:
            return EMMET__AP_STA_STATE__ASSOCIATED;
        case MMWLAN_AP_STA_AUTHORIZED:
            return EMMET__AP_STA_STATE__AUTHORIZED;
    }
    return EMMET__AP_STA_STATE__UNKNOWN;
}

static void handle_ap_get_sta_status(struct emmet_raw_rsp *raw_rsp, Emmet__ApGetStaStatusCmd *cmd)
{
    struct mmwlan_ap_sta_status sta_status = {};
    enum mmwlan_status status = mmwlan_ap_get_sta_status(cmd->sta_addr.data, &sta_status);

    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error getting AP STA status");
        return;
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__ApGetStaStatusRsp status_rsp = EMMET__AP_GET_STA_STATUS_RSP__INIT;
    status_rsp.state = convert_mmwlan_ap_sta_state_to_emmet(sta_status.state);
    status_rsp.aid = sta_status.aid;
    rsp.ap_get_sta_status = &status_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_AP_GET_STA_STATUS;
    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void handle_cr_preamble_1mhz_bw(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__SetControlResponsePreamble1mhzOutEnabledCmd *wlancmd)
{
    enum mmwlan_status status = mmwlan_set_control_response_preamble_1mhz_out_en(wlancmd->enabled);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error calling handle_cr_preamble_1mhz_bw()");
    }
}

static void handle_set_dynamic_ps_timout(struct emmet_raw_rsp *raw_rsp,
                                         Emmet__SetDynamicPSTimeoutCmd *cmd)
{
    enum mmwlan_status status = mmwlan_set_dynamic_ps_timeout(cmd->timeout_ms);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
        return;
    }

    emmet_pack_basic_rsp(raw_rsp,
                         convert_mmwlan_status_to_emmet_status_code(status),
                         "Error calling handle_set_dynamic_ps_timout()");
}

static void handle_relay_set_depth_override(struct emmet_raw_rsp *raw_rsp,
                                            Emmet__RelaySetDepthOverrideCmd *cmd)
{
    if (cmd->depth > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Relay depth out of range");
        return;
    }

    enum mmwlan_status status = mmwlan_relay_ate_set_depth_override((uint8_t)cmd->depth);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error setting relay depth");
    }
}

static void handle_relay_get_status(struct emmet_raw_rsp *raw_rsp, Emmet__RelayGetStatusCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_relay_status status;
    enum mmwlan_status ret = mmwlan_relay_get_status(&status);
    if (ret != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(ret),
                             "Error getting relay status");
        return;
    }

    Emmet__WlanRsp rsp = EMMET__WLAN_RSP__INIT;
    Emmet__RelayGetStatusRsp relay_get_status_rsp = EMMET__RELAY_GET_STATUS_RSP__INIT;
    rsp.relay_get_status = &relay_get_status_rsp;
    rsp.rsp_case = EMMET__WLAN_RSP__RSP_RELAY_GET_STATUS;

    switch (status.state)
    {
        case MMWLAN_RELAY_STATE_DISABLED:
            rsp.relay_get_status->state = EMMET__RELAY_STATE__RELAY_STATE_DISABLED;
            break;

        case MMWLAN_RELAY_STATE_ROOT:
            rsp.relay_get_status->state = EMMET__RELAY_STATE__RELAY_STATE_ROOT;
            break;

        case MMWLAN_RELAY_STATE_STA:
            rsp.relay_get_status->state = EMMET__RELAY_STATE__RELAY_STATE_STA;
            break;

        case MMWLAN_RELAY_STATE_REPEATER:
            rsp.relay_get_status->state = EMMET__RELAY_STATE__RELAY_STATE_REPEATER;
            break;
    }

    switch (status.sta_state)
    {
        case MMWLAN_STA_DISABLED:
            rsp.relay_get_status->sta_state = EMMET__STA_STATE__STA_DISABLED;
            break;

        case MMWLAN_STA_CONNECTING:
            rsp.relay_get_status->sta_state = EMMET__STA_STATE__STA_CONNECTING;
            break;

        case MMWLAN_STA_CONNECTED:
            rsp.relay_get_status->sta_state = EMMET__STA_STATE__STA_CONNECTED;
            break;
    }

    rsp.relay_get_status->depth = status.depth;

    emmet_pack_wlanrsp(raw_rsp, &rsp);
}

static void emmet_relay_status_callback(const struct mmwlan_relay_status *status, void *arg)
{
    MM_UNUSED(arg);

    const char *relay_state_str;
    switch (status->state)
    {
        case MMWLAN_RELAY_STATE_DISABLED:
            relay_state_str = "Disabled";
            break;

        case MMWLAN_RELAY_STATE_ROOT:
            relay_state_str = "Root";
            break;

        case MMWLAN_RELAY_STATE_STA:
            relay_state_str = "STA";
            break;

        case MMWLAN_RELAY_STATE_REPEATER:
            relay_state_str = "Repeater";
            break;

        default:
            relay_state_str = "?";
            break;
    }

    const char *sta_state_str;
    switch (status->sta_state)
    {
        case MMWLAN_STA_DISABLED:
            sta_state_str = "Disabled";
            break;

        case MMWLAN_STA_CONNECTING:
            sta_state_str = "Connecting";
            break;

        case MMWLAN_STA_CONNECTED:
            sta_state_str = "Connected";
            break;

        default:
            sta_state_str = "?";
            break;
    }
    MMLOG_APP("Relay state: %s, Sta state: %s, Depth: %u\n",
              relay_state_str,
              sta_state_str,
              status->depth);
}

static bool prepare_relay_ap_enable(struct emmet_raw_rsp *raw_rsp,
                                    struct mmwlan_relay_ap_args *args,
                                    const Emmet__RelayApEnableCmd *cmd)
{
    args->sta_status_cb = emmet_ap_sta_status;

    if (cmd->bssid.len == MMWLAN_MAC_ADDR_LEN)
    {
        memcpy(args->bssid, cmd->bssid.data, MMWLAN_MAC_ADDR_LEN);
    }
    else if (cmd->bssid.len != 0)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid BSSID length");
        return false;
    }

    if (cmd->beacon_interval_tus > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "beacon_interval_tus exceeds UINT16_MAX");
        return false;
    }
    args->beacon_interval_tus = cmd->beacon_interval_tus;

    if (cmd->dtim_period > UINT16_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "dtim_period exceeds UINT16_MAX");
        return false;
    }
    args->dtim_period = cmd->dtim_period;

    if (cmd->max_stas > UINT8_MAX)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "max_stas exceeds UINT8_MAX");
        return false;
    }
    args->max_stas = cmd->max_stas;

    return true;
}

static void handle_relay_enable(struct emmet_raw_rsp *raw_rsp, Emmet__RelayEnableCmd *cmd)
{
    struct mmwlan_relay_args args = MMWLAN_RELAY_ARGS_INIT;
    struct mmwlan_sta_args relay_sta_args = MMWLAN_STA_ARGS_INIT;
    struct mmwlan_relay_ap_args relay_ap_args = MMWLAN_RELAY_AP_ARGS_INIT;
    struct mmwlan_ap_args root_ap_args = MMWLAN_AP_ARGS_INIT;

    args.forwarding_table_size = cmd->forwarding_table_size;
    if (cmd->recovery_timeout_ms != 0)
    {
        args.recovery_timeout_ms = cmd->recovery_timeout_ms;
    }

    if (cmd->relay_ap != NULL)
    {
        if (!prepare_relay_ap_enable(raw_rsp, &relay_ap_args, cmd->relay_ap))
        {
            return;
        }
        args.relay_ap_args = &relay_ap_args;
    }

    if (cmd->root_ap != NULL)
    {
        if (!prepare_ap_args(raw_rsp, &root_ap_args, cmd->root_ap))
        {
            return;
        }
        args.root_ap_args = &root_ap_args;
    }

    if (cmd->relay_sta != NULL)
    {
        if (!prepare_sta_args(raw_rsp, &relay_sta_args, cmd->relay_sta))
        {
            return;
        }
        args.relay_sta_args = &relay_sta_args;
        args.sta_status_cb = emmet_sta_status_callback;
    }

    enum mmwlan_status status = mmwlan_register_relay_status_cb(emmet_relay_status_callback, NULL);
    if (status == MMWLAN_SUCCESS)
    {
        status = mmwlan_relay_enable(&args);
    }

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error enabling S1G Relay");
    }
}

static void handle_relay_ap_enable(struct emmet_raw_rsp *raw_rsp, Emmet__RelayApEnableCmd *cmd)
{
    struct mmwlan_relay_ap_args args = MMWLAN_RELAY_AP_ARGS_INIT;
    if (!prepare_relay_ap_enable(raw_rsp, &args, cmd))
    {
        return;
    }

    enum mmwlan_status status = mmwlan_relay_ap_enable(&args);

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error enabling relay AP");
    }
}

static void handle_relay_ap_disable(struct emmet_raw_rsp *raw_rsp)
{
    enum mmwlan_status status = mmwlan_relay_ap_disable();

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error disabling relay AP");
    }
}

static void handle_relay_disable(struct emmet_raw_rsp *raw_rsp)
{
    enum mmwlan_status status = mmwlan_relay_disable();

    if (status == MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             convert_mmwlan_status_to_emmet_status_code(status),
                             "Error disabling S1G Relay");
    }
}

void emmet_handle_wlancmd(struct emmet_raw_rsp *raw_rsp, Emmet__WlanCmd *wlancmd)
{
    switch (wlancmd->cmd_case)
    {
        case EMMET__WLAN_CMD__CMD_SET_MCS_OVERRIDE:
            handle_set_mcs_override(raw_rsp, wlancmd->set_mcs_override);
            break;

        case EMMET__WLAN_CMD__CMD_SET_RTS_THRESHOLD:
            handle_set_rts_threshold(raw_rsp, wlancmd->set_rts_threshold);
            break;

        case EMMET__WLAN_CMD__CMD_SET_COUNTRY_CODE:
            handle_set_country_code(raw_rsp, wlancmd->set_country_code);
            break;

        case EMMET__WLAN_CMD__CMD_SET_TX_POWER_OVERRIDE:
            handle_set_tx_power_override(raw_rsp, wlancmd->set_tx_power_override);
            break;

        case EMMET__WLAN_CMD__CMD_SET_SGI_ENABLED:
            handle_set_sgi_enabled(raw_rsp, wlancmd->set_sgi_enabled);
            break;

        case EMMET__WLAN_CMD__CMD_SET_SUBBANDS_ENABLED:
            handle_set_subbands_enabled(raw_rsp, wlancmd->set_subbands_enabled);
            break;

        case EMMET__WLAN_CMD__CMD_SET_POWER_SAVE_MODE:
            handle_set_power_save_mode(raw_rsp, wlancmd->set_power_save_mode);
            break;

        case EMMET__WLAN_CMD__CMD_SET_WNM_SLEEP_ENABLED:
            handle_set_wnm_sleep_enabled(raw_rsp, wlancmd->set_wnm_sleep_enabled);
            break;

        case EMMET__WLAN_CMD__CMD_SET_AMPDU_ENABLED:
            handle_set_ampdu_enabled(raw_rsp, wlancmd->set_ampdu_enabled);
            break;

        case EMMET__WLAN_CMD__CMD_SET_FRAGMENT_THRESHOLD:
            handle_set_fragment_threshold(raw_rsp, wlancmd->set_fragment_threshold);
            break;

        case EMMET__WLAN_CMD__CMD_SET_SCAN_CONFIG:
            handle_set_scan_config(raw_rsp, wlancmd->set_scan_config);
            break;

        case EMMET__WLAN_CMD__CMD_SET_HEALTH_CHECK_INTERVAL:
            handle_set_health_check_interval(raw_rsp, wlancmd->set_health_check_interval);
            break;

        case EMMET__WLAN_CMD__CMD_SET_DEFAULT_QOS_QUEUE_PARAMS:
            handle_set_default_qos_queue_params(raw_rsp, wlancmd->set_default_qos_queue_params);
            break;

        case EMMET__WLAN_CMD__CMD_SET_MCS10_MODE:
            handle_set_mcs10_mode(raw_rsp, wlancmd->set_mcs10_mode);
            break;

        case EMMET__WLAN_CMD__CMD_SET_DUTY_CYCLE_MODE:
            handle_set_duty_cycle_mode(raw_rsp, wlancmd->set_duty_cycle_mode);
            break;

        case EMMET__WLAN_CMD__CMD_SET_NON_TIM_MODE_ENABLED:
            handle_set_non_tim_mode_enabled(raw_rsp, wlancmd->set_non_tim_mode_enabled);
            break;

        case EMMET__WLAN_CMD__CMD_STA_ENABLE:
            handle_sta_enable(raw_rsp, wlancmd->sta_enable);
            break;

        case EMMET__WLAN_CMD__CMD_STA_DISABLE:
            handle_sta_disable(raw_rsp, wlancmd->sta_disable);
            break;

        case EMMET__WLAN_CMD__CMD_STA_GET_STATE:
            handle_sta_get_state(raw_rsp, wlancmd->sta_get_state);
            break;

        case EMMET__WLAN_CMD__CMD_GET_MAC_ADDRESS:
            handle_get_mac_address(raw_rsp, wlancmd->get_mac_address);
            break;

        case EMMET__WLAN_CMD__CMD_GET_BSSID:
            handle_get_bssid(raw_rsp, wlancmd->get_bssid);
            break;

        case EMMET__WLAN_CMD__CMD_GET_VIF_CHANNEL_INFO:
            handle_get_vif_channel_info(raw_rsp, wlancmd->get_vif_channel_info);
            break;

        case EMMET__WLAN_CMD__CMD_BOOT:
            handle_boot(raw_rsp, wlancmd->boot);
            break;

        case EMMET__WLAN_CMD__CMD_SHUTDOWN:
            handle_shutdown(raw_rsp, wlancmd->shutdown);
            break;

        case EMMET__WLAN_CMD__CMD_GET_VERSION:
            handle_get_version(raw_rsp, wlancmd->get_version);
            break;

        case EMMET__WLAN_CMD__CMD_GET_BCF_METADATA:
            handle_get_bcf_metadata(raw_rsp, wlancmd->get_bcf_metadata);
            break;

        case EMMET__WLAN_CMD__CMD_GET_DUTY_CYCLE_STATS:
            handle_get_duty_cycle_stats(raw_rsp, wlancmd->get_duty_cycle_stats);
            break;

        case EMMET__WLAN_CMD__CMD_STA_SCAN_REQUEST:
            handle_sta_scan_request(raw_rsp, wlancmd->sta_scan_request);
            break;

        case EMMET__WLAN_CMD__CMD_STA_SCAN_ABORT:
            handle_sta_scan_abort(raw_rsp, wlancmd->sta_scan_abort);
            break;

        case EMMET__WLAN_CMD__CMD_STA_SCAN_RESULTS:
            handle_sta_scan_results(raw_rsp, wlancmd->sta_scan_results);
            break;

        case EMMET__WLAN_CMD__CMD_GET_RSSI:
            handle_get_rssi(raw_rsp, wlancmd->get_rssi);
            break;

        case EMMET__WLAN_CMD__CMD_GET_RC_STATS:
            handle_get_rc_stats(raw_rsp, wlancmd->get_rc_stats);
            break;

        case EMMET__WLAN_CMD__CMD_GET_MORSE_STATS:
            handle_get_morse_stats(raw_rsp, wlancmd->get_morse_stats);
            break;

        case EMMET__WLAN_CMD__CMD_TRIGGER_CORE_ASSERT:
            handle_trigger_core_assert(raw_rsp, wlancmd->trigger_core_assert);
            break;

        case EMMET__WLAN_CMD__CMD_EXECUTE_TEST_COMMAND:
            handle_execute_test_command(raw_rsp, wlancmd->execute_test_command);
            break;

        case EMMET__WLAN_CMD__CMD_GET_KEYS:
            handle_get_keys(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_GET_UMAC_STATS:
            handle_get_umac_stats(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_GET_TX_FC_STATS:
            handle_get_tx_fc_stats(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_BEACON_VENDOR_IE_FILTER_ADD:
            handle_beacon_vendor_ie_filter_add(raw_rsp, wlancmd->beacon_vendor_ie_filter_add);
            break;

        case EMMET__WLAN_CMD__CMD_BEACON_VENDOR_IE_FILTER_CLEAR:
            handle_beacon_vendor_ie_filter_clear(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_BEACON_VENDOR_IE_FILTER_GET_STATS:
            handle_beacon_vendor_ie_filter_get_stats(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_VENDOR_IE_ADD_IE:
            handle_vendor_ie_add_ie(raw_rsp, wlancmd->vendor_ie_add_ie);
            break;

        case EMMET__WLAN_CMD__CMD_VENDOR_IE_CLEAR_IES:
            handle_vendor_ie_clear_ies(raw_rsp, wlancmd->vendor_ie_clear_ies);
            break;

        case EMMET__WLAN_CMD__CMD_SET_LISTEN_INTERVAL:
            handle_set_listen_interval(raw_rsp, wlancmd->set_listen_interval);
            break;

        case EMMET__WLAN_CMD__CMD_SET_BEACON_LOSS_THRESHOLD:
            handle_set_beacon_loss_threshold(raw_rsp, wlancmd->set_beacon_loss_threshold);
            break;

        case EMMET__WLAN_CMD__CMD_AP_ENABLE:
            handle_ap_enable(raw_rsp, wlancmd->ap_enable);
            break;

        case EMMET__WLAN_CMD__CMD_AP_DISABLE:
            handle_ap_disable(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_AP_GET_STA_STATUS:
            handle_ap_get_sta_status(raw_rsp, wlancmd->ap_get_sta_status);
            break;

        case EMMET__WLAN_CMD__CMD_SET_CONTROL_RESPONSE_PREAMBLE_1MHZ_OUT_EN:
            handle_cr_preamble_1mhz_bw(raw_rsp, wlancmd->set_control_response_preamble_1mhz_out_en);
            break;

        case EMMET__WLAN_CMD__CMD_SET_DYNAMIC_PS_TIMEOUT:
            handle_set_dynamic_ps_timout(raw_rsp, wlancmd->set_dynamic_ps_timeout);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_SET_DEPTH_OVERRIDE:
            handle_relay_set_depth_override(raw_rsp, wlancmd->relay_set_depth_override);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_GET_STATUS:
            handle_relay_get_status(raw_rsp, wlancmd->relay_get_status);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_ENABLE:
            handle_relay_enable(raw_rsp, wlancmd->relay_enable);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_DISABLE:
            handle_relay_disable(raw_rsp);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_AP_ENABLE:
            handle_relay_ap_enable(raw_rsp, wlancmd->relay_ap_enable);
            break;

        case EMMET__WLAN_CMD__CMD_RELAY_AP_DISABLE:
            handle_relay_ap_disable(raw_rsp);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
