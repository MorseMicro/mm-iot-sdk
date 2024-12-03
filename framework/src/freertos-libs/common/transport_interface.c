/*
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023 Morse Micro.
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
 * @file tls_freertos.c
 * @brief TLS transport interface implementations. This implementation uses
 * mbedTLS.
 */

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* TLS transport header. */
#include "transport_interface.h"
#include "netdb.h"

/* Default timeout for transport layer reads */
#define TRANSPORT_TIMEOUT       1000

/**
 * @brief Initialize the mbed TLS structures in a network connection.
 *
 * @param[in] pSslContext The SSL context to initialize.
 */
static void sslContextInit( SSLContext_t * pSslContext );

/**
 * @brief Free the mbed TLS structures in a network connection.
 *
 * @param[in] pSslContext The SSL context to free.
 */
static void sslContextFree( SSLContext_t * pSslContext );

/**
 * @brief Add X509 certificate to the trusted list of root certificates.
 *
 * OpenSSL does not provide a single function for reading and loading certificates
 * from files into stores, so the file API must be called. Start with the
 * root certificate.
 *
 * @param[out] pSslContext SSL context to which the trusted server root CA is to be added.
 * @param[in] pRootCa PEM-encoded string of the trusted server root CA.
 * @param[in] rootCaSize Size of the trusted server root CA.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setRootCa( SSLContext_t * pSslContext,
                          const uint8_t * pRootCa,
                          size_t rootCaSize );

/**
 * @brief Set X509 certificate as client certificate for the server to authenticate.
 *
 * @param[out] pSslContext SSL context to which the client certificate is to be set.
 * @param[in] pClientCert PEM-encoded string of the client certificate.
 * @param[in] clientCertSize Size of the client certificate.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setClientCertificate( SSLContext_t * pSslContext,
                                     const uint8_t * pClientCert,
                                     size_t clientCertSize );

/**
 * @brief Set private key for the client's certificate.
 *
 * @param[out] pSslContext SSL context to which the private key is to be set.
 * @param[in] pPrivateKey PEM-encoded string of the client private key.
 * @param[in] privateKeySize Size of the client private key.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setPrivateKey( SSLContext_t * pSslContext,
                              const uint8_t * pPrivateKey,
                              size_t privateKeySize );

/**
 * @brief Passes TLS credentials to the OpenSSL library.
 *
 * Provides the root CA certificate, client certificate, and private key to the
 * OpenSSL library. If the client certificate or private key is not NULL, mutual
 * authentication is used when performing the TLS handshake.
 *
 * @param[out] pSslContext SSL context to which the credentials are to be imported.
 * @param[in] pNetworkCredentials TLS credentials to be imported.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setCredentials( SSLContext_t * pSslContext,
                               const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Set optional configurations for the TLS connection.
 *
 * This function is used to set SNI and ALPN protocols.
 *
 * @param[in] pSslContext SSL context to which the optional configurations are to be set.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 */
