/*
 * Copyright 2024-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmagic_core_utils.h"
#include "mbedtls/net.h"
#include "core_sntp_serializer.h"
#include "core/autogen/mmagic_core_types.h"
#include "core/autogen/mmagic_core_tls.h"
#include "core_mqtt_serializer.h"
#include "transport_interface.h"
#include "mmhal.h"

/** Arbitrary "recent" timestamp threshold to detect if the RTC is synced via NTP.
 * Corresponds to January 1st 2025 UTC (seconds) */
#define NTP_SYNC_THRESHOLD_SEC 1735689600

enum mmagic_status mmagic_mmipal_status_to_mmagic_status(enum mmipal_status status)
{
    switch (status)
    {
    case MMIPAL_SUCCESS:
        return MMAGIC_STATUS_OK;

    case MMIPAL_INVALID_ARGUMENT:
        return MMAGIC_STATUS_INVALID_ARG;

    case MMIPAL_NO_MEM:
        return MMAGIC_STATUS_NO_MEM;

    case MMIPAL_NO_LINK:
        return MMAGIC_STATUS_NO_LINK;

    case MMIPAL_NOT_SUPPORTED:
        return MMAGIC_STATUS_NOT_SUPPORTED;

        /* Note that we do not add a default case here. The compiler should therefore give us a
         * warning if new fields get added to mmipal_status and we do not handle them. */
    }

    return MMAGIC_STATUS_ERROR;
}

enum mmagic_status mmagic_mmwlan_status_to_mmagic_status(enum mmwlan_status status)
{
    switch (status)
    {
    case MMWLAN_SUCCESS:
        return MMAGIC_STATUS_OK;

    case MMWLAN_ERROR:
        return MMAGIC_STATUS_ERROR;

    case MMWLAN_INVALID_ARGUMENT:
        return MMAGIC_STATUS_INVALID_ARG;

    case MMWLAN_UNAVAILABLE:
        return MMAGIC_STATUS_UNAVAILABLE;

    case MMWLAN_CHANNEL_LIST_NOT_SET:
        return MMAGIC_STATUS_CHANNEL_LIST_NOT_SET;

    case MMWLAN_NO_MEM:
        return MMAGIC_STATUS_NO_MEM;

    case MMWLAN_TIMED_OUT:
        return MMAGIC_STATUS_TIMEOUT;

    case MMWLAN_SHUTDOWN_BLOCKED:
        return MMAGIC_STATUS_SHUTDOWN_BLOCKED;

    case MMWLAN_CHANNEL_INVALID:
        return MMAGIC_STATUS_CHANNEL_INVALID;

    case MMWLAN_NOT_FOUND:
        return MMAGIC_STATUS_NOT_FOUND;

    case MMWLAN_NOT_RUNNING:
        return MMAGIC_STATUS_NOT_RUNNING;

        /* Note that we do not add a default case here. The compiler should therefore give us a
         * warning if new fields get added to mmwlan_status and we do not handle them. */
    }

    return MMAGIC_STATUS_ERROR;
}

enum mmagic_status mmagic_mbedtls_return_code_to_mmagic_status(int ret)
{
    switch (ret)
    {
    case MBEDTLS_ERR_NET_UNKNOWN_HOST:
        return MMAGIC_STATUS_UNKNOWN_HOST;

    case MBEDTLS_ERR_NET_SOCKET_FAILED:
        return MMAGIC_STATUS_SOCKET_FAILED;

    case MBEDTLS_ERR_NET_CONNECT_FAILED:
        return MMAGIC_STATUS_SOCKET_CONNECT_FAILED;

    case MBEDTLS_ERR_NET_BIND_FAILED:
        return MMAGIC_STATUS_SOCKET_BIND_FAILED;

    case MBEDTLS_ERR_NET_LISTEN_FAILED:
        return MMAGIC_STATUS_SOCKET_LISTEN_FAILED;

    case MBEDTLS_ERR_NET_SEND_FAILED:
        return MMAGIC_STATUS_SOCKET_SEND_FAILED;

    case MBEDTLS_ERR_NET_CONN_RESET:
        return MMAGIC_STATUS_CLOSED;

    default:
        return MMAGIC_STATUS_ERROR;
    }
}

