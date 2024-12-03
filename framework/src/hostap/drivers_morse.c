/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include "hostap_morse_common.h"

/** WPA Supplicant driver operations descriptor implemented by morselib. */
extern const struct wpa_driver_ops mmwlan_wpas_ops;

const struct wpa_driver_ops *const wpa_drivers[] =
{
    &mmwlan_wpas_ops,
    NULL
};
