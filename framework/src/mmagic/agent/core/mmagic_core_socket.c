/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>
#include <string.h>

#include "transport_interface.h"
#include "core/autogen/mmagic_core_types.h"
#include "mmagic_core_utils.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmconfig.h"
#include "mmipal.h"

/* We use MBEDTLS transport functions to abstract underlying IP stack */
#include "mbedtls/net.h"
#include "mbedtls/error.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_socket.h"
#include "core/autogen/mmagic_core_tls.h"
#include "mmagic.h"
#include "m2m_api/mmagic_m2m_agent.h"

/** Priority of the socketevt task. */
#ifndef SOCKET_EVENT_HANDLER_TASK_PRIO
#define SOCKET_EVENT_HANDLER_TASK_PRIO (MMOSAL_TASK_PRI_LOW)
#endif

/** Stack size of the socketevt task (in 32-bit words). */
#ifndef SOCKET_EVENT_HANDLER_TASK_STACK_WORDS
#define SOCKET_EVENT_HANDLER_TASK_STACK_WORDS (192)
#endif

/** Private data structure specific to this module. */
struct mmagic_core_socket_private_data
{
    /** Background task for handling SOCKET stream events. */
    struct mmosal_task *evt_task;
    /** Binary semaphore to manage sleeping/waking of @c evt_task. */
    struct mmosal_semb *evt_task_waker;
    /** Track which streams have pending RX ready events. */
    atomic_uint pending_rx_ready;
};

void mmagic_core_socket_event_handler(void *arg)
{
    struct mmagic_data *core = (struct mmagic_data *)arg;
    struct mmagic_core_socket_private_data *priv =
        (struct mmagic_core_socket_private_data *)core->socket_data.priv;

    while (true)
    {
        for (size_t stream_id = 1; stream_id < MMAGIC_MAX_STREAMS; stream_id++)
        {
            if (mmagic_m2m_agent_get_stream_subsystem_id(core, stream_id) == mmagic_socket)
            {
                NetworkContext_t *network_context =
                    (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, stream_id);
                if (network_context == NULL)
                {
                    /* This can occur if the socket is in the process of closing. */
                    continue;
                }

                if (mbedtls_net_check_and_clear_rx_ready(&(network_context->socket)) > 0)
                {
                    struct mmagic_core_event_socket_rx_ready_args args = { .stream_id = stream_id };
                    mmagic_core_event_socket_rx_ready(core, &args);
                }
            }
        }

        mmosal_semb_wait(priv->evt_task_waker, UINT32_MAX);
    }
}

void mmagic_core_socket_init(struct mmagic_data *core)
{
    struct mmagic_core_socket_private_data *priv =
        (struct mmagic_core_socket_private_data *)mmosal_calloc(1, sizeof(*priv));
    MMOSAL_ASSERT(priv != NULL);
    core->socket_data.priv = priv;
}

void mmagic_core_socket_start(struct mmagic_data *core)
{
    struct mmagic_core_socket_private_data *priv =
        (struct mmagic_core_socket_private_data *)core->socket_data.priv;
    priv->evt_task_waker = mmosal_semb_create("socketwk");
    MMOSAL_ASSERT(priv->evt_task_waker != NULL);
    priv->evt_task = mmosal_task_create(mmagic_core_socket_event_handler,
                                        core,
                                        SOCKET_EVENT_HANDLER_TASK_PRIO,
                                        SOCKET_EVENT_HANDLER_TASK_STACK_WORDS,
                                        "socketevt");
    MMOSAL_ASSERT(priv->evt_task != NULL);
    core->socket_data.is_started = true;
}

