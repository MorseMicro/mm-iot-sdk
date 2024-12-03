/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @defgroup LWIP_MMWLAN LwIP wrapper for Morse Micro Wireless LAN
 *
 * This module provides a wrapper around Lightweight IP (LwIP) and the Morse Micro Wireless
 * LAN (MMWLAN) interface. It simplifies the process of initialising and configuring the
 * IP stack.
 *
 * The main starting point is @ref lwip_mmwlan_init(). This function will intialise LwIP,
 * create a netif for MMWLAN, and power on the MM chip to read information such as MAC address
 * and firmware version.
 *
 * Other functions in this module allow for IP configuration to be set and retrieved, and
 * for a callback to be configured which will be invoked when the link state changes.
 *
 * @section LWIP_MMWLAN_IPV6 IPv6
 * IPv4/IPv6 Dual Stack is enabled by default. IPv6 support can be disabled by setting
 * @c LWIP_IPV6 to 0 in lwipopts.h.
 *
 * @{
 */

#pragma once

#include <stdint.h>

#include "lwip/api.h"
#include "lwip/autoip.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/dhcp6.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/ip4_frag.h"
#include "lwip/ip6_frag.h"
#include "lwip/mem.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of an IP address string, including null-termiantor. */
#define LWIP_MMWLAN_IPADDR_STR_MAXLEN   (48)

/** Enumeration of netif link status. */
enum link_state
{
    /** Netif link is down. */
    LINK_DOWN,
    /** Netif link is up. */
    LINK_UP,
};


/** Enumeration of IP address allocation modes. */
enum lwip_mmwlan_addr_mode
{
    /** Static IP address. */
    IP_ADDR_STATIC,
    /** IP address allocated via DHCP. @c LWIP_DHCP must be set to 1 */
    IP_ADDR_DHCP,
    /** IP address allocated via AutoIP. @c LWIP_DHCP must be set to 1 */
    IP_ADDR_AUTOIP,
};

/** IP configuration structure. */
struct lwip_mmwlan_ip_config
{
    /** IP address allocation mode */
    bool dhcp_enabled;
    /** local IP address */
    ip_addr_t ip_addr;
    /** Netmask address */
    ip_addr_t netmask;
    /** Gateway address */
    ip_addr_t gateway_addr;
};

/** IP address string type. */
typedef char ip_addr_str_t[LWIP_MMWLAN_IPADDR_STR_MAXLEN];

/** IP configuration structure using strings instead of ip_addr_t. */
struct lwip_mmwlan_ip_config_str
{
    /** IP address allocation mode */
    bool dhcp_enabled;
    /** local IP address */
    ip_addr_str_t ip_addr;
    /** Netmask address */
    ip_addr_str_t netmask;
    /** Gateway address */
    ip_addr_str_t gateway_addr;
};

/** Enumeration of IPv6 address allocation modes. */
enum lwip_mmwlan_ip6_addr_mode
{
    /** Static IPv6 addresses. */
    IP6_ADDR_STATIC,
    /** IPv6 address allocated via autoconfiguration. @c LWIP_IPV6_AUTOCONFIG must be set to 1 */
    IP6_ADDR_AUTOCONFIG,
    /** IPv6 address allocated via stateless DHCPv6.
     * @c LWIP_IPV6_DHCP6_STATELESS must be set to 1 */
    IP6_ADDR_DHCP6_STATELESS,
};


/** IPv6 configuration structure. */
struct lwip_mmwlan_ip6_config
{
    /** IPv6 addresses allocation mode*/
    enum lwip_mmwlan_ip6_addr_mode ip6_mode;
    /** Array of IPv6 addresses. */
    ip_addr_t ip6_addr[LWIP_IPV6_NUM_ADDRESSES];
};

/** IPv6 configuration structure using strings instead of ip_addr_t. */
struct lwip_mmwlan_ip6_config_str
{
    /** IPv6 addresses allocation mode*/
    enum lwip_mmwlan_ip6_addr_mode ip6_mode;
    /** Array of IPv6 addresses. */
    ip_addr_str_t ip6_addr[LWIP_IPV6_NUM_ADDRESSES];
};

/**
 * Initialise arguments structure.
 *
 * This should be initialised using @c LWIP_MMWLAN_INIT_ARGS_DEFAULT.
 * For example:
 *
 * @code{.c}
 * struct lwip_mmwlan_init_args args = LWIP_MMWLAN_INIT_ARGS_DEFAULT;
 * @endcode
 */
struct lwip_mmwlan_init_args
{
#if LWIP_IPV4
    /** IP address allocation mode to use. */
    enum lwip_mmwlan_addr_mode mode;
    /** IP address to use (if @c mode is @c IP_ADDR_STATIC). */
    ip_addr_t ip_addr;
    /** Netmask to use (if @c mode is @c IP_ADDR_STATIC). */
    ip_addr_t netmask;
    /** Gateway IP address to use (if @c mode is @c IP_ADDR_STATIC). */
    ip_addr_t gateway_addr;
#endif
#if LWIP_IPV6
    /** IPv6 address allocation mode to use. */
    enum lwip_mmwlan_ip6_addr_mode ip6_mode;
    /** IPv6 address to use (if @c ip6_mode is @c IP6_ADDR_STATIC). */
    ip_addr_t ip6_addr;
#endif
};

/** Initaliser for @ref lwip_mmwlan_init_args. */
#if LWIP_IPV4 && LWIP_IPV6
#define LWIP_MMWLAN_INIT_ARGS_DEFAULT   { IP_ADDR_STATIC, IPADDR4_INIT(IPADDR_ANY),              \
                                          IPADDR4_INIT(IPADDR_ANY), IPADDR4_INIT(IPADDR_ANY),    \
                                          IP6_ADDR_AUTOCONFIG, IPADDR6_INIT(0ul, 0ul, 0ul, 0ul) }
