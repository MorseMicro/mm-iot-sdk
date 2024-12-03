/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Throughput measurement using iperf.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to run iperf using the Morse Micro WLAN API.
 *
 * This file loads the required Wi-Fi and iperf parameters like SSID and password from the
 * config store to initialize the wlan interface.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 */

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmiperf.h"

#include "mmipal.h"
#include "mm_app_common.h"

/* ------------------------ Configuration options ------------------------ */

/** Iperf configurations. */
enum iperf_type
{
    IPERF_TCP_SERVER,   /**< TCP server (RX) */
    IPERF_UDP_SERVER,    /**< UDP server (RX) */
    IPERF_TCP_CLIENT,   /**< TCP client (TX) */
    IPERF_UDP_CLIENT,   /**< UDP client (TX) */
};

#ifndef IPERF_TYPE
/** Type of iperf instance to start. */
#define IPERF_TYPE                      IPERF_UDP_SERVER
#endif

#ifndef IPERF_SERVER_IP
/** IP address of server to connect to when in client mode. */
#define IPERF_SERVER_IP                 "192.168.1.1"
#endif

#ifndef IPERF_TIME_AMOUNT
/**
 * Duration for client transfers specified either in seconds or bytes.
 * If this is negative, it specifies a time in seconds; if positive, it
 * specifies the number of bytes to transmit.
 */
#define IPERF_TIME_AMOUNT               -10
#endif
#ifndef IPERF_SERVER_PORT
/** Specifies the port to listen on in server mode. */
#define IPERF_SERVER_PORT               5001
#endif

/* ------------------------ End of configuration options ------------------------ */

/** Array of power of 10 unit specifiers. */
static const char units[] = {' ', 'K', 'M', 'G', 'T'};

/**
 * Function to format a given number of bytes into an appropriate SI base. I.e if you give it 1400
 * it will return 1 with unit_index set to 1 for Kilo.
 *
 * @warning This uses power of 10 units (kilo, mega, giga, etc). Not to be confused with power of 2
 *          units (kibi, mebi, gibi, etc).
 *
 * @param[in]   bytes       Original number of bytes
 * @param[out]  unit_index  Index into the @ref units array. Must not be NULL
 *
 * @return Number of bytes formatted to the appropriate unit given by the unit index.
 */
static uint32_t format_bytes(uint64_t bytes, uint8_t *unit_index)
{
    MMOSAL_ASSERT(unit_index != NULL);
    *unit_index = 0;

    while (bytes >= 1000 && *unit_index < 4)
    {
        bytes /= 1000;
        (*unit_index)++;
    }

    return bytes;
}

/**
 * Handle a report at the end of an iperf transfer.
 *
 * @param report    The iperf report.
 * @param arg       Opaque argument specified when iperf was started.
 * @param handle    The iperf instance handle returned when iperf was started.
 */
static void iperf_report_handler(const struct mmiperf_report *report, void *arg,
                                 mmiperf_handle_t handle)
{
    (void)arg;
    (void)handle;

    uint8_t bytes_transferred_unit_index = 0;
    uint32_t bytes_transferred_formatted = format_bytes(report->bytes_transferred,
                                                        &bytes_transferred_unit_index);

    printf("\nIperf Report\n");
    printf("  Remote Address: %s:%d\n", report->remote_addr, report->remote_port);
    printf("  Local Address:  %s:%d\n", report->local_addr, report->local_port);
    printf("  Transferred: %lu %cBytes, duration: %lu ms, bandwidth: %lu kbps\n",
           bytes_transferred_formatted, units[bytes_transferred_unit_index],
           report->duration_ms, report->bandwidth_kbitpsec);
    printf("\n");

    if ((report->report_type == MMIPERF_UDP_DONE_SERVER) ||
        (report->report_type == MMIPERF_TCP_DONE_SERVER))
    {
        printf("Waiting for client to connect...\n");
    }
}

/** Start iperf as a TCP client. */
static void start_tcp_client(void)
{
    uint32_t server_port = 0;
    struct mmiperf_client_args args = MMIPERF_CLIENT_ARGS_DEFAULT;

    /* Get the Server IP */
    strncpy(args.server_addr, IPERF_SERVER_IP, sizeof(args.server_addr));
    mmconfig_read_string("iperf.server", args.server_addr, sizeof(args.server_addr));

    mmconfig_read_uint32("iperf.port", &server_port);
    MMOSAL_ASSERT(server_port <= UINT16_MAX);
    args.server_port = server_port;

    int amount = IPERF_TIME_AMOUNT;
    (void)mmconfig_read_int("iperf.amount", &amount);
    args.amount = amount;
    if (args.amount < 0)
    {
        args.amount *= 100;
    }
    args.report_fn = iperf_report_handler;

    mmiperf_start_tcp_client(&args);
    printf("\nIperf TCP client started, waiting for completion...\n");
}

