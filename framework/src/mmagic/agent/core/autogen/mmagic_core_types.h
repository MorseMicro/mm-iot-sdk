/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Warning: this file is autogenerated. Do not modify by hand.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mmutils.h"

enum mmagic_subsystems
{
    mmagic_wlan  = 1,
    mmagic_ip    = 2,
    mmagic_ping  = 3,
    mmagic_iperf = 4,
    mmagic_sys   = 5,
    mmagic_tcp   = 6,
};

enum mmagic_wlan_cmds
{
    mmagic_wlan_cmd_get,
    mmagic_wlan_cmd_set,
    mmagic_wlan_cmd_load,
    mmagic_wlan_cmd_commit,
    mmagic_wlan_cmd_connect,
    mmagic_wlan_cmd_disconnect,
    mmagic_wlan_cmd_scan,
    mmagic_wlan_cmd_get_rssi,
    mmagic_wlan_cmd_get_mac_addr,
    mmagic_wlan_cmd_wnm_sleep,
    mmagic_wlan_cmd_beacon_monitor_enable,
    mmagic_wlan_cmd_beacon_monitor_disable,
    mmagic_wlan_cmd_standby_enter,
    mmagic_wlan_cmd_standby_exit,
    mmagic_wlan_cmd_standby_set_status_payload,
    mmagic_wlan_cmd_standby_set_wake_filter,
    mmagic_wlan_cmd_standby_set_config,
};

enum mmagic_wlan_events
{
    mmagic_wlan_event_beacon_rx    = 1,
    mmagic_wlan_event_standby_exit = 2,
};

enum mmagic_ip_cmds
{
    mmagic_ip_cmd_get,
    mmagic_ip_cmd_set,
    mmagic_ip_cmd_load,
    mmagic_ip_cmd_commit,
    mmagic_ip_cmd_status,
    mmagic_ip_cmd_reload,
    mmagic_ip_cmd_enable_tcp_keepalive_offload,
    mmagic_ip_cmd_disable_tcp_keepalive_offload,
    mmagic_ip_cmd_set_whitelist_filter,
    mmagic_ip_cmd_clear_whitelist_filter,
};

enum mmagic_ping_cmds
{
    mmagic_ping_cmd_get,
    mmagic_ping_cmd_set,
    mmagic_ping_cmd_load,
    mmagic_ping_cmd_commit,
    mmagic_ping_cmd_run,
};

enum mmagic_iperf_cmds
{
    mmagic_iperf_cmd_get,
    mmagic_iperf_cmd_set,
    mmagic_iperf_cmd_load,
    mmagic_iperf_cmd_commit,
    mmagic_iperf_cmd_run,
};

enum mmagic_sys_cmds
{
    mmagic_sys_cmd_get,
    mmagic_sys_cmd_set,
    mmagic_sys_cmd_load,
    mmagic_sys_cmd_commit,
    mmagic_sys_cmd_reset,
    mmagic_sys_cmd_deep_sleep,
    mmagic_sys_cmd_get_version,
};

enum mmagic_tcp_cmds
{
    mmagic_tcp_cmd_get,
    mmagic_tcp_cmd_set,
    mmagic_tcp_cmd_load,
    mmagic_tcp_cmd_commit,
    mmagic_tcp_cmd_connect,
    mmagic_tcp_cmd_bind,
    mmagic_tcp_cmd_recv,
    mmagic_tcp_cmd_send,
    mmagic_tcp_cmd_read_poll,
    mmagic_tcp_cmd_write_poll,
    mmagic_tcp_cmd_accept,
    mmagic_tcp_cmd_close,
};

/** Connection security type */
enum mmagic_security_type
{
    /** Simultaneous Authentication of Equals (password-based authentication) */
    MMAGIC_SECURITY_TYPE_SAE  = 0,
    /** Opportunistic Wireless Encryption (encrypted, passwordless) */
    MMAGIC_SECURITY_TYPE_OWE  = 1,
    /** No security enabled at all */
    MMAGIC_SECURITY_TYPE_OPEN = 2,
};

