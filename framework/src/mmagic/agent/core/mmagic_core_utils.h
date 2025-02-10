/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
