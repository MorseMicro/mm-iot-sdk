/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "mmhal.h"
#include "transport_interface.h"
#include "core/autogen/mmagic_core_types.h"
#include "mmagic_core_utils.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmconfig.h"
#include "mmipal.h"

/* We use MBEDTLS transport functions to abstract underlying IP stack */
#include "mbedtls/net.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_tcp.h"
#include "core/autogen/mmagic_core_tls.h"
#include "mmagic.h"
#include "m2m_api/mmagic_m2m_agent.h"

void mmagic_core_tcp_init(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

void mmagic_core_tcp_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

enum mmagic_status mmagic_core_tcp_connect(struct mmagic_data *core,
                                           const struct mmagic_core_tcp_connect_cmd_args *cmd_args,
                                           struct mmagic_core_tcp_connect_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    /* TLS credentials */
    NetworkCredentials_t creds = { 0 };
    if (cmd_args->enable_tls)
    {
        struct mmagic_tls_data *tls_data = mmagic_data_get_tls(core);
        enum mmagic_status status = mmagic_init_tls_credentials(&creds, tls_data);
        if (status != MMAGIC_STATUS_OK)
        {
            return status;
        }
    }

    /* TCP/TLS context */
    NetworkContext_t *network_context = (NetworkContext_t *)mmosal_malloc(sizeof(*network_context));
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    memset(network_context, 0, sizeof(*network_context));

    /* TCP connection and TLS handshake */
    const char *url = cmd_args->url.data;
    const NetworkCredentials_t *creds_ptr = cmd_args->enable_tls ? &creds : NULL;
    TransportStatus_t connect_status = transport_connect(network_context, url, cmd_args->port,
                                                         creds_ptr);
    if (connect_status != TRANSPORT_SUCCESS)
    {
        mmosal_free(network_context);
        return mmagic_transport_status_to_mmagic_status(connect_status);
    }

    /* mmagic stream */
    enum mmagic_status stream_status = mmagic_m2m_agent_open_stream(core, network_context,
                                                                    &rsp_args->stream_id);
    if (stream_status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(network_context);
        mmosal_free(network_context);
        return stream_status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_bind(struct mmagic_data *core,
                                        const struct mmagic_core_tcp_bind_cmd_args *cmd_args,
                                        struct mmagic_core_tcp_bind_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    NetworkContext_t *network_context = (NetworkContext_t *)mmosal_malloc(sizeof(*network_context));
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    memset(network_context, 0, sizeof(*network_context));

    mbedtls_net_init(&network_context->tcpSocket);

    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%u", cmd_args->port);

    int ret = mbedtls_net_bind(&network_context->tcpSocket, NULL, portstr, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0)
    {
        mmosal_free(network_context);
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    enum mmagic_status status =
        mmagic_m2m_agent_open_stream(core, network_context, &rsp_args->stream_id);
    if (status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(network_context);
        mmosal_free(network_context);
        return status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_recv(struct mmagic_data *core,
                                        const struct mmagic_core_tcp_recv_cmd_args *cmd_args,
                                        struct mmagic_core_tcp_recv_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    size_t bytestoreceive = cmd_args->len;
    if (bytestoreceive > sizeof(rsp_args->buffer.data))
    {
        bytestoreceive = sizeof(rsp_args->buffer.data);
    }

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    /* Reads with either TLS or clear TCP */
    /* This block of code is equivalent to transport_recv except 2 key differences
     * - Has configurable timeout
     * - Returns a timeout error on timeout, transport_recv returns 0
     */
    int len = 0;
    if (network_context->sslContext.useTLS)
    {
        uint32_t old_timeout = network_context->sslContext.config.MBEDTLS_PRIVATE(read_timeout);
        mbedtls_ssl_conf_read_timeout(&network_context->sslContext.config, timeout);

        len = mbedtls_ssl_read(&network_context->sslContext.context, rsp_args->buffer.data,
                               bytestoreceive);

        mbedtls_ssl_conf_read_timeout(&network_context->sslContext.config, old_timeout);
    }
    else
    {
        len = mbedtls_net_recv_timeout(&network_context->tcpSocket, rsp_args->buffer.data,
                                       bytestoreceive, timeout);
    }

    if (len == MBEDTLS_ERR_SSL_TIMEOUT)
    {
        rsp_args->buffer.len = 0;
        return MMAGIC_STATUS_TIMEOUT;
    }
    else if (len == 0 || len == MBEDTLS_ERR_NET_CONN_RESET)
    {
        /* Special case, when we haven't timed out but we receive a length of 0,
         * it means that the other side has closed the connection.
         * This will also be triggered if there is an explicit status
         * indicating the other side has closed the connection.
         */
        rsp_args->buffer.len = 0;
        return MMAGIC_STATUS_CLOSED;
    }
    else if (len < 0)
    {
        rsp_args->buffer.len = 0;
        return MMAGIC_STATUS_ERROR;
    }
    rsp_args->buffer.len = (uint16_t)len;

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_send(struct mmagic_data *core,
                                        const struct mmagic_core_tcp_send_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    int send_status = transport_send(network_context, cmd_args->buffer.data, cmd_args->buffer.len);
    if (send_status < 0)
    {
        return mmagic_mbedtls_return_code_to_mmagic_status(send_status);
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_read_poll(struct mmagic_data *core,
                                             const struct mmagic_core_tcp_read_poll_cmd_args *
                                             cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    int ret = mbedtls_net_poll(&network_context->tcpSocket, MBEDTLS_NET_POLL_READ, timeout);

    if (ret < 0)
    {
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    if (ret == 0)
    {
        return MMAGIC_STATUS_TIMEOUT;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_write_poll(struct mmagic_data *core,
                                              const struct mmagic_core_tcp_write_poll_cmd_args *
                                              cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    int ret = mbedtls_net_poll(&network_context->tcpSocket, MBEDTLS_NET_POLL_WRITE, timeout);

    if (ret < 0)
    {
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    if (ret == 0)
    {
        return MMAGIC_STATUS_TIMEOUT;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_accept(struct mmagic_data *core,
                                          const struct mmagic_core_tcp_accept_cmd_args *cmd_args,
                                          struct mmagic_core_tcp_accept_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    NetworkContext_t *client_context = (NetworkContext_t *)mmosal_malloc(sizeof(*client_context));
    if (client_context == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    memset(client_context, 0, sizeof(*client_context));

    mbedtls_net_init(&client_context->tcpSocket);
    int ret = mbedtls_net_accept(&network_context->tcpSocket, &client_context->tcpSocket, NULL, 0,
                                 NULL);
    if (ret != 0)
    {
        mmosal_free(client_context);
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    enum mmagic_status status =
        mmagic_m2m_agent_open_stream(core, client_context, &rsp_args->stream_id);
    if (status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(client_context);
        mmosal_free(client_context);
        return status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_close(struct mmagic_data *core,
                                         const struct mmagic_core_tcp_close_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }
    transport_disconnect(network_context);
    mmagic_m2m_agent_close_stream(core, cmd_args->stream_id);
    mmosal_free(network_context);

    return MMAGIC_STATUS_OK;
}