/** Protected management frame mode */
enum mmagic_pmf_mode
{
    /** Protected management frames must be used */
    MMAGIC_PMF_MODE_REQUIRED = 0,
    /** No protected management frames */
    MMAGIC_PMF_MODE_DISABLED = 1,
};

/** Enumeration of supported 802.11 power save modes. */
enum mmagic_power_save_mode
{
    /** Power save disabled. */
    MMAGIC_POWER_SAVE_MODE_DISABLED = 0,
    /** Power save enabled. */
    MMAGIC_POWER_SAVE_MODE_ENABLED  = 1,
};

/** Enumeration of S1G non-AP STA types. */
enum mmagic_station_type
{
    /** Sensor type. */
    MMAGIC_STATION_TYPE_SENSOR     = 0,
    /** Non-Sensor type. */
    MMAGIC_STATION_TYPE_NON_SENSOR = 1,
};

/** Enumeration of return status codes. */
enum mmagic_status
{
    /** Operation was successful. */
    MMAGIC_STATUS_OK             = 0,
    /** The operation failed with an unspecified error. */
    MMAGIC_STATUS_ERROR          = 1,
    /** The operation failed due to an invalid argument. */
    MMAGIC_STATUS_INVALID_ARG    = 2,
    /** Functionality is temporarily unavailable. */
    MMAGIC_STATUS_UNAVAILABLE    = 3,
    /** The operation failed due to an invalid argument. */
    MMAGIC_STATUS_TIMEOUT        = 4,
    /** An invalid stream was specified. */
    MMAGIC_STATUS_INVALID_STREAM = 5,
    /** Specified operation was not found. */
    MMAGIC_STATUS_NOT_FOUND      = 6,
    /** Specified operation is not supported. */
    MMAGIC_STATUS_NOT_SUPPORTED  = 7,
    /** An error occured during transmission. */
    MMAGIC_STATUS_TX_ERROR       = 8,
    /** Failed due to memory allocation failure. */
    MMAGIC_STATUS_NO_MEM         = 9,
    /** Failed due to stream being closed from the other side. */
    MMAGIC_STATUS_CLOSED         = 10,
    /** Maximum status value possible */
    MMAGIC_STATUS_MAX            = 11,
};

/** Mode to use when running the iperf. */
enum mmagic_iperf_mode
{
    /** Iperf UDP server (RX). */
    MMAGIC_IPERF_MODE_UDP_SERVER = 0,
    /** Iperf TCP server (RX). */
    MMAGIC_IPERF_MODE_TCP_SERVER = 1,
    /** Iperf UDP client (TX). */
    MMAGIC_IPERF_MODE_UDP_CLIENT = 2,
    /** Iperf TCP client (TX). */
    MMAGIC_IPERF_MODE_TCP_CLIENT = 3,
};

/** Current state of iperf session. */
enum mmagic_iperf_state
{
    /** Iperf session not started. */
    MMAGIC_IPERF_STATE_NOT_STARTED = 0,
    /** Iperf session is in progress. */
    MMAGIC_IPERF_STATE_RUNNING     = 1,
    /** Iperf session has completed. */
    MMAGIC_IPERF_STATE_FINISHED    = 2,
    /** Iperf session has been aborted. */
    MMAGIC_IPERF_STATE_ABORTED     = 3,
};

/** Status of the IP link. */
enum mmagic_ip_link_state
{
    /** Link is down because the interface is down or DHCP has not yet completed. */
    MMAGIC_IP_LINK_STATE_DOWN = 0,
    /** Link is up. This implies that the interface is up and DHCP has completed. */
    MMAGIC_IP_LINK_STATE_UP   = 1,
};

/** Deep sleep modes for the agent MCU. */
enum mmagic_deep_sleep_mode
{
    /** Deep sleep is disabled. */
    MMAGIC_DEEP_SLEEP_MODE_DISABLED = 0,
    /** Deep sleep is enabled until activity occurs on the datalink. */
    MMAGIC_DEEP_SLEEP_MODE_ONE_SHOT = 1,
    /** The datalink layer is responsible for waking the agent. */
    MMAGIC_DEEP_SLEEP_MODE_HARDWARE = 2,
};

