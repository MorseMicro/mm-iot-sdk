/**
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmwlan.h"
#include "mmipal.h"
#include "mmutils.h"
#include "mmconfig.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_ip.h"
#include "mmagic.h"
#include "mmagic_core_utils.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_ip.def"

/** Maximum number of DNS servers to attempt to retrieve from config store. */
#ifndef DNS_MAX_SERVERS
#define DNS_MAX_SERVERS                 2
#endif

static struct mmagic_ip_config default_config =
{
    .dhcp_enabled = true,
    .ip_addr = {.addr = "192.168.1.2"},
    .gateway = {.addr = "192.168.1.1"},
    .netmask = {.addr = "255.255.255.0"},
};

static enum mmipal_link_state current_link_state = MMIPAL_LINK_DOWN;

/**
 * Link status callback
 *
 * @param link_status The current link status.
 */
static void mmagic_core_ip_link_status_cb(const struct mmipal_link_status *link_status, void *arg)
{
    struct mmagic_ip_data *data = (struct mmagic_ip_data *)arg;

    uint32_t time_ms = mmosal_get_time_ms();
    current_link_state = link_status->link_state;
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        size_t ii;

        /* Set the DNS servers using the given overrides (if any). This will override the values
         * set by DHCP (if any). */
        if (data->config.dns_server0.addr[0] != '\0')
        {
            (void)mmipal_set_dns_server(0, data->config.dns_server0.addr);
        }

        if (data->config.dns_server1.addr[0] != '\0')
        {
            (void)mmipal_set_dns_server(0, data->config.dns_server1.addr);
        }

        printf("Link is up. Time: %lu ms", time_ms);
        printf(", IP: %s", link_status->ip_addr);
        printf(", Netmask: %s", link_status->netmask);
        printf(", Gateway: %s", link_status->gateway);

        for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
        {
            enum mmipal_status status;
            char addr_str[MMIPAL_IPADDR_STR_MAXLEN];

            status = mmipal_get_dns_server(ii, addr_str);
            if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
            {
                printf(", DNS server %u: %s\n", ii, addr_str);
            }
        }

        printf("\n");
    }
    else
    {
        printf("Link is down. Time: %lu ms\n", time_ms);
    }
}

