/*
 * Copyright 2022 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#include "emmet_private.h"
#include "mmwlan.h"
#include "common/morse_commands.h"
#include "common/morse_command_utils.h"
#include "mmipal.h"
#include "mmiperf.h"
#include "mmping.h"

#define WFA_DEFAULT_GATEWAY "10.0.0.1"

#define TWT_WAKE_DURATION_UNIT_US (256)

enum traffic_stream_profile
{
    TRAFFIC_PROFILE_FILE_TRANSFER = 0,
    TRAFFIC_PROFILE_MULTICAST = 1,
    TRAFFIC_PROFILE_IPTV = 2,
    TRAFFIC_PROFILE_TRANSACTION = 3,
    TRAFFIC_PROFILE_START_SYNC = 4,
    TRAFFIC_PROFILE_UAPSD = 5,
};

enum traffic_agent_direction
{
    TRAFFIC_DIRECTION_SEND = 0,
    TRAFFIC_DIRECTION_RECEIVE = 1,
};

static struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;

struct mmwlan_twt_config_args twt_config_args = MMWLAN_TWT_CONFIG_ARGS_INIT;

static struct
{
    int unified_scaling_factor;
    int unscaled_interval;
    uint16_t listen_interval;
    bool listen_interval_enabled;
} listen_interval_args;

static struct
{
    enum traffic_stream_profile profile;
    enum traffic_agent_direction direction;
    char source_ip_addr[MMIPERF_IPADDR_MAXLEN];
    uint16_t source_port;
    char destination_ip_addr[MMIPERF_IPADDR_MAXLEN];
    uint16_t destination_port;
    uint16_t duration;
    uint16_t payload_size;
    uint8_t traffic_class;
    uint16_t frame_rate;
    uint8_t hti;
    uint16_t stream_id;
} traffic_agent_args;

struct traffic_agent_instance
{
    mmiperf_handle_t handle;
    volatile enum traffic_agent_state state;
    struct mmiperf_report report;
};

static struct
{
    struct traffic_agent_instance server;
    struct traffic_agent_instance client;
} traffic_agent_state;

static void traffic_agent_report_handler(const struct mmiperf_report *report,
                                         void *arg,
                                         mmiperf_handle_t handle)
{
    struct traffic_agent_instance *inst = (struct traffic_agent_instance *)arg;

    if (inst->handle != handle)
    {
        MMLOG_WRN("Unexpected traffic agent handle received\n");
        return;
    }

    if (inst->state != TRAFFIC_AGENT_RUNNING)
    {
        MMLOG_WRN("Unexpected traffic agent state received\n");
        return;
    }

    inst->report = *report;

    switch (report->report_type)
    {
        case MMIPERF_UDP_DONE_SERVER:
            break;

        case MMIPERF_UDP_DONE_CLIENT:
        case MMIPERF_STOPPED:
            inst->state = TRAFFIC_AGENT_STOPPED;
            inst->handle = NULL;
            break;

        default:
            MMLOG_WRN("Unexpected traffic agent report received\n");
            break;
    }
}

void emmet_capi_init(void)
{
}

#define PACK_RSP(_raw_rsp, _rsp_data, _cmd_id, _cmd_id_upper) \
    do {                                                      \
        Emmet__CapiRsp rsp = EMMET__CAPI_RSP__INIT;           \
        rsp._cmd_id = &_rsp_data;                             \
        rsp.rsp_case = EMMET__CAPI_RSP__RSP_##_cmd_id_upper;  \
        emmet_pack_capirsp(raw_rsp, &rsp);                    \
    } while (0)

static bool parse_option_argument(const char *argument_str, bool *value, bool *arg_received)
{
    if (argument_str[0] == '\0')
    {
        *arg_received = false;
        return true;
    }

    if (strcasecmp(argument_str, "enable") == 0 || strcasecmp(argument_str, "on") == 0)
    {
        *value = true;
        *arg_received = true;
        return true;
    }
    else if (strcasecmp(argument_str, "disable") == 0 || strcasecmp(argument_str, "off") == 0)
    {
        *value = false;
        *arg_received = true;
        return true;
    }
    return false;
}

static bool parse_integer_argument(const char *argument_str,
                                   int *value,
                                   int min,
                                   int max,
                                   bool *arg_received)
{
    if (argument_str[0] == '\0')
    {
        *arg_received = false;
        return true;
    }

    *value = atoi(argument_str);

    if (*value < min || *value > max)
    {
        *arg_received = false;
        return false;
    }

    *arg_received = true;
    return true;
}

static bool parse_boolean_argument(const char *argument_str, bool *value, bool *arg_received)
{
    if (argument_str[0] == '\0')
    {
        *arg_received = false;
        return true;
    }

    if (strcasecmp(argument_str, "1") == 0)
    {
        *value = true;
        *arg_received = true;
        return true;
    }
    else if (strcasecmp(argument_str, "0") == 0)
    {
        *value = false;
        *arg_received = true;
        return true;
    }
    return false;
}

static bool parse_group_ids(const uint8_t *argument_str, uint32_t argument_str_len, int *group_ids)
{
    int current_group_id = 0;
    unsigned i = 0, count = 0;

    while (true)
    {
        if (i >= argument_str_len || argument_str[i] == ' ' || argument_str[i] == ',')
        {
            if (count >= MMWLAN_MAX_EC_GROUPS - 1)
            {
                return false;
            }
            group_ids[count++] = current_group_id;
            current_group_id = 0;
            if (i >= argument_str_len)
            {
                return true;
            }
        }
        else if (argument_str[i] >= '0' && argument_str[i] <= '9')
        {
            current_group_id *= 10;
            current_group_id += argument_str[i] - '0';
        }
        else
        {
            return false;
        }
        i++;
    }
}

static void emmet_pack_capirsp(struct emmet_raw_rsp *raw_rsp, Emmet__CapiRsp *capirsp)
{
    Emmet__DutRsp dutrsp = EMMET__DUT_RSP__INIT;
    dutrsp.capi = capirsp;
    dutrsp.rspgrp_case = EMMET__DUT_RSP__RSPGRP_CAPI;
    emmet_pack_rsp(raw_rsp, &dutrsp);
}

static void handle_device_get_info(struct emmet_raw_rsp *raw_rsp, Emmet__CapiDeviceGetInfoCmd *cmd)
{
    MM_UNUSED(cmd);

    Emmet__CapiDeviceGetInfoRsp info_rsp = EMMET__CAPI_DEVICE_GET_INFO_RSP__INIT;
    enum mmwlan_status result = MMWLAN_ERROR;
    struct mmwlan_version version;
    char version_str[64];

    result = mmwlan_get_version(&version);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "mmwlan_get_version() failed");
        return;
    }

    info_rsp.vendor = "MorseMicro";
    info_rsp.model = "MMIoTSDK";
    snprintf(version_str,
             sizeof(version_str),
             "%s_%s",
             version.morselib_version,
             version.morse_fw_version);
    info_rsp.version = version_str;
    PACK_RSP(raw_rsp, info_rsp, device_get_info, DEVICE_GET_INFO);
}

static void handle_device_list_interfaces(struct emmet_raw_rsp *raw_rsp,
                                          Emmet__CapiDeviceListInterfacesCmd *cmd)
{
    Emmet__CapiDeviceListInterfacesRsp info_rsp = EMMET__CAPI_DEVICE_LIST_INTERFACES_RSP__INIT;
    info_rsp.interfacetype = cmd->interfacetype;
    info_rsp.interfaceid = "wlan0";
    PACK_RSP(raw_rsp, info_rsp, device_list_interfaces, DEVICE_LIST_INTERFACES);
}

static void handle_sta_reset_default(struct emmet_raw_rsp *raw_rsp,
                                     Emmet__CapiStaResetDefaultCmd *cmd)
{
    MM_UNUSED(cmd);
    enum mmwlan_status result = MMWLAN_ERROR;

    sta_args.sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
    memset(sta_args.sae_owe_ec_groups, 0, sizeof(sta_args.sae_owe_ec_groups));

    listen_interval_args.listen_interval = 0;
    listen_interval_args.unscaled_interval = 0;
    listen_interval_args.unified_scaling_factor = 0;
    listen_interval_args.listen_interval_enabled = false;

    result = mmwlan_shutdown();
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "mmwlan_shutdown() failed");
        return;
    }

    result = mmwlan_boot(NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "mmwlan_boot() failed");
        return;
    }

    result = mmwlan_ate_override_rate_control(MMWLAN_MCS_NONE, MMWLAN_BW_NONE, MMWLAN_GI_NONE);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate override");
        return;
    }

    result = mmwlan_set_rts_threshold(0);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set rts threshold");
        return;
    }

    result = mmwlan_set_fragment_threshold(0);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set fragment threshold");
        return;
    }

    result = mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to enable power save");
        return;
    }

    struct morse_cmd_req_iot_configure_interop command =
        MORSE_COMMAND_INIT(command,
                           MORSE_CMD_ID_IOT_CONFIGURE_INTEROP,
                           MMDRV_VIF_ID_INVALID,
                           .disable_op_class_checking = 1,
                           .enable_channel_width_workaround = 1);

    result = mmwlan_ate_execute_command((uint8_t *)&command, sizeof(command), NULL, NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to execute ate command");
        return;
    }

    struct morse_cmd_req_set_transmission_rate tx_cmd =
        MORSE_COMMAND_INIT(tx_cmd,
                           MORSE_CMD_ID_SET_TRANSMISSION_RATE,
                           MMDRV_VIF_ID_INVALID,
                           .mcs_index = -1,
                           .bandwidth_mhz = -1,
                           .tx_80211ah_format = -1,
                           .use_traveling_pilots = -1,
                           .use_sgi = -1,
                           .enabled = 0);

    result = mmwlan_ate_execute_command((uint8_t *)&tx_cmd, sizeof(tx_cmd), NULL, NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate transmission rate");
        return;
    }

    result = mmwlan_set_control_response_preamble_1mhz_out_en(false);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate control response");
        return;
    }

    struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
    scan_config.ndp_probe_enabled = false;

    result = mmwlan_set_scan_config(&scan_config);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate ndp probe support");
        return;
    }

    result = mmwlan_set_ampdu_enabled(false);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to disable ampdu");
        return;
    }

    struct morse_cmd_req_turbo_mode turbo_cmd = MORSE_COMMAND_INIT(turbo_cmd,
                                                                   MORSE_CMD_ID_TURBO_MODE,
                                                                   MMDRV_VIF_ID_INVALID,
                                                                   .aid = 0,
                                                                   .enabled = 0);

    result = mmwlan_ate_execute_command((uint8_t *)&turbo_cmd, sizeof(turbo_cmd), NULL, NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate turbo mode");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_preset_testparameters(struct emmet_raw_rsp *raw_rsp,
                                             Emmet__CapiStaPresetTestparametersCmd *cmd)
{
    MM_UNUSED(cmd);

    enum mmwlan_status result = mmwlan_set_fragment_threshold(atoi(cmd->frgmnt));
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set fragment threshold");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_get_mac_address(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__CapiStaGetMacAddressCmd *cmd)
{
    MM_UNUSED(cmd);
    Emmet__CapiStaGetMacAddressRsp info_rsp = EMMET__CAPI_STA_GET_MAC_ADDRESS_RSP__INIT;
    uint8_t sta_mac[MMWLAN_MAC_ADDR_LEN];
    char mac_address_str[MM_MAC_ADDR_STR_LEN];

    enum mmwlan_status result = mmwlan_get_mac_addr(sta_mac);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to get mac address");
        return;
    }

    mm_mac_addr_to_str(sta_mac, mac_address_str);
    info_rsp.mac = mac_address_str;
    PACK_RSP(raw_rsp, info_rsp, sta_get_mac_address, STA_GET_MAC_ADDRESS);
}

static void handle_sta_set_wireless(struct emmet_raw_rsp *raw_rsp,
                                    Emmet__CapiStaSetWirelessCmd *cmd)
{
    enum mmwlan_status result = MMWLAN_ERROR;
    bool enabled, arg_received;
    int value;

    result = mmwlan_boot(NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to boot");
        return;
    }

    if ((cmd->txsp_stream[0] != '\0') && (strcasecmp(cmd->txsp_stream, "1SS") != 0))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Unsupported Tx spatial stream");
        return;
    }

    if ((cmd->rxsp_stream[0] != '\0') && (strcasecmp(cmd->rxsp_stream, "1SS") != 0))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Unsupported Rx spatial stream");
        return;
    }

    if (!parse_option_argument(cmd->sgi, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->sgi,
                             "SGI");
        return;
    }

    if (arg_received)
    {
        result = mmwlan_set_sgi_enabled(enabled);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to set sgi");
            return;
        }
    }

    if (!parse_option_argument(cmd->ampdu, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->ampdu,
                             "ampdu");
        return;
    }

    if (arg_received)
    {
        result = mmwlan_set_ampdu_enabled(enabled);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to set ampdu");
            return;
        }
    }

    if (!parse_integer_argument(cmd->mcs_fixedrate,
                                &value,
                                MMWLAN_MCS_NONE,
                                MMWLAN_MCS_MAX,
                                &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->mcs_fixedrate,
                             "mcs_fixedrate");
        return;
    }

    if (arg_received)
    {
        result = mmwlan_ate_override_rate_control((enum mmwlan_mcs)value,
                                                  MMWLAN_BW_NONE,
                                                  MMWLAN_GI_NONE);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate override");
            return;
        }
    }

    if (!parse_integer_argument(cmd->sta_type_support, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->sta_type_support,
                             "sta_type_support");
        return;
    }

    if (arg_received)
    {
        switch (value)
        {
            case 0:
                sta_args.sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
                break;

            case 1:
                sta_args.sta_type = MMWLAN_STA_TYPE_SENSOR;
                break;

            case 2:
                sta_args.sta_type = MMWLAN_STA_TYPE_NON_SENSOR;
                break;

            default:
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                     "Invalid sta type %d",
                                     value);
                return;
        }
    }

    if (!parse_option_argument(cmd->dup_1mhz_supt, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->dup_1mhz_supt,
                             "dup_1mhz_supt");
        return;
    }

    if (arg_received)
    {
        struct morse_cmd_req_set_transmission_rate command =
            MORSE_COMMAND_INIT(command,
                               MORSE_CMD_ID_SET_TRANSMISSION_RATE,
                               MMDRV_VIF_ID_INVALID,
                               .mcs_index = -1,
                               .bandwidth_mhz = -1,
                               .tx_80211ah_format = enabled ? 1 : -1,
                               .use_traveling_pilots = -1,
                               .use_sgi = -1,
                               .enabled = enabled ? 1 : 0);

        result = mmwlan_ate_execute_command((uint8_t *)&command, sizeof(command), NULL, NULL);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate transmission rate");
            return;
        }
    }

    if (!parse_option_argument(cmd->listen_interval, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->listen_interval,
                             "listen_interval");
        return;
    }
    if (arg_received)
    {
        listen_interval_args.listen_interval_enabled = enabled;
    }

    if (!parse_integer_argument(cmd->usf, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->usf,
                             "usf");
        return;
    }
    if (arg_received)
    {
        listen_interval_args.unified_scaling_factor = value;
    }

    if (!parse_integer_argument(cmd->unscaled_interval, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->unscaled_interval,
                             "unscaled_interval");
        return;
    }
    if (arg_received)
    {
        listen_interval_args.unscaled_interval = value;
    }

    uint16_t listen_interval = 0;
    if (listen_interval_args.listen_interval_enabled)
    {
        listen_interval_args.listen_interval =
            ((listen_interval_args.unified_scaling_factor << 14) |
             listen_interval_args.unscaled_interval);
        listen_interval = listen_interval_args.listen_interval;
    }
    result = mmwlan_set_listen_interval(listen_interval);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to set ate listen interval");
        return;
    }

    if (!parse_option_argument(cmd->nontim_mode, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->nontim_mode,
                             "nontim_mode");
        return;
    }
    if (arg_received && enabled)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Unsupported value %s for command %s",
                             cmd->nontim_mode,
                             "nontim_mode");
        return;
    }

    if (!parse_option_argument(cmd->ctrlresp_1mhz_preamble, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->ctrlresp_1mhz_preamble,
                             "ctrlresp_1mhz_preamble");
        return;
    }
    if (arg_received)
    {
        result = mmwlan_set_control_response_preamble_1mhz_out_en(enabled);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate control response");
            return;
        }
    }

    if (!parse_integer_argument(cmd->ndp_probing_support, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->ndp_probing_support,
                             "ndp_probing_support");
        return;
    }
    if (arg_received)
    {
        struct mmwlan_scan_config scan_config = MMWLAN_SCAN_CONFIG_INIT;
        scan_config.ndp_probe_enabled = value ? true : false;

        result = mmwlan_set_scan_config(&scan_config);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate ndp probe support");
            return;
        }
    }

    if (!parse_option_argument(cmd->twt_reqsupport, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->twt_reqsupport,
                             "twt_reqsupport");
        return;
    }
    if (arg_received)
    {
        twt_config_args.twt_mode = enabled ? MMWLAN_TWT_REQUESTER : MMWLAN_TWT_DISABLED;
    }

    if (twt_config_args.twt_mode == MMWLAN_TWT_REQUESTER)
    {
        if (!parse_integer_argument(cmd->wakeintervalexp, &value, 0, UINT8_MAX, &arg_received))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->wakeintervalexp,
                                 "wakeintervalexp");
            return;
        }
        if (arg_received)
        {
            twt_config_args.twt_wake_interval_exponent = value;
        }

        if (!parse_integer_argument(cmd->nominalminwakedur, &value, 0, UINT8_MAX, &arg_received))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->nominalminwakedur,
                                 "nominalminwakedur");
            return;
        }
        if (arg_received)
        {
            twt_config_args.twt_min_wake_duration_us = (value * TWT_WAKE_DURATION_UNIT_US);
        }

        if (!parse_integer_argument(cmd->wakeintervalmantissa,
                                    &value,
                                    0,
                                    UINT16_MAX,
                                    &arg_received))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->wakeintervalmantissa,
                                 "wakeintervalmantissa");
            return;
        }
        if (arg_received)
        {
            twt_config_args.twt_wake_interval_mantissa = value;
        }

        if (!parse_integer_argument(cmd->setupcommand, &value, 0, UINT16_MAX, &arg_received))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->setupcommand,
                                 "setupcommand");
            return;
        }
        if (arg_received)
        {
            switch (value)
            {
                case 0:
                    twt_config_args.twt_setup_command = MMWLAN_TWT_SETUP_REQUEST;
                    break;

                case 1:
                    twt_config_args.twt_setup_command = MMWLAN_TWT_SETUP_SUGGEST;
                    break;

                default:
                    emmet_pack_basic_rsp(raw_rsp,
                                         EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                         "Invalid TWT setup command %d",
                                         value);
                    return;
            }

            enum mmwlan_status status = mmwlan_twt_add_configuration(&twt_config_args);
            if (status != MMWLAN_SUCCESS)
            {
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__UNAVAILABLE,
                                     "mmwlan_twt_add_configuration() failed");
                return;
            }
        }
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_set_security(struct emmet_raw_rsp *raw_rsp,
                                    Emmet__CapiStaSetSecurityCmd *cmd)
{
    uint8_t ssid_len = strlen(cmd->ssid);
    uint8_t passphrase_len = strlen(cmd->passphrase);

    if (ssid_len < 1 || ssid_len >= MMWLAN_SSID_MAXLEN)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid SSID length");
        return;
    }

    sta_args.ssid_len = ssid_len;
    memcpy(sta_args.ssid, cmd->ssid, ssid_len);
    sta_args.passphrase_len = passphrase_len;
    if (passphrase_len)
    {
        memcpy(sta_args.passphrase, cmd->passphrase, passphrase_len);
    }
    else
    {
        memset(sta_args.passphrase, 0, MMWLAN_PASSPHRASE_MAXLEN);
    }
    sta_args.raw_sta_priority = -1;

    if (strcasecmp(cmd->type, "SAE") == 0)
    {
        sta_args.security_type = MMWLAN_SAE;
    }
    else if (strcasecmp(cmd->type, "OWE") == 0)
    {
        sta_args.security_type = MMWLAN_OWE;
    }
    else if (strcasecmp(cmd->type, "OPEN") == 0)
    {
        sta_args.security_type = MMWLAN_OPEN;
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid security type");
        return;
    }

    sta_args.pmf_mode = MMWLAN_PMF_REQUIRED;
    if (strcasecmp(cmd->pmf, "Required") == 0)
    {
        sta_args.pmf_mode = MMWLAN_PMF_REQUIRED;
    }
    else if (strcasecmp(cmd->pmf, "Disable") == 0)
    {
        sta_args.pmf_mode = MMWLAN_PMF_DISABLED;
    }

    memset(sta_args.sae_owe_ec_groups, 0, sizeof(sta_args.sae_owe_ec_groups));
    uint32_t ec_group_len = strlen(cmd->ecgroupid);
    if (ec_group_len)
    {
        if (!parse_group_ids((const uint8_t *)cmd->ecgroupid,
                             ec_group_len,
                             sta_args.sae_owe_ec_groups))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->ecgroupid,
                                 "ecgroupid");
            return;
        }
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_set_ip_config(struct emmet_raw_rsp *raw_rsp,
                                     Emmet__CapiStaSetIpConfigCmd *cmd)
{
    struct mmipal_ip_config ip_config = {};
    bool enabled, arg_received;

    if (!parse_boolean_argument(cmd->dhcp, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->dhcp,
                             "dhcp");
        return;
    }

    if (arg_received && enabled)
    {
        ip_config.mode = MMIPAL_DHCP;
    }
    else
    {
        ip_config.mode = MMIPAL_STATIC;
    }

    (void)mmosal_safer_strcpy(ip_config.ip_addr, cmd->ip, sizeof(ip_config.ip_addr));
    (void)mmosal_safer_strcpy(ip_config.netmask, cmd->mask, sizeof(ip_config.netmask));
    if (cmd->defaultgateway[0] == '\0')
    {
        (void)mmosal_safer_strcpy(ip_config.gateway_addr,
                                  WFA_DEFAULT_GATEWAY,
                                  sizeof(ip_config.gateway_addr));
    }
    else
    {
        (void)mmosal_safer_strcpy(ip_config.gateway_addr,
                                  cmd->defaultgateway,
                                  sizeof(ip_config.gateway_addr));
    }

    mmipal_set_ip_config(&ip_config);

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_associate(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaAssociateCmd *cmd)
{
    uint8_t ssid_len = strlen(cmd->ssid);
    uint8_t bssid[MMWLAN_MAC_ADDR_LEN] = { 0 };
    bool result;

    if (ssid_len < 1 || ssid_len >= MMWLAN_SSID_MAXLEN)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid SSID length");
        return;
    }

    sta_args.ssid_len = ssid_len;
    memcpy(sta_args.ssid, cmd->ssid, ssid_len);

    if (cmd->bssid != NULL && cmd->bssid[0] != '\0')
    {
        result = mm_str_to_mac_addr(cmd->bssid, bssid);

        if (!result)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid BSSID");
            return;
        }

        memcpy(sta_args.bssid, bssid, MMWLAN_MAC_ADDR_LEN);
    }

    enum mmwlan_status status = mmwlan_sta_enable(&sta_args, NULL);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "mmwlan_sta_enable() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_is_connected(struct emmet_raw_rsp *raw_rsp,
                                    Emmet__CapiStaIsConnectedCmd *cmd)
{
    MM_UNUSED(cmd);
    Emmet__CapiStaIsConnectedRsp info_rsp = EMMET__CAPI_STA_IS_CONNECTED_RSP__INIT;

    if (mmwlan_get_sta_state() == MMWLAN_STA_CONNECTED)
    {
        info_rsp.connected = 1;
    }
    else
    {
        info_rsp.connected = 0;
    }

    PACK_RSP(raw_rsp, info_rsp, sta_is_connected, STA_IS_CONNECTED);
}

static void handle_sta_get_bssid(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaGetBssidCmd *cmd)
{
    MM_UNUSED(cmd);
    Emmet__CapiStaGetBssidRsp info_rsp = EMMET__CAPI_STA_GET_BSSID_RSP__INIT;
    uint8_t bssid[MMWLAN_MAC_ADDR_LEN] = { 0 };
    char bssid_str[MM_MAC_ADDR_STR_LEN];

    enum mmwlan_status result = mmwlan_get_bssid(bssid);
    if ((result != MMWLAN_UNAVAILABLE) && (result != MMWLAN_SUCCESS))
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "Failed to get bssid");
        return;
    }

    mm_mac_addr_to_str(bssid, bssid_str);
    info_rsp.bssid = bssid_str;
    PACK_RSP(raw_rsp, info_rsp, sta_get_bssid, STA_GET_BSSID);
}

static void handle_sta_get_ip_config(struct emmet_raw_rsp *raw_rsp,
                                     Emmet__CapiStaGetIpConfigCmd *cmd)
{
    struct mmipal_ip_config ip_config = {};
    Emmet__CapiStaGetIpConfigRsp info_rsp = EMMET__CAPI_STA_GET_IP_CONFIG_RSP__INIT;
    enum mmipal_status status;

    MM_UNUSED(cmd);

    status = mmipal_get_ip_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to get ip config (status=%u)",
                             status);
        return;
    }

    if (ip_config.mode == MMIPAL_DISABLED)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__NOT_SUPPORTED, "IPv4 is disabled");
        return;
    }

    if (ip_config.mode == MMIPAL_DHCP)
    {
        info_rsp.dhcp = 1;
    }
    else
    {
        info_rsp.dhcp = 0;
    }

    info_rsp.ip = ip_config.ip_addr;
    info_rsp.mask = ip_config.netmask;
    info_rsp.primary_dns = ip_config.gateway_addr;

    PACK_RSP(raw_rsp, info_rsp, sta_get_ip_config, STA_GET_IP_CONFIG);
}

static void handle_sta_disconnect(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaDisconnectCmd *cmd)
{
    MM_UNUSED(cmd);
    enum mmwlan_status result = MMWLAN_ERROR;
    bool arg_received;
    int value = 0;

    if (mmwlan_get_sta_state() == MMWLAN_STA_CONNECTED)
    {
        if (!parse_integer_argument(cmd->maintain_profile, &value, 0, INT16_MAX, &arg_received))
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__INVALID_ARGUMENT,
                                 "Invalid value %s for command %s",
                                 cmd->maintain_profile,
                                 "maintain_profile");
            return;
        }
        if (arg_received && value)
        {
            result = mmwlan_sta_disable();
            if ((result != MMWLAN_SUCCESS) && (result != MMWLAN_SHUTDOWN_BLOCKED))
            {
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__OTHER_ERROR,
                                     "mmwlan_sta_disable() failed");
                return;
            }
        }
        else
        {
            result = mmwlan_shutdown();
            if (result != MMWLAN_SUCCESS)
            {
                emmet_pack_basic_rsp(raw_rsp,
                                     EMMET__STATUS_CODE__OTHER_ERROR,
                                     "mmwlan_shutdown() failed");
                return;
            }
        }
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_set_power_save(struct emmet_raw_rsp *raw_rsp,
                                      Emmet__CapiStaSetPowerSaveCmd *cmd)
{
    enum mmwlan_ps_mode mode;
    bool enabled, arg_received;

    if (!parse_option_argument(cmd->powersave, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->powersave,
                             "powersave");
        return;
    }

    if (!arg_received)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Parameter powersave not present");
        return;
    }

    mode = enabled ? MMWLAN_PS_ENABLED : MMWLAN_PS_DISABLED;
    enum mmwlan_status status = mmwlan_set_power_save_mode(mode);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "mmwlan_set_power_save_mode() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_set_rfeature(struct emmet_raw_rsp *raw_rsp,
                                    Emmet__CapiStaSetRfeatureCmd *cmd)
{
    bool enabled, arg_received;
    enum mmwlan_status result = MMWLAN_ERROR;
    int value;

    result = mmwlan_boot(NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__OTHER_ERROR, "mmwlan_boot() failed");
        return;
    }

    if (!parse_option_argument(cmd->ctrlresp_1mhz_preamble, &enabled, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->ctrlresp_1mhz_preamble,
                             "ctrlresp_1mhz_preamble");
        return;
    }
    if (arg_received)
    {
        result = mmwlan_set_control_response_preamble_1mhz_out_en(enabled);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate control response");
            return;
        }
    }

    if (!parse_integer_argument(cmd->response_indication, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->response_indication,
                             "response_indication");
        return;
    }
    if (arg_received)
    {
        struct morse_cmd_req_set_response_indication command =
            MORSE_COMMAND_INIT(command,
                               MORSE_CMD_ID_SET_RESPONSE_INDICATION,
                               MMDRV_VIF_ID_INVALID,
                               .response_indication = value);

        result = mmwlan_ate_execute_command((uint8_t *)&command, sizeof(command), NULL, NULL);
        if (result != MMWLAN_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__OTHER_ERROR,
                                 "Failed to set ate response indication");
            return;
        }
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_exec_action(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaExecActionCmd *cmd)
{
    MM_UNUSED(cmd);
    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_send_addba(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaSendAddbaCmd *cmd)
{
    enum mmwlan_status result = MMWLAN_ERROR;
    bool status, arg_received;
    uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
    uint8_t tid = 0;
    int value;

    if (!parse_integer_argument(cmd->tid, &value, 0, INT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->tid,
                             "tid");
        return;
    }
    if (arg_received)
    {
        tid = value;
    }

    if (cmd->dest_mac != NULL && cmd->dest_mac[0] != '\0')
    {
        status = mm_str_to_mac_addr(cmd->dest_mac, mac_addr);
        if (!status)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid dest_mac");
            return;
        }
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid dest_mac");
        return;
    }

    struct morse_cmd_req_iot_send_addba command =
        MORSE_COMMAND_INIT(command, MORSE_CMD_ID_IOT_SEND_ADDBA, MMDRV_VIF_ID_INVALID, .tid = tid);

    memcpy(command.mac_addr.octet, mac_addr, MMWLAN_MAC_ADDR_LEN);

    result = mmwlan_ate_execute_command((uint8_t *)&command, sizeof(command), NULL, NULL);
    if (result != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to execute sta_send_addba");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_sta_reassoc(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaReassocCmd *cmd)
{
    uint8_t bssid[MMWLAN_MAC_ADDR_LEN] = { 0 };
    bool result;
    enum mmwlan_status status = MMWLAN_ERROR;

    if (cmd->bssid != NULL && cmd->bssid[0] != '\0')
    {
        result = mm_str_to_mac_addr(cmd->bssid, bssid);

        if (!result)
        {
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid BSSID");
            return;
        }

        memcpy(sta_args.bssid, bssid, MMWLAN_MAC_ADDR_LEN);
    }

    struct morse_cmd_req command =
        MORSE_COMMAND_INIT(command, MORSE_CMD_ID_IOT_STA_REASSOC, MMDRV_VIF_ID_INVALID);

    status = mmwlan_ate_execute_command((uint8_t *)&command, sizeof(command), NULL, NULL);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to execute sta_reassoc");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void scan_rx_callback(const struct mmwlan_scan_result *result, void *arg)
{
    MM_UNUSED(result);
    MM_UNUSED(arg);
}

static void scan_complete_callback(enum mmwlan_scan_state state, void *arg)
{
    MM_UNUSED(state);
    MM_UNUSED(arg);
}

static void handle_sta_scan(struct emmet_raw_rsp *raw_rsp, Emmet__CapiStaScanCmd *cmd)
{
    MM_UNUSED(cmd);

    struct mmwlan_scan_req scan_req = MMWLAN_SCAN_REQ_INIT;
    scan_req.scan_rx_cb = scan_rx_callback;
    scan_req.scan_complete_cb = scan_complete_callback;

    enum mmwlan_status status = mmwlan_scan_request(&scan_req);
    if (status != MMWLAN_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "mmwlan_scan_request() failed");
        return;
    }

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_traffic_agent_reset(struct emmet_raw_rsp *raw_rsp,
                                       Emmet__CapiTrafficAgentResetCmd *cmd)
{
    MM_UNUSED(cmd);

    memset(&traffic_agent_args, 0, sizeof(traffic_agent_args));
    memset(&traffic_agent_state, 0, sizeof(traffic_agent_state));

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_traffic_send_ping(struct emmet_raw_rsp *raw_rsp,
                                     Emmet__CapiTrafficSendPingCmd *cmd)
{
    int framesize, framerate, duration;
    bool arg_received;
    Emmet__CapiTrafficSendPingRsp info_rsp = EMMET__CAPI_TRAFFIC_SEND_PING_RSP__INIT;
    struct mmping_stats stats;
    struct mmping_args ping_args = MMPING_ARGS_DEFAULT;
    enum mmipal_status status;

    framesize = MMPING_DEFAULT_DATA_SIZE;
    if (!parse_integer_argument(cmd->framesize,
                                &framesize,
                                MMPING_MIN_DATA_SIZE,
                                MMPING_MAX_DATA_SIZE,
                                &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->framesize,
                             "framesize");
        return;
    }

    framerate = 1;
    if (!parse_integer_argument(cmd->framerate, &framerate, 1, 100, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->framerate,
                             "framerate");
        return;
    }

    duration = 0;
    if (!parse_integer_argument(cmd->duration, &duration, 0, SHRT_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->duration,
                             "duration");
        return;
    }

    mmping_stats(&stats);
    if (stats.ping_is_running)
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Ping already running");
        return;
    }

    (void)mmosal_safer_strcpy(ping_args.ping_target,
                              cmd->destination,
                              sizeof(ping_args.ping_target));
    ping_args.ping_size = framesize;
    ping_args.ping_interval_ms = (1000 / framerate);
    ping_args.ping_count = duration;

    status = mmipal_get_local_addr(ping_args.ping_src, ping_args.ping_target);
    if (status != MMIPAL_SUCCESS)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__UNAVAILABLE,
                             "Failed to get local address");
        return;
    }

    mmping_start(&ping_args);

    info_rsp.streamid = 1;
    PACK_RSP(raw_rsp, info_rsp, traffic_send_ping, TRAFFIC_SEND_PING);
}

static void handle_traffic_stop_ping(struct emmet_raw_rsp *raw_rsp,
                                     Emmet__CapiTrafficStopPingCmd *cmd)
{
    int streamid;
    bool arg_received;
    struct mmping_stats stats;
    Emmet__CapiTrafficStopPingRsp info_rsp = EMMET__CAPI_TRAFFIC_STOP_PING_RSP__INIT;

    if (!parse_integer_argument(cmd->streamid, &streamid, 1, 1, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }
    if (!arg_received)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Parameter streamid not present");
    }

    mmping_stats(&stats);
    if (stats.ping_is_running)
    {
        mmping_stop();
        mmping_stats(&stats);
    }

    info_rsp.sent = stats.ping_total_count;
    info_rsp.replies = stats.ping_recv_count;
    PACK_RSP(raw_rsp, info_rsp, traffic_stop_ping, TRAFFIC_STOP_PING);
}

static void handle_traffic_agent_config(struct emmet_raw_rsp *raw_rsp,
                                        Emmet__CapiTrafficAgentConfigCmd *cmd)
{
    int value;
    bool arg_received;
    Emmet__CapiTrafficAgentConfigRsp info_rsp = EMMET__CAPI_TRAFFIC_AGENT_CONFIG_RSP__INIT;

    if (strcasecmp(cmd->profile, "iptv") == 0)
    {
        traffic_agent_args.profile = TRAFFIC_PROFILE_IPTV;
    }
    else if (strcasecmp(cmd->profile, "multicast") == 0)
    {
        traffic_agent_args.profile = TRAFFIC_PROFILE_MULTICAST;
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid profile");
        return;
    }

    if (strcasecmp(cmd->direction, "send") == 0)
    {
        traffic_agent_args.direction = TRAFFIC_DIRECTION_SEND;
    }
    else if (strcasecmp(cmd->direction, "receive") == 0)
    {
        traffic_agent_args.direction = TRAFFIC_DIRECTION_RECEIVE;
    }
    else
    {
        emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_ARGUMENT, "Invalid direction");
        return;
    }

    (void)mmosal_safer_strcpy(traffic_agent_args.source_ip_addr,
                              cmd->source,
                              sizeof(traffic_agent_args.source_ip_addr));

    if (!parse_integer_argument(cmd->sourceport, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->sourceport,
                             "sourceport");
        return;
    }
    if (arg_received)
    {
        traffic_agent_args.source_port = value;
    }

    (void)mmosal_safer_strcpy(traffic_agent_args.destination_ip_addr,
                              cmd->destination,
                              sizeof(traffic_agent_args.destination_ip_addr));

    if (!parse_integer_argument(cmd->destinationport, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->destinationport,
                             "destinationport");
        return;
    }
    if (arg_received)
    {
        traffic_agent_args.destination_port = value;
    }

    if (!parse_integer_argument(cmd->duration, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->duration,
                             "duration");
        return;
    }
    if (arg_received)
    {
        traffic_agent_args.duration = value;
    }

    if (!parse_integer_argument(cmd->payloadsize, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->payloadsize,
                             "payloadsize");
        return;
    }
    if (arg_received)
    {
        traffic_agent_args.payload_size = value;
    }


    traffic_agent_args.stream_id = 1;
    info_rsp.streamid = traffic_agent_args.stream_id;
    PACK_RSP(raw_rsp, info_rsp, traffic_agent_config, TRAFFIC_AGENT_CONFIG);
}

static void handle_traffic_agent_send(struct emmet_raw_rsp *raw_rsp,
                                      Emmet__CapiTrafficAgentSendCmd *cmd)
{
    int streamid;
    bool arg_received;
    Emmet__CapiTrafficAgentSendRsp info_rsp = EMMET__CAPI_TRAFFIC_AGENT_SEND_RSP__INIT;

    if (!parse_integer_argument(cmd->streamid, &streamid, 1, 1, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }
    if (!arg_received)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Parameter streamid not present");
    }

    struct traffic_agent_instance *inst = &(traffic_agent_state.client);
    if (inst->state == TRAFFIC_AGENT_RUNNING)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to start traffic agent client");
        return;
    }

    MMOSAL_ASSERT(inst->handle == NULL);

    struct mmiperf_client_args iperf_args = MMIPERF_CLIENT_ARGS_DEFAULT;
    (void)mmosal_safer_strcpy(iperf_args.server_addr,
                              traffic_agent_args.destination_ip_addr,
                              sizeof(iperf_args.server_addr));
    iperf_args.server_port = traffic_agent_args.destination_port;
    iperf_args.packet_size = traffic_agent_args.payload_size;
    iperf_args.amount = (traffic_agent_args.duration * (-100));
    iperf_args.report_fn = traffic_agent_report_handler;
    iperf_args.report_arg = inst;
    iperf_args.version = IPERF_VERSION_2_0_9;

    inst->state = TRAFFIC_AGENT_RUNNING;
    inst->handle = mmiperf_start_udp_client(&iperf_args);

    while (inst->state == TRAFFIC_AGENT_RUNNING)
    {
        mmosal_task_sleep(1000);
    }

    info_rsp.streamid = traffic_agent_args.stream_id;
    info_rsp.txframes = inst->report.tx_frames;
    info_rsp.txpayloadbytes = inst->report.bytes_transferred;
    info_rsp.rxframes = 0;
    info_rsp.rxpayloadbytes = 0;
    PACK_RSP(raw_rsp, info_rsp, traffic_agent_send, TRAFFIC_AGENT_SEND);
}

static void handle_traffic_agent_receive_start(struct emmet_raw_rsp *raw_rsp,
                                               Emmet__CapiTrafficAgentReceiveStartCmd *cmd)
{
    int value;
    bool arg_received;

    if (!parse_integer_argument(cmd->streamid, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }
    if (!arg_received || (value != traffic_agent_args.stream_id))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }

    struct traffic_agent_instance *inst = &(traffic_agent_state.server);
    if (inst->state == TRAFFIC_AGENT_RUNNING)
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__OTHER_ERROR,
                             "Failed to start traffic agent server");
        return;
    }

    MMOSAL_ASSERT(inst->handle == NULL);

    struct mmiperf_server_args iperf_args = MMIPERF_SERVER_ARGS_DEFAULT;
    iperf_args.local_port = traffic_agent_args.destination_port;
    (void)mmosal_safer_strcpy(iperf_args.local_addr,
                              traffic_agent_args.destination_ip_addr,
                              sizeof(iperf_args.local_addr));
    iperf_args.report_fn = traffic_agent_report_handler;
    iperf_args.report_arg = inst;
    iperf_args.version = IPERF_VERSION_2_0_9;

    inst->state = TRAFFIC_AGENT_RUNNING;

    if (strlen(iperf_args.local_addr) == 0)
    {
        enum mmipal_status status =
            mmipal_get_local_addr(iperf_args.local_addr, traffic_agent_args.source_ip_addr);
        if (status != MMIPAL_SUCCESS)
        {
            emmet_pack_basic_rsp(raw_rsp,
                                 EMMET__STATUS_CODE__UNAVAILABLE,
                                 "Failed to get local address");
            return;
        }
    }

    inst->handle = mmiperf_start_udp_server(&iperf_args);

    emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__SUCCESS, NULL);
}

static void handle_traffic_agent_receive_stop(struct emmet_raw_rsp *raw_rsp,
                                              Emmet__CapiTrafficAgentReceiveStopCmd *cmd)
{
    int value;
    bool arg_received;
    Emmet__CapiTrafficAgentReceiveStopRsp info_rsp =
        EMMET__CAPI_TRAFFIC_AGENT_RECEIVE_STOP_RSP__INIT;

    if (!parse_integer_argument(cmd->streamid, &value, 0, UINT16_MAX, &arg_received))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }
    if (!arg_received || (value != traffic_agent_args.stream_id))
    {
        emmet_pack_basic_rsp(raw_rsp,
                             EMMET__STATUS_CODE__INVALID_ARGUMENT,
                             "Invalid value %s for command %s",
                             cmd->streamid,
                             "streamid");
        return;
    }

    struct traffic_agent_instance *inst = &(traffic_agent_state.server);
    if (!mmiperf_stop(inst->handle))
    {
        MMLOG_WRN("Failed to stop iperf server\n");
    }

    if (inst->report.report_type != MMIPERF_STOPPED)
    {
        MMLOG_WRN("Server report not received\n");
    }

    info_rsp.streamid = traffic_agent_args.stream_id;
    info_rsp.outofsequenceframes = inst->report.out_of_sequence_frames;
    info_rsp.rxframes = inst->report.rx_frames;
    info_rsp.rxpayloadbytes = inst->report.bytes_transferred;
    info_rsp.txframes = inst->report.tx_frames;
    info_rsp.txpayloadbytes = 0;
    PACK_RSP(raw_rsp, info_rsp, traffic_agent_receive_stop, TRAFFIC_AGENT_RECEIVE_STOP);
}

void emmet_handle_capicmd(struct emmet_raw_rsp *raw_rsp, Emmet__CapiCmd *capicmd)
{
    switch (capicmd->cmd_case)
    {
        case EMMET__CAPI_CMD__CMD_DEVICE_GET_INFO:
            handle_device_get_info(raw_rsp, capicmd->device_get_info);
            break;

        case EMMET__CAPI_CMD__CMD_DEVICE_LIST_INTERFACES:
            handle_device_list_interfaces(raw_rsp, capicmd->device_list_interfaces);
            break;

        case EMMET__CAPI_CMD__CMD_STA_RESET_DEFAULT:
            handle_sta_reset_default(raw_rsp, capicmd->sta_reset_default);
            break;

        case EMMET__CAPI_CMD__CMD_STA_PRESET_TESTPARAMETERS:
            handle_sta_preset_testparameters(raw_rsp, capicmd->sta_preset_testparameters);
            break;

        case EMMET__CAPI_CMD__CMD_STA_GET_MAC_ADDRESS:
            handle_sta_get_mac_address(raw_rsp, capicmd->sta_get_mac_address);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SET_WIRELESS:
            handle_sta_set_wireless(raw_rsp, capicmd->sta_set_wireless);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SET_SECURITY:
            handle_sta_set_security(raw_rsp, capicmd->sta_set_security);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SET_IP_CONFIG:
            handle_sta_set_ip_config(raw_rsp, capicmd->sta_set_ip_config);
            break;

        case EMMET__CAPI_CMD__CMD_STA_ASSOCIATE:
            handle_sta_associate(raw_rsp, capicmd->sta_associate);
            break;

        case EMMET__CAPI_CMD__CMD_STA_IS_CONNECTED:
            handle_sta_is_connected(raw_rsp, capicmd->sta_is_connected);
            break;

        case EMMET__CAPI_CMD__CMD_STA_GET_BSSID:
            handle_sta_get_bssid(raw_rsp, capicmd->sta_get_bssid);
            break;

        case EMMET__CAPI_CMD__CMD_STA_GET_IP_CONFIG:
            handle_sta_get_ip_config(raw_rsp, capicmd->sta_get_ip_config);
            break;

        case EMMET__CAPI_CMD__CMD_STA_DISCONNECT:
            handle_sta_disconnect(raw_rsp, capicmd->sta_disconnect);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SET_POWER_SAVE:
            handle_sta_set_power_save(raw_rsp, capicmd->sta_set_power_save);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SET_RFEATURE:
            handle_sta_set_rfeature(raw_rsp, capicmd->sta_set_rfeature);
            break;

        case EMMET__CAPI_CMD__CMD_STA_EXEC_ACTION:
            handle_sta_exec_action(raw_rsp, capicmd->sta_exec_action);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SEND_ADDBA:
            handle_sta_send_addba(raw_rsp, capicmd->sta_send_addba);
            break;

        case EMMET__CAPI_CMD__CMD_STA_REASSOC:
            handle_sta_reassoc(raw_rsp, capicmd->sta_reassoc);
            break;

        case EMMET__CAPI_CMD__CMD_STA_SCAN:
            handle_sta_scan(raw_rsp, capicmd->sta_scan);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_AGENT_RESET:
            handle_traffic_agent_reset(raw_rsp, capicmd->traffic_agent_reset);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_SEND_PING:
            handle_traffic_send_ping(raw_rsp, capicmd->traffic_send_ping);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_STOP_PING:
            handle_traffic_stop_ping(raw_rsp, capicmd->traffic_stop_ping);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_AGENT_CONFIG:
            handle_traffic_agent_config(raw_rsp, capicmd->traffic_agent_config);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_AGENT_SEND:
            handle_traffic_agent_send(raw_rsp, capicmd->traffic_agent_send);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_AGENT_RECEIVE_START:
            handle_traffic_agent_receive_start(raw_rsp, capicmd->traffic_agent_receive_start);
            break;

        case EMMET__CAPI_CMD__CMD_TRAFFIC_AGENT_RECEIVE_STOP:
            handle_traffic_agent_receive_stop(raw_rsp, capicmd->traffic_agent_receive_stop);
            break;

        default:
            emmet_pack_basic_rsp(raw_rsp, EMMET__STATUS_CODE__INVALID_COMMAND, "Invalid command");
            break;
    }
}