/** Reasons for exiting standby mode. */
enum mmagic_standby_mode_exit_reason
{
    /** Standby mode was exited manually through a call to standby_exit. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_NONE                = 0,
    /** We were woken up by a wakeup frame. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_WAKEUP_FRAME        = 1,
    /** We just reassociated with the AP. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_ASSOCIATE           = 2,
    /** We were woken up by an external input trigger. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_EXT_INPUT           = 3,
    /** We received a packet from a whitelist source. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_WHITELIST_PKT       = 4,
    /** An open TCP connection was lost. */
    MMAGIC_STANDBY_MODE_EXIT_REASON_STANDBY_EXIT_TCP_CONNECTION_LOST = 5,
};

/** Packet buffer for standard 1536 byte size packets */
struct MM_PACKED struct_packet_buffer
{
    /** Length of the data in @c data */
    uint16_t len;
    /** Array containing the data */
    uint8_t data[1536];
};

/** Data type for string up to 254 characters (with space for a null terminating
 * character). */
struct MM_PACKED struct_string_254
{
    /** Length of the data in @c data (excluding null terminator) */
    uint8_t len;
    /** Array containing the string */
    uint8_t data[255];
};

/** Data type for string up to 32 characters (with space for a null terminating
 * character). */
struct MM_PACKED struct_string_32
{
    /** Length of the data in @c data (excluding null terminator) */
    uint8_t len;
    /** Array containing the string */
    uint8_t data[33];
};

/** Data type to contain mac address byte array */
struct MM_PACKED struct_mac_addr
{
    /** Array containing the mac addr */
    uint8_t addr[6];
};

/** Data type to contain Two character country code (null-terminated) used to
 * identify the regulatory domain. */
struct MM_PACKED struct_country_code
{
    /** Array containing the country code, including room for null-terminator. */
    char country_code[3];
};

/** Data type to contain a three octet OUI. */
struct MM_PACKED struct_oui
{
    /** The 3 octet OUI */
    uint8_t oui[3];
};

/** Data type to contain a list of three octet OUIs. */
struct MM_PACKED struct_oui_list
{
    /** The number of OUIs in the list. */
    uint8_t count;
    /** The OUI data. */
    struct struct_oui ouis[5];
};

/** Structure for holding a scan result. */
struct MM_PACKED struct_scan_result
{
    /** SSID of the AP. */
    struct struct_string_32 ssid;
    /** BSSID of the AP network. */
    struct struct_mac_addr bssid;
    /** RSSI of the AP in dBm. */
    int32_t rssi;
};

/** Structure for devise firmware and hardware versions. */
struct MM_PACKED struct_version_info
{
    /** Version of the application software. */
    struct struct_string_32 application_version;
    /** Version of the bootloader software. */
    struct struct_string_32 bootloader_version;
    /** Version of the user hardware. */
    struct struct_string_32 user_hardware_version;
    /** Version of the Morse firmware. */
    struct struct_string_32 morse_firmware_version;
    /** Version of the Morse IoT SDK library. */
    struct struct_string_32 morselib_version;
    /** Version of the Morse hardware. */
    struct struct_string_32 morse_hardware_version;
};

/** Structure for returning the scan status */
struct MM_PACKED struct_scan_status
{
    /** Array of scan results. */
    struct struct_scan_result results[10];
    /** Number of results retrieved */
    uint8_t num;
};

/** Data type for a string representation of an IP address. Dotted decimal notation
 * for IPv4 addresses and colon-separated hexadecimal notation for IPv6 addresses. */
struct MM_PACKED struct_ip_addr
{
    /** Array containing the IP string */
    char addr[48];
};

/** Structure to contain the current IP status */
struct MM_PACKED struct_ip_status
{
    /** Current link state */
    enum mmagic_ip_link_state link_state;
    /** Whether or not dhcp is enabled */
    bool dhcp_enabled;
    /** Current IP address */
    struct struct_ip_addr ip_addr;
    /** Current IP network mask */
    struct struct_ip_addr netmask;
    /** Current IP gateway */
    struct struct_ip_addr gateway;
    /** Current broadcast IP address */
    struct struct_ip_addr broadcast;
    /** DNS server IP addresses */
    struct struct_ip_addr dns_servers[2];
};

