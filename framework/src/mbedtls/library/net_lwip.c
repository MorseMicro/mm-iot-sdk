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

#if defined(MBEDTLS_NET_LWIP_C)

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/error.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#define IS_EINTR(ret) ((ret) == EINTR)

/*
 * Return 0 if the file descriptor is valid, an error otherwise.
 * If for_select != 0, check whether the file descriptor is within the range
 * allowed for fd_set used for the FD_xxx macros and the select() function.
 */
static int check_fd(int fd, int for_select)
{
    if (fd < 0) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    /* A limitation of select() is that it only works with file descriptors
     * that are strictly less than FD_SETSIZE. This is a limitation of the
     * fd_set type. Error out early, because attempting to call FD_SET on a
     * large file descriptor is a buffer overflow on typical platforms. */
    if (for_select && fd >= FD_SETSIZE) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    return 0;
}

/*
 * Initialize a context
 */
void mbedtls_net_init(mbedtls_net_context *ctx)
{
    ctx->fd = -1;
}

/*
 * Initiate a TCP connection with host:port and the given protocol
 */
int mbedtls_net_connect(mbedtls_net_context *ctx, const char *host,
                        const char *port, int proto)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct addrinfo hints, *addr_list, *cur;

    /* Do name resolution with both IPv6 and IPv4 */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;

    if( lwip_getaddrinfo( host, port, &hints, &addr_list ) != 0 )
        return( MBEDTLS_ERR_NET_UNKNOWN_HOST );

    /* Try the sockaddrs until a connection succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for( cur = addr_list; cur != NULL; cur = cur->ai_next )
    {
        ctx->fd = (int) lwip_socket( cur->ai_family, cur->ai_socktype,
                            cur->ai_protocol );
        if( ctx->fd < 0 )
        {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if( lwip_connect( ctx->fd, cur->ai_addr, cur->ai_addrlen ) == 0 )
        {
            ret = 0;
            break;
        }

        lwip_close( ctx->fd );
        ctx->fd = -1;
        ret = MBEDTLS_ERR_NET_CONNECT_FAILED;
    }

    lwip_freeaddrinfo( addr_list );

    return( ret );
}

/*
 * Create a listening socket on bind_ip:port
 */
