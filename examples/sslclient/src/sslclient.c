/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SSL Client example to demonstrate connecting to a HTTPS server, TLS handshake and
 *        retrieve data.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * @ref sslclient.c is an example application that demonstrates how to use TLS to connect
 * to a HTTPS server.  In this example we attempt to connect to https://www.google.com/ on
 * the standard HTTPS port 443 and download the content (In this case the Google Search page).
 *
 * @section SSLCLIENT_HOW_IT_WORKS How it works
 *
 * In the function @ref app_init we first initialize the Wi-Fi subsystem and wait for a
 * connection to be established.
 *
 * Once the connection is established we initialize the various TLS modules as follows:
 * @code
 * mbedtls_net_init(&server_fd);
 * mbedtls_ssl_init(&ssl);
 * mbedtls_ssl_config_init(&conf);
 * mbedtls_x509_crt_init(&cacert);
 * mbedtls_x509_crt_init(&clicert);
 * mbedtls_pk_init(&pkey);
 * mbedtls_ctr_drbg_init(&ctr_drbg);
 * mbedtls_entropy_init(&entropy);
 * @endcode
 *
 * Once initialized we generate entropy with a call to:
 * @code
 *  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
 *           (const unsigned char *) pers, strlen(pers));
 * @endcode
 *
 * We then load and parse the CA Root certificate, Client certificate and client keys.
 * These certificates and keys are stored in the file @ref default_certs.h. Test certificates
 * have been used in this example, but you may generate and use your own certificates
 * and keys.
 * @code
 *   printf("Loading the CA root certificate ...");
 *   ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *) mbedtls_test_cas_pem,
 *           mbedtls_test_cas_pem_len);
 *
 *   printf("Loading the client cert...");
 *   ret = mbedtls_x509_crt_parse(&clicert, (const unsigned char *) mbedtls_test_cli_crt,
 *           mbedtls_test_cli_crt_len);
 *
 *   printf("Loading the client key...");
 *   ret = mbedtls_pk_parse_key(&pkey, (const unsigned char *) mbedtls_test_cli_key,
 *           mbedtls_test_cli_key_len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
 * @endcode
 *
 * We now initialize the SSL context:
 * @code
 *   mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
 *           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
 *   mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
 *   mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
 *   mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
 *   mbedtls_ssl_setup(&ssl, &conf);
 *   mbedtls_ssl_set_hostname(&ssl, "test.morsemicro.com")) != 0)
 *   mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);
 * @endcode
 * mbedtls_ssl_set_bio() passes the network read/write functions to mbedtls to use for
 * reading and writing from the network. We use the blocking version of @c recv().
 *
 * Now that the SSL setup is done, we open a TCP/IP connection to the server:
 * @code
 *   mbedtls_net_connect(&server_fd, sslclient_server, sslclient_port, MBEDTLS_NET_PROTO_TCP);
 * @endcode
 * The above call does a DNS lookup and then attempts to connect to the resolved IP's
 * by one till it succeeds.  For this to work we need a DNS server, which is why
 * we need DHCP to be enabled so we can setup the DNS server using DHCP.
 *
 * Once a connection is setup we perform the TLS handshake to start secure communications:
 * @code
 *    ret = mbedtls_ssl_handshake(&ssl);
 * @endcode
 * It is possible to communicate in the clear before initiating a TLS handshake.  This is
 * commonly done in email clients using the @c STARTTLS protocol.
 *
 * Once a handshake is completed, we can verify the server certificates using:
 * @code
 *    ret = mbedtls_ssl_get_verify_result(&ssl);
 * @endcode
 * If the above call fails, it means the authenticity of the server could not be verified.
 * However, TLS communications are still possible.
 *
 * We can now write and read data securely using:
 * @code
 *     ret = mbedtls_ssl_write(&ssl, (const unsigned char *) GET_REQUEST, sizeof(GET_REQUEST) - 1);
 *     ret = mbedtls_ssl_read(&ssl, buf, len);
 * @endcode
 *
 * Once done, we can tear down the connection and free memory using:
 * @code
 *     mbedtls_ssl_close_notify(&ssl);
 *     mbedtls_net_free(&server_fd);
 *     mbedtls_ssl_free(&ssl);
 *     mbedtls_ssl_config_free(&conf);
 *     mbedtls_ctr_drbg_free(&ctr_drbg);
 *     mbedtls_entropy_free(&entropy);
 * @endcode
 *
 * @section SSLCLIENT_CUSTOMIZE Loading custom settings
 *
 * ## Using custom keys and certificates
 * This example application contains embedded certificates and keys for ease of testing.
 * However, you should generate and load your own certificates and keys for real world
 * applications using the steps below.
 *
 * ### Download the Root CA certificate
 * This application needs a root CA certificate that you can download from a certificate
 * authority such as @c Verisign or @c Thawte. Ensure you download the root certificate
 * in @c PEM file format from the certifying authority and also ensure the encryption protocols
 * required by the certificate are enabled in the @c mbedtls_config.h file for this application.
 * Out of the box we support RSA x509 root certificates in @c PEM format.
 *
 * @note Using an unsupported encryption protocol in the certificate and keys are will cause
 * the application to fail while loading the certificates and keys.
 *
 * ### Generate client certificate and keys
 * Use the steps below to generate a set of client keys and a self signed client certificate.
 * - Generate the client keys and certificate using the commands below:
 *   @code
 *   openssl genrsa -out sslclient.key 2048
 *   openssl req -new -x509 -days 365 -key sslclient.key -out sslclient.crt
 *   @endcode
 *   Answer the prompts for information (email, country, organization, etc) as requested.
 *
 * ### Write the keys and certificates to the device
 * - Enter the path for the root certificate you downloaded earlier under the key
 *   @c sslclient.rootca in the @c config.hjson file in this example folder.
 * - Enter the path for the @c sslclient.key file above under the key @c sslclient.clientkeys
 *   in the @c config.hjson file in this example folder.
 * - Enter the path for the @c sslclient.crt file above under the key @c sslclient.clientcert
 *   in the @c config.hjson file in this example folder.
 * - Follow the instructions in @ref MMCONFIG_PROGRAMMING to load the certificates
 *   and keys in the @c config.hjson file to the device.
 *
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 */