/** Data structure to store ping results */
struct MM_PACKED struct_ping_status
{
    /** IP address of the device receiving the ping requests */
    struct struct_ip_addr receiver_addr;
    /** Total number of requests sent */
    uint32_t total_count;
    /** The number of ping responses received */
    uint32_t recv_count;
    /** The minimum latency in ms between request sent and response received */
    uint32_t min_time_ms;
    /** The average latency in ms between request sent and response received */
    uint32_t avg_time_ms;
    /** The maximum latency in ms between request sent and response received */
    uint32_t max_time_ms;
    /** Stores non-zero session ID whilst ping session is running */
    uint16_t session_id;
};

/** Data structure to store iperf results */
struct MM_PACKED struct_iperf_status
{
    /** IP address of the remote device */
    struct struct_ip_addr remote_addr;
    /** Port number of the remote device */
    uint16_t remote_port;
    /** IP address of the local device */
    struct struct_ip_addr local_addr;
    /** Port number of the local device */
    uint16_t local_port;
    /** The number of bytes of data transferred during the iperf test */
    uint64_t bytes_transferred;
    /** The duration of the iperf test in milliseconds */
    uint32_t duration_ms;
    /** The average throughput in kbps */
    uint32_t bandwidth_kbitpsec;
};

/** Generic 64 byte buffer */
struct MM_PACKED struct_buffer64
{
    /** The 64 byte buffer */
    uint8_t buffer[64];
    /** Leangth of data in the buffer */
    uint8_t len;
};

