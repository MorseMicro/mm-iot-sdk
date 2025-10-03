/*
 * Copyright 2023-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @file
 * @brief This file includes default implementations of the MQTTAgentTaskConfigHooks_t.
 *        simple light bulb example.
 */

#include "core_mqtt_serializer.h"
#include "transport_interface.h"

/**
 * @def CONFIG_KEY_MQTT_USERNAME
 * @brief Sets the key used to find the config value for the MQTT username.
 *
 * Set this to define the config store key used to find the MQTT username setting.
 * If not defined, the MQTT username will be set to the default user agent string.
 *
 * Example:
 *   #define CONFIG_KEY_MQTT_USERNAME "mqtt.username"
 */

/**
 * @def CONFIG_KEY_MQTT_PASSWORD
 * @brief Sets the key used to find the config value for the MQTT password.
 *
 * Set this to define the config store key used to find the MQTT password setting.
 * If not defined, the MQTT password will not be set.
 *
 * Example:
 *   #define CONFIG_KEY_MQTT_PASSWORD  "mqtt.password"
 */

/**
 * @def CONFIG_KEY_MQTT_KEEPALIVE
 * @brief Sets the key used to find the config value for the MQTT keep alive.
 *
 * Set this to define the config store key used to find the MQTT keep alive setting.
 * If not defined, the MQTT keep alive will default to 60 seconds.
 *
 * NB: If this configuration is provided and is set to 0 or a value that is not a valid int, keep
 * alive will be disabled.
 *
 * Example:
 *   #define CONFIG_KEY_MQTT_USERNAME "mqtt.keepalive"
 */

typedef struct MQTTBrokerEndpoint
{
    const char *pcMqttEndpoint;                     /**< MQTT endpoint to connect to */
    size_t uxMqttEndpointLen;                       /**< Length of above string */
    uint32_t ulMqttPort;                            /**< TCP Port to connect on */
    bool secure;                                    /**< Does broker require TLS */
} MQTTBrokerEndpoint_t;

/**
 * Returns the user agent string for AWS connection
 * @returns A static user agent string
 */
char *mqtt_agent_config_get_default_user_agent_string(void);

/**
 * @brief initialise_ the @c MQTTConnectInfo_t provided.
 *
 * This should set all configuration required, suchsk as username, password, etc.
 */
void mqtt_agent_config_initialise_connect_info(MQTTConnectInfo_t *pxConnectInfo);

/**
 * @brief Validates the @c MQTTConnectInfo_t initialised by the initialiser.
 *
 * @return @c MQTTBadParameter if any configuration is invalid.
 *         @c MQTTSuccess if configuration is valid.
 */
MQTTStatus_t mqtt_agent_config_validate_connect_info(MQTTConnectInfo_t *pxConnectInfo);

/**
 * @brief Clean up and release any resouces created by the initialiser.
 */
void mqtt_agent_config_cleanup_connect_info(MQTTConnectInfo_t *pxConnectInfo);

/**
 * @brief initialise_ the @c MQTTBrokerEndpoint_t provided.
 *
 * This should set the endpoint and port of the broker.
 */
void mqtt_agent_config_initialise_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint);

/**
 * @brief Validates the @c MQTTBrokerEndpoint_t initialised by the initialiser.
 *
 * @return @c MQTTBadParameter if any configuration is invalid.
 *         @c MQTTSuccess if configuration is valid.
 */
MQTTStatus_t mqtt_agent_config_validate_mqtt_broker_endpoint(
    MQTTBrokerEndpoint_t *pxBrokerEndpoint);

/**
 * @brief Clean up and release any resouces created by the initialiser.
 */
void mqtt_agent_config_cleanup_mqtt_broker_endpoint(MQTTBrokerEndpoint_t *pxBrokerEndpoint);

/**
 * @brief initialise_ the @c NetworkCredentials_t provided.
 *
 * This should set the necessary configuration such as RootCa, client certificates etc.
 */
void mqtt_agent_config_initialise_tls_credentials(NetworkCredentials_t *pxCredentials);

/**
 * @brief Validates the @c NetworkCredentials_t initialised by the initialiser.
 *
 * @return @c MQTTBadParameter if any configuration is invalid.
 *         @c MQTTSuccess if configuration is valid.
 */
MQTTStatus_t mqtt_agent_config_validate_tls_credentials(NetworkCredentials_t *pxCredentials);

/**
 * @brief Clean up and release any resouces created by the initialiser.
 */
void mqtt_agent_config_cleanup_tls_credentials(NetworkCredentials_t *pxCredentials);

void mqtt_agent_broker_connection_event(MQTTStatus_t mqtt_status, TransportStatus_t transport_status);
