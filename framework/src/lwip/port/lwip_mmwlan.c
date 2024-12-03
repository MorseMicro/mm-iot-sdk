/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lwip_mmwlan.h"
#include "mmnetif.h"
#include "mmwlan.h"
#include "mmutils.h"
#include "lwip/stats.h"
#include "lwip/ip_addr.h"

static struct netif lwip_mmnetif;

static link_status_cb_fn_t netif_status_callback;
static enum link_state current_link_state = LINK_DOWN;

#if LWIP_IPV4
static bool dhcp_enabled;
#endif
#if LWIP_IPV6
static enum lwip_mmwlan_ip6_addr_mode ip6_mode = IP6_ADDR_STATIC;
#endif


#if LWIP_IPV4
void lwip_mmwlan_get_ip_config(struct lwip_mmwlan_ip_config *config)
{
    if (config != NULL)
    {
        config->dhcp_enabled = dhcp_enabled;
        ip_addr_copy(config->ip_addr, lwip_mmnetif.ip_addr);
        ip_addr_copy(config->netmask, lwip_mmnetif.netmask);
        ip_addr_copy(config->gateway_addr, lwip_mmnetif.gw);
    }
}


void lwip_mmwlan_set_ip_config(const struct lwip_mmwlan_ip_config *config)
{
    struct netif *netif = &lwip_mmnetif;
    err_t result;

    struct lwip_mmwlan_init_args args;

    if (config->dhcp_enabled == true)
    {
        args.mode = IP_ADDR_DHCP;
        ip_addr_copy(args.ip_addr, ip_addr_any);
        ip_addr_copy(args.netmask, ip_addr_any);
        ip_addr_copy(args.gateway_addr, ip_addr_any);
    }
    else
    {
        args.mode = IP_ADDR_STATIC;
        ip_addr_copy(args.ip_addr, config->ip_addr);
        ip_addr_copy(args.netmask, config->netmask);
        ip_addr_copy(args.gateway_addr, config->gateway_addr);
    }

    LOCK_TCPIP_CORE();

    if (args.mode == IP_ADDR_STATIC && dhcp_enabled)
    {
        /* Stop DHCP if it was started earlier before setting static IP */
        dhcp_stop(netif);
    }

    netif_set_addr(netif, ip_2_ip4(&(config->ip_addr)),
                    ip_2_ip4(&(config->netmask)), ip_2_ip4(&(config->gateway_addr)));
    if (args.mode == IP_ADDR_DHCP)
    {
        result = dhcp_start(netif);
        LWIP_ASSERT("DHCP start error", result == ERR_OK);
        dhcp_enabled = true;
    }
    else
    {
        dhcp_enabled = false;
    }

    UNLOCK_TCPIP_CORE();
}

void lwip_mmwlan_get_ip_config_str(struct lwip_mmwlan_ip_config_str *config)
{
    struct lwip_mmwlan_ip_config _config;
    char *result;

    lwip_mmwlan_get_ip_config(&_config);
    config->dhcp_enabled = _config.dhcp_enabled;

    result = ipaddr_ntoa_r(&_config.ip_addr, config->ip_addr, sizeof(config->ip_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);

    result = ipaddr_ntoa_r(&_config.netmask, config->netmask, sizeof(config->netmask));
    LWIP_ASSERT("IP buf too short", result != NULL);

    result = ipaddr_ntoa_r(&_config.gateway_addr,
                           config->gateway_addr, sizeof(config->gateway_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);
}

err_t lwip_mmwlan_set_ip_config_str(const struct lwip_mmwlan_ip_config_str *config)
{
    struct lwip_mmwlan_ip_config _config;
    int result;

    _config.dhcp_enabled = config->dhcp_enabled;

    result = ipaddr_aton(config->ip_addr, &_config.ip_addr);
    if (!result)
    {
        return ERR_ARG;
    }
    result = ipaddr_aton(config->netmask, &_config.netmask);
    if (!result)
    {
        return ERR_ARG;
    }
    result = ipaddr_aton(config->gateway_addr, &_config.gateway_addr);
    if (!result)
    {
        return ERR_ARG;
    }

    lwip_mmwlan_set_ip_config(&_config);
    return ERR_OK;
}
#else
void lwip_mmwlan_get_ip_config(struct lwip_mmwlan_ip_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
}


void lwip_mmwlan_set_ip_config(const struct lwip_mmwlan_ip_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
}

void lwip_mmwlan_get_ip_config_str(struct lwip_mmwlan_ip_config_str *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
}

err_t lwip_mmwlan_set_ip_config_str(const struct lwip_mmwlan_ip_config_str *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
    return ERR_ARG;
}
#endif

#if LWIP_IPV6
void lwip_mmwlan_get_ip6_config(struct lwip_mmwlan_ip6_config *config)
{
    struct netif *netif = &lwip_mmnetif;
    int i;
    if (config == NULL)
    {
        return;
    }
    config->ip6_mode = ip6_mode;
    for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++)
    {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)))
        {
            ip_addr_copy(config->ip6_addr[i], lwip_mmnetif.ip6_addr[i]);
        }
        else
        {
            ip_addr_copy(config->ip6_addr[i], ip6_addr_any);
        }
    }
}