#elif LWIP_IPV4
#define LWIP_MMWLAN_INIT_ARGS_DEFAULT   { IP_ADDR_STATIC, IPADDR4_INIT(IPADDR_ANY),              \
                                          IPADDR4_INIT(IPADDR_ANY), IPADDR4_INIT(IPADDR_ANY) }
#elif LWIP_IPV6
#define LWIP_MMWLAN_INIT_ARGS_DEFAULT   { IP6_ADDR_STATIC, IPADDR6_INIT(0ul, 0ul, 0ul, 0ul) }
#endif


/** Call back function for getting DHCP IP. */
typedef void (*link_status_cb_fn_t)(enum link_state link_state, const ip_addr_t *ip_addr,
                                    const ip_addr_t *netmask, const ip_addr_t *gateway);

/**
 * Initialise LwIP and enable the MMWLAN interface.
 *
 * This will implicitly initialise and boot MMWLAN, and will block until this has completed.
 *
 * @note This function will boot the Morse Micro transceiver using @ref mmwlan_boot() in order
 *       to read the MAC address. It is the responsibility of the caller to shut down the
 *       transceiver using @ref mmwlan_shutdown() as required.
 *
 * @warning @ref mmwlan_init() must be called before invoking this function.
 *
 * @param args  Initialisation arguments.
 *
 * @return ERR_OK on success. otherwise an appropriate error code.
 */
err_t lwip_mmwlan_init(const struct lwip_mmwlan_init_args *args);

/**
 * Sets the callback function for netif set status.
 *
 * This will be used when DHCP is enabled.
 *
 * @note This is for IPv4 only. To get IPv6 status use @c lwip_mmwlan_get_ip6_config.
 *
 * @param fn  Function pointer to the callback function.
 */
void lwip_mmwlan_set_link_status_callback(link_status_cb_fn_t fn);

/**
 * Get the total number of transmitted and received packets on the MMWLAN interface
 *
 * @note This function requires LWIP_STATS to be defined in your application, otherwise packet
 *       counters will always return as 0.
 *
 * @param tx_packets Pointer to location to store total tx packets
 * @param rx_packets Pointer to location to store total rx packets
 */
void lwip_mmwlan_get_link_packet_counts(uint32_t *tx_packets, uint32_t *rx_packets);

/**
 * Set the QoS Traffic ID to use when transmitting.
 *
 * @param tid   The QoS TID to use (0 - @ref MMWLAN_MAX_QOS_TID).
 */
void lwip_mmwlan_set_tx_qos_tid(uint8_t tid);