enum mmagic_status mmagic_core_socket_connect(
    struct mmagic_data *core,
    const struct mmagic_core_socket_connect_cmd_args *cmd_args,
    struct mmagic_core_socket_connect_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    if (cmd_args->protocol != MMAGIC_SOCKET_PROTO_TCP &&
        cmd_args->protocol != MMAGIC_SOCKET_PROTO_UDP)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

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

    /* SOCKET/TLS context */
    NetworkContext_t *network_context = (NetworkContext_t *)mmosal_malloc(sizeof(*network_context));
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    memset(network_context, 0, sizeof(*network_context));

    /* SOCKET connection and TLS handshake */
    const char *url = cmd_args->url.data;
    const NetworkCredentials_t *creds_ptr = cmd_args->enable_tls ? &creds : NULL;
    TransportProtocol_t protocol = (cmd_args->protocol == MMAGIC_SOCKET_PROTO_TCP) ? TRANSPORT_TCP :
                                                                                     TRANSPORT_UDP;
    TransportStatus_t connect_status =
        transport_connect(network_context, url, protocol, cmd_args->port, creds_ptr);
    if (connect_status != TRANSPORT_SUCCESS)
    {
        mmosal_free(network_context);
        return mmagic_transport_status_to_mmagic_status(connect_status);
    }

    /* mmagic stream */
    enum mmagic_status stream_status =
        mmagic_m2m_agent_open_stream(core, network_context, mmagic_socket, &rsp_args->stream_id);
    if (stream_status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(network_context);
        mmosal_free(network_context);
        return stream_status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_socket_bind(struct mmagic_data *core,
                                           const struct mmagic_core_socket_bind_cmd_args *cmd_args,
                                           struct mmagic_core_socket_bind_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    if (cmd_args->protocol != MMAGIC_SOCKET_PROTO_TCP &&
        cmd_args->protocol != MMAGIC_SOCKET_PROTO_UDP)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    NetworkContext_t *network_context = (NetworkContext_t *)mmosal_malloc(sizeof(*network_context));
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    memset(network_context, 0, sizeof(*network_context));

    mbedtls_net_init(&network_context->socket);

    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%u", cmd_args->port);
    int protocol = (cmd_args->protocol == MMAGIC_SOCKET_PROTO_TCP) ? MBEDTLS_NET_PROTO_TCP :
                                                                     MBEDTLS_NET_PROTO_UDP;
    int ret = mbedtls_net_bind(&network_context->socket, NULL, portstr, protocol);
    if (ret != 0)
    {
        mmosal_free(network_context);
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    enum mmagic_status status =
        mmagic_m2m_agent_open_stream(core, network_context, mmagic_socket, &rsp_args->stream_id);
    if (status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(network_context);
        mmosal_free(network_context);
        return status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_socket_recv(struct mmagic_data *core,
                                           const struct mmagic_core_socket_recv_cmd_args *cmd_args,
                                           struct mmagic_core_socket_recv_rsp_args *rsp_args)
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

    /* Reads with either TLS or clear SOCKET
     * A quirk in MBEDTLS treats timeout of 0 as indefinite, set to 1 instead */
    int len = transport_recv_with_timeout(network_context,
                                          rsp_args->buffer.data,
                                          bytestoreceive,
                                          MM_MAX(cmd_args->timeout, 1));

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

enum mmagic_status mmagic_core_socket_send(struct mmagic_data *core,
                                           const struct mmagic_core_socket_send_cmd_args *cmd_args)
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

enum mmagic_status mmagic_core_socket_read_poll(
    struct mmagic_data *core,
    const struct mmagic_core_socket_read_poll_cmd_args *cmd_args)
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

    int ret = mbedtls_net_poll(&network_context->socket, MBEDTLS_NET_POLL_READ, timeout);

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

enum mmagic_status mmagic_core_socket_write_poll(
    struct mmagic_data *core,
    const struct mmagic_core_socket_write_poll_cmd_args *cmd_args)
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

    int ret = mbedtls_net_poll(&network_context->socket, MBEDTLS_NET_POLL_WRITE, timeout);

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

enum mmagic_status mmagic_core_socket_accept(
    struct mmagic_data *core,
    const struct mmagic_core_socket_accept_cmd_args *cmd_args,
    struct mmagic_core_socket_accept_rsp_args *rsp_args)
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

    mbedtls_net_init(&client_context->socket);
    int ret = mbedtls_net_accept(&network_context->socket, &client_context->socket, NULL, 0, NULL);
    if (ret != 0)
    {
        mmosal_free(client_context);
        return mmagic_mbedtls_return_code_to_mmagic_status(ret);
    }

    enum mmagic_status status =
        mmagic_m2m_agent_open_stream(core, client_context, mmagic_socket, &rsp_args->stream_id);
    if (status != MMAGIC_STATUS_OK)
    {
        transport_disconnect(client_context);
        mmosal_free(client_context);
        return status;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_socket_close(
    struct mmagic_data *core,
    const struct mmagic_core_socket_close_cmd_args *cmd_args)
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
    enum mmagic_status status = mmagic_m2m_agent_close_stream(core, cmd_args->stream_id);
    mmosal_free(network_context);

    return status;
}

static void mmagic_core_socket_rx_ready_handler(struct mbedtls_net_context *net_ctx, void *arg)
{
    struct mmagic_data *core = (struct mmagic_data *)arg;
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(net_ctx != NULL);

    struct mmagic_core_socket_private_data *priv =
        (struct mmagic_core_socket_private_data *)core->socket_data.priv;

    mmosal_semb_give(priv->evt_task_waker);
}

enum mmagic_status mmagic_core_socket_set_rx_ready_evt_enabled(
    struct mmagic_data *core,
    const struct mmagic_core_socket_set_rx_ready_evt_enabled_cmd_args *cmd_args)
{
    NetworkContext_t *network_context =
        (NetworkContext_t *)mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (network_context == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    mbedtls_net_rx_callback_t rx_callback =
        cmd_args->enabled ? mmagic_core_socket_rx_ready_handler : NULL;

    int ret = mbedtls_net_register_rx_callback(&(network_context->socket), rx_callback, core);
    if (ret == 0)
    {
        return MMAGIC_STATUS_OK;
    }
    else if (ret == MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED)
    {
        return MMAGIC_STATUS_NOT_SUPPORTED;
    }
    else
    {
        return MMAGIC_STATUS_ERROR;
    }
}
