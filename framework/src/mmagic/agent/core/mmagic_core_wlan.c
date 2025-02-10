/**
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmutils.h"
#include "mmwlan.h"

#include "mmagic.h"
#include "mmagic_core_utils.h"
#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_wlan.h"
#include "m2m_api/mmagic_m2m_agent.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_wlan.def"

static struct mmagic_wlan_config default_config =
{
    .ssid = {.data = "MorseMicro", .len = sizeof("MorseMicro") - 1},
    .password = {.data = "12345678", .len = sizeof("12345678")},
    .security = MMAGIC_SECURITY_TYPE_SAE,
    .raw_priority = 0,
    .bssid = {.addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    .pmf_mode = MMAGIC_PMF_MODE_REQUIRED,
    .station_type = MMAGIC_STATION_TYPE_NON_SENSOR,
    .country_code = {.country_code = "??"},
    .rts_threshold = 0,
    .sgi_enabled = true,
    .subbands_enabled = true,
    .ampdu_enabled = true,
    .power_save_mode = MMAGIC_POWER_SAVE_MODE_ENABLED,
    .fragment_threshold = 0,
    .cac_enabled = false,
    .ndp_probe_enabled = false,
};

static void mmagic_core_wlan_init_mmwlan(
    struct mmagic_wlan_data *data, const struct mmwlan_regulatory_db *reg_db)
{
    const struct mmwlan_s1g_channel_list *channel_list =
        mmwlan_lookup_regulatory_domain(reg_db, data->config.country_code.country_code);
    if (channel_list == NULL)
    {
        printf("No regulatory domain matching code %s\n", data->config.country_code.country_code);
    }

    /* Initialise MMWLAN interface and set channel list */
    mmwlan_init();
    mmwlan_set_channel_list(channel_list);
}

/** Binary semaphore used to indicate the sta status has changed. */
static struct mmosal_semb *sta_status;

static void mmagic_core_wlan_shim_sta_status_cb(enum mmwlan_sta_state sta_state)
{
    switch (sta_state)
    {
    case MMWLAN_STA_DISABLED:
        printf("WLAN STA disabled\n");
        break;

    case MMWLAN_STA_CONNECTING:
        printf("WLAN STA connecting\n");
        break;

    case MMWLAN_STA_CONNECTED:
        printf("WLAN STA connected\n");
        mmosal_semb_give(sta_status);
        break;
    }
}

/**
 * Function to install any parameters that are not set by @c mmwlan_sta_enable . This will shutdown
 * the WLAN interface and set parameters such as country code.
 *
 * @param  data   Reference to the wlan data struct.
 * @param  reg_db Reference to the regulatory database
 *
 * @return        @c 0 if successful else @c -1
 */
static int wlan_shim_reconfigure(
    struct mmagic_wlan_data *data, const struct mmwlan_regulatory_db *reg_db)
{
    mmwlan_shutdown();
    const struct mmwlan_s1g_channel_list *channel_list =
        mmwlan_lookup_regulatory_domain(reg_db, data->config.country_code.country_code);
    if (channel_list == NULL)
    {
        printf("No regulatory domain matching code %s\n", data->config.country_code.country_code);
        return -1;
    }

    mmwlan_set_channel_list(channel_list);

    return 0;
}

void mmagic_core_wlan_init(struct mmagic_data *core)
{
    struct mmagic_wlan_data *data = mmagic_data_get_wlan(core);
    memcpy(&data->config, &default_config, sizeof(data->config));
}

void mmagic_core_wlan_start(struct mmagic_data *core)
{
    struct mmagic_wlan_data *data = mmagic_data_get_wlan(core);
    mmagic_core_wlan_init_mmwlan(data, core->reg_db);

    sta_status = mmosal_semb_create("sta_status");
}

/********* MMAGIC Core WLAN ops **********/