/** Start iperf as a UDP client. */
static void start_udp_client(void)
{
    uint32_t server_port = 0;
    struct mmiperf_client_args args = MMIPERF_CLIENT_ARGS_DEFAULT;

    strncpy(args.server_addr, IPERF_SERVER_IP, sizeof(args.server_addr));
    mmconfig_read_string("iperf.server", args.server_addr, sizeof(args.server_addr));

    mmconfig_read_uint32("iperf.port", &server_port);
    MMOSAL_ASSERT(server_port <= UINT16_MAX);
    args.server_port = server_port;

    int amount = IPERF_TIME_AMOUNT;
    (void)mmconfig_read_int("iperf.amount", &amount);
    args.amount = amount;
    if (args.amount < 0)
    {
        args.amount *= 100;
    }
    args.report_fn = iperf_report_handler;

    mmiperf_start_udp_client(&args);
    printf("\nIperf UDP client started, waiting for completion...\n");
}

/** Start iperf as a TCP server. */
static void start_tcp_server(void)
{
    struct mmiperf_server_args args = MMIPERF_SERVER_ARGS_DEFAULT;

    uint32_t local_port = IPERF_SERVER_PORT;
    mmconfig_read_uint32("iperf.port", &local_port);
    args.local_port = (uint16_t) local_port;

    args.report_fn = iperf_report_handler;

    mmiperf_handle_t iperf_handle = mmiperf_start_tcp_server(&args);
    if (iperf_handle == NULL)
    {
        printf("Failed to get local address\n");
        return;
    }
    printf("\nIperf TCP server started, waiting for client to connect...\n");
    struct mmipal_ip_config ip_config;
    enum mmipal_status status;
    status = mmipal_get_ip_config(&ip_config);
    if (status == MMIPAL_SUCCESS)
    {
        printf("Execute cmd on AP 'iperf -c %s -p %u -i 1' for IPv4\n",
               ip_config.ip_addr, args.local_port);
    }

    struct mmipal_ip6_config ip6_config;
    status = mmipal_get_ip6_config(&ip6_config);
    if (status == MMIPAL_SUCCESS)
    {
        printf("Execute cmd on AP 'iperf -c %s%%wlan0 -p %u -i 1 -V' for IPv6\n",
               ip6_config.ip6_addr[0], args.local_port);
    }
}

/** Start iperf as a UDP server. */
static void start_udp_server(void)
{
    struct mmiperf_server_args args = MMIPERF_SERVER_ARGS_DEFAULT;

    uint32_t local_port = IPERF_SERVER_PORT;
    mmconfig_read_uint32("iperf.port", &local_port);
    args.local_port = (uint16_t) local_port;

    args.report_fn = iperf_report_handler;

    mmiperf_handle_t iperf_handle = mmiperf_start_udp_server(&args);
    if (iperf_handle == NULL)
    {
        printf("Failed to start iperf server\n");
        return;
    }

    printf("\nIperf UDP server started, waiting for client to connect...\n");
    struct mmipal_ip_config ip_config;
    enum mmipal_status status;
    status = mmipal_get_ip_config(&ip_config);
    if (status == MMIPAL_SUCCESS)
    {
        printf("Execute cmd on AP 'iperf -c %s -p %u -i 1 -u -b 20M' for IPv4\n",
               ip_config.ip_addr, args.local_port);
    }

    struct mmipal_ip6_config ip6_config;
    status = mmipal_get_ip6_config(&ip6_config);
    if (status == MMIPAL_SUCCESS)
    {
        printf("Execute cmd on AP 'iperf -c %s%%wlan0 -p %u -i 1 -V -u -b 20M' for IPv6\n",
               ip6_config.ip6_addr[0], args.local_port);
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nMorse Iperf Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    enum iperf_type iperf_mode = IPERF_TYPE;
    char iperf_mode_str[32];
    if (mmconfig_read_string("iperf.mode", iperf_mode_str, sizeof(iperf_mode_str)) > 0)
    {
        if (strncmp(iperf_mode_str, "udp_server", sizeof(iperf_mode_str)) == 0)
        {
            iperf_mode = IPERF_UDP_SERVER;
        }
        else if (strncmp(iperf_mode_str, "tcp_server", sizeof(iperf_mode_str)) == 0)
        {
            iperf_mode = IPERF_TCP_SERVER;
        }
        else if (strncmp(iperf_mode_str, "tcp_client", sizeof(iperf_mode_str)) == 0)
        {
            iperf_mode = IPERF_TCP_CLIENT;
        }
        else if (strncmp(iperf_mode_str, "udp_client", sizeof(iperf_mode_str)) == 0)
        {
            iperf_mode = IPERF_UDP_CLIENT;
        }
        else
        {
            printf("Unsupported iperf mode: %s\n", iperf_mode_str);
            return;
        }
    }

    switch (iperf_mode)
    {
    case IPERF_TCP_SERVER:
        start_tcp_server();
        break;

    case IPERF_UDP_SERVER:
        start_udp_server();
        break;

    case IPERF_UDP_CLIENT:
        start_udp_client();
        break;

    case IPERF_TCP_CLIENT:
        start_tcp_client();
        break;
    }
}
