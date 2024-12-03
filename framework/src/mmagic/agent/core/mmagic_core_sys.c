/**
 * Copyright 2023-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "mmosal.h"
#include "mmconfig.h"
#include "mmwlan.h"
#include "mmhal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_sys.h"
#include "mmagic.h"

/* This should be included after all the header files */
#include "core/autogen/mmagic_core_sys.def"

void mmagic_core_sys_init(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

void mmagic_core_sys_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

/********* MMAGIC Core Sys ops **********/
enum mmagic_status mmagic_core_sys_reset(struct mmagic_data *core)
{
    MM_UNUSED(core);

    mmhal_reset();

    /* Note: does not get here */
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_sys_deep_sleep(struct mmagic_data *core,
                                              const struct mmagic_core_sys_deep_sleep_cmd_args *
                                              cmd_args)
{
    bool ok;
    enum mmagic_status ret = MMAGIC_STATUS_UNAVAILABLE;

    if (core->set_deep_sleep_mode_cb != NULL)
    {
        ok = core->set_deep_sleep_mode_cb(cmd_args->mode, core->set_deep_sleep_mode_cb_arg);
        if (ok)
        {
            ret = MMAGIC_STATUS_OK;
        }
    }

    return ret;
}

enum mmagic_status mmagic_core_sys_get_version(
    struct mmagic_data *core, struct mmagic_core_sys_get_version_rsp_args *rsp_args)
{
    struct mmwlan_version version;
    enum mmwlan_status status;

    memset(&(rsp_args->results), 0, sizeof(rsp_args->results));

    /* Copy application version */
    mmosal_safer_strcpy((char *)&rsp_args->results.application_version.data,
                        core->app_version,
                        sizeof(rsp_args->results.application_version.data) - 1);
    rsp_args->results.application_version.len =
        MM_MIN(strlen(core->app_version), sizeof(rsp_args->results.application_version.data) - 1);
    /* Get hardware version */
    mmhal_get_hardware_version((char *)&rsp_args->results.user_hardware_version.data,
                               sizeof(rsp_args->results.user_hardware_version.data) - 1);
    rsp_args->results.user_hardware_version.len =
        MM_MIN(strlen((char *)&rsp_args->results.user_hardware_version.data),
               sizeof(rsp_args->results.user_hardware_version.data) - 1);

    /* Get bootloader version from config store */
    rsp_args->results.bootloader_version.len =
        mmconfig_read_string("BOOTLOADER_VERSION",
                             (char *)&rsp_args->results.bootloader_version.data,
                             sizeof(rsp_args->results.bootloader_version.data) - 1);
    if (rsp_args->results.bootloader_version.len <= 0)
    {
        /* Did not find bootloader version in config store */
        mmosal_safer_strcpy((char *)&rsp_args->results.bootloader_version.data, "N/A",
                            sizeof(rsp_args->results.bootloader_version.data) - 1);
        rsp_args->results.bootloader_version.len =
            MM_MIN(strlen("N/A"),
                   sizeof(rsp_args->results.bootloader_version.data) - 1);
    }

    /* Get Morse versions */
    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        return MMAGIC_STATUS_ERROR;
    }

    /* Copy firmware version */
    mmosal_safer_strcpy((char *)&rsp_args->results.morse_firmware_version.data,
                        version.morse_fw_version,
                        sizeof(rsp_args->results.morse_firmware_version.data) - 1);
    rsp_args->results.morse_firmware_version.len =
        MM_MIN(strlen(version.morse_fw_version),
               sizeof(rsp_args->results.morse_firmware_version.data) - 1);

    /* Copy SDK version */
    mmosal_safer_strcpy((char *)&rsp_args->results.morselib_version.data, version.morselib_version,
                        sizeof(rsp_args->results.morselib_version.data) - 1);
    rsp_args->results.morselib_version.len =
        MM_MIN(strlen(version.morselib_version),
               sizeof(rsp_args->results.morselib_version.data) - 1);

    /* Copy Morse hardware version */
    snprintf((char *)&rsp_args->results.morse_hardware_version.data,
             sizeof(rsp_args->results.morse_hardware_version.data),
             "%lx", version.morse_chip_id);
    rsp_args->results.morse_hardware_version.len =
        MM_MIN(strlen((char *)&rsp_args->results.morse_hardware_version.data),
               sizeof(rsp_args->results.morse_hardware_version.data) - 1);

    return MMAGIC_STATUS_OK;
}
