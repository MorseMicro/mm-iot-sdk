/*
 * Copyright 2024-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "core_sntp_serializer.h"
#include "transport_interface.h"
#include "core_mqtt_serializer.h"
#include "core/autogen/mmagic_core_tls.h"
#include "mmipal.h"
#include "mmwlan.h"
#include "mmagic.h"

/**
 * Converts the given @c mmwlan_status code to an @c mmagic_status code.
 *
 * @param status The @c mmwlan_status code.
 *
 * @returns The @c mmagic_status code.
 */
enum mmagic_status mmagic_mmwlan_status_to_mmagic_status(enum mmwlan_status status);

/**
 * Converts the given @c mmipal_status code to an @c mmagic_status code.
 *
 * @param status The @c mmipal_status code.
 *
 * @returns The @c mmagic_status code.
 */
enum mmagic_status mmagic_mmipal_status_to_mmagic_status(enum mmipal_status status);

/**
 * Converts the given mbedtls status code to an @c mmagic_status code.
 *
 * @param status The mbedtls code.
 *
 * @returns The @c mmagic_status code.
 */
enum mmagic_status mmagic_mbedtls_return_code_to_mmagic_status(int ret);

/**
 * Converts the given @c TransportStatus_t code to an @c mmagic_status code.
 *
 * @param status The mbedtls code.
 *
 * @returns The @c mmagic_status code.
 */
enum mmagic_status mmagic_transport_status_to_mmagic_status(TransportStatus_t status);

/**
 * Converts the given coreSNTP status code to an @c mmagic_status code.
 *
 * @param status The coreSNTP code.
 *
 * @returns The @c mmagic_status code.
 */
enum mmagic_status mmagic_coresntp_to_mmagic_status(int status);

/**
 * Initialises a NetworkCredentials_t structure from TLS data
 */
enum mmagic_status mmagic_init_tls_credentials(NetworkCredentials_t *creds,
                                               struct mmagic_tls_data *tls_data);

enum mmagic_status mmagic_mqtt_status_to_mmagic_status(MQTTStatus_t mqtt_status);
