/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Simple DNS client demonstration.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to use DNS to resolve a hostname using the Morse Micro WLAN API.
 *
 * The application sets the primary and secondary DNS servers from the config store keys
 * @c ip.dns_server0 and @c ip.dns_server1, falling back to @c dns.server0 and @c dns.server1
 * if these are not found for legacy support (note that this is done as part of the loading of
 * configuration during app_wlan_init()/app_wlan_start()). It then tries to resolve the IP for
 * the hostname specified in config store key @c dns.lookup -- if the key is not specified then
 * it tries to lookup @c www.google.com. The application has been written in a generic way to
 * display all IP addresses for a given hostname, but currently LwIP returns only 1 IP for
 * each lookup - but this may change in the future.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 */

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"

#include "mmipal.h"
#include "netdb.h"
#include "sys/socket.h"

#include "mm_app_common.h"

/** This is the default hostname to lookup if none are specified in config store */
#define DEFAULT_LOOKUP  "www.morsemicro.com"

#ifndef DNS_MAX_NAME_LENGTH
/** Maximum supported length of hostname for DNS lookup. */
#define DNS_MAX_NAME_LENGTH (256)
#endif


/**
 * Perform a DNS lookup for the given hostname and print the results.
 *
 * @param hostname      Hostname to look up.
 * @param ai_family     Address family (@c AF_INET or @c AF_INET6) to request.
*/
static void dns_lookup(const char *hostname, int ai_family)
{
    /*
     * Hints is optional and can be specified as NULL in getaddrinfo()
     * if you just want to lookup an IP. But if you intend to use the returned data
     * to actually connect (as you would in a real use case), then you need to specify
     * hints so that the correct protocol and ports are returned in addr_list below.
     */
    struct addrinfo hints = { 0 };
    hints.ai_family = ai_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addr_list, *cur;
    int ret = getaddrinfo(hostname, NULL, &hints, &addr_list);
    if (ret == 0)
    {
        /*
         * Enumerate all addresses returned.
         * Technically this API can return multiple addresses including combinations
         * of IPv4 and IPv6 addresses (depending on the hints given).
         */
        for (cur = addr_list; cur != NULL; cur = cur->ai_next)
        {
            /*
             * In a typical use case, you can directly use the first element in addr_list
             * to open a socket and connect as shown below:
             *
             * int fd = socket(addr_list->ai_family, addr_list->ai_socktype, addr_list->ai_protocol);
             * connect(fd, addr_list->ai_addr, addr_list->ai_addrlen);
             *
             * However in this example we want to print the list of IP addeesses found
             * so we enumerate the addr_list, and do some conversions to convert and print the
             * IP addresses.
             */

            char addr_str[MMIPAL_IPADDR_STR_MAXLEN];
            const char *result = NULL;

            if (cur->ai_family == AF_INET)
            {
#if MMIPAL_IPV4_ENABLED
                const struct sockaddr_in *sockaddr = (const struct sockaddr_in *)cur->ai_addr;
                result = inet_ntop(cur->ai_family, &sockaddr->sin_addr,
                                   addr_str, sizeof(addr_str));
#endif
            }
#if MMIPAL_IPV6_ENABLED
            else if (cur->ai_family == AF_INET6)
            {
                const struct sockaddr_in6 *sockaddr = (const struct sockaddr_in6 *)cur->ai_addr;
                result = inet_ntop(cur->ai_family, &sockaddr->sin6_addr,
                                   addr_str, sizeof(addr_str));
            }
#endif

            if (result != NULL)
            {
                printf("    %s\n", result);
            }
            else
            {
                printf("Error: Failed to convert IP address to string\n");
            }
        }
        freeaddrinfo(addr_list);
    }
    else
    {
        const char *family_str;
        switch (ai_family)
        {
        case AF_INET:
            family_str = "IPv4";
            break;

        case AF_INET6:
            family_str = "IPv6";
            break;

        default:
            family_str = "??";
            break;
        }

        printf("Could not resolve %s address for hostname %s! (Error code %d)\n",
               family_str, hostname, ret);
    }
}


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    static char hostname[DNS_MAX_NAME_LENGTH + 1];

    printf("\n\nMorse DNS client Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    /* Get the hostname to lookup, if not provided use the default */
    strncpy(hostname, DEFAULT_LOOKUP, sizeof(hostname));
    (void)mmconfig_read_string("dns.lookup", hostname, sizeof(hostname));

    printf("Hostname %s resolves to:\n", hostname);
    dns_lookup(hostname, AF_INET);
    dns_lookup(hostname, AF_INET6);

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}