enum mmagic_status mmagic_core_wlan_connect(
    struct mmagic_data *core, const struct mmagic_core_wlan_connect_cmd_args *cmd_args)
{
    struct mmagic_wlan_data *data = mmagic_data_get_wlan(core);
    wlan_shim_reconfigure(data, core->reg_db);
    struct mmwlan_sta_args args = MMWLAN_STA_ARGS_INIT;
    memcpy(args.ssid, data->config.ssid.data, data->config.ssid.len);
    args.ssid_len = data->config.ssid.len;
    memcpy(args.bssid, data->config.bssid.addr, sizeof(args.bssid));

    switch (data->config.security)
    {
    case MMAGIC_SECURITY_TYPE_SAE:
        args.security_type = MMWLAN_SAE; break;

    case MMAGIC_SECURITY_TYPE_OWE:
        args.security_type = MMWLAN_OWE; break;

    case MMAGIC_SECURITY_TYPE_OPEN:
        args.security_type = MMWLAN_OPEN; break;
    }

    memcpy(args.passphrase, data->config.password.data, data->config.password.len);
    args.passphrase_len = data->config.password.len;

    switch (data->config.pmf_mode)
    {
    case MMAGIC_PMF_MODE_REQUIRED:
        args.pmf_mode = MMWLAN_PMF_REQUIRED; break;

    case MMAGIC_PMF_MODE_DISABLED:
        args.pmf_mode = MMWLAN_PMF_DISABLED; break;
    }

    args.raw_sta_priority = data->config.raw_priority;

    switch (data->config.station_type)
    {
    case MMAGIC_STATION_TYPE_SENSOR:
        args.sta_type = MMWLAN_STA_TYPE_SENSOR; break;

    case MMAGIC_STATION_TYPE_NON_SENSOR:
        args.sta_type = MMWLAN_STA_TYPE_NON_SENSOR; break;
    }

    args.scan_interval_base_s = data->config.sta_scan_interval_base_s;
    args.scan_interval_limit_s = data->config.sta_scan_interval_limit_s;

    struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
    scan_config.ndp_probe_enabled = data->config.ndp_probe_enabled;
    mmwlan_set_scan_config(&scan_config);

    enum mmwlan_status status = mmwlan_sta_enable(&args, mmagic_core_wlan_shim_sta_status_cb);
    if (status != MMWLAN_SUCCESS)
    {
        return mmagic_mmwlan_status_to_mmagic_status(status);
    }

    /* If no timeout is set return immediately */
    if (!cmd_args->timeout)
    {
        return MMAGIC_STATUS_OK;
    }

    mmosal_semb_wait(sta_status, cmd_args->timeout);