static void mmagic_core_ip_init_interface(struct mmagic_ip_data *data)
{
    struct mmipal_init_args init_args = MMIPAL_INIT_ARGS_DEFAULT;

    mmosal_safer_strcpy(init_args.ip_addr, data->config.ip_addr.addr,
                        sizeof(init_args.ip_addr));
    mmosal_safer_strcpy(init_args.netmask, data->config.netmask.addr,
                        sizeof(init_args.netmask));
    mmosal_safer_strcpy(init_args.gateway_addr, data->config.gateway.addr,
                        sizeof(init_args.gateway_addr));

    init_args.mode = data->config.dhcp_enabled ? data->config.dhcp_offload ?
        MMIPAL_DHCP_OFFLOAD : MMIPAL_DHCP : MMIPAL_STATIC;

    /* Initialize IP stack. */
    if (mmipal_init(&init_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_ext_link_status_callback(mmagic_core_ip_link_status_cb, data);
}

void mmagic_core_ip_init(struct mmagic_data *core)
{
    struct mmagic_ip_data *data = mmagic_data_get_ip(core);
    memcpy(&data->config, &default_config, sizeof(data->config));
}

void mmagic_core_ip_start(struct mmagic_data *core)
{
    struct mmagic_ip_data *data = mmagic_data_get_ip(core);
    mmagic_core_ip_init_interface(data);
}

/********* MMAGIC Core IP ops **********/
enum mmagic_status mmagic_core_ip_status(struct mmagic_data *core,
                                         struct mmagic_core_ip_status_rsp_args *rsp_args)
{
    struct mmagic_ip_data *data = mmagic_data_get_ip(core);
    struct mmipal_ip_config ip_config = {};
    mmipal_ip_addr_t broadcast_addr = {};
    enum mmipal_status status;
    size_t ii;

    (void)data;

    memset(rsp_args, 0, sizeof(*rsp_args));

    status = mmipal_get_ip_config(&ip_config);
    if (status != MMIPAL_SUCCESS)
    {
        return mmagic_mmipal_status_to_mmagic_status(status);
    }

    rsp_args->status.link_state =
        current_link_state == MMIPAL_LINK_UP ? MMAGIC_IP_LINK_STATE_UP : MMAGIC_IP_LINK_STATE_DOWN;

    rsp_args->status.dhcp_enabled = ip_config.mode == MMIPAL_DHCP;

    mmosal_safer_strcpy(rsp_args->status.ip_addr.addr, ip_config.ip_addr,
                        sizeof(rsp_args->status.ip_addr.addr));

    mmosal_safer_strcpy(rsp_args->status.netmask.addr, ip_config.netmask,
                        sizeof(rsp_args->status.netmask.addr));

    mmosal_safer_strcpy(rsp_args->status.gateway.addr, ip_config.gateway_addr,
                        sizeof(rsp_args->status.gateway.addr));

    status = mmipal_get_ip_broadcast_addr(broadcast_addr);
    if (status != MMIPAL_SUCCESS)
    {
        return mmagic_mmipal_status_to_mmagic_status(status);
    }
    mmosal_safer_strcpy(rsp_args->status.broadcast.addr, broadcast_addr,
                        sizeof(rsp_args->status.broadcast.addr));

    MM_STATIC_ASSERT(sizeof(rsp_args->status.dns_servers[0].addr) >= MMIPAL_IPADDR_STR_MAXLEN,
                     "DNS server IP address buffer too small");
    for (ii = 0; ii < MM_ARRAY_COUNT(rsp_args->status.dns_servers); ii++)
    {
        (void)mmipal_get_dns_server(ii, rsp_args->status.dns_servers[ii].addr);
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_ip_reload(struct mmagic_data *core)
{
    struct mmagic_ip_data *data = mmagic_data_get_ip(core);
    struct mmipal_ip_config config;

    mmosal_safer_strcpy(config.ip_addr, data->config.ip_addr.addr,
                        sizeof(config.ip_addr));
    mmosal_safer_strcpy(config.netmask, data->config.netmask.addr,
                        sizeof(config.netmask));
    mmosal_safer_strcpy(config.gateway_addr, data->config.gateway.addr,
                        sizeof(config.gateway_addr));
    config.mode = data->config.dhcp_enabled ? data->config.dhcp_offload ?
        MMIPAL_DHCP_OFFLOAD : MMIPAL_DHCP : MMIPAL_STATIC;

    mmipal_set_ip_config(&config);

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_ip_enable_tcp_keepalive_offload(
    struct mmagic_data *core,
    const struct mmagic_core_ip_enable_tcp_keepalive_offload_cmd_args *cmd_args)
{
    struct mmwlan_tcp_keepalive_offload_args args = MMWLAN_TCP_KEEPALIVE_OFFLOAD_ARGS_INIT;
    MM_UNUSED(core);

    args.period_s = cmd_args->period_s;
    args.retry_count = cmd_args->retry_count;
    args.retry_interval_s = cmd_args->retry_interval_s;
    args.set_cfgs = MMWLAN_TCP_KEEPALIVE_SET_CFG_TIMING_ONLY;

    enum mmwlan_status status = mmwlan_enable_tcp_keepalive_offload(&args);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_ip_disable_tcp_keepalive_offload(struct mmagic_data *core)
{
    MM_UNUSED(core);

    enum mmwlan_status status = mmwlan_disable_tcp_keepalive_offload();
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_ip_set_whitelist_filter(struct mmagic_data *core,
                                                       const struct
                                                       mmagic_core_ip_set_whitelist_filter_cmd_args
                                                       *cmd_args)
{
    uint32_t ip1, ip2, ip3, ip4;
    struct mmwlan_config_whitelist whitelist = { 0 };

    MM_UNUSED(core);

    whitelist.flags = 0;
    int ret = sscanf(cmd_args->src_ip.addr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4);
    if (ret != 4)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    whitelist.src_ip = ip1 + (ip2 << 8) + (ip3 << 16) + (ip4 << 24);

    ret = sscanf(cmd_args->dest_ip.addr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4);
    if (ret != 4)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    whitelist.dest_ip = ip1 + (ip2 << 8) + (ip3 << 16) + (ip4 << 24);

    ret = sscanf(cmd_args->netmask.addr, "%lu.%lu.%lu.%lu", &ip1, &ip2, &ip3, &ip4);
    if (ret != 4)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    whitelist.netmask = ip1 + (ip2 << 8) + (ip3 << 16) + (ip4 << 24);

    whitelist.src_port = cmd_args->src_port;
    whitelist.dest_port = cmd_args->dest_port;
    whitelist.ip_protocol = cmd_args->ip_protocol;
    whitelist.llc_protocol = cmd_args->llc_protocol;

    enum mmwlan_status status = mmwlan_set_whitelist_filter(&whitelist);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}

enum mmagic_status mmagic_core_ip_clear_whitelist_filter(struct mmagic_data *core)
{
    struct mmwlan_config_whitelist whitelist = { 0 };

    MM_UNUSED(core);

    whitelist.flags = MMWLAN_WHITELIST_FLAGS_CLEAR;

    enum mmwlan_status status = mmwlan_set_whitelist_filter(&whitelist);
    return mmagic_mmwlan_status_to_mmagic_status(status);
}
