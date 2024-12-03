/**
 * @file config-ccm-psk-dtls1_2.h
 *
 * @brief Small configuration for TLS 1.2 with PSK and AES-CCM cipher suites
 */
/*
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
/*
 * Minimal configuration for TLS 1.2 with PSK and AES-CCM cipher suites
 *
 * Distinguishing features:
 * - Optimized for small code size, low bandwidth (on an unreliable transport),
 *   and low RAM usage.
 * - Fully modern and secure (provided the pre-shared keys are generated and
 *   stored securely).
 * - Very low record overhead with CCM-8.
 *
 * See README.txt for usage instructions.
 */

#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

#include "mmhal.h"
#include "mmosal.h"

/* System support */
// #define MBEDTLS_HAVE_TIME /* Optionally used in Hello messages */
#ifdef MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#endif
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_DEBUG_C
#define MBEDTLS_PLATFORM_SNPRINTF_MACRO         snprintf
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO           mmosal_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO             mmosal_free

/* Mbed TLS modules */
#define MBEDTLS_AES_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_OID_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PSA_CRYPTO_C
#define MBEDTLS_MD_C
#define MBEDTLS_NET_C
#define MBEDTLS_NET_LWIP_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C

/* The library does not currently support enabling SHA-224 without SHA-256.
 * A future version of the library will have this option disabled
 * by default. */
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_COOKIE_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_TIMING_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CIPHER_C

/* TLS protocol feature support */
#define MBEDTLS_KEY_EXCHANGE_PSK_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH

/*
 * Use only CCM_8 ciphersuites, and
 * save ROM and a few bytes of RAM by specifying our own ciphersuite list
 */
#if 0
#define MBEDTLS_SSL_CIPHERSUITES                        \
        MBEDTLS_TLS_PSK_WITH_AES_256_CCM_8,             \
        MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8
#endif

/*
 * Save RAM at the expense of interoperability: do this only if you control
 * both ends of the connection!  (See comments in "mbedtls/ssl.h".)
 * The optimal size here depends on the typical size of records.
 */
#define MBEDTLS_SSL_IN_CONTENT_LEN              8192
#define MBEDTLS_SSL_OUT_CONTENT_LEN             8192

/* Save RAM at the expense of ROM */
#define MBEDTLS_AES_ROM_TABLES

/* Save some RAM by adjusting to your exact needs */
#define MBEDTLS_PSK_MAX_LEN    16 /* 128-bits keys are generally enough */

/*
 * You should adjust this to the exact number of sources you're using: default
 * is the "platform_entropy_poll" source, but you may want to add other ones
 * Minimum is 2 for the entropy test suite.
 */
#define MBEDTLS_ENTROPY_MAX_SOURCES 2

/* These defines are present so that the config modifying scripts can enable
 * them during tests/scripts/test-ref-configs.pl */
// #define MBEDTLS_USE_PSA_CRYPTO
// #define MBEDTLS_PSA_CRYPTO_C

/* Error messages and TLS debugging traces
 * (huge code size increase, needed for tests/ssl-opt.sh) */
// #define MBEDTLS_DEBUG_C
// #define MBEDTLS_ERROR_C


/* Ensure that mbedTLS features that are required for morselib are enabled. */
#include "mm_mbedtls_config.h"

#endif
