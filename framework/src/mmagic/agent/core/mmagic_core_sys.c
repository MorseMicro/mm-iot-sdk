/**
 * Copyright 2023-2026 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "mmosal.h"
#include "mmconfig.h"
#include "mmwlan.h"
#include "mmwlan_stats.h"
#include "mmhal_app.h"
#include "mmhal_os.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_data.h"
#include "core/autogen/mmagic_core_sys.h"
#include "mmagic.h"
#include "mmagic_core_utils.h"

void mmagic_core_sys_init(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

void mmagic_core_sys_start(struct mmagic_data *core)
{
    core->sys_data.is_started = true;
}

/********* MMAGIC Core Sys ops **********
 * NOTE: commands in this module may be invoked when the WLAN module is not started.
 * CHECK with mmagic_core_wlan_is_started() if this may lead to undefined or confusing
 * behavior.
 */

enum mmagic_status mmagic_core_sys_reset(struct mmagic_data *core)
{
    MM_UNUSED(core);

    mmhal_reset();

    /* Note: does not get here */
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_sys_deep_sleep(
    struct mmagic_data *core,
    const struct mmagic_core_sys_deep_sleep_cmd_args *cmd_args)
{
    bool ok;
    enum mmagic_status ret = MMAGIC_STATUS_UNAVAILABLE;

    if (mmagic_core_wlan_is_started(core) && core->set_deep_sleep_mode_cb != NULL)
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
    struct mmagic_data *core,
    struct mmagic_core_sys_get_version_rsp_args *rsp_args)
{
    int ret;
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
    ret = mmconfig_read_string("BOOTLOADER_VERSION",
                               (char *)&rsp_args->results.bootloader_version.data,
                               sizeof(rsp_args->results.bootloader_version.data) - 1);
    if (ret > 0)
    {
        rsp_args->results.bootloader_version.len = ret;
    }
    else
    {
        /* Did not find bootloader version in config store */
        mmosal_safer_strcpy((char *)&rsp_args->results.bootloader_version.data,
                            "N/A",
                            sizeof(rsp_args->results.bootloader_version.data) - 1);
        rsp_args->results.bootloader_version.len =
            MM_MIN(strlen("N/A"), sizeof(rsp_args->results.bootloader_version.data) - 1);
    }

    if (!mmagic_core_wlan_is_started(core))
    {
        /* The transceiver is not started.
         * Report the version information that was retrieved above.
         */
        return MMAGIC_STATUS_OK;
    }

    /* Get Morse versions */
    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        /* There is a problem with the transceiver.
         * Report the version information that was retrieved above.
         */
        return MMAGIC_STATUS_OK;
    }

    /* Copy firmware version */
    mmosal_safer_strcpy((char *)&rsp_args->results.morse_firmware_version.data,
                        version.morse_fw_version,
                        sizeof(rsp_args->results.morse_firmware_version.data) - 1);
    rsp_args->results.morse_firmware_version.len =
        MM_MIN(strlen(version.morse_fw_version),
               sizeof(rsp_args->results.morse_firmware_version.data) - 1);

    /* Copy SDK version */
    mmosal_safer_strcpy((char *)&rsp_args->results.morselib_version.data,
                        version.morselib_version,
                        sizeof(rsp_args->results.morselib_version.data) - 1);
    rsp_args->results.morselib_version.len =
        MM_MIN(strlen(version.morselib_version),
               sizeof(rsp_args->results.morselib_version.data) - 1);

    /* Copy Morse hardware version */
    snprintf((char *)&rsp_args->results.morse_hardware_version.data,
             sizeof(rsp_args->results.morse_hardware_version.data),
             "%lx",
             version.morse_chip_id);
    rsp_args->results.morse_hardware_version.len =
        MM_MIN(strlen((char *)&rsp_args->results.morse_hardware_version.data),
               sizeof(rsp_args->results.morse_hardware_version.data) - 1);

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_sys_get_stats(
    struct mmagic_data *core,
    const struct mmagic_core_sys_get_stats_cmd_args *cmd_args,
    struct mmagic_core_sys_get_stats_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(cmd_args != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    if (!mmagic_core_wlan_is_started(core))
    {
        return MMAGIC_STATUS_UNAVAILABLE;
    }

    if (cmd_args->subsystem == MMAGIC_SUBSYSTEM_ID_UMAC)
    {
        struct mmwlan_stats_umac_data stats;
        enum mmwlan_status status = mmwlan_get_umac_stats(&stats);
        if (status != MMWLAN_SUCCESS)
        {
            return MMAGIC_STATUS_ERROR;
        }
        MMOSAL_ASSERT(sizeof(stats) <= sizeof(rsp_args->buffer.data));
        memcpy(rsp_args->buffer.data, &stats, sizeof(stats));
        rsp_args->buffer.len = sizeof(stats);

        if (cmd_args->reset)
        {
            status = mmwlan_clear_umac_stats();
            if (status != MMWLAN_SUCCESS)
            {
                /* If the reset fails, return an error, even though the stats retrieval succeeded */
                return MMAGIC_STATUS_ERROR;
            }
        }
    }
    else
    {
        struct mmwlan_morse_stats *stats =
            mmwlan_get_morse_stats(cmd_args->subsystem, cmd_args->reset);
        if (stats == NULL)
        {
            return MMAGIC_STATUS_ERROR;
        }
        MMOSAL_ASSERT(stats->len <= sizeof(rsp_args->buffer.data));
        memcpy(rsp_args->buffer.data, stats->buf, stats->len);
        rsp_args->buffer.len = stats->len;
        mmwlan_free_morse_stats(stats);
    }

    return MMAGIC_STATUS_OK;
}
