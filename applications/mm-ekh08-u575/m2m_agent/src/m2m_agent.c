/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief M2M Agent example application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * See @ref m2m_controller.c for detailed instructions on how to setup and run this demonstration.
 */

#include "mmosal.h"
#include "mmutils.h"
#include "mmagic.h"

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

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nM2M Agent Example (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize mbedTLS threading (required if MBEDTLS_THREADING_ALT is defined) */
#ifdef MBEDTLS_THREADING_ALT
    mbedtls_platform_threading_init();
#endif

    const struct mmagic_m2m_agent_init_args init_args = {
        .app_version = APPLICATION_VERSION,
    };
    struct mmagic_m2m_agent *m2m_agent = mmagic_m2m_agent_init(&init_args);
    MM_UNUSED(m2m_agent);
    printf("M2M interface enabled\n");
}