void lwip_mmwlan_set_ip6_config(const struct lwip_mmwlan_ip6_config *config)
{
    struct netif *netif = &lwip_mmnetif;
    err_t result;
    int i;

    LOCK_TCPIP_CORE();

    if (config->ip6_mode == IP6_ADDR_STATIC)
    {
        if (ip6_mode != IP6_ADDR_STATIC)
        {
            dhcp6_disable(netif);
            netif_set_ip6_autoconfig_enabled(netif, 0);
            ip6_mode = IP6_ADDR_STATIC;
        }

        if (!ip6_addr_islinklocal(ip_2_ip6(&(config->ip6_addr[0]))))
        {
            printf("First address must be linklocal address (address start with fe80)\n");
        }

        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++)
        {
            if (ip_addr_isany_val(config->ip6_addr[i]))
            {
                netif_ip6_addr_set(netif, i, IP6_ADDR_ANY6);
                netif_ip6_addr_set_state(netif, i, IP6_ADDR_INVALID);
            }
            else
            {
                netif_ip6_addr_set(netif, i, ip_2_ip6(&(config->ip6_addr[i])));
                netif_ip6_addr_set_state(netif, i, IP6_ADDR_TENTATIVE);
                netif_ip6_addr_set_valid_life(netif, i, IP6_ADDR_LIFE_STATIC);
            }
        }
    }
    else
    {
        if (ip6_mode == IP6_ADDR_STATIC)
        {
            for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++)
            {
                netif_ip6_addr_set(netif, i, IP6_ADDR_ANY6);
                netif_ip6_addr_set_state(netif, i, IP6_ADDR_INVALID);
            }
        }
        netif_set_ip6_autoconfig_enabled(netif, 1);
        netif_create_ip6_linklocal_address(netif, 1);
        ip6_mode = IP6_ADDR_AUTOCONFIG;
    }

    if (config->ip6_mode == IP6_ADDR_DHCP6_STATELESS)
    {
        result = dhcp6_enable_stateless(netif);
        LWIP_ASSERT("Stateless DHCP6 start error", result == ERR_OK);
        ip6_mode = IP6_ADDR_DHCP6_STATELESS;
    }
    else
    {
        dhcp6_disable(netif);
    }

    UNLOCK_TCPIP_CORE();
}

void lwip_mmwlan_get_ip6_config_str(struct lwip_mmwlan_ip6_config_str *config)
{
    struct lwip_mmwlan_ip6_config _config = { IP6_ADDR_STATIC };
    unsigned ii;

    lwip_mmwlan_get_ip6_config(&_config);

    config->ip6_mode = _config.ip6_mode;
    for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
    {
        char *result = ipaddr_ntoa_r(&_config.ip6_addr[ii],
                                     config->ip6_addr[ii], sizeof(config->ip6_addr[ii]));
        LWIP_ASSERT("IP buf too short", result != NULL);
    }
}

err_t lwip_mmwlan_set_ip6_config_str(const struct lwip_mmwlan_ip6_config_str *config)
{
    struct lwip_mmwlan_ip6_config _config = { .ip6_mode = config->ip6_mode };
    unsigned ii;

    for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
    {
        int result = ipaddr_aton(config->ip6_addr[ii], &_config.ip6_addr[ii]);
        if (!result)
        {
            return ERR_ARG;
        }
    }

    lwip_mmwlan_set_ip6_config(&_config);
    return ERR_OK;
}
#else
void lwip_mmwlan_get_ip6_config(struct lwip_mmwlan_ip6_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
}


void lwip_mmwlan_set_ip6_config(const struct lwip_mmwlan_ip6_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
}

void lwip_mmwlan_get_ip6_config_str(struct lwip_mmwlan_ip6_config_str *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
}

