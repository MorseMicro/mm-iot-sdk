/**
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmutils.h"
#include "mmconfig.h"
#include "mmipal.h"

/* We use MBEDTLS transport functions to abstract underlying IP stack */
#include "mbedtls/net.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_tcp.h"
#include "mmagic.h"
#include "m2m_api/mmagic_m2m_agent.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_tcp.def"

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

    char portstr[8];

    struct mbedtls_net_context *tcp_context =
        (struct mbedtls_net_context *)mmosal_malloc(sizeof(struct mbedtls_net_context));

    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    mbedtls_net_init(tcp_context);
    snprintf(portstr, sizeof(portstr), "%7d", cmd_args->port);

    int status = mbedtls_net_connect(tcp_context, (const char *)cmd_args->url.data,
                                     portstr, MBEDTLS_NET_PROTO_TCP);
    if (status != 0)
    {
        mmosal_free(tcp_context);
        return MMAGIC_STATUS_ERROR;
    }

    if (mmagic_m2m_agent_open_stream(core, tcp_context, &rsp_args->stream_id) != MMAGIC_STATUS_OK)
    {
        mbedtls_net_close(tcp_context);
        mmosal_free(tcp_context);
        return MMAGIC_STATUS_ERROR;
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

    char portstr[8];

    struct mbedtls_net_context *tcp_context =
        (struct mbedtls_net_context *)mmosal_malloc(sizeof(struct mbedtls_net_context));
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    mbedtls_net_init(tcp_context);
    snprintf(portstr, sizeof(portstr), "%7d", cmd_args->port);

    int status = mbedtls_net_bind(tcp_context, NULL, portstr, MBEDTLS_NET_PROTO_TCP);
    if (status != 0)
    {
        mmosal_free(tcp_context);
        return MMAGIC_STATUS_ERROR;
    }

    if (mmagic_m2m_agent_open_stream(core, tcp_context, &rsp_args->stream_id) != MMAGIC_STATUS_OK)
    {
        mbedtls_net_close(tcp_context);
        mmosal_free(tcp_context);
        return MMAGIC_STATUS_ERROR;
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

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    int len = mbedtls_net_recv_timeout(tcp_context, rsp_args->buffer.data,
                                       bytestoreceive, timeout);

    if (len == MBEDTLS_ERR_SSL_TIMEOUT)
    {
        rsp_args->buffer.len = 0;
        return MMAGIC_STATUS_TIMEOUT;
    }
    else if (len == 0)
    {
        /* Special case, when we haven't timed out but we receive a length of 0,
         * it means that the other side has closed the connection */
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

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    mbedtls_net_send(tcp_context, cmd_args->buffer.data, cmd_args->buffer.len);

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_read_poll(struct mmagic_data *core,
                                             const struct mmagic_core_tcp_read_poll_cmd_args *
                                             cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    int status = mbedtls_net_poll(tcp_context, MBEDTLS_NET_POLL_READ, timeout);

    if (status < 0)
    {
        return MMAGIC_STATUS_ERROR;
    }

    if (status == 0)
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

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    uint32_t timeout = cmd_args->timeout;

    /* A quirk in MBEDTLS treats timeout of 0 as indefinite, so work around it */
    if (timeout == 0)
    {
        timeout = 1;
    }

    int status = mbedtls_net_poll(tcp_context, MBEDTLS_NET_POLL_WRITE, timeout);

    if (status < 0)
    {
        return MMAGIC_STATUS_ERROR;
    }

    if (status == 0)
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

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    struct mbedtls_net_context *client_context =
        (struct mbedtls_net_context *)mmosal_malloc(sizeof(struct mbedtls_net_context));
    if (client_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    int status = mbedtls_net_accept(tcp_context, client_context, NULL, 0, NULL);
    if (status != 0)
    {
        mmosal_free(client_context);
        return MMAGIC_STATUS_ERROR;
    }

    if (mmagic_m2m_agent_open_stream(core, client_context,
                                     &rsp_args->stream_id) != MMAGIC_STATUS_OK)
    {
        mbedtls_net_close(client_context);
        mmosal_free(client_context);
        return MMAGIC_STATUS_ERROR;
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_tcp_close(struct mmagic_data *core,
                                         const struct mmagic_core_tcp_close_cmd_args *cmd_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);

    struct mbedtls_net_context *tcp_context = (struct mbedtls_net_context *)
        mmagic_m2m_agent_get_stream_context(core, cmd_args->stream_id);
    if (tcp_context == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }
    mbedtls_net_close(tcp_context);
    mmagic_m2m_agent_close_stream(core, cmd_args->stream_id);
    mmosal_free(tcp_context);

    return MMAGIC_STATUS_OK;
}
