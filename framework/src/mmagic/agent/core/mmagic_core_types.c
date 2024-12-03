/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "core/autogen/mmagic_core_types.h"

int bool_to_string(bool *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%s", *value ? "true" : "false");
}

int string_to_bool(bool *value, const char *buf)
{
    if (strcasecmp("true", buf) == 0)
    {
        *value = true;
        return 0;
    }
    else if (strcasecmp("false", buf) == 0)
    {
        *value = false;
        return 0;
    }

    return -1;
}

int enum_pmf_mode_to_string(enum mmagic_pmf_mode *value, char *buf, size_t len)
{
    switch (*value)
    {
    case MMAGIC_PMF_MODE_DISABLED:
        return snprintf(buf, len, "disabled");

    case MMAGIC_PMF_MODE_REQUIRED:
        return snprintf(buf, len, "required");
    }

    return -1;
}

int string_to_enum_pmf_mode(enum mmagic_pmf_mode *value, const char *buf)
{
    if (strncmp("required", buf, sizeof("required")) == 0)
    {
        *value = MMAGIC_PMF_MODE_REQUIRED;
        return 0;
    }
    else if (strncmp("disabled", buf, sizeof("disabled")) == 0)
    {
        *value = MMAGIC_PMF_MODE_DISABLED;
        return 0;
    }

    return -1;
}

int enum_power_save_mode_to_string(enum mmagic_power_save_mode *value, char *buf, size_t len)
{
    switch (*value)
    {
    case MMAGIC_POWER_SAVE_MODE_DISABLED:
        return snprintf(buf, len, "disabled");

    case MMAGIC_POWER_SAVE_MODE_ENABLED:
        return snprintf(buf, len, "enabled");
    }

    return -1;
}

int string_to_enum_power_save_mode(enum mmagic_power_save_mode *value, const char *buf)
{
    if (strncmp("required", buf, sizeof("required")) == 0)
    {
        *value = MMAGIC_POWER_SAVE_MODE_DISABLED;
        return 0;
    }
    else if (strncmp("enabled", buf, sizeof("enabled")) == 0)
    {
        *value = MMAGIC_POWER_SAVE_MODE_ENABLED;
        return 0;
    }

    return -1;
}

int enum_security_type_to_string(enum mmagic_security_type *value, char *buf, size_t len)
{
    switch (*value)
    {
    case MMAGIC_SECURITY_TYPE_SAE:
        return snprintf(buf, len, "sae");

    case MMAGIC_SECURITY_TYPE_OWE:
        return snprintf(buf, len, "owe");

    case MMAGIC_SECURITY_TYPE_OPEN:
        return snprintf(buf, len, "open");
    }

    return -1;
}

int string_to_enum_security_type(enum mmagic_security_type *value, const char *buf)
{
    if (strncmp("sae", buf, sizeof("sae")) == 0)
    {
        *value = MMAGIC_SECURITY_TYPE_SAE;
        return 0;
    }
    else if (strncmp("owe", buf, sizeof("owe")) == 0)
    {
        *value = MMAGIC_SECURITY_TYPE_OWE;
        return 0;
    }
    else if (strncmp("open", buf, sizeof("open")) == 0)
    {
        *value = MMAGIC_SECURITY_TYPE_OPEN;
        return 0;
    }

    return -1;
}

int enum_station_type_to_string(enum mmagic_station_type *value, char *buf, size_t len)
{
    switch (*value)
    {
    case MMAGIC_STATION_TYPE_NON_SENSOR:
        return snprintf(buf, len, "non_sensor");

    case MMAGIC_STATION_TYPE_SENSOR:
        return snprintf(buf, len, "sensor");
    }

    return -1;
}

int string_to_enum_station_type(enum mmagic_station_type *value, const char *buf)
{
    if (strncmp("non_sensor", buf, sizeof("non_sensor")) == 0)
    {
        *value = MMAGIC_STATION_TYPE_NON_SENSOR;
        return 0;
    }
    else if (strncmp("sensor", buf, sizeof("sensor")) == 0)
    {
        *value = MMAGIC_STATION_TYPE_SENSOR;
        return 0;
    }

    return -1;
}

int enum_iperf_mode_to_string(enum mmagic_iperf_mode *value, char *buf, size_t len)
{
    switch (*value)
    {
    case MMAGIC_IPERF_MODE_UDP_SERVER:
        return snprintf(buf, len, "udp_server");

    case MMAGIC_IPERF_MODE_TCP_SERVER:
        return snprintf(buf, len, "tcp_server");

    case MMAGIC_IPERF_MODE_UDP_CLIENT:
        return snprintf(buf, len, "udp_client");

    case MMAGIC_IPERF_MODE_TCP_CLIENT:
        return snprintf(buf, len, "tcp_client");
    }

    return -1;
}