err_t lwip_mmwlan_set_ip6_config_str(const struct lwip_mmwlan_ip6_config_str *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
    return ERR_ARG;
}
#endif

static bool lwip_mmwlan_link_status_check(struct netif *netif)
{
    bool ip4_addr_check = true;
    bool ip6_addr_check = true;

#if LWIP_IPV4
    ip4_addr_check = !ip_addr_isany(&(netif->ip_addr));
#endif

#if LWIP_IPV6
    ip6_addr_check = !ip_addr_isany(netif->ip6_addr)
                 && ip6_addr_isvalid(netif_ip6_addr_state(netif, 0));
#endif

    return ip4_addr_check && ip6_addr_check && netif_is_link_up(netif);
}

static void lwip_mmwlan_link_status_callback(struct netif *netif)
{
    if (netif_status_callback != NULL)
    {
        enum link_state new_link_state = LINK_DOWN;

        if (lwip_mmwlan_link_status_check(netif))
        {
            new_link_state = LINK_UP;
        }

        if (current_link_state != new_link_state)
        {
            current_link_state = new_link_state;
#if LWIP_IPV4
            netif_status_callback(new_link_state, &netif->ip_addr, &netif->netmask, &netif->gw);
#endif
        }
    }
}

void lwip_mmwlan_set_link_status_callback(link_status_cb_fn_t fn)
{
    struct netif *netif = &lwip_mmnetif;
    netif_status_callback = fn;

    LOCK_TCPIP_CORE();

    if (fn == NULL)
    {
        netif_set_link_callback(netif, NULL);
        netif_set_status_callback(netif, NULL);
    }
    else
    {
        netif_set_link_callback(netif, lwip_mmwlan_link_status_callback);
        netif_set_status_callback(netif, lwip_mmwlan_link_status_callback);
    }

    UNLOCK_TCPIP_CORE();
}

static volatile bool tcpip_init_done = false;

static void tcpip_init_done_handler(void *arg)
{
    struct netif *netif = &lwip_mmnetif;
    struct lwip_mmwlan_init_args *args = (struct lwip_mmwlan_init_args *)arg;

    netif_add_noaddr(netif, NULL, mmnetif_init, tcpip_input);
    netif_set_default(netif);
    netif_set_up(netif);

#if LWIP_IPV4
    err_t result;
    if (args->mode == IP_ADDR_DHCP)
    {
        result = dhcp_start(netif);
        LWIP_ASSERT("DHCP start error", result == ERR_OK);
        dhcp_enabled = true;
    }
    else
    {
        netif_set_addr(netif, ip_2_ip4(&(args->ip_addr)),
                        ip_2_ip4(&(args->netmask)), ip_2_ip4(&(args->gateway_addr)));
    }
#endif

#if LWIP_IPV6
    err_t result6;
    if (args->ip6_mode == IP6_ADDR_STATIC)
    {
        ip6_mode = IP6_ADDR_STATIC;
        netif_ip6_addr_set(netif, 0, ip_2_ip6(&(args->ip6_addr)));
        netif_ip6_addr_set_state(netif, 0, IP6_ADDR_TENTATIVE);
    }
    else
    {
        ip6_mode = IP6_ADDR_AUTOCONFIG;
        netif_set_ip6_autoconfig_enabled(netif, 1);
        netif_create_ip6_linklocal_address(netif, 1);
    }

    if (args->ip6_mode == IP6_ADDR_DHCP6_STATELESS)
    {
        result6 = dhcp6_enable_stateless(netif);
        LWIP_ASSERT("Stateless DHCP6 start error", result6 == ERR_OK);
        ip6_mode = IP6_ADDR_DHCP6_STATELESS;
    }
#endif

    mmosal_free(args);

    tcpip_init_done = true;
}

