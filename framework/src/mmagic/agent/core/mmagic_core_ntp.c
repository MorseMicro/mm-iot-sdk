/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "core/autogen/mmagic_core_types.h"
#include "mmagic_core_utils.h"
#include "mmhal.h"
#include "mmutils.h"
#include "core/autogen/mmagic_core_data.h"
#include "sntp_client.h"

/** NTP default server */
#define NTP_DEFAULT_SERVER "0.pool.ntp.org"
/** Minimum NTP timeout per attempt */
#define NTP_MIN_TIMEOUT_MS 3000

static const struct mmagic_ntp_config default_config = {
    .server = { sizeof(NTP_DEFAULT_SERVER) - 1, NTP_DEFAULT_SERVER },
};

void mmagic_core_ntp_init(struct mmagic_data *core)
{
    struct mmagic_ntp_data *data = mmagic_data_get_ntp(core);
    memcpy(&data->config, &default_config, sizeof(data->config));
}

void mmagic_core_ntp_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}

enum mmagic_status mmagic_core_ntp_sync(struct mmagic_data *core)
{
    MMOSAL_ASSERT(core != NULL);

    struct mmagic_ntp_config *config = &mmagic_data_get_ntp(core)->config;

    int status = sntp_sync(config->server.data, NTP_MIN_TIMEOUT_MS);
    if (status != 0)
    {
        return mmagic_coresntp_to_mmagic_status(status);
    }

    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_core_ntp_get_time(struct mmagic_data *core,
                                            struct mmagic_core_ntp_get_time_rsp_args *rsp_args)
{
    MMOSAL_ASSERT(core != NULL);
    MMOSAL_ASSERT(rsp_args != NULL);

    rsp_args->timestamp = mmhal_get_time();

    return MMAGIC_STATUS_OK;
}
