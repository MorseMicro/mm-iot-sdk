/*
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Simple TCP/UDP socket demonstration.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This file demonstrates how to send raw UDP/TCP packets using the mbedtls API.
 * To setup a basic TCP server on a linux host use `nc -v -l -p 1337`. Add the `-u` flag for UDP.
 */

#include "mmconfig.h"
#include "mmosal.h"
#include "mmhal_app.h"
#include "mmipal.h"

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include "mbedtls/net.h"

#include "mm_app_common.h"
#include "mmwlan.h"

/** Default IP address of target server */
#define DEFAULT_TARGET_ADDR "192.168.1.1"
/** Default port to use for contacting server. */
#define DEFAULT_PORT "1337"
/** Default protocol to use as an mbedtls int code. */
#define DEFAULT_IP_PROTOCOL MBEDTLS_NET_PROTO_TCP
/** Default protocol in string form. Options: {"tcp", "udp"}.
 *  Must coincide with @ref DEFAULT_IP_PROTOCOL */
#define DEFAULT_IP_PROTOCOL_STR "tcp"

/** Default number of packets to send.  */
#define DEFAULT_PACKET_COUNT 5
/** Default period between transmissions in ms. Must be greater than PACKET_SEND_DURATION_MS. */
#define DEFAULT_PACKET_INTERVAL_MS 10000
/** Arbitrary maximum data size for TCP/UDP packets. Determines the size of the buffer.
 *  Must be <= INT32_MAX as this is the return type of IP send functions. */
#define MAX_DATA_SIZE_BYTES (uint32_t)1500
/** Default data size for TCP/UDP packets. Must be <= @ref MAX_DATA_SIZE_BYTES. */
#define DEFAULT_DATA_SIZE_BYTES 500
/** Format of string to place at start of TCP/UDP packet. Must be shorter than MAX_DATA_SIZE_BYTES
 */
#define PACKET_DATA_FORMAT "Packet index %ld, len=%ld: "
/** Length of time in ms to wait for packet to send. Only used if ENABLE_POWER_MEAS=1.
 *  Should be increased for lower link bandwidths or larger packet sizes. */
#define PACKET_SEND_DURATION_MS 200
/** Number of times to attempt mbedtls connect. */
#define CONNECT_MAX_ATTEMPTS 5

#if defined(ENABLE_PWR_MEAS) && ENABLE_PWR_MEAS
/** Enable delays between states for accurate power consumption measurements. */
#define PWR_MEAS_DELAY_MS(delay_ms) mmosal_task_sleep(delay_ms)
/** Set GPIO debug pins on state change to track states. */
#define SET_DEBUG_STATE(state) mmhal_set_debug_pins(MMHAL_ALL_DEBUG_PINS, state);
#else
#include "mmutils.h"
/** Disable delays which are only useful for power consumption accuracy. */
#define PWR_MEAS_DELAY_MS(delay_ms) MM_UNUSED(delay_ms)
/** Disable GPIO writing if not measuring power consumption. */
#define SET_DEBUG_STATE(state)      MM_UNUSED(state)
#endif

/** Buffer holding UDP/TCP packet data. */
uint8_t buf[MAX_DATA_SIZE_BYTES] = {};

/**
 * Enumeration of debug states that will be reflected on debug pins. Note that due to limited
 * availability of pins, the values are mapped to 2-bit codes and so are not unique. The code
 * sequence has been chosen to be gray code like in that only one bit changes at a time.
 */
