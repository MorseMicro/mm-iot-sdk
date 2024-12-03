/*
 *  TCP/IP or UDP/IP networking functions
 *
 *  Copyright The Mbed TLS Contributors
 *  Copyright 2023 Morse Micro
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include <endian.h>
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/error.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DNS.h"

/*
 * Initialize a context
 */
void mbedtls_net_init(mbedtls_net_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}


typedef int (*dns_success_callback_t)(mbedtls_net_context *ctx, struct freertos_addrinfo *addr);


static int dns_lookup(mbedtls_net_context *ctx, dns_success_callback_t callback,
                      const char *host, const char *port, int proto, int family)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct freertos_addrinfo hints, *addr_list, *cur;
    char *port_tail = NULL;
    int port_num = strtol(port, &port_tail, 10);

    if (port_tail == NULL || *port_tail != '\0')
    {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    ctx->freertos.type = proto == MBEDTLS_NET_PROTO_UDP ? FREERTOS_SOCK_DGRAM : FREERTOS_SOCK_STREAM;

    if (FreeRTOS_getaddrinfo(host, port, &hints, &addr_list) != 0)
    {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for(cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        cur->ai_addr->sin_port = htobe16(port_num);
        cur->ai_addr->sin_family = cur->ai_family;
        ret = callback(ctx, cur);
        if (ret == 0)
        {
            break;
        }
    }

    FreeRTOS_freeaddrinfo(addr_list);

    return ret;
}

static int net_connect_dns_callback(mbedtls_net_context *ctx, struct freertos_addrinfo *addr)
{
    int ret;
    int proto = (ctx->freertos.type == FREERTOS_SOCK_DGRAM) ? FREERTOS_IPPROTO_UDP : FREERTOS_IPPROTO_TCP;
    ctx->socket = FreeRTOS_socket(addr->ai_family, ctx->freertos.type, proto);
    if (ctx->socket == NULL)
    {
        return MBEDTLS_ERR_NET_SOCKET_FAILED;
    }

    ret = FreeRTOS_connect(ctx->socket, addr->ai_addr, addr->ai_addrlen);
    if (ret == 0)
    {
        ctx->freertos.socket_set = FreeRTOS_CreateSocketSet();
        return 0;
    }

    FreeRTOS_closesocket(ctx->socket);
    ctx->socket = NULL;
    return MBEDTLS_ERR_NET_CONNECT_FAILED;
}

/*
 * Initiate a TCP connection with host:port and the given protocol
 */
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                        const char *port, int proto)
{
    int ret = dns_lookup(ctx, net_connect_dns_callback, host, port, proto,
                         FREERTOS_AF_INET);
    if (ret == MBEDTLS_ERR_NET_UNKNOWN_HOST)
    {
        ret = dns_lookup(ctx, net_connect_dns_callback, host, port, proto,
                         FREERTOS_AF_INET6);
    }
    ctx->freertos.non_blocking_flag = 0;
    return ret;
}

static int net_bind_dns_callback(mbedtls_net_context *ctx, struct freertos_addrinfo *addr)
{
    if (FreeRTOS_bind(ctx->socket, addr->ai_addr, addr->ai_addrlen) != 0)
    {
        FreeRTOS_closesocket(ctx->socket);
        return MBEDTLS_ERR_NET_BIND_FAILED;
    }

    /* Listen only makes sense for TCP */
    if (addr->ai_protocol == MBEDTLS_NET_PROTO_TCP)
    {
        if(FreeRTOS_listen(ctx->socket, MBEDTLS_NET_LISTEN_BACKLOG) != 0)
        {
            FreeRTOS_closesocket(ctx->socket);
            return MBEDTLS_ERR_NET_LISTEN_FAILED;
        }
    }

    return 0;
}


/*
 * Create a listening socket on bind_ip:port
 */
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto)
{
    int ret = dns_lookup(ctx, net_bind_dns_callback, bind_ip, port, proto,
                         FREERTOS_AF_INET);
    if (ret == MBEDTLS_ERR_NET_UNKNOWN_HOST)
    {
        ret = dns_lookup(ctx, net_bind_dns_callback, bind_ip, port, proto,
                         FREERTOS_AF_INET6);
    }
    return ret;
}

/*
 * Accept a connection from a remote client
 */
int mbedtls_net_accept(mbedtls_net_context *bind_ctx,
                       mbedtls_net_context *client_ctx,
                       void *client_ip, size_t buf_size, size_t *ip_len)
{
    struct freertos_sockaddr client_addr;

    if (xSocketValid(bind_ctx->socket) == pdFALSE)
    {
        return MBEDTLS_ERR_NET_SOCKET_FAILED;
    }

    if (bind_ctx->freertos.type == FREERTOS_SOCK_STREAM)
    {
        /* TCP: actual accept() */
        client_ctx->socket = FreeRTOS_accept(bind_ctx->socket, &client_addr, NULL);
        if (client_ctx->socket == NULL)
        {
            return MBEDTLS_ERR_NET_BIND_FAILED;
        }
    }
    else
    {
        /* UDP not yet supported. */
        return MBEDTLS_ERR_NET_BIND_FAILED;
    }

    if (client_ip != NULL)
    {
        if (client_addr.sin_family == FREERTOS_AF_INET)
        {
            *ip_len = 4;
        }
        else
        {
            *ip_len = 16;
        }

        if (buf_size < *ip_len)
        {
            return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
        }
        memcpy(client_ip, &client_addr.sin_address, *ip_len);
    }

    return 0;
}