int mbedtls_net_bind(mbedtls_net_context *ctx, const char *bind_ip, const char *port, int proto)
{
    int n, ret;
    struct timeval t;
    struct addrinfo hints, *addr_list, *cur;

    /* Bind to IPv6 and/or IPv4, but only in the desired protocol */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = proto == MBEDTLS_NET_PROTO_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = proto == MBEDTLS_NET_PROTO_UDP ? IPPROTO_UDP : IPPROTO_TCP;
    if( bind_ip == NULL )
        hints.ai_flags = AI_PASSIVE;

    if( lwip_getaddrinfo( bind_ip, port, &hints, &addr_list ) != 0 )
        return( MBEDTLS_ERR_NET_UNKNOWN_HOST );

    /* Try the sockaddrs until a binding succeeds */
    ret = MBEDTLS_ERR_NET_UNKNOWN_HOST;
    for( cur = addr_list; cur != NULL; cur = cur->ai_next )
    {
        ctx->fd = (int) lwip_socket( cur->ai_family, cur->ai_socktype,
                            cur->ai_protocol );
        if( ctx->fd < 0 )
        {
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        n = 1;
        if( lwip_setsockopt( ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                        (const char *) &n, sizeof( n ) ) != 0 )
        {
            close( ctx->fd );
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        /* Set receive timeout to a high value, by default this is 0 */
        t.tv_sec = __INT32_MAX__ / 1000;
        t.tv_usec = 0;
        if( lwip_setsockopt( ctx->fd, SOL_SOCKET, SO_RCVTIMEO,
                        (const char *) &t, sizeof( t ) ) != 0 )
        {
            close( ctx->fd );
            ret = MBEDTLS_ERR_NET_SOCKET_FAILED;
            continue;
        }

        if( lwip_bind( ctx->fd, cur->ai_addr, cur->ai_addrlen ) != 0 )
        {
            close( ctx->fd );
            ret = MBEDTLS_ERR_NET_BIND_FAILED;
            continue;
        }

        /* Listen only makes sense for TCP */
        if( proto == MBEDTLS_NET_PROTO_TCP )
        {
            if( lwip_listen( ctx->fd, MBEDTLS_NET_LISTEN_BACKLOG ) != 0 )
            {
                lwip_close( ctx->fd );
                ret = MBEDTLS_ERR_NET_LISTEN_FAILED;
                continue;
            }
        }

        /* Bind was successful */
        ret = 0;
        break;
    }

    lwip_freeaddrinfo( addr_list );

    return( ret );
}

/*
 * Check if the requested operation would be blocking on a non-blocking socket
 * and thus 'failed' with a negative return value.
 *
 * Note: on a blocking socket this function always returns 0!
 */
static int net_would_block(const mbedtls_net_context *ctx)
{
    int err = errno;

    /*
     * Never return 'WOULD BLOCK' on a blocking socket
     */
    if ((lwip_fcntl(ctx->fd, F_GETFL, 0) & O_NONBLOCK) != O_NONBLOCK) {
        errno = err;
        return 0;
    }

    switch (errno = err) {
        case EAGAIN:
            return 1;
    }
    return 0;
}

/*
 * Accept a connection from a remote client
 */
int mbedtls_net_accept(mbedtls_net_context *bind_ctx,
                       mbedtls_net_context *client_ctx,
                       void *client_ip, size_t buf_size, size_t *ip_len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int type;

    struct sockaddr_storage client_addr;

    int n = (int) sizeof(client_addr);
    int type_len = (int) sizeof(type);

    /* Is this a TCP or UDP socket? */
    if (lwip_getsockopt(bind_ctx->fd, SOL_SOCKET, SO_TYPE,
                   (void *) &type, (socklen_t*)&type_len) != 0 ||
        (type != SOCK_STREAM && type != SOCK_DGRAM)) {
        return MBEDTLS_ERR_NET_ACCEPT_FAILED;
    }

    if (type == SOCK_STREAM) {
        /* TCP: actual accept() */
        ret = client_ctx->fd = (int) lwip_accept(bind_ctx->fd,
                                            (struct sockaddr *) &client_addr, (socklen_t*)&n);
    } else {
        /* UDP: wait for a message, but keep it in the queue */
        char buf[1] = { 0 };

        ret = (int) lwip_recvfrom(bind_ctx->fd, buf, sizeof(buf), MSG_PEEK,
                             (struct sockaddr *) &client_addr, (socklen_t*)&n);

    }

    if (ret < 0) {
        if (net_would_block(bind_ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        return MBEDTLS_ERR_NET_ACCEPT_FAILED;
    }

    /* UDP: hijack the listening socket to communicate with the client,
     * then bind a new socket to accept new connections */
    if (type != SOCK_STREAM) {
        struct sockaddr_storage local_addr;
        int one = 1;

        if (lwip_connect(bind_ctx->fd, (struct sockaddr *) &client_addr, n) != 0) {
            return MBEDTLS_ERR_NET_ACCEPT_FAILED;
        }

        client_ctx->fd = bind_ctx->fd;
        bind_ctx->fd   = -1; /* In case we exit early */

        n = sizeof(struct sockaddr_storage);
        if (lwip_getsockname(client_ctx->fd,
                        (struct sockaddr *) &local_addr, (socklen_t*)&n) != 0 ||
            (bind_ctx->fd = (int) socket(local_addr.ss_family,
                                         SOCK_DGRAM, IPPROTO_UDP)) < 0 ||
            lwip_setsockopt(bind_ctx->fd, SOL_SOCKET, SO_REUSEADDR,
                       (const char *) &one, sizeof(one)) != 0) {
            return MBEDTLS_ERR_NET_SOCKET_FAILED;
        }

        if (lwip_bind(bind_ctx->fd, (struct sockaddr *) &local_addr, n) != 0) {
            return MBEDTLS_ERR_NET_BIND_FAILED;
        }
    }

    if (client_ip != NULL) {
#if defined(LWIP_IPV4) && LWIP_IPV4
        if (client_addr.ss_family == AF_INET) {
            struct sockaddr_in *addr4 = (struct sockaddr_in *) &client_addr;
            *ip_len = sizeof(addr4->sin_addr.s_addr);

            if (buf_size < *ip_len) {
                return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
            }

            memcpy(client_ip, &addr4->sin_addr.s_addr, *ip_len);
        }
#endif
#if defined(LWIP_IPV4) && LWIP_IPV6
        if  (client_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) &client_addr;
            *ip_len = sizeof(addr6->sin6_addr.s6_addr);

            if (buf_size < *ip_len) {
                return MBEDTLS_ERR_NET_BUFFER_TOO_SMALL;
            }

            memcpy(client_ip, &addr6->sin6_addr.s6_addr, *ip_len);
        }
#endif
    }

    return 0;
}

/*
 * Set the socket blocking or non-blocking
 */
int mbedtls_net_set_block(mbedtls_net_context *ctx)
{
    return lwip_fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL, 0) & ~O_NONBLOCK);
}

int mbedtls_net_set_nonblock(mbedtls_net_context *ctx)
{
    return lwip_fcntl(ctx->fd, F_SETFL, fcntl(ctx->fd, F_GETFL, 0) | O_NONBLOCK);
}

/*
 * Check if data is available on the socket
 */

int mbedtls_net_poll(mbedtls_net_context *ctx, uint32_t rw, uint32_t timeout)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct timeval tv;

    fd_set read_fds;
    fd_set write_fds;

    int fd = ctx->fd;

    ret = check_fd(fd, 1);
    if (ret != 0) {
        return ret;
    }

    FD_ZERO(&read_fds);
    if (rw & MBEDTLS_NET_POLL_READ) {
        rw &= ~MBEDTLS_NET_POLL_READ;
        FD_SET(fd, &read_fds);
    }

    FD_ZERO(&write_fds);
    if (rw & MBEDTLS_NET_POLL_WRITE) {
        rw &= ~MBEDTLS_NET_POLL_WRITE;
        FD_SET(fd, &write_fds);
    }

    if (rw != 0) {
        return MBEDTLS_ERR_NET_BAD_INPUT_DATA;
    }

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    do {
        ret = lwip_select(fd + 1, &read_fds, &write_fds, NULL,
                     timeout == (uint32_t) -1 ? NULL : &tv);
    } while (IS_EINTR(ret));

    if (ret < 0) {
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    ret = 0;
    if (FD_ISSET(fd, &read_fds)) {
        ret |= MBEDTLS_NET_POLL_READ;
    }
    if (FD_ISSET(fd, &write_fds)) {
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
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 0);
    if (ret != 0) {
        return ret;
    }

    ret = (int) lwip_read(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        if (errno == EPIPE || errno == ECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }

        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    return ret;
}

/*
 * Read at most 'len' characters, blocking for at most 'timeout' ms
 */
int mbedtls_net_recv_timeout(void *ctx, unsigned char *buf,
                             size_t len, uint32_t timeout)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    struct timeval tv;
    fd_set read_fds;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 1);
    if (ret != 0) {
        return ret;
    }

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    ret = lwip_select(fd + 1, &read_fds, NULL, NULL, timeout == 0 ? NULL : &tv);

    /* Zero fds ready means we timed out */
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }

    if (ret < 0) {
        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    /* This call will not block */
    return mbedtls_net_recv(ctx, buf, len);
}

/*
 * Write at most 'len' characters
 */
int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    int fd = ((mbedtls_net_context *) ctx)->fd;

    ret = check_fd(fd, 0);
    if (ret != 0) {
        return ret;
    }

    ret = (int) write(fd, buf, len);

    if (ret < 0) {
        if (net_would_block(ctx) != 0) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

        if (errno == EPIPE || errno == ECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }

        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    return ret;
}

/*
 * Close the connection
 */
void mbedtls_net_close(mbedtls_net_context *ctx)
{
    if (ctx->fd == -1) {
        return;
    }

    lwip_close(ctx->fd);

    ctx->fd = -1;
}

/*
 * Gracefully close the connection
 */
void mbedtls_net_free(mbedtls_net_context *ctx)
{
    if (ctx->fd == -1) {
        return;
    }

    lwip_shutdown(ctx->fd, 2);
    lwip_close(ctx->fd);

    ctx->fd = -1;
}

#endif /* MBEDTLS_NET_LWIP_C */