    enum mmwlan_sta_state state = mmwlan_get_sta_state();
    if (state != MMWLAN_STA_CONNECTED)
    {
        mmagic_core_wlan_disconnect(core);
        return MMAGIC_STATUS_TIMEOUT;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_wlan_disconnect(struct mmagic_data *core)
{
    MM_UNUSED(core);
    mmwlan_sta_disable();
    mmwlan_shutdown();

    return MMAGIC_STATUS_OK;
}

struct mmagic_core_wlan_scan_args
{
    struct mmosal_semb *scan_complete_semb;
    struct struct_scan_status *results;
};

/**
 * Scan rx callback.
 *
 * @param result Pointer to the scan result.
 * @param arg    Opaque argument.
 */
static void mmagic_core_wlan_scan_rx_callback(const struct mmwlan_scan_result *result, void *arg)
{
    struct mmagic_core_wlan_scan_args *cb_args = (struct mmagic_core_wlan_scan_args *)arg;

    if ((uint32_t)cb_args->results->num + 1 >=
        (sizeof(cb_args->results->results) / sizeof(cb_args->results->results[0])))
    {
        /* We have reach the maximum number of scan results we can store */
        return;
    }

    struct struct_scan_result *curr_result = &cb_args->results->results[cb_args->results->num];
    cb_args->results->num++;

    memcpy(curr_result->bssid.addr, result->bssid, MMWLAN_MAC_ADDR_LEN);

    memcpy((char *)curr_result->ssid.data, (const char *)result->ssid, result->ssid_len);
    curr_result->ssid.len = result->ssid_len;

    curr_result->rssi = result->rssi;
}

/**
 * Scan complete callback.
 *
 * @param state Scan complete status.
 * @param arg   Opaque argument.
 */
static void mmagic_core_wlan_scan_complete_callback(enum mmwlan_scan_state state, void *arg)
{
    (void)(state);
    struct mmagic_core_wlan_scan_args *cb_args = (struct mmagic_core_wlan_scan_args *)arg;

    mmosal_semb_give(cb_args->scan_complete_semb);
}

enum mmagic_status mmagic_core_wlan_scan(struct mmagic_data *core,
                                         const struct mmagic_core_wlan_scan_cmd_args *cmd_args,
                                         struct mmagic_core_wlan_scan_rsp_args *rsp_args)
{
    MM_UNUSED(core);

    memset(&(rsp_args->results), 0, sizeof(rsp_args->results));

    struct mmagic_core_wlan_scan_args cb_args = {
        .scan_complete_semb = mmosal_semb_create("scan_complete"),
        .results = &(rsp_args->results),
    };
    MMOSAL_ASSERT(cb_args.scan_complete_semb);

    struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_cb_arg = (void *)&cb_args;
    scan_req.scan_rx_cb = mmagic_core_wlan_scan_rx_callback;
    scan_req.scan_complete_cb = mmagic_core_wlan_scan_complete_callback;

    if (cmd_args->ssid.len != 0)
    {
        if (cmd_args->ssid.len > MMWLAN_SSID_MAXLEN)
        {
            return MMAGIC_STATUS_INVALID_ARG;
        }
        memcpy(scan_req.args.ssid, cmd_args->ssid.data, cmd_args->ssid.len);
        scan_req.args.ssid_len = cmd_args->ssid.len;
    }

    struct mmagic_wlan_data *data = mmagic_data_get_wlan(core);
    struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
    scan_config.ndp_probe_enabled = data->config.ndp_probe_enabled;
    mmwlan_set_scan_config(&scan_config);

    enum mmwlan_status status = mmwlan_scan_request(&scan_req);
    if (status != MMWLAN_SUCCESS)
    {
        mmosal_semb_delete(cb_args.scan_complete_semb);
        return mmagic_mmwlan_status_to_mmagic_status(status);
    }

    bool ok = mmosal_semb_wait(cb_args.scan_complete_semb, cmd_args->timeout);
    if (!ok)
    {
        mmwlan_scan_abort();
        /* Wait to be notified by scan complete callback */
        mmosal_semb_wait(cb_args.scan_complete_semb, 1000);
    }

    mmosal_semb_delete(cb_args.scan_complete_semb);
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_wlan_get_rssi(struct mmagic_data *core,
                                             struct mmagic_core_wlan_get_rssi_rsp_args *rsp_args)
{
    MM_UNUSED(core);

    rsp_args->rssi = mmwlan_get_rssi();

    return (rsp_args->rssi != INT32_MIN) ? MMAGIC_STATUS_OK : MMAGIC_STATUS_ERROR;
}

enum mmagic_status mmagic_core_wlan_get_mac_addr(
    struct mmagic_data *core, struct mmagic_core_wlan_get_mac_addr_rsp_args *rsp_args)
{
    MM_UNUSED(core);

    enum mmwlan_status status = mmwlan_get_mac_addr(rsp_args->mac_addr.addr);

    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_wlan_wnm_sleep(
    struct mmagic_data *core, const struct mmagic_core_wlan_wnm_sleep_cmd_args *cmd_args)
{
    MM_UNUSED(core);
    enum mmwlan_status status = mmwlan_set_wnm_sleep_enabled(cmd_args->wnm_sleep_enabled);

    return mmagic_mmwlan_status_to_mmagic_status(status);
}

/* Note: in future this will get refactored into the MMAGIC data structure and we will pass
 *        a pointer to that instead. */
struct mmagic_beacon_monitor_data
{
    struct mmosal_mutex *mutex;
    struct mmagic_data *volatile core;
    struct mmwlan_beacon_vendor_ie_filter filter_args;
    struct mmagic_core_event_wlan_beacon_rx_args evt_args;
};

static struct mmagic_beacon_monitor_data mmagic_beacon_monitor_data;

static void mmagic_vendor_ie_filter_handler(const uint8_t *ies, uint32_t ies_len, void *arg)
{
    bool ok;
    int ii;
    struct mmagic_beacon_monitor_data *data = (struct mmagic_beacon_monitor_data *)arg;

    /* If data->core NULL this indicates that the beacon montioring is disabled. */
    if (data->core == NULL)
    {
        return;
    }

    ok = mmosal_mutex_get(data->mutex, UINT32_MAX);
    if (!ok)
    {
        return;
    }

    memset(&data->evt_args, 0, sizeof(data->evt_args));

    for (ii = 0; ii < data->filter_args.n_ouis; ii++)
    {
        int offset = 0;
        while (offset >= 0)
        {
            offset = mm_find_vendor_specific_ie_from_offset(ies, ies_len, offset,
                                                            data->filter_args.ouis[ii],
                                                            MMWLAN_OUI_SIZE);
            if (offset >= 0)
            {
                /* Note that we rely on mm_find_vendor_specific_ie_from_offset() to validate
                 * that the IE does not extend past the end of the given buffer. Add 2 as the
                 * encoded length does not include the type and length bytes. */
                uint8_t length = ies[offset + 1] + 2;

                if (length + data->evt_args.vendor_ies.len > sizeof(data->evt_args.vendor_ies.data))
                {
                    printf("Beacon monitor buffer overflow\n");
                    return;
                }

                memcpy(&data->evt_args.vendor_ies.data[data->evt_args.vendor_ies.len],
                       &ies[offset], length);
                data->evt_args.vendor_ies.len += length;
                offset += length;
            }
        }
    }

    mmagic_core_event_wlan_beacon_rx(data->core, &data->evt_args);

    mmosal_mutex_release(data->mutex);
}

enum mmagic_status mmagic_core_wlan_beacon_monitor_enable(
    struct mmagic_data *core,
    const struct mmagic_core_wlan_beacon_monitor_enable_cmd_args *cmd_args)
{
    struct mmagic_beacon_monitor_data *data = &mmagic_beacon_monitor_data;
    enum mmwlan_status status;
    size_t ii;
    bool ok;

    if (cmd_args->oui_filter.count > MMWLAN_BEACON_VENDOR_IE_MAX_OUI_FILTERS ||
        cmd_args->oui_filter.count > MM_ARRAY_COUNT(cmd_args->oui_filter.ouis))
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    if (data->core == NULL)
    {
        data->mutex = mmosal_mutex_create("bcnmon");
        data->core = core;
        data->filter_args.cb = mmagic_vendor_ie_filter_handler;
        data->filter_args.cb_arg = &mmagic_beacon_monitor_data;
    }

    ok = mmosal_mutex_get(data->mutex, UINT32_MAX);
    if (!ok)
    {
        return MMAGIC_STATUS_ERROR;
    }

    for (ii = 0; ii < cmd_args->oui_filter.count; ii++)
    {
        memcpy(data->filter_args.ouis[ii], cmd_args->oui_filter.ouis[ii].oui,
               sizeof(data->filter_args.ouis[ii]));
    }
    data->filter_args.n_ouis = cmd_args->oui_filter.count;

    status = mmwlan_update_beacon_vendor_ie_filter(&data->filter_args);
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to configure Vendor IE filter\n");
        return mmagic_mmwlan_status_to_mmagic_status(status);
    }

    mmosal_mutex_release(data->mutex);

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_wlan_beacon_monitor_disable(
    struct mmagic_data *core)
{
    enum mmwlan_status status;
    struct mmagic_beacon_monitor_data *data = &mmagic_beacon_monitor_data;

    MM_UNUSED(core);

    if (data->core == NULL)
    {
        return MMAGIC_STATUS_OK;
    }

    data->core = NULL;

    status = mmwlan_update_beacon_vendor_ie_filter(NULL);
    if (status != MMWLAN_SUCCESS)
    {
        return mmagic_mmwlan_status_to_mmagic_status(status);
    }

    mmosal_mutex_delete(data->mutex);
    data->mutex = NULL;

    return MMAGIC_STATUS_OK;
}

static void mmagic_standby_exit_handler(uint8_t reason, void *arg)
{
    struct mmagic_data *core = (struct mmagic_data *)arg;
    struct mmagic_core_event_wlan_standby_exit_args event_args;

    event_args.reason = (enum mmagic_standby_mode_exit_reason)reason;
    mmagic_core_event_wlan_standby_exit(core, &event_args);
}

enum mmagic_status mmagic_core_wlan_standby_enter(struct mmagic_data *core)
{
    struct mmwlan_standby_enter_args args;

    args.standby_exit_cb = mmagic_standby_exit_handler;
    args.standby_exit_arg = (void *)core;

    enum mmwlan_status status = mmwlan_standby_enter(&args);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_wlan_standby_exit(struct mmagic_data *core)
{
    enum mmwlan_status status;
    MM_UNUSED(core);

    status = mmwlan_standby_exit();
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_wlan_standby_set_status_payload(
    struct mmagic_data *core,
    const struct mmagic_core_wlan_standby_set_status_payload_cmd_args *cmd_args)
{
    MM_UNUSED(core);
    struct mmwlan_standby_set_status_payload_args args;

    memcpy(&args.payload, cmd_args->payload.buffer, sizeof(args.payload));
    args.payload_len = cmd_args->payload.len;

    enum mmwlan_status status = mmwlan_standby_set_status_payload(&args);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_wlan_standby_set_wake_filter(
    struct mmagic_data *core,
    const struct mmagic_core_wlan_standby_set_wake_filter_cmd_args *cmd_args)
{
    MM_UNUSED(core);
    struct mmwlan_standby_set_wake_filter_args args;
    memcpy(&args.filter, cmd_args->filter.buffer, sizeof(args.filter));
    args.filter_len = cmd_args->filter.len;
    args.offset = cmd_args->offset;

    enum mmwlan_status status = mmwlan_standby_set_wake_filter(&args);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_wlan_standby_set_config(struct mmagic_data *core,
                                                       const struct
                                                       mmagic_core_wlan_standby_set_config_cmd_args
                                                       *cmd_args)
{
    uint32_t ip1, ip2, ip3, ip4;
    struct mmwlan_standby_config config = { 0 };

    MM_UNUSED(core);

    config.notify_period_s = cmd_args->notify_period_s;

    int ret = sscanf(cmd_args->src_ip.addr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4);
    if (ret != 4)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    config.src_ip = ip1 + (ip2 << 8) + (ip3 << 16) + (ip4 << 24);

    ret = sscanf(cmd_args->dst_ip.addr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4);
    if (ret != 4)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    config.dst_ip = ip1 + (ip2 << 8) + (ip3 << 16) + (ip4 << 24);

    config.dst_port = cmd_args->dst_port;
    config.bss_inactivity_before_snooze_s = cmd_args->bss_inactivity_s;
    config.snooze_period_s = cmd_args->snooze_period_s;
    config.snooze_increment_s = cmd_args->snooze_increment_s;
    config.snooze_max_s = cmd_args->snooze_max_s;

    enum mmwlan_status status = mmwlan_standby_set_config(&config);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}