int string_to_enum_iperf_mode(enum mmagic_iperf_mode *value, const char *buf)
{
    if (strncmp("udp_server", buf, sizeof("udp_server")) == 0)
    {
        *value = MMAGIC_IPERF_MODE_UDP_SERVER;
        return 0;
    }
    else if (strncmp("tcp_server", buf, sizeof("tcp_server")) == 0)
    {
        *value = MMAGIC_IPERF_MODE_TCP_SERVER;
        return 0;
    }
    else if (strncmp("udp_client", buf, sizeof("udp_client")) == 0)
    {
        *value = MMAGIC_IPERF_MODE_UDP_CLIENT;
        return 0;
    }
    else if (strncmp("tcp_client", buf, sizeof("tcp_client")) == 0)
    {
        *value = MMAGIC_IPERF_MODE_TCP_CLIENT;
        return 0;
    }

    return -1;
}

int uint16_t_to_string(uint16_t *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%u", *value);
}

int string_to_uint16_t(uint16_t *value, const char *buf)
{
    int32_t parsed_val = strtoul(buf, NULL, 10);
    if (parsed_val < 0 || parsed_val > UINT16_MAX)
    {
        return -1;
    }
    *value = (uint16_t)parsed_val;
    return 0;
}

int int16_t_to_string(int16_t *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%d", *value);
}

int string_to_int16_t(int16_t *value, const char *buf)
{
    int parsed_val = strtol(buf, NULL, 10);
    if (parsed_val < 0 || parsed_val > INT16_MAX || parsed_val < INT16_MIN)
    {
        return -1;
    }
    *value = (int16_t)parsed_val;
    return 0;
}

int struct_country_code_to_string(struct struct_country_code *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%.*s", 2, value->country_code);
}

int string_to_struct_country_code(struct struct_country_code *value, const char *buf)
{
    size_t len = strnlen(buf, sizeof(value->country_code));
    if (len > sizeof(value->country_code) - 1)
    {
        return -1;
    }

    strncpy((char *)value->country_code, buf, len);

    return 0;
}

int struct_ip_addr_to_string(struct struct_ip_addr *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%.*s", strnlen(value->addr, sizeof(value->addr)), value->addr);
}

int string_to_struct_ip_addr(struct struct_ip_addr *value, const char *buf)
{
    size_t len = strnlen(buf, sizeof(value->addr));
    if (len > sizeof(value->addr) - 1)
    {
        return -1;
    }

    strncpy((char *)value->addr, buf, len);
    /* Set the last character to terminate the string */
    value->addr[len] = 0;

    return 0;
}

int struct_mac_addr_to_string(struct struct_mac_addr *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
                    value->addr[0], value->addr[1], value->addr[2],
                    value->addr[3], value->addr[4], value->addr[5]);
}

int string_to_struct_mac_addr(struct struct_mac_addr *value, const char *buf)
{
    /* Need to provide an array of ints to sscanf otherwise it will overflow */
    int temp[6];
    int i;

    int ret = sscanf(buf, "%x:%x:%x:%x:%x:%x",
                     &temp[0], &temp[1], &temp[2],
                     &temp[3], &temp[4], &temp[5]);
    if (ret != 6)
    {
        return -1;
    }

    for (i = 0; i < 6; i++)
    {
        if (temp[i] > UINT8_MAX || temp[i] < 0)
        {
            return -1;
        }

        value->addr[i] = (uint8_t)temp[i];
    }

    return 0;
}

int struct_string_32_to_string(struct struct_string_32 *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%.*s", (int)value->len, value->data);
}

int string_to_struct_string_32(struct struct_string_32 *value, const char *buf)
{
    size_t len = strnlen(buf, sizeof(value->data));
    if (len > sizeof(value->data) - 1)
    {
        return -1;
    }

    strncpy((char *)value->data, buf, len);
    value->len = len;

    return 0;
}

int uint32_t_to_string(uint32_t *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%lu", *value);
}

int string_to_uint32_t(uint32_t *value, const char *buf)
{
    int parsed_val = strtoul(buf, NULL, 10);
    if (parsed_val < 0)
    {
        return parsed_val;
    }
    *value = (uint32_t)parsed_val;
    return 0;
}

int int32_t_to_string(int32_t *value, char *buf, size_t len)
{
    return snprintf(buf, len, "%ld", *value);
}

int string_to_int32_t(int32_t *value, const char *buf)
{
    int32_t parsed_val = strtol(buf, NULL, 10);
    *value = (int32_t)parsed_val;
    return 0;
}
