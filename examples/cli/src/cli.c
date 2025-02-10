/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example command line interface (CLI) over UART.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include "mmconfig.h"
#include "mmhal_uart.h"
#include "mmutils.h"

#include "mmagic.h"
#include "mm_app_regdb.h"

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#ifdef MBEDTLS_THREADING_ALT
#include "threading_alt.h"
#endif


#ifndef APPLICATION_VERSION
#error Please define APPLICATION_VERSION to an appropriate value.
#endif

/** Pointer to context for CLI receive callback. */
struct mmagic_cli *mmagic_cli_ctx;

/**
 * Handler for the UART receive callback.
 *
 * @param data      Received data.
 * @param length    Length of received data.
 * @param arg       Opaque argument (unused).
*/
void cli_uart_rx_handler(const uint8_t *data, size_t length, void *arg)
{
    MM_UNUSED(arg);
    if (mmagic_cli_ctx != NULL)
    {
        mmagic_cli_rx(mmagic_cli_ctx, (const char *)data, length);
    }
}


/**
 * Handler for the CLI transmit callback.
 *
 * @param data      Data to transmit.
 * @param length    Length of data to transmit.
 * @param arg       Opaque argument (unused).
*/
void cli_tx_handler(const char *data, size_t length, void *arg)
{
    MM_UNUSED(arg);
    mmhal_uart_tx((const uint8_t *)data, length);
}

/**
 * Handler for the CLI set deep sleep mode callback.
 *
 * @param mode    The deep sleep mode to set.
 * @param arg       Opaque argument (unused).
 *
 * @returns true on success, false on failure.
*/
bool cli_set_deep_sleep_mode_handler(enum mmagic_deep_sleep_mode mode, void *arg)
{
    enum mmhal_uart_deep_sleep_mode mode_uart;

    MM_UNUSED(arg);

    switch (mode)
    {
    case MMAGIC_DEEP_SLEEP_MODE_DISABLED:
        mode_uart = MMHAL_UART_DEEP_SLEEP_DISABLED;
        break;

    case MMAGIC_DEEP_SLEEP_MODE_ONE_SHOT:
        mode_uart = MMHAL_UART_DEEP_SLEEP_ONE_SHOT;
        /* 1ms delay to flush out the LF that follows a CR - or else this will wake us up */
        mmosal_task_sleep(1);
        break;

    default:
        return false;
    }

    return mmhal_uart_set_deep_sleep_mode(mode_uart);
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    char cli_mode[4] = "cli";
    mmconfig_read_string("cli.mode", cli_mode, sizeof(cli_mode));

    mmhal_uart_init(cli_uart_rx_handler, NULL);

    /* Initialize mbedTLS threading (required if MBEDTLS_THREADING_ALT is defined) */
#ifdef MBEDTLS_THREADING_ALT
    mbedtls_platform_threading_init();
#endif

    /* If the cli.mode config variable is set to m2m then use MMAGIC in binary machine-to-machine
     * mode, otherwise use it in interactive CLI mode. */
    if (strcasecmp(cli_mode, "m2m"))
    {
        const struct mmagic_cli_init_args init_args = {
            .app_version = APPLICATION_VERSION,
            .tx_cb = cli_tx_handler,
            .set_deep_sleep_mode_cb = cli_set_deep_sleep_mode_handler,
            .reg_db = get_regulatory_db(),
        };
        mmagic_cli_ctx = mmagic_cli_init(&init_args);
        printf("CLI interface enabled\n");
    }
    else
    {
        const struct mmagic_m2m_agent_init_args init_args = {
            .app_version = APPLICATION_VERSION,
            .reg_db = get_regulatory_db(),
        };
        struct mmagic_m2m_agent *mmagic_m2m_agent = mmagic_m2m_agent_init(&init_args);
        MM_UNUSED(mmagic_m2m_agent);
        printf("M2M interface enabled\n");
    }
}