static void setOptionalConfigurations( SSLContext_t * pSslContext,
                                       const char * pHostName,
                                       const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Setup TLS by initializing contexts and setting configurations.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TRANSPORT_SUCCESS, #TRANSPORT_INSUFFICIENT_MEMORY, #TRANSPORT_INVALID_CREDENTIALS,
 * or #TRANSPORT_INTERNAL_ERROR.
 */
static TransportStatus_t tlsSetup( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Perform the TLS handshake on a TCP connection.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TRANSPORT_SUCCESS, #TRANSPORT_HANDSHAKE_FAILED, #TRANSPORT_AUTHENTICATION_FAILED
 *         or #TRANSPORT_INTERNAL_ERROR.
 */
static TransportStatus_t tlsHandshake( NetworkContext_t * pNetworkContext,
                                          const NetworkCredentials_t * pNetworkCredentials );


/*-----------------------------------------------------------*/

static void sslContextInit( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    /* Initialize contexts for random number generation. */
    mbedtls_entropy_init( &( pSslContext->entropyContext ) );
    mbedtls_ctr_drbg_init( &( pSslContext->ctrDrgbContext ) );
    mbedtls_ssl_config_init( &( pSslContext->config ) );
    mbedtls_x509_crt_init( &( pSslContext->rootCa ) );
    mbedtls_pk_init( &( pSslContext->privKey ) );
    mbedtls_x509_crt_init( &( pSslContext->clientCert ) );
    mbedtls_ssl_init( &( pSslContext->context ) );

    /* Seed the random number generator. */
    mbedtls_ctr_drbg_seed(&( pSslContext->ctrDrgbContext ),
                          mbedtls_entropy_func,
                          &( pSslContext->entropyContext ),
                          NULL,
                          0);
}
/*-----------------------------------------------------------*/

static void sslContextFree( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    mbedtls_ssl_free( &( pSslContext->context ) );
    mbedtls_x509_crt_free( &( pSslContext->rootCa ) );
    mbedtls_x509_crt_free( &( pSslContext->clientCert ) );
    mbedtls_pk_free( &( pSslContext->privKey ) );
    mbedtls_entropy_free( &( pSslContext->entropyContext ) );
    mbedtls_ctr_drbg_free( &( pSslContext->ctrDrgbContext ) );
    mbedtls_ssl_config_free( &( pSslContext->config ) );
}
/*-----------------------------------------------------------*/

static int32_t setRootCa( SSLContext_t * pSslContext,
                          const uint8_t * pRootCa,
                          size_t rootCaSize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pRootCa != NULL );

    /* Parse the server root CA certificate into the SSL context. */
    mbedtlsError = mbedtls_x509_crt_parse( &( pSslContext->rootCa ),
                                           pRootCa,
                                           rootCaSize );

    if( mbedtlsError == 0 )
    {
        mbedtls_ssl_conf_ca_chain( &( pSslContext->config ),
                                   &( pSslContext->rootCa ),
                                   NULL );
    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setClientCertificate( SSLContext_t * pSslContext,
                                     const uint8_t * pClientCert,
                                     size_t clientCertSize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pClientCert != NULL );

    /* Setup the client certificate. */
    mbedtlsError = mbedtls_x509_crt_parse( &( pSslContext->clientCert ),
                                           pClientCert,
                                           clientCertSize );

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setPrivateKey( SSLContext_t * pSslContext,
                              const uint8_t * pPrivateKeyPath,
                              size_t privateKeySize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pPrivateKeyPath != NULL );

    /* Setup the client private key. */
    mbedtlsError = mbedtls_pk_parse_key( &( pSslContext->privKey ),
                                         pPrivateKeyPath,
                                         privateKeySize,
                                         NULL,
                                         0, mbedtls_ctr_drbg_random, &(pSslContext->ctrDrgbContext));

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setCredentials( SSLContext_t * pSslContext,
                               const NetworkCredentials_t * pNetworkCredentials )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pNetworkCredentials != NULL );

    /* Set up the certificate security profile, starting from the default value. */
    pSslContext->certProfile = mbedtls_x509_crt_profile_default;

    /* Set SSL authmode and the RNG context. */
    mbedtls_ssl_conf_authmode( &( pSslContext->config ),
                               MBEDTLS_SSL_VERIFY_REQUIRED );
    mbedtls_ssl_conf_rng( &( pSslContext->config ),
                          mbedtls_ctr_drbg_random,
                          &( pSslContext->ctrDrgbContext ) );
    mbedtls_ssl_conf_cert_profile( &( pSslContext->config ),
                                   &( pSslContext->certProfile ) );

    mbedtlsError = setRootCa( pSslContext,
                              pNetworkCredentials->pRootCa,
                              pNetworkCredentials->rootCaSize );

    if( ( pNetworkCredentials->pClientCert != NULL ) &&
        ( pNetworkCredentials->pPrivateKey != NULL ) )
    {
        if( mbedtlsError == 0 )
        {
            mbedtlsError = setClientCertificate( pSslContext,
                                                 pNetworkCredentials->pClientCert,
                                                 pNetworkCredentials->clientCertSize );
        }

        if( mbedtlsError == 0 )
        {
            mbedtlsError = setPrivateKey( pSslContext,
                                          pNetworkCredentials->pPrivateKey,
                                          pNetworkCredentials->privateKeySize );
        }

        if( mbedtlsError == 0 )
        {
            mbedtlsError = mbedtls_ssl_conf_own_cert( &( pSslContext->config ),
                                                      &( pSslContext->clientCert ),
                                                      &( pSslContext->privKey ) );
        }
    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static void setOptionalConfigurations( SSLContext_t * pSslContext,
                                       const char * pHostName,
                                       const NetworkCredentials_t * pNetworkCredentials )
{
    int mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pHostName != NULL );
    configASSERT( pNetworkCredentials != NULL );

    if( pNetworkCredentials->pAlpnProtos != NULL )
    {
        /* Include an application protocol list in the TLS ClientHello
         * message. */
        mbedtlsError = mbedtls_ssl_conf_alpn_protocols( &( pSslContext->config ),
                                                        pNetworkCredentials->pAlpnProtos );
        if (mbedtlsError)
        {
          printf("WRN: (%s:%d) mbedtls_ssl_conf_alpn_protocols returned %d.",
                 __FILE__, __LINE__, mbedtlsError);
        }
    }

    /* Enable SNI if requested. */
    if( pNetworkCredentials->disableSni == pdFALSE )
    {
        mbedtlsError = mbedtls_ssl_set_hostname( &( pSslContext->context ),
                                                 pHostName );
        if (mbedtlsError)
        {
          printf("WRN: (%s:%d) mbedtls_ssl_set_hostname returned %d.",
                 __FILE__, __LINE__, mbedtlsError);
        }
    }

    /* Set Maximum Fragment Length if enabled. */
    #ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH

        /* Enable the max fragment extension. 4096 bytes is currently the largest fragment size permitted.
         * See RFC 8449 https://tools.ietf.org/html/rfc8449 for more information.
         *
         * Smaller values can be found in "mbedtls/include/ssl.h".
         */
        mbedtlsError = mbedtls_ssl_conf_max_frag_len( &( pSslContext->config ), MBEDTLS_SSL_MAX_FRAG_LEN_4096 );
        if (mbedtlsError)
        {
          printf("WRN: (%s:%d) mbedtls_ssl_conf_max_frag_len returned %d.",
                 __FILE__, __LINE__, mbedtlsError);
        }
    #endif /* ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */
}
/*-----------------------------------------------------------*/

static TransportStatus_t tlsSetup( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials )
{
    TransportStatus_t returnStatus = TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;

    configASSERT( pNetworkContext != NULL );
    configASSERT( pHostName != NULL );
    configASSERT( pNetworkCredentials != NULL );
    configASSERT( pNetworkCredentials->pRootCa != NULL );

    /* Initialize the mbed TLS context structures. */
    sslContextInit( &( pNetworkContext->sslContext ) );

    mbedtlsError = mbedtls_ssl_config_defaults( &( pNetworkContext->sslContext.config ),
                                                MBEDTLS_SSL_IS_CLIENT,
                                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                                MBEDTLS_SSL_PRESET_DEFAULT );
    /* Set SSL timeout */
    mbedtls_ssl_conf_read_timeout(&( pNetworkContext->sslContext.config ), TRANSPORT_TIMEOUT);

    if( mbedtlsError != 0 )
    {
        /* Per mbed TLS docs, mbedtls_ssl_config_defaults only fails on memory allocation. */
        returnStatus = TRANSPORT_INSUFFICIENT_MEMORY;
    }

    if( returnStatus == TRANSPORT_SUCCESS )
    {
        mbedtlsError = setCredentials( &( pNetworkContext->sslContext ),
                                       pNetworkCredentials );

        if( mbedtlsError != 0 )
        {
            returnStatus = TRANSPORT_INVALID_CREDENTIALS;
        }
        else
        {
            /* Optionally set SNI and ALPN protocols. */
            setOptionalConfigurations( &( pNetworkContext->sslContext ),
                                       pHostName,
                                       pNetworkCredentials );
        }
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

static TransportStatus_t tlsHandshake( NetworkContext_t * pNetworkContext,
                                          const NetworkCredentials_t * pNetworkCredentials )
{
    TransportStatus_t returnStatus = TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;

    configASSERT( pNetworkContext != NULL );
    configASSERT( pNetworkCredentials != NULL );

    /* Initialize the mbed TLS secured connection context. */
    mbedtlsError = mbedtls_ssl_setup( &( pNetworkContext->sslContext.context ),
                                      &( pNetworkContext->sslContext.config ) );

    if( mbedtlsError != 0 )
    {
        returnStatus = TRANSPORT_INTERNAL_ERROR;
    }
    else
    {
        /* Set the underlying IO for the TLS connection. */

        /* MISRA Rule 11.2 flags the following line for casting the second
         * parameter to void *. This rule is suppressed because
         * #mbedtls_ssl_set_bio requires the second parameter as void *.
         */
        /* coverity[misra_c_2012_rule_11_2_violation] */
        mbedtls_ssl_set_bio(&(pNetworkContext->sslContext.context),
                            &(pNetworkContext->tcpSocket),
                            mbedtls_net_send,
                            NULL,
                            mbedtls_net_recv_timeout);
    }

    if( returnStatus == TRANSPORT_SUCCESS )
    {
        /* Perform the TLS handshake. */
        do
        {
            mbedtlsError = mbedtls_ssl_handshake( &( pNetworkContext->sslContext.context ) );
        } while( ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_READ ) ||
                 ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ) );

        if( mbedtlsError == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED )
        {
            returnStatus = TRANSPORT_AUTHENTICATION_FAILED;
        }
        else if( mbedtlsError != 0 )
        {
            returnStatus = TRANSPORT_HANDSHAKE_FAILED;
        }
        else
        {
            /* Handshake passed, but check authentication passed too */
            mbedtlsError = mbedtls_ssl_get_verify_result(&(pNetworkContext->sslContext.context));
            if (mbedtlsError != 0)
            {
                /* We could have failed for any one of the reasons below:
                 * 1. Either our or their certificates expired or is in the future
                 * 2. Either our or their certificates could not be authenticated by
                 *    the RootCA
                 */
                return TRANSPORT_AUTHENTICATION_FAILED;
            }
        }
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

TransportStatus_t transport_connect( NetworkContext_t * pNetworkContext,
                                           const char * pHostName,
                                           uint16_t port,
                                           const NetworkCredentials_t * pNetworkCredentials)
{
    TransportStatus_t returnStatus = TRANSPORT_SUCCESS;
    int socketStatus = 0;
    char portstr[8];

    if((pNetworkContext == NULL) || (pHostName == NULL))
    {
        returnStatus = TRANSPORT_INVALID_PARAMETER;
    }

    /* Establish a TCP connection with the server. */
    if (returnStatus == TRANSPORT_SUCCESS)
    {
        mbedtls_net_init( &(pNetworkContext->tcpSocket) );

        sprintf(portstr, "%7d", port);
        socketStatus = mbedtls_net_connect(&(pNetworkContext->tcpSocket), pHostName, portstr, MBEDTLS_NET_PROTO_TCP);
        if (socketStatus != 0)
        {
            returnStatus = TRANSPORT_CONNECT_FAILURE;
        }
    }

    if ((pNetworkCredentials != NULL) && (returnStatus == TRANSPORT_SUCCESS))
    {
        /* Connect with TLS */
        pNetworkContext->sslContext.useTLS = 1;

        if (pNetworkCredentials->pRootCa == NULL)
        {
            returnStatus = TRANSPORT_INVALID_PARAMETER;
        }

        /* Initialize TLS contexts and set credentials. */
        if( returnStatus == TRANSPORT_SUCCESS )
        {
            returnStatus = tlsSetup( pNetworkContext, pHostName, pNetworkCredentials );
        }

        /* Perform TLS handshake. */
        if( returnStatus == TRANSPORT_SUCCESS )
        {
            returnStatus = tlsHandshake( pNetworkContext, pNetworkCredentials );
        }

        /* Clean up on failure. */
        if( returnStatus != TRANSPORT_SUCCESS )
        {
            if( pNetworkContext != NULL )
            {
                sslContextFree( &( pNetworkContext->sslContext ) );
            }
        }
    }
    else
    {
        /* Connect without TLS */
        pNetworkContext->sslContext.useTLS = 0;
    }


    /* Close socket on failure. */
    if( returnStatus != TRANSPORT_SUCCESS )
    {
        if( pNetworkContext != NULL )
        {
            if (socketStatus == 0)
            {
                mbedtls_net_free(&(pNetworkContext->tcpSocket));
            }
        }
    }

    return returnStatus;
}
/*-----------------------------------------------------------*/

void transport_disconnect( NetworkContext_t * pNetworkContext )
{
    if( pNetworkContext != NULL )
    {
        if(pNetworkContext->sslContext.useTLS)
        {
            /* Attempting to terminate TLS connection. */
            mbedtls_ssl_close_notify( &( pNetworkContext->sslContext.context ) );
            /* Free mbed TLS contexts. */
            sslContextFree( &( pNetworkContext->sslContext ) );
        }

        /* Call socket shutdown function to close connection - note: does not use mbedtls */
        mbedtls_net_free(&(pNetworkContext->tcpSocket));
    }
}
/*-----------------------------------------------------------*/

int32_t transport_recv( NetworkContext_t * pNetworkContext,
                           void * pBuffer,
                           size_t bytesToRecv )
{
    int32_t readStatus = 0;

    if (pNetworkContext->sslContext.useTLS)
    {
        /* Read with TLS */
        readStatus = ( int32_t ) mbedtls_ssl_read( &( pNetworkContext->sslContext.context ),
                                              pBuffer,
                                              bytesToRecv );
    }
    else
    {
        /* Read in the clear */
        readStatus = (int32_t) mbedtls_net_recv_timeout(&(pNetworkContext->tcpSocket),
                                              pBuffer,
                                              bytesToRecv, TRANSPORT_TIMEOUT);
    }

    /* Timeout implies 0 bytes read */
    if (readStatus == MBEDTLS_ERR_SSL_TIMEOUT)
    {
        readStatus = 0;
    }

    return readStatus;
}
/*-----------------------------------------------------------*/

int32_t transport_send( NetworkContext_t * pNetworkContext,
                           const void * pBuffer,
                           size_t bytesToSend )
{
    int32_t sendStatus = 0;

    if (pNetworkContext->sslContext.useTLS)
    {
        /* Send with TLS */
        sendStatus = ( int32_t ) mbedtls_ssl_write( &( pNetworkContext->sslContext.context ),
                                               pBuffer,
                                               bytesToSend );
    }
    else
    {
        /* Send in the clear */
        sendStatus = ( int32_t ) mbedtls_net_send( &( pNetworkContext->tcpSocket ),
                                               pBuffer,
                                               bytesToSend );
    }

    return sendStatus;
}
