/*
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023 Morse Micro.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file transport_interface.h
 * Transport interface definitions to send and receive data over the
 * network.
 */
#ifndef TRANSPORT_INTERFACE_H_
#define TRANSPORT_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>

/* mbed TLS includes. */
#if !defined(MBEDTLS_CONFIG_FILE)
#error "You must specify MBEDTLS_CONFIG_FILE"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ssl.h"
#include "mbedtls/threading.h"
#include "mbedtls/x509.h"
#include "mbedtls/net.h"

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/**
 * @transportpage
 * The transport interface definition.
 *
 * @transportsectionoverview
 *
 * The transport interface is a set of APIs that must be implemented using an
 * external transport layer protocol. The transport interface is defined in
 * @ref transport_interface.h. This interface allows protocols like MQTT and
 * HTTP to send and receive data over the transport layer. This
 * interface does not handle connection and disconnection to the server of
 * interest. The connection, disconnection, and other transport settings, like
 * timeout and TLS setup, must be handled in the user application.
 * <br>
 *
 * The functions that must be implemented are:<br>
 * - [Transport Receive](@ref TransportRecv_t)
 * - [Transport Send](@ref TransportSend_t)
 *
 * Each of the functions above take in an opaque context @ref NetworkContext_t.
 * The functions above and the context are also grouped together in the
 * @ref TransportInterface_t structure:<br><br>
 * @snippet this define_transportinterface
 * <br>
 *
 * @transportsectionimplementation
 *
 * The following steps give guidance on implementing the transport interface:
 *
 * -# Implementing @ref NetworkContext_t<br><br>
 * @snippet this define_networkcontext
 * <br>
 * @ref NetworkContext_t is the incomplete type <b>struct NetworkContext</b>.
 * The implemented struct NetworkContext must contain all of the information
 * that is needed to receive and send data with the @ref TransportRecv_t
 * and the @ref TransportSend_t implementations.<br>
 * In the case of TLS over TCP, struct NetworkContext is typically implemented
 * with the TCP socket context and a TLS context.<br><br>
 * <b>Example code:</b>
 * @code{c}
 * struct NetworkContext
 * {
 *     struct MyTCPSocketContext tcpSocketContext;
 *     struct MyTLSContext tlsContext;
 * };
 * @endcode
 * <br>
 * -# Implementing @ref TransportRecv_t<br><br>
 * @snippet this define_transportrecv
 * <br>
 * This function is expected to populate a buffer, with bytes received from the
 * transport, and return the number of bytes placed in the buffer.
 * In the case of TLS over TCP, @ref TransportRecv_t is typically implemented by
 * calling the TLS layer function to receive data. In case of plaintext TCP
 * without TLS, it is typically implemented by calling the TCP layer receive
 * function. @ref TransportRecv_t may be invoked multiple times by the protocol
 * library, if fewer bytes than were requested to receive are returned.
 * <br><br>
 * -# Implementing @ref TransportSend_t<br><br>
 * @snippet this define_transportsend
 * <br>
 * This function is expected to send the bytes, in the given buffer over the
 * transport, and return the number of bytes sent.
 * In the case of TLS over TCP, @ref TransportSend_t is typically implemented by
 * calling the TLS layer function to send data. In case of plaintext TCP
 * without TLS, it is typically implemented by calling the TCP layer send
 * function. @ref TransportSend_t may be invoked multiple times by the protocol
 * library, if fewer bytes than were requested to send are returned.
 * <br><br>
 */

/**
 * @transportstruct
 * @typedef NetworkContext_t
 * The NetworkContext is an incomplete type. An implementation of this
 * interface must define struct NetworkContext for the system requirements.
 * This context is passed into the network interface functions.
 */
/* @[define_networkcontext] */
struct NetworkContext;
typedef struct NetworkContext NetworkContext_t;
/* @[define_networkcontext] */

/**
 * @transportcallback
 * Transport interface for receiving data on the network.
 *
 * @note It is RECOMMENDED that the transport receive implementation
 * does NOT block when requested to read a single byte. A single byte
 * read request can be made by the caller to check whether there is a
 * new frame available on the network for reading.
 * However, the receive implementation MAY block for a timeout period when
 * it is requested to read more than 1 byte. This is because once the caller
 * is aware that a new frame is available to read on the network, then
 * the likelihood of reading more than one byte over the network becomes high.
 *
 * @param[in] pNetworkContext Implementation-defined network context.
 * @param[in] pBuffer Buffer to receive the data into.
 * @param[in] bytesToRecv Number of bytes requested from the network.
 *
 * @return The number of bytes received or a negative value to indicate
 * error.
 *
 * @note If no data is available on the network to read and no error
 * has occurred, zero MUST be the return value. A zero return value
 * SHOULD represent that the read operation can be retried by calling
 * the API function. Zero MUST NOT be returned if a network disconnection
 * has occurred.
 */