#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmipal.h"
#include "mbedtls/build_info.h"
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mm_app_common.h"
#include "default_certs.h"

/** HTTPS port number to connect to */
#define DEFAULT_PORT "443"
/** HTTPS server to connect to */
#define DEFAULT_SERVER "www.google.com"
/** HTTPS get request string */
#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

/** Statically allocated buffer for HTTP GET request, just under 1 packet size */
char buf[1408];

/**
 * Optional mbedtls debug callback handler.
 *
 * @param ctx
 * @param level
 * @param file
 * @param line
 * @param str
 */
void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void) level);
    ((void) ctx);

    printf("%s:%04d: %s", file, line, str);
}


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    uint8_t *allocptr = NULL;

    printf("\n\nMorse SSL Client Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    int ret = 0;
    int len = 0;
    size_t total = 0;
    mbedtls_net_context server_fd;
    const char *pers = "sslclient";

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;

    /*
     * 0. Initialize and setup mbedtls
     */
    printf("Initialising MbedTLS...");
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);

    /* Uncomment the following lines to enable verbose debugging */
    // mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);
    // mbedtls_debug_set_threshold(4);

    mbedtls_x509_crt_init(&cacert);
    mbedtls_x509_crt_init(&clicert);
    mbedtls_pk_init(&pkey);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
            (const unsigned char *) pers, strlen(pers));
    if (ret != 0)
    {
        printf(" failed %d in mbedtls_ctr_drbg_seed()\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    /*
     * 1. Load & setup certificates
     */
    allocptr = (uint8_t*)DEFAULT_ROOT_CERT;
    len =  mmconfig_read_bytes("sslclient.rootca", NULL, 0, 0);
    if (len > 0)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes("sslclient.rootca", allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len++] = 0;
        }
        else
        {
            printf("Failed to allocate memory for root certificate!\n\n");
            goto exit;
        }
    }
    else
    {
        len = sizeof(DEFAULT_ROOT_CERT);
    }
    printf("Loading the CA root certificate ...");
    ret = mbedtls_x509_crt_parse(&cacert, allocptr, len);
    if (ret < 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    allocptr = (uint8_t*)DEFAULT_CLIENT_CERT;
    len =  mmconfig_read_bytes("sslclient.clientcert", NULL, 0, 0);
    if (len > 0)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes("sslclient.clientcert", allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len++] = 0;
        }
        else
        {
            printf("Failed to allocate memory for client certificate!\n\n");
            goto exit;
        }
    }
    else
    {
        len = sizeof(DEFAULT_CLIENT_CERT);
    }
    printf("Loading the client cert...");
    ret = mbedtls_x509_crt_parse(&clicert, allocptr, len);
    if (ret != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    allocptr = (uint8_t*)DEFAULT_CLIENT_KEY;
    len =  mmconfig_read_bytes("sslclient.clientkey", NULL, 0, 0);
    if (len > 0)
    {
        /* Looks like we have a valid certificate */
        allocptr = (uint8_t*) mmosal_malloc(len + 1);
        if (allocptr)
        {
            /* Now read the bytes in */
            mmconfig_read_bytes("sslclient.clientkey", allocptr, len, 0);
            /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
            allocptr[len++] = 0;
        }
        else
        {
            printf("Failed to allocate memory for client key!\n\n");
            goto exit;
        }
    }
    else
    {
        len = sizeof(DEFAULT_CLIENT_KEY);
    }
    printf("Loading the client key...");
    ret = mbedtls_pk_parse_key(&pkey, allocptr, len, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    printf("Setting up client certs/key...");
    if ((ret = mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey)) != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    /*
     * 2. Setup SSL
     */
    printf("Setting up SSL...");
    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0)
    {
        printf(" failed %d in mbedtls_ssl_config_defaults()\n\n", ret);
        goto exit;
    }
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0)
    {
        printf(" failed %d in mbedtls_ssl_setup()\n\n", ret);
        goto exit;
    }

    static char sslclient_server[64];
    strncpy(sslclient_server, DEFAULT_SERVER, sizeof(sslclient_server));
    mmconfig_read_string("sslclient.server", sslclient_server, sizeof(sslclient_server));
    if ((ret = mbedtls_ssl_set_hostname(&ssl, sslclient_server)) != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    /*
     * 3. Start the connection
     */
    /* First parse the URL to extract the server, port and resource */
    static char sslclient_port[8];
    strncpy(sslclient_port, DEFAULT_PORT, sizeof(sslclient_port));
    mmconfig_read_string("sslclient.port", sslclient_port, sizeof(sslclient_port));

    printf("Connecting to %s:%s...", sslclient_server, sslclient_port);
    fflush(stdout);
    ret = mbedtls_net_connect(&server_fd, sslclient_server, sslclient_port, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

    /*
     * 4. Handshake
     */
    printf("Performing the SSL/TLS handshake...");
    ret = mbedtls_ssl_handshake(&ssl);
    if (ret != 0)
    {
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" ok\n");

    /*
     * 5. Verify the server certificate
     */
    printf("Verifying peer X.509 certificate...");
    ret = mbedtls_ssl_get_verify_result(&ssl);
    if (ret != 0)
    {
        /* In real life, we probably want to bail out when ret != 0 */
        printf(" failed %d, did you set the time?\n\n", ret);
    }
    else
    {
        printf(" ok\n");
    }

    /*
     * 6. Write the GET request
     */
    printf("Write to server:");
    ret = mbedtls_ssl_write(&ssl, (const unsigned char *) GET_REQUEST, sizeof(GET_REQUEST) - 1);
    if (ret <= 0)
    {
        // mbedtls_ssl_write failed
        printf(" failed %d\n\n", ret);
        goto exit;
    }
    printf(" %d bytes written\n\n%s", ret, GET_REQUEST);

    /*
     * 7. Read the HTTP response
     */
    printf("Reading response from server:\n");
    memset(buf, 0, sizeof(buf));
    ret = mbedtls_ssl_read(&ssl, (unsigned char*)buf, sizeof(buf) - 1);

    if (ret > 0)
    {
        total = ret;
        printf("Printing headers only:\n\n");

        /* Search for blank line signifying end of headers */
        char* end_headers = strstr(buf, "\n\n");
        if (end_headers)
        {
            /* terminate the string at end of headers */
            *end_headers = 0;
        }
        end_headers = strstr(buf, "\r\n\r\n");
        if (end_headers)
        {
            /* terminate the string at end of headers */
            *end_headers = 0;
        }

        /* Print headers */
        puts(buf);

        /* Read the rest */
        while (1)
        {
            ret = mbedtls_ssl_read(&ssl, (unsigned char*)buf, sizeof(buf));

            if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;

            if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                break;

            if (ret < 0)
            {
                printf(" failed with error code %d\n\n", ret);
                break;
            }

            if (ret == 0)
            {
                /* No more data to read */
                break;
            }

            total += ret;
        }
    }

    if (total > 0)
    {
        printf("\nSuccess! %u bytes read in total.\n", total);
    }
    else
    {
        printf("\nFailed to read response from server!\n");
    }

    /*
     * 8. Close the connection
     */
    mbedtls_ssl_close_notify(&ssl);

exit:
    mbedtls_net_free(&server_fd);

    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