enum debug_state
{
    /** Initial state at startup. */
    DEBUG_STATE_INIT = 0x00,
    /** Indicates that we are booting the MM chip (note that this will also include the
     *  host MCU startup time. */
    DEBUG_STATE_BOOTING_CHIP = 0x01,
    /** Indicates we are connecting to the AP. */
    DEBUG_STATE_CONNECTING = 0x03,
    /** Indicates we are connected to the AP. */
    DEBUG_STATE_CONNECTED = 0x02,
    /** Indicates that we have connected to the AP, but have not connected to the TCP server. */
    DEBUG_STATE_CONNECTED_IDLE = 0x00,
    /** Indicates we are initializing the mbedtls client. */
    DEBUG_STATE_MBEDTLS_INIT = 0x02,
    /** Indicates we are connecting to the TCP server on the AP. */
    DEBUG_STATE_MBEDTLS_CONNECTING = 0x03,
    /** Indicates we are sending a TCP packet to the AP. */
    DEBUG_STATE_MBEDTLS_SEND = 0x01,
    /** Indicates we have sent a TCP packet to the AP and are waiting for communications to
     * cool down. */
    DEBUG_STATE_MBEDTLS_SEND_DONE = 0x00,
    /** Indicates that we are disconnecting from the AP. */
    DEBUG_STATE_TERMINATING = 0x03,
};

/**
 * Parse the protocol from a string to an MBEDTLS protocol integer.
 *
 * @param protocol The protocol string to parse.
 * @param len      The length of the string to parse.
 * @return The mbedtls protocol int code on success, else -1 on error.
 */
int parse_protocol_str(const char *protocol, uint8_t len)
{
    if (strncmp("tcp", protocol, len) == 0)
    {
        return MBEDTLS_NET_PROTO_TCP;
    }
    else if (strncmp("udp", protocol, len) == 0)
    {
#if defined(ipconfigUSE_IPv4) || defined(ipconfigUSE_IPv6)
        printf("WARNING: The FreeRTOS+TCP socket implementation for Mbed TLS dos not currently "
               "support UDP\n");
        return -1;
#endif
        return MBEDTLS_NET_PROTO_UDP;
    }
    return -1;
}

/**
 * Handle a report at the end of an iperf transfer.
 *
 * @param packet_idx Index of the packet which is included in the packet data.
 * @param len Length of the buffer.
 * @param buf Buffer for data to be written into.
 */