enum mmagic_status mmagic_transport_status_to_mmagic_status(TransportStatus_t status)
{
    switch (status)
    {
    case TRANSPORT_INVALID_PARAMETER:
        return MMAGIC_STATUS_INVALID_ARG;

    case TRANSPORT_INSUFFICIENT_MEMORY:
        return MMAGIC_STATUS_NO_MEM;

    case TRANSPORT_INVALID_CREDENTIALS:
        return MMAGIC_STATUS_INVALID_CREDENTIALS;

    case TRANSPORT_HANDSHAKE_FAILED:
        return MMAGIC_STATUS_HANDSHAKE_FAILED;

    case TRANSPORT_AUTHENTICATION_FAILED:
        return MMAGIC_STATUS_AUTHENTICATION_FAILED;

    case TRANSPORT_CONNECT_FAILURE:
        return MMAGIC_STATUS_SOCKET_CONNECT_FAILED;

    default:
        return MMAGIC_STATUS_ERROR;
    }
}

enum mmagic_status mmagic_coresntp_to_mmagic_status(int status)
{
    switch (status)
    {
    case 1: /* SntpErrorBadParameter */
        return MMAGIC_STATUS_INVALID_ARG;

    case 2: /* SntpRejectedResponse */
    case 3: /* SntpRejectedResponseChangeServer */
    case 5: /* SntpRejectedResponseOtherCode */
        return MMAGIC_STATUS_NTP_KOD_RECEIVED;

    case 4: /* SntpRejectedResponseRetryWithBackoff */
        return MMAGIC_STATUS_NTP_KOD_BACKOFF_RECEIVED;

    case 10: /* SntpErrorDnsFailure */
        return MMAGIC_STATUS_UNKNOWN_HOST;

    default:
        return MMAGIC_STATUS_ERROR;
    }
}

enum mmagic_status mmagic_init_tls_credentials(NetworkCredentials_t *creds,
                                               struct mmagic_tls_data *tls_data)
{
    creds->pRootCa = tls_data->config.root_ca_certificate.data;
    creds->rootCaSize = tls_data->config.root_ca_certificate.len;
    creds->pPrivateKey = tls_data->config.client_private_key.data;
    creds->privateKeySize = tls_data->config.client_private_key.len;
    creds->pClientCert = tls_data->config.client_certificate.data;
    creds->clientCertSize = tls_data->config.client_certificate.len;

    if (!creds->rootCaSize || !creds->privateKeySize || !creds->clientCertSize)
    {
        return MMAGIC_STATUS_MISSING_CREDENTIALS;
    }

    /*
     * If get_time returns a timestamp that is less than that number, we assume it has not been
     * updated using NTP
     * Without that check, the TLS handshake would fail during certificate verification anyway
     */
    if ((uint64_t)mmhal_get_time() < NTP_SYNC_THRESHOLD_SEC)
    {
        return MMAGIC_STATUS_TIME_NOT_SYNCHRONIZED;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_mqtt_status_to_mmagic_status(MQTTStatus_t mqtt_status)
{
    switch (mqtt_status)
    {
    case MQTTBadParameter:
        return MMAGIC_STATUS_INVALID_ARG;

    case MQTTNoMemory:
        return MMAGIC_STATUS_NO_MEM;

    case MQTTSendFailed:
        return MMAGIC_STATUS_SOCKET_SEND_FAILED;

    case MQTTServerRefused:
        return MMAGIC_STATUS_MQTT_REFUSED;

    case MQTTKeepAliveTimeout:
        return MMAGIC_STATUS_MQTT_KEEPALIVE_TIMEOUT;

    default:
        return MMAGIC_STATUS_ERROR;
    }
}
