/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief @ref EMMET example.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * @ref EMMET is a firmware subsystem that allows various aspects of the firmware to be driven by a
 * connected computer via a connected OpenOCD server. This is intended as an aid for development and
 * automated test.
 *
 * This example application simply initializes the Emmet subsystem, making it ready to receive
 * commands.
 *
 * @section EMMET_APP_HOWTO How to use the Emmet example application
 *
 * The following instructions demonstrate how to connect to an AP and perform a ping.
 *
 * Begin by following the instructions in the @ref GETTING_STARTED guide to build the `emmet.elf`
 * firmware and use OpenOCD/GDB to program it to the microcontroller (see also
 * @ref APP_COMMON_API for more details of WLAN and IP stack configuration). Once programming is
 * complete leave OpenOCD running. If this is not the case OpenOCD can be started with the following
 * command (the exact command will vary between platforms, see @ref GETTING_STARTED_OPENOCD):
 *
 * @code
 * openocd -f src/platforms/mm-ekh08-u575/openocd.cfg
 * @endcode
 *
 * In another terminal execute the @c wlan-sta-connect.py script to connect to the AP:
 *
 * @code
 * pipenv run ./tools/ace/examples/wlan-sta-connect.py -H localhost -s MorseMicro -S SAE -p 12345678
 * @endcode
 *
 * This will connect to the AP with SSID `MorseMicro`, using SAE encryption and passphrase
 * `12345678`.
 *
 * @note
 * Every example script has a help flag for more information about the script parameters. For
 * example:
 * @code
 * pipenv run ./tools/ace/examples/wlan-sta-connect.py -h
 * @endcode
 *
 * The script will return once connection is complete. In addition the following message similar to
 * the following will be observed in the firmware log:
 *
 * @code
 * Link is up. Time: 48364 ms, IP: 192.168.1.2, Netmask: 255.255.255.0, Gateway: 192.168.1.1
 * @endcode
 *
 * To ping a device on the network use the @c ping.py script. For example:
 *
 * @code
 * pipenv run ./tools/ace/examples/ping.py -H localhost -I 192.168.1.1 -c 10
 * @endcode
 *
 * This will ping 192.168.1.1, sending 10 ping requests at an interval of 1000 ms.
 *
 * A number of other scripts for exercising various aspects of the firmware can be found in the
 * `tools/ace/examples` directory.
 *
 *
 */

#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mm_app_regdb.h"
#include "emmet.h"
#include "mm_app_loadconfig.h"
#include "mm_app_common.h"

#include "mmipal.h"

/**
 * Link status callback
 *
 * @param link_status   Current link status
 */
static void link_status_callback(const struct mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        printf("Link is up. Time: %lu ms", time_ms);
        printf(", IP: %s", link_status->ip_addr);
        printf(", Netmask: %s", link_status->netmask);
        printf(", Gateway: %s\n", link_status->gateway);
    }
    else
    {
        printf("Link is down. Time: %lu ms\n", time_ms);
    }
}


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    /* Initialize Emmet first to ensure host interface is in a valid state. */
    emmet_init();

    printf("\n\nMorse Emmet Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize MMWLAN interface */
    mmwlan_init();
    mmwlan_set_channel_list(load_channel_list());

    /* Boot the WLAN interface so that we can retrieve the firmware version. */
    struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    (void)mmwlan_boot(&boot_args);
    app_print_version_info();


    /* Load IP stack settings from config store or defaults */
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    load_mmipal_init_args(&mmipal_init_args);

    /* Initialize IP stack. */
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_link_status_callback(link_status_callback);

    /* Configure and start Emmet so that we can begin receiving commands from the host. */
    emmet_set_reg_db(get_regulatory_db());
    emmet_start();
}
