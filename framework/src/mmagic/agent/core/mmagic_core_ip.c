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

/** Maximum number of DNS servers to attempt to retrieve from config store. */
#ifndef DNS_MAX_SERVERS
#define DNS_MAX_SERVERS 2
#endif

static struct mmagic_ip_config default_config = {
    .ip_addr = { .addr = "192.168.1.2" },
    .netmask = { .addr = "255.255.255.0" },
    .gateway = { .addr = "192.168.1.1" },
    .dns_server0 = { .addr = "" },
    .dns_server1 = { .addr = "" },
    .dhcp_enabled = true,
    .link_status_evt_en = true,
};

static enum mmipal_link_state current_link_state = MMIPAL_LINK_DOWN;

/**
 * Link status callback
 *
 * @param link_status The current link status.
 */
static void mmagic_core_ip_link_status_cb(const struct mmipal_link_status *link_status, void *arg)
{
    struct mmagic_data *data = (struct mmagic_data *)arg;
    current_link_state = link_status->link_state;

    struct mmagic_ip_data *ip_data = mmagic_data_get_ip(data);
    if (!ip_data->config.link_status_evt_en)
    {
        return;
    }

    struct mmagic_core_ip_status_rsp_args ip_status;
    mmagic_core_ip_status(data, &ip_status);

    uint32_t time_ms = mmosal_get_time_ms();

    struct mmagic_core_event_ip_link_status_args link_status_args;
    memcpy(&link_status_args.ip_link_status, &ip_status.status, sizeof(ip_status.status));
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        size_t ii;

        /* Set the DNS servers using the given overrides (if any). This will override the values
         * set by DHCP (if any). */
        if (ip_data->config.dns_server0.addr[0] != '\0')
        {
            (void)mmipal_set_dns_server(0, ip_data->config.dns_server0.addr);
        }

        if (ip_data->config.dns_server1.addr[0] != '\0')
        {
            (void)mmipal_set_dns_server(0, ip_data->config.dns_server1.addr);
        }

        mmosal_printf("Link is up. Time: %lu ms", time_ms);
        mmosal_printf(", IP: %s", link_status->ip_addr);
        mmosal_printf(", Netmask: %s", link_status->netmask);
        mmosal_printf(", Gateway: %s", link_status->gateway);

        for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
        {
            enum mmipal_status status;
            char addr_str[MMIPAL_IPADDR_STR_MAXLEN];

            status = mmipal_get_dns_server(ii, addr_str);
            if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
            {
                mmosal_printf(", DNS server %u: %s\n", ii, addr_str);
            }
        }

        mmosal_printf("\n");
    }
    else
    {
        mmosal_printf("Link is down. Time: %lu ms\n", time_ms);
    }

    mmagic_core_event_ip_link_status(data, &link_status_args);
}

static void mmagic_core_ip_init_interface(struct mmagic_data *data)
{
    struct mmipal_init_args init_args = MMIPAL_INIT_ARGS_DEFAULT;
    struct mmagic_ip_data *ip_data = mmagic_data_get_ip(data);

    mmosal_safer_strcpy(init_args.ip_addr, ip_data->config.ip_addr.addr, sizeof(init_args.ip_addr));
    mmosal_safer_strcpy(init_args.netmask, ip_data->config.netmask.addr, sizeof(init_args.netmask));
    mmosal_safer_strcpy(init_args.gateway_addr,
                        ip_data->config.gateway.addr,
                        sizeof(init_args.gateway_addr));

    init_args.mode = ip_data->config.dhcp_enabled ? MMIPAL_DHCP : MMIPAL_STATIC;

    /* Initialize IP stack. */
    if (mmipal_init(&init_args) != MMIPAL_SUCCESS)
    {
        mmosal_printf("Error initializing network interface\n");
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
    mmagic_core_ip_init_interface(core);
    data->is_started = true;
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

    if (ip_config.mode == MMIPAL_DISABLED)
    {
        return MMAGIC_STATUS_NOT_SUPPORTED;
    }

    rsp_args->status.link_state = current_link_state == MMIPAL_LINK_UP ? MMAGIC_IP_LINK_STATE_UP :
                                                                         MMAGIC_IP_LINK_STATE_DOWN;

    rsp_args->status.dhcp_enabled = ip_config.mode == MMIPAL_DHCP;

    mmosal_safer_strcpy(rsp_args->status.ip_addr.addr,
                        ip_config.ip_addr,
                        sizeof(rsp_args->status.ip_addr.addr));

    mmosal_safer_strcpy(rsp_args->status.netmask.addr,
                        ip_config.netmask,
                        sizeof(rsp_args->status.netmask.addr));

    mmosal_safer_strcpy(rsp_args->status.gateway.addr,
                        ip_config.gateway_addr,
                        sizeof(rsp_args->status.gateway.addr));

    status = mmipal_get_ip_broadcast_addr(broadcast_addr);
    if (status != MMIPAL_SUCCESS)
    {
        return mmagic_mmipal_status_to_mmagic_status(status);
    }
    mmosal_safer_strcpy(rsp_args->status.broadcast.addr,
                        broadcast_addr,
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

    mmosal_safer_strcpy(config.ip_addr, data->config.ip_addr.addr, sizeof(config.ip_addr));
    mmosal_safer_strcpy(config.netmask, data->config.netmask.addr, sizeof(config.netmask));
    mmosal_safer_strcpy(config.gateway_addr,
                        data->config.gateway.addr,
                        sizeof(config.gateway_addr));
    config.mode = data->config.dhcp_enabled ? MMIPAL_DHCP : MMIPAL_STATIC;

    mmipal_set_ip_config(&config);

    return MMAGIC_STATUS_OK;
}
