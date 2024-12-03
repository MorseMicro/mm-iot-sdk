/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * Contains the configuration parameters for the AWS IoT application.
 */

#pragma once

#include <stdio.h>

/** Maximum length of JSON strings */
#define MAX_JSON_LEN                    512

/**
 * The config store key storing the thing name,
 * if not found it is assumed the device needs provisioning
 */
#define AWS_KEY_THING_NAME              "aws.thingname"

/**
 * The config store key storing the AWS endpoint. This is mandatory.
 */
#define AWS_KEY_ENDPOINT                "aws.endpoint"

/**
 * The config store key storing the AWS endpoint port.
 * If not found the port is assumed to be 443.
 */
#define AWS_KEY_PORT                    "aws.port"

/**
 * The config store key storing the AWS shadow name.
 * If not found a classic shadow is used.
 */
#define AWS_KEY_SHADOW_NAME             "aws.shadowname"

/**
 * The config store key storing the device certificate.
 * If fleet provisioning is used this key is updated with the new certificate.
 * This is mandatory if fleet provisioning is not used.
 */
#define AWS_KEY_DEVICE_CERTIFICATE      "aws.devicecert"

/**
 * The config store key storing the device private key.
 * If fleet provisioning is used this key is updated with the new key.
 * This is mandatory if fleet provisioning is not used.
 */
#define AWS_KEY_DEVICE_KEYS             "aws.devicekeys"

/**
 * The config store key storing the AWS root certificate. This is mandatory.
 */
#define AWS_KEY_ROOT_CA                 "aws.rootca"

/**
 * The config store key storing the provisioning certificate.
 * This key may be the same as @ref AWS_KEY_DEVICE_CERTIFICATE in which case
 * the fleet provisioning certificate is replaced with the device certificate on
 * successful provisioning.
 * This is mandatory if fleet provisioning is used.
 */
#define AWS_KEY_PROVISIONING_CERT       "aws.devicecert"

/**
 * The config store key storing the provisioning private key.
 * This key may be the same as @ref AWS_KEY_DEVICE_KEYS in which case the fleet
 * provisioning key is replaced with the device key on successful provisioning.
 * This is mandatory if fleet provisioning is used.
 */
#define AWS_KEY_PROVISIONING_KEYS       "aws.devicekeys"

/**
 * The config store key storing the fleet provisioning template name.
 * This is mandatory if fleet provisioning is used.
 */
#define AWS_KEY_PROVISIONING_TEMPLATE   "aws.provisioningtemplate"

/**
 * The config store key storing the OTA certificate for validating OTA images.
 * This is mandatory if OTA update is used.
 */
#define AWS_KEY_OTA_CERTIFICATE         "aws.ota_cert"

/** Macro to log error messages */
#if defined(AWS_LOG_LEVEL) && (AWS_LOG_LEVEL >= 1)
    #define LogError(message)    do { printf("Error: "); \
                                      printf message;    \
                                      printf("\n");      \
                                    } while (0)
#else
    #define LogError(message)
#endif

/** Macro to log warning messages */
#if defined(AWS_LOG_LEVEL) && (AWS_LOG_LEVEL >= 2)
    #define LogWarn(message)     do { printf("Warn: ");  \
                                      printf message;    \
                                      printf("\n");      \
                                    } while (0)
#else
    #define LogWarn(message)
#endif

/** Macro to log info messages */
#if defined(AWS_LOG_LEVEL) && (AWS_LOG_LEVEL >= 3)
    #define LogInfo(message)     do { printf("Info: ");  \
                                      printf message;    \
                                      printf("\n");      \
                                    } while (0)
#else
    #define LogInfo(message)
#endif

/** Macro to log debug messages */
#if defined(AWS_LOG_LEVEL) && (AWS_LOG_LEVEL >= 4)
    #define LogDebug(message)    do { printf("Debug: "); \
                                      printf message;    \
                                      printf("\n");      \
                                    } while (0)
#else
    #define LogDebug(message)
#endif