/* @[define_transportrecv] */
typedef int32_t ( * TransportRecv_t )( NetworkContext_t * pNetworkContext,
                                       void * pBuffer,
                                       size_t bytesToRecv );
/* @[define_transportrecv] */

/**
 * @transportcallback
 * Transport interface for sending data over the network.
 *
 * @param[in] pNetworkContext Implementation-defined network context.
 * @param[in] pBuffer Buffer containing the bytes to send over the network stack.
 * @param[in] bytesToSend Number of bytes to send over the network.
 *
 * @return The number of bytes sent or a negative value to indicate error.
 *
 * @note If no data is transmitted over the network due to a full TX buffer and
 * no network error has occurred, this MUST return zero as the return value.
 * A zero return value SHOULD represent that the send operation can be retried
 * by calling the API function. Zero MUST NOT be returned if a network disconnection
 * has occurred.
 */
/* @[define_transportsend] */
typedef int32_t ( * TransportSend_t )( NetworkContext_t * pNetworkContext,
                                       const void * pBuffer,
                                       size_t bytesToSend );
/* @[define_transportsend] */

/**
 * Transport vector structure for sending multiple messages.
 */
typedef struct TransportOutVector
{
    /**
     * Base address of data.
     */
    const void * iov_base;

    /**
     * Length of data in buffer.
     */
    size_t iov_len;
} TransportOutVector_t;

/**
 * @transportcallback
 * Transport interface function for "vectored" / scatter-gather based
 * writes. This function is expected to iterate over the list of vectors pIoVec
 * having ioVecCount entries containing portions of one MQTT message at a maximum.
 * If the proper functionality is available, then the data in the list should be
 * copied to the underlying TCP buffer before flushing the buffer. Implementing it
 * in this fashion  will lead to sending of fewer TCP packets for all the values
 * in the list.
 *
 * @note If the proper write functionality is not present for a given device/IP-stack,
 * then there is no strict requirement to implement write. Only the send and recv
 * interfaces must be defined for the application to work properly.
 *
 * @param[in] pNetworkContext Implementation-defined network context.
 * @param[in] pIoVec An array of TransportIoVector_t structs.
 * @param[in] ioVecCount Number of TransportIoVector_t in pIoVec.
 *
 * @return The number of bytes written or a negative value to indicate error.
 *
 * @note If no data is written to the buffer due to the buffer being full this MUST
 * return zero as the return value.
 * A zero return value SHOULD represent that the write operation can be retried
 * by calling the API function. Zero MUST NOT be returned if a network disconnection
 * has occurred.
 */
/* @[define_transportwritev] */
typedef int32_t ( * TransportWritev_t )( NetworkContext_t * pNetworkContext,
                                         TransportOutVector_t * pIoVec,
                                         size_t ioVecCount );
/* @[define_transportwritev] */

/**
 * @transportstruct
 * The transport layer interface.
 */
/* @[define_transportinterface] */
typedef struct TransportInterface
{
    TransportRecv_t recv;               /**< Transport receive function pointer. */
    TransportSend_t send;               /**< Transport send function pointer. */
    TransportWritev_t writev;           /**< Transport writev function pointer. */
    NetworkContext_t * pNetworkContext; /**< Implementation-defined network context. */
} TransportInterface_t;
/* @[define_transportinterface] */

/**
 * Secured connection context.
 */
typedef struct SSLContext
{
    int useTLS;                              /**< If set, use SSL for connection */
    mbedtls_ssl_config config;               /**< SSL connection configuration. */
    mbedtls_ssl_context context;             /**< SSL connection context */
    mbedtls_x509_crt_profile certProfile;    /**< Certificate security profile for this connection. */
    mbedtls_x509_crt rootCa;                 /**< Root CA certificate context. */
    mbedtls_x509_crt clientCert;             /**< Client certificate context. */
    mbedtls_pk_context privKey;              /**< Client private key context. */
    mbedtls_entropy_context entropyContext;  /**< Entropy context for random number generation. */
    mbedtls_ctr_drbg_context ctrDrgbContext; /**< CTR DRBG context for random number generation. */
} SSLContext_t;

/**
 * Definition of the network context for the transport interface
 * implementation that uses mbedTLS and FreeRTOS+TLS sockets.
 */