/**
 * Gets the local address for the MMWLAN LWIP interface that is appropriate for a given
 * destination address.
 *
 * The following table shows how the returned @c local_addr is selected:
 *
 * | @p dest_addr | @p v6 | @c local_addr returned    |
 * |--------------|-------|---------------------------|
 * | type is IPv4 | X     | IPv4 address              |
 * | type is IPv6 | X     | An IPv6 source address selected from @p netif.ip6_address or ERR_CONN |
 * | NULL         | false | IPv4 address              |
 * | NULL         | true  | IPv6 link local address   |
 *
 * (X = don't care)
 *
 * If the given parameters would result in a @p local_addr type of IPv4 and LWIP_IPV4 is
 * not enabled, or IPv6 and LWIP_IPV6 is not enabled, then ERR_ARG will be returned.
 *
 * @param[out] local_addr   Pointer to local address for MMWLAN LwIP interface, as noted above.
 * @param[in]  dest_addr    Destination address (optional), see table above.
 * @param[in]  v6           In case of ambiguity, selects whether to prefer returning
 *                          an IPv6 address -- see table above.
 *
 * @return ERR_OK if @p local_addr successfully set. otherwise an appropriate error code.
 */
err_t lwip_mmwlan_get_local_addr(ip_addr_t *local_addr, const ip_addr_t *dest_addr, bool v6);


/**
 * Same as @ref lwip_mmwlan_get_local_addr(), but stores the local_addr into the buffer as
 * a null-terminated string.
 *
 * @param local_addr    A buffer to receive the null-terminated string. Must be at least
 *                      @ref LWIP_MMWLAN_IPADDR_STR_MAXLEN chars.
 * @param dest_addr     See @ref lwip_mmwlan_get_local_addr() docs.
 * @param v6            See @ref lwip_mmwlan_get_local_addr() docs.
 *
 * @return See @ref lwip_mmwlan_get_local_addr() docs.
 */
err_t lwip_mmwlan_get_local_addr_str(char *local_addr, const char *dest_addr, bool v6);

#if LWIP_IPV4
/**
 * Get the IP configurations.
 *
 * This can be used to get the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 */
void lwip_mmwlan_get_ip_config(struct lwip_mmwlan_ip_config *config);

/**
 * Set the IP configurations.
 *
 * This can be used to set the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 */
void lwip_mmwlan_set_ip_config(const struct lwip_mmwlan_ip_config *config);

/**
 * Get the IP configurations in string representation.
 *
 * This can be used to get the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 */
void lwip_mmwlan_get_ip_config_str(struct lwip_mmwlan_ip_config_str *config);

/**
 * Set the IP configurations from string representation.
 *
 * This can be used to set the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 *
 * @returns @c ERR_OK on success, @c ERR_ARG if one or more IP address strings malformed.
 */
err_t lwip_mmwlan_set_ip_config_str(const struct lwip_mmwlan_ip_config_str *config);
#endif

#if LWIP_IPV6

/**
 * Get the IP configurations.
 *
 * This can be used to get the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 */
void lwip_mmwlan_get_ip6_config(struct lwip_mmwlan_ip6_config *config);


/**
 * Set the IPv6 configurations.
 *
 * This can be used to set the local IPv6 configurations.
 *
 * @param config  Pointer to the IPv6 configurations.
 */
void lwip_mmwlan_set_ip6_config(const struct lwip_mmwlan_ip6_config *config);

/**
 * Get the IP configurations in string representation.
 *
 * This can be used to get the local IP configurations.
 *
 * @param config  Pointer to the IP configurations.
 */
void lwip_mmwlan_get_ip6_config_str(struct lwip_mmwlan_ip6_config_str *config);


/**
 * Set the IPv6 configurations from string representation.
 *
 * This can be used to set the local IPv6 configurations.
 *
 * @param config  Pointer to the IPv6 configurations.
 *
 * @returns @c ERR_OK on success, @c ERR_ARG if one or more IP address strings malformed.
 */
err_t lwip_mmwlan_set_ip6_config_str(const struct lwip_mmwlan_ip6_config_str *config);
#endif

/**
 * Get current netif link state.
 *
 * @return enum link_state LINKUP or LINKDOWN.
 */
enum link_state lwip_mmwlan_get_link_state(void);

#ifdef __cplusplus
}
#endif

/** @} */
