/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Fleet provisioning task API
 */

#pragma once

/**
 * This function provisions the device and then resets.
 * It only returns on failure.
 */
void do_fleet_provisioning(void);