err_t lwip_mmwlan_init(const struct lwip_mmwlan_init_args *args)
{
    netif_status_callback = NULL;

    /* Validate arguments */
#if LWIP_IPV4
    switch (args->mode)
    {
    case IP_ADDR_STATIC:
        if (ip_addr_isany_val(args->ip_addr))
        {
            printf("IP address not specified\n");
            return ERR_VAL;
        }
        break;

    case IP_ADDR_DHCP:
        if (LWIP_DHCP == 0)
        {
            printf("DHCP not compiled in\n");
            return ERR_ARG;
        }
        break;

    case IP_ADDR_AUTOIP:
        if (LWIP_AUTOIP == 0)
        {
            printf("AUTOIP not compiled in\n");
            return ERR_ARG;
        }
        break;
    }
#endif

#if LWIP_IPV6
    switch (args->ip6_mode)
    {
    case IP6_ADDR_STATIC:
        if (ip_addr_isany_val(args->ip6_addr))
        {
            printf("IP address not specified\n");
            return ERR_VAL;
        }
        break;

    case IP6_ADDR_AUTOCONFIG:
        if (LWIP_IPV6_AUTOCONFIG == 0)
        {
            printf("AUTOCONFIG not compiled in\n");
            return ERR_ARG;
        }
        break;

    case IP6_ADDR_DHCP6_STATELESS:
        if (LWIP_IPV6_DHCP6_STATELESS == 0)
        {
            printf("DHCP6_STATELESS not compiled in\n");
            return ERR_ARG;
        }
        break;
    }
#endif

    /* We take a copy of the arguments so that we can pass them to the second stage initialiser. */
    struct lwip_mmwlan_init_args *args_copy =
            (struct lwip_mmwlan_init_args *)mmosal_malloc(sizeof(*args));
    if (args_copy == NULL)
    {
        printf("malloc failure\n");
        return ERR_MEM;
    }
    memcpy(args_copy, args, sizeof(*args));

    tcpip_init(tcpip_init_done_handler, args_copy);

    /* Block until initialisation is complete */
    while (!tcpip_init_done)
    {
        mmosal_task_sleep(10);
    }

    return ERR_OK;
}

void lwip_mmwlan_get_link_packet_counts(uint32_t *tx_packets, uint32_t *rx_packets)
{
#if LWIP_STATS
    *tx_packets = lwip_stats.link.xmit;
    *rx_packets = lwip_stats.link.recv;
#else
    *tx_packets = 0;
    *rx_packets = 0;
#endif
}


void lwip_mmwlan_set_tx_qos_tid(uint8_t tid)
{
    struct netif *netif = &lwip_mmnetif;
    MMOSAL_ASSERT(tcpip_init_done);
    mmnetif_set_tx_qos_tid(netif, tid);
}

enum link_state lwip_mmwlan_get_link_state(void)
{
    return current_link_state;
}

err_t lwip_mmwlan_get_local_addr(ip_addr_t *local_addr, const ip_addr_t *dest_addr, bool v6)
{
#if LWIP_IPV6
    struct netif *netif = &lwip_mmnetif;
    if (dest_addr != NULL)
    {
        if (IP_IS_V6(dest_addr))
        {
            const ip_addr_t *src_addr = ip6_select_source_address(netif, ip_2_ip6(dest_addr));
            if (src_addr == NULL)
            {
                return ERR_CONN;
            }
            ip_addr_copy(*local_addr, *src_addr);
            return ERR_OK;
        }
    }
    else if (v6)
    {
        ip_addr_copy(*local_addr, lwip_mmnetif.ip6_addr[0]);
        return ERR_OK;
    }
#endif

#if LWIP_IPV4
    if (dest_addr != NULL)
    {
        if (IP_IS_V4(dest_addr))
        {
            ip_addr_copy(*local_addr, lwip_mmnetif.ip_addr);
            return ERR_OK;
        }
    }
    else if (!v6)
    {
        ip_addr_copy(*local_addr, lwip_mmnetif.ip_addr);
        return ERR_OK;
    }
#endif

#if !LWIP_IPV4 && !LWIP_IPV6
    MM_UNUSED(local_addr);
    MM_UNUSED(dest_addr);
    MM_UNUSED(v6);
#endif

    return ERR_ARG;
}

err_t lwip_mmwlan_get_local_addr_str(char *local_addr, const char *dest_addr, bool v6)
{
    ip_addr_t _lwip_dest_addr;
    ip_addr_t *lwip_dest_addr = NULL;
    ip_addr_t lwip_local_addr;
    err_t result;
    int ok;

    if (dest_addr != NULL)
    {
        lwip_dest_addr = &_lwip_dest_addr;
        ok = ipaddr_aton(dest_addr, lwip_dest_addr);
        if (!ok)
        {
            return ERR_ARG;
        }
    }

    result = lwip_mmwlan_get_local_addr(&lwip_local_addr, lwip_dest_addr, v6);
    if (result != ERR_OK)
    {
        return result;
    }

    if (ipaddr_ntoa_r(&lwip_local_addr, local_addr, LWIP_MMWLAN_IPADDR_STR_MAXLEN) == NULL)
    {
        return ERR_MEM;
    }
    else
    {
        return ERR_OK;
    }
}
