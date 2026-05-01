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
#include "mmregdb.h"
#include "mmwlan.h"

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

/** Prints various version information. */
static void app_print_version_info(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version = { 0 };
    struct mmwlan_bcf_metadata bcf_metadata = { 0 };
    char hw_version_string[32] = { 0 };

    bool ok = mmhal_get_hardware_version(hw_version_string, sizeof(hw_version_string));
    if (!ok)
    {
        snprintf(hw_version_string, sizeof(hw_version_string), "%s", "Unknown");
    }

    printf("-----------------------------------\n");

    printf("  HW Version:              %s\n", hw_version_string);
    status = mmwlan_get_bcf_metadata(&bcf_metadata);
    if (status == MMWLAN_SUCCESS)
    {
        printf("  BCF API version:         %u.%u.%u\n",
               bcf_metadata.version.major,
               bcf_metadata.version.minor,
               bcf_metadata.version.patch);
        if (bcf_metadata.build_version[0] != '\0')
        {
            printf("  BCF build version:       %s\n", bcf_metadata.build_version);
        }
        if (bcf_metadata.board_desc[0] != '\0')
        {
            printf("  BCF board description:   %s\n", bcf_metadata.board_desc);
        }
    }
    else
    {
        printf("  !! BCF metadata retrival failed !!\n");
    }

    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        printf("  !! Error occured whilst retrieving version info !!\n");
    }
    printf("  Morselib version:        %s\n", version.morselib_version);
    printf("  Morse firmware version:  %s\n", version.morse_fw_version);
    printf("  Morse chip ID:           0x%04lx\n", version.morse_chip_id);
    printf("  Morse chip name:         %s\n", version.morse_chip_id_string);
    printf("  Application version:     %s\n", APPLICATION_VERSION);
    printf("-----------------------------------\n");

    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
}

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
        .reg_db = get_regulatory_db(),
    };
    struct mmagic_m2m_agent *m2m_agent = mmagic_m2m_agent_init(&init_args);
    MM_UNUSED(m2m_agent);
    printf("M2M interface enabled\n");
    app_print_version_info();
}