struct NetworkContext
{
    mbedtls_net_context tcpSocket;
    SSLContext_t sslContext;
    uint32_t receiveTimeoutMs;
    uint32_t sendTimeoutMs;
};

/**
 * Contains the credentials necessary for tls connection setup.
 */
typedef struct NetworkCredentials
{
    /**
     * To use ALPN, set this to a NULL-terminated list of supported
     * protocols in decreasing order of preference.
     *
     * See [this link]
     * (https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/)
     * for more information.
     */
    const char ** pAlpnProtos;

    /**
     * Disable server name indication (SNI) for a TLS session.
     */
    int disableSni;

    const uint8_t * pRootCa;     /**< String representing a trusted server root certificate. */
    size_t rootCaSize;           /**< Size associated with #NetworkCredentials.pRootCa. */
    const uint8_t * pClientCert; /**< String representing the client certificate. */
    size_t clientCertSize;       /**< Size associated with #NetworkCredentials.pClientCert. */
    const uint8_t * pPrivateKey; /**< String representing the client certificate's private key. */
    size_t privateKeySize;       /**< Size associated with #NetworkCredentials.pPrivateKey. */
} NetworkCredentials_t;

/**
 * TLS Connect / Disconnect return status.
 */
typedef enum TransportStatus
{
    TRANSPORT_SUCCESS = 0,           /**< Function successfully completed. */
    TRANSPORT_INVALID_PARAMETER,     /**< At least one parameter was invalid. */
    TRANSPORT_INSUFFICIENT_MEMORY,   /**< Insufficient memory required to establish connection. */
    TRANSPORT_INVALID_CREDENTIALS,   /**< Provided credentials were invalid. */
    TRANSPORT_HANDSHAKE_FAILED,      /**< Performing TLS handshake with server failed. */
    TRANSPORT_AUTHENTICATION_FAILED, /**< Handshake failed as server credentials could not be validated. */
    TRANSPORT_INTERNAL_ERROR,        /**< A call to a system API resulted in an internal error. */
    TRANSPORT_CONNECT_FAILURE        /**< Initial connection to the server failed. */
} TransportStatus_t;

/**
 * Create a TLS connection with FreeRTOS sockets.
 *
 * @param[out] pNetworkContext Pointer to a network context to contain the
 * initialized socket handle.
 * @param[in] pHostName The hostname of the remote endpoint.
 * @param[in] port The destination port.
 * @param[in] pNetworkCredentials Credentials for the TLS connection.
 * @param[in] receiveTimeoutMs Receive socket timeout.
 * @param[in] sendTimeoutMs Send socket timeout.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INSUFFICIENT_MEMORY, #TLS_TRANSPORT_INVALID_CREDENTIALS,
 * #TLS_TRANSPORT_HANDSHAKE_FAILED, #TLS_TRANSPORT_INTERNAL_ERROR, or #TLS_TRANSPORT_CONNECT_FAILURE.
 */
TransportStatus_t transport_connect( NetworkContext_t * pNetworkContext,
                                           const char * pHostName,
                                           uint16_t port,
                                           const NetworkCredentials_t * pNetworkCredentials);

/**
 * Gracefully disconnect an established TLS connection.
 *
 * @param[in] pNetworkContext Network context.
 */
void transport_disconnect( NetworkContext_t * pNetworkContext );

/**
 * Receives data from an established TLS connection.
 *
 * This is the TLS version of the transport interface's
 * #TransportRecv_t function.
 *
 * @param[in] pNetworkContext The Network context.
 * @param[out] pBuffer Buffer to receive bytes into.
 * @param[in] bytesToRecv Number of bytes to receive from the network.
 *
 * @return Number of bytes (> 0) received if successful;
 * 0 if the socket times out without reading any bytes;
 * negative value on error.
 */
int32_t transport_recv( NetworkContext_t * pNetworkContext,
                           void * pBuffer,
                           size_t bytesToRecv );

/**
 * Sends data over an established TLS connection.
 *
 * This is the TLS version of the transport interface's
 * #TransportSend_t function.
 *
 * @param[in] pNetworkContext The network context.
 * @param[in] pBuffer Buffer containing the bytes to send.
 * @param[in] bytesToSend Number of bytes to send from the buffer.
 *
 * @return Number of bytes (> 0) sent on success;
 * 0 if the socket times out without sending any bytes;
 * else a negative value to represent error.
 */
int32_t transport_send( NetworkContext_t * pNetworkContext,
                           const void * pBuffer,
                           size_t bytesToSend );


/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* ifndef TRANSPORT_INTERFACE_H_ */