void generate_packet_data(uint32_t packet_idx, uint32_t len, unsigned char *buf)
{
    snprintf(((char *)buf), sizeof(PACKET_DATA_FORMAT), PACKET_DATA_FORMAT, packet_idx, len);
    for (uint32_t i = sizeof(PACKET_DATA_FORMAT); i < len; i++)
    {
        buf[i] = '0' + (i % 10);
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    SET_DEBUG_STATE(DEBUG_STATE_BOOTING_CHIP);

    printf("\n\nSockets Example (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();

    SET_DEBUG_STATE(DEBUG_STATE_CONNECTING);

    app_wlan_start();

    PWR_MEAS_DELAY_MS(150);

    SET_DEBUG_STATE(DEBUG_STATE_CONNECTED);

    /* Delay to allow communications to settle so we measure only idle current */
    PWR_MEAS_DELAY_MS(600);

    SET_DEBUG_STATE(DEBUG_STATE_CONNECTED_IDLE);

    PWR_MEAS_DELAY_MS(1000);

    SET_DEBUG_STATE(DEBUG_STATE_MBEDTLS_INIT);

    mbedtls_net_context server_fd;
    mbedtls_net_init(&server_fd);

    PWR_MEAS_DELAY_MS(100);

    uint32_t data_size_bytes = DEFAULT_DATA_SIZE_BYTES;
    (void)mmconfig_read_uint32("socket.data_size_bytes", &data_size_bytes);
    if (data_size_bytes > MAX_DATA_SIZE_BYTES)
    {
        data_size_bytes = MAX_DATA_SIZE_BYTES;
        printf("Selected data size (socket.data_size_bytes) too large. Using the maximum of %ld "
               "instead",
               MAX_DATA_SIZE_BYTES);
    }

    struct mmipal_ip_config ip_config = MMIPAL_IP_CONFIG_DEFAULT;
    enum mmipal_status status = mmipal_get_ip_config(&ip_config);
    char target_ip[48] = DEFAULT_TARGET_ADDR;
    if (status == MMIPAL_SUCCESS && ip_config.mode != MMIPAL_DISABLED)
    {
        memcpy(target_ip, ip_config.gateway_addr, sizeof(ip_config.gateway_addr));
    }
    else
    {
        printf("Failed to retrieve IP config\n");
    }
    /* If socket.target is set, we use it as an override */
    (void)mmconfig_read_string("socket.target", target_ip, sizeof(target_ip));

    char target_port[6] = DEFAULT_PORT;
    (void)mmconfig_read_string("socket.port", target_port, sizeof(target_port));

    int ip_protocol;
    char protocol_str[4];
    (void)mmconfig_read_string("socket.protocol", protocol_str, sizeof(protocol_str));
    if ((ip_protocol = parse_protocol_str(protocol_str, sizeof(protocol_str))) < 0)
    {
        ip_protocol = DEFAULT_IP_PROTOCOL;
        snprintf(protocol_str, sizeof(protocol_str), DEFAULT_IP_PROTOCOL_STR);
        printf("Failed to parse socket.protocol config. Defaulting to %s\n", protocol_str);
    }

    SET_DEBUG_STATE(DEBUG_STATE_MBEDTLS_CONNECTING);

    printf("Connecting to %s server at %s:%s...\n", protocol_str, target_ip, target_port);

    int ret;
    uint32_t retries = CONNECT_MAX_ATTEMPTS;
    while ((ret = mbedtls_net_connect(&server_fd, target_ip, target_port, ip_protocol)) &&
           retries--)
    {
        printf("Failed to connect to server. Status: %d\n", ret);
    }
    if (ret == 0)
    {
        printf("Successfully connected to server.\n");
    }

    /* Send an empty packet to ensure all ARP activity completes. This prevents the first
     * packet from taking longer and consuming more power. Wait 2 seconds for network
     * activity to finish. */
    mbedtls_net_send(&server_fd, NULL, 0);
    PWR_MEAS_DELAY_MS(2000);

    uint32_t packet_interval_ms = DEFAULT_PACKET_INTERVAL_MS;
    uint32_t packet_count = DEFAULT_PACKET_COUNT;
    mmconfig_read_uint32("socket.packet_interval_ms", &packet_interval_ms);
    mmconfig_read_uint32("socket.count", &packet_count);
    MMOSAL_ASSERT(packet_interval_ms >= PACKET_SEND_DURATION_MS);
    for (uint32_t packet_idx = 0; packet_idx < packet_count; packet_idx++)
    {
        SET_DEBUG_STATE(DEBUG_STATE_MBEDTLS_SEND);
        generate_packet_data(packet_idx, data_size_bytes, buf);
        printf("Sending packet number %ld... ", packet_idx);
        if ((ret = mbedtls_net_send(&server_fd, buf, data_size_bytes)) < 0)
        {
            printf("Packet failed to send. Return code: %d\n", ret);
        }
        else if ((uint32_t)ret < data_size_bytes)
        {
            printf("Packet not fully sent: %d of %ld\n", ret, data_size_bytes);
        }
        else
        {
            printf("Packet successfully sent\n");
        }
#if defined(ENABLE_PWR_MEAS) && ENABLE_PWR_MEAS
        PWR_MEAS_DELAY_MS(PACKET_SEND_DURATION_MS);

        SET_DEBUG_STATE(DEBUG_STATE_MBEDTLS_SEND_DONE);

        PWR_MEAS_DELAY_MS(packet_interval_ms - PACKET_SEND_DURATION_MS);
#else
        mmosal_task_sleep(packet_interval_ms);
#endif
    }

    SET_DEBUG_STATE(DEBUG_STATE_TERMINATING);

    /* Disconnect from Wi-Fi */
    app_wlan_stop();
}
