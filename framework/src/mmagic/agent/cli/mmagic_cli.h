/*
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "cli/embedded_cli.h"
#include "core/autogen/mmagic_core_data.h"
#include "mmagic.h"

struct mmagic_cli;

struct mmagic_cli_config_accessor
{
    char name[10];
    void (*get)(struct mmagic_cli *ctx, EmbeddedCli *cli, const char *config_var);
    void (*set)(struct mmagic_cli *ctx, EmbeddedCli *cli, const char *config_var, const char *val);
    void (*commit)(struct mmagic_cli *ctx, EmbeddedCli *cli, const char *config_var);
    struct mmagic_cli_config_accessor *next;
};

/**
 * CLI state data.
 */
struct mmagic_cli
{
    /** Opaque arg to pass to @c tx_cb. */
    void *tx_cb_arg;
    /** Transmit callback. */
    void (*tx_cb)(const char *buf, size_t len, void *cb_arg);

    struct mmagic_data core;

    EmbeddedCli *cli;
    EmbeddedCliConfig *config;
    struct mmagic_cli_config_accessor *config_accessors;
};
