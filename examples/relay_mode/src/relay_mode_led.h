/*
 * Copyright 2026 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief Optional LED status indication for the Relay Mode Example Application.
 */

#ifndef RELAY_MODE_LED_H
#define RELAY_MODE_LED_H

#include <stdint.h>

/**
 * Initializes the LED status module and starts its background task.
 */
void relay_led_init(void);

/** Indicates that the app is attempting to connect to Wi-Fi */
void relay_led_set_connecting(void);

/** Indicates that the app has connected to Wi-Fi. */
void relay_led_set_connected(void);

/** Indicates that the app has failed to connect. */
void relay_led_set_failed(void);

/**
 * Indicates the current relay depth by pulsing the green LED @p depth times.
 *
 * @param depth The current relay depth, as reported by @c mmwlan_register_depth_change_cb().
 */
void relay_led_set_depth(uint8_t depth);

#endif /* RELAY_MODE_LED_H */
