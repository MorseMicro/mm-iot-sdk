/**
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmutils.h"

#include "core/autogen/mmagic_core_data.h"

void mmagic_core_tls_init(struct mmagic_data *core)
{
    struct mmagic_tls_data *data = mmagic_data_get_tls(core);
    memset(&data->config, 0, sizeof(data->config));
}

void mmagic_core_tls_start(struct mmagic_data *core)
{
    MM_UNUSED(core);
}