/**
 * Convert from @c bool to a string.
 *
 * @param  value Reference to the @c bool
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int bool_to_string(bool *value, char *buf, size_t len);

/**
 * Convert from a string to @c bool.
 *
 * @param  value Reference to the @c bool to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_bool(bool *value, const char *buf);

/**
 * Convert from @c enum_iperf_mode to a string.
 *
 * @param  value Reference to the @c enum_iperf_mode
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int enum_iperf_mode_to_string(enum mmagic_iperf_mode *value, char *buf, size_t len);

/**
 * Convert from a string to @c enum_iperf_mode.
 *
 * @param  value Reference to the @c enum_iperf_mode to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_enum_iperf_mode(enum mmagic_iperf_mode *value, const char *buf);

/**
 * Convert from @c enum_pmf_mode to a string.
 *
 * @param  value Reference to the @c enum_pmf_mode
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int enum_pmf_mode_to_string(enum mmagic_pmf_mode *value, char *buf, size_t len);

/**
 * Convert from a string to @c enum_pmf_mode.
 *
 * @param  value Reference to the @c enum_pmf_mode to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_enum_pmf_mode(enum mmagic_pmf_mode *value, const char *buf);

/**
 * Convert from @c enum_power_save_mode to a string.
 *
 * @param  value Reference to the @c enum_power_save_mode
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int enum_power_save_mode_to_string(enum mmagic_power_save_mode *value, char *buf, size_t len);

/**
 * Convert from a string to @c enum_power_save_mode.
 *
 * @param  value Reference to the @c enum_power_save_mode to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_enum_power_save_mode(enum mmagic_power_save_mode *value, const char *buf);

/**
 * Convert from @c enum_security_type to a string.
 *
 * @param  value Reference to the @c enum_security_type
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int enum_security_type_to_string(enum mmagic_security_type *value, char *buf, size_t len);

/**
 * Convert from a string to @c enum_security_type.
 *
 * @param  value Reference to the @c enum_security_type to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_enum_security_type(enum mmagic_security_type *value, const char *buf);

/**
 * Convert from @c enum_station_type to a string.
 *
 * @param  value Reference to the @c enum_station_type
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int enum_station_type_to_string(enum mmagic_station_type *value, char *buf, size_t len);

/**
 * Convert from a string to @c enum_station_type.
 *
 * @param  value Reference to the @c enum_station_type to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_enum_station_type(enum mmagic_station_type *value, const char *buf);

/**
 * Convert from @c int16_t to a string.
 *
 * @param  value Reference to the @c int16_t
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int int16_t_to_string(int16_t *value, char *buf, size_t len);

/**
 * Convert from a string to @c int16_t.
 *
 * @param  value Reference to the @c int16_t to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_int16_t(int16_t *value, const char *buf);

/**
 * Convert from @c int32_t to a string.
 *
 * @param  value Reference to the @c int32_t
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int int32_t_to_string(int32_t *value, char *buf, size_t len);

/**
 * Convert from a string to @c int32_t.
 *
 * @param  value Reference to the @c int32_t to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_int32_t(int32_t *value, const char *buf);

/**
 * Convert from @c struct_country_code to a string.
 *
 * @param  value Reference to the @c struct_country_code
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int struct_country_code_to_string(struct struct_country_code *value, char *buf, size_t len);

/**
 * Convert from a string to @c struct_country_code.
 *
 * @param  value Reference to the @c struct_country_code to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_struct_country_code(struct struct_country_code *value, const char *buf);

/**
 * Convert from @c struct_ip_addr to a string.
 *
 * @param  value Reference to the @c struct_ip_addr
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int struct_ip_addr_to_string(struct struct_ip_addr *value, char *buf, size_t len);

/**
 * Convert from a string to @c struct_ip_addr.
 *
 * @param  value Reference to the @c struct_ip_addr to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_struct_ip_addr(struct struct_ip_addr *value, const char *buf);

/**
 * Convert from @c struct_mac_addr to a string.
 *
 * @param  value Reference to the @c struct_mac_addr
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int struct_mac_addr_to_string(struct struct_mac_addr *value, char *buf, size_t len);

/**
 * Convert from a string to @c struct_mac_addr.
 *
 * @param  value Reference to the @c struct_mac_addr to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_struct_mac_addr(struct struct_mac_addr *value, const char *buf);

/**
 * Convert from @c struct_string_32 to a string.
 *
 * @param  value Reference to the @c struct_string_32
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int struct_string_32_to_string(struct struct_string_32 *value, char *buf, size_t len);

/**
 * Convert from a string to @c struct_string_32.
 *
 * @param  value Reference to the @c struct_string_32 to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_struct_string_32(struct struct_string_32 *value, const char *buf);

/**
 * Convert from @c uint16_t to a string.
 *
 * @param  value Reference to the @c uint16_t
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int uint16_t_to_string(uint16_t *value, char *buf, size_t len);

/**
 * Convert from a string to @c uint16_t.
 *
 * @param  value Reference to the @c uint16_t to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_uint16_t(uint16_t *value, const char *buf);

/**
 * Convert from @c uint32_t to a string.
 *
 * @param  value Reference to the @c uint32_t
 * @param  buf   Reference to the buffer where string is to be placed
 * @param  len   Length of the buffer provided in bytes
 *
 * @return       On success the number of bytes characters written (excluding the null byte). A
 *               return
 *         value of @c len or more means that the output was truncated. On error a negative value is
 *         returned.
 *
 * @note The behaviour is the same as @c snprintf() . This will not write more than @c len in bytes
 *       (including the terminating null byte). If the output was truncated due to this limit, then
 *       the return value is the  number of characters (excluding the terminating null byte) which
 *       would have been written to the final string if enough space had been available.
 */
int uint32_t_to_string(uint32_t *value, char *buf, size_t len);

/**
 * Convert from a string to @c uint32_t.
 *
 * @param  value Reference to the @c uint32_t to place the parsed data.
 * @param  buf   Reference to a null terminated string to parse
 *
 * @return       On success 0. On error a negative value is returned.
 */
int string_to_uint32_t(uint32_t *value, const char *buf);

/** Event arguments structure for wlan_beacon_rx */
struct MM_PACKED mmagic_core_event_wlan_beacon_rx_args
{
    struct struct_packet_buffer vendor_ies;
};/** Event arguments structure for wlan_standby_exit */

struct MM_PACKED mmagic_core_event_wlan_standby_exit_args
{
    enum mmagic_standby_mode_exit_reason reason;
};