/*
 * Set the socket blocking or non-blocking
 */
int mbedtls_net_set_block(mbedtls_net_context *ctx)
{
    ctx->freertos.non_blocking_flag = 0;
    return 0;
}

int mbedtls_net_set_nonblock(mbedtls_net_context *ctx)
{
    ctx->freertos.non_blocking_flag = FREERTOS_MSG_DONTWAIT;
    return 0;
}

/**
 * @note The FreeRTOS+TCP implementation of this function creates a shared
 * Socket Set structure for each socket. This means that you cannot have
 * concurrent calls to mbedtls_net_poll() for the same socket.
 */

int mbedtls_net_poll(mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout)
{
    EventBits_t event_bits = eSELECT_EXCEPT;
    int ret;

    if (xSocketValid(ctx->socket) == pdFALSE)
    {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    if (ctx->freertos.socket_set == NULL)
    {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    if (rw & MBEDTLS_NET_POLL_READ)
    {
        event_bits |= eSELECT_READ;
    }

    if (rw & MBEDTLS_NET_POLL_WRITE)
    {
        event_bits |= eSELECT_WRITE;
    }

    FreeRTOS_FD_SET(ctx->socket, ctx->freertos.socket_set, event_bits);

    event_bits = FreeRTOS_select(ctx->freertos.socket_set, pdMS_TO_TICKS(timeout));

    ret = 0;

    if (event_bits & eSELECT_READ)
    {
        ret |= MBEDTLS_NET_POLL_READ;
    }
    if (event_bits & eSELECT_WRITE)
    {
        ret |= MBEDTLS_NET_POLL_WRITE;
    }

    return ret;
}

/*
 * Portable usleep helper
 */
void mbedtls_net_usleep(unsigned long usec)
{
    mmosal_task_sleep((usec + 500) / 1000);
}

/*
 * Read at most 'len' characters
 */
int mbedtls_net_recv(void *vctx, unsigned char *buf, size_t len)
{
    int ret;
    mbedtls_net_context *ctx = (mbedtls_net_context *)vctx;

    if (xSocketValid(ctx->socket) == pdFALSE)
    {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    ret = FreeRTOS_recv(ctx->socket, buf, len, ctx->freertos.non_blocking_flag);
    if (ret <= 0)
    {
        switch (ret)
        {
        case 0:
        case -pdFREERTOS_ERRNO_EINTR:
            return MBEDTLS_ERR_SSL_WANT_READ;

        case -pdFREERTOS_ERRNO_ENOTCONN:
            return MBEDTLS_ERR_NET_CONN_RESET;

        default:
            return MBEDTLS_ERR_NET_RECV_FAILED;
        }
    }

    return ret;
}

/*
 * Read at most 'len' characters, blocking for at most 'timeout' ms
 */
int mbedtls_net_recv_timeout(void *vctx, unsigned char *buf,
                             size_t len, uint32_t timeout)
{
    int ret;
    mbedtls_net_context *ctx = (mbedtls_net_context *)vctx;

    if (xSocketValid(ctx->socket) == pdFALSE)
    {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    if (timeout == 0)
    {
        /* If timeout is 0, mbedtls expects us to wait indefinitely */
        timeout = UINT32_MAX;
    }

    ret = mbedtls_net_poll(ctx, MBEDTLS_NET_POLL_READ, timeout);
    if (ret <= 0)
    {
        return ret;
    }

    if (ret & MBEDTLS_NET_POLL_READ)
    {
        return mbedtls_net_recv(vctx, buf, len);
    }

    return MBEDTLS_ERR_SSL_TIMEOUT;
}

/*
 * Write at most 'len' characters
 */
int mbedtls_net_send(void *vctx, const unsigned char *buf, size_t len)
{
    int ret;
    mbedtls_net_context *ctx = (mbedtls_net_context *)vctx;

    if (xSocketValid(ctx->socket) == pdFALSE)
    {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    ret = FreeRTOS_send(ctx->socket, buf, len, ctx->freertos.non_blocking_flag);
    if (ret <= 0)
    {
        switch (ret)
        {
        case 0:
        case -pdFREERTOS_ERRNO_EINTR:
            return MBEDTLS_ERR_SSL_WANT_WRITE;

        case -pdFREERTOS_ERRNO_ENOTCONN:
            return MBEDTLS_ERR_NET_CONN_RESET;

        default:
            return MBEDTLS_ERR_NET_SEND_FAILED;
        }
    }

    return ret;
}

/*
 * Close the connection
 */
void mbedtls_net_close(mbedtls_net_context *ctx)
{
    if (ctx->socket == NULL)
    {
        return;
    }

    FreeRTOS_DeleteSocketSet(ctx->freertos.socket_set);
    ctx->freertos.socket_set = NULL;
    FreeRTOS_closesocket(ctx->socket);
    ctx->socket = NULL;
}

/*
 * Gracefully close the connection
 */
void mbedtls_net_free(mbedtls_net_context *ctx)
{
    if (ctx->socket == NULL)
    {
        return;
    }

    FreeRTOS_DeleteSocketSet(ctx->freertos.socket_set);
    ctx->freertos.socket_set = NULL;
    FreeRTOS_shutdown(ctx->socket, 2);
    FreeRTOS_closesocket(ctx->socket);
    ctx->socket = NULL;
}
