/*
 * Copyright 2021-2024 Morse Micro
 * SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-MorseMicroCommercial
 */

#pragma once

#include "emmet.h"
#include "emmet-api.pb-c.h"
#include "mmlog.h"
#include "mmosal.h"
#include "mmutils.h"


void emmet_lwip_init(void);

void emmet_wlan_init(void);

void emmet_capi_init(void);


void emmet_wlan_start(void);

struct emmet_raw_rsp
{
    uint8_t *rsp_buf;
    size_t buf_size;
    size_t rsp_len;
};

void emmet_pack_rsp(struct emmet_raw_rsp *raw_rsp, const Emmet__DutRsp *dutrsp);

void emmet_pack_basic_rsp(struct emmet_raw_rsp *raw_rsp,
                          Emmet__StatusCode status_code,
                          const char *fmt,
                          ...)
    __attribute__((format(printf, 3, 4)));


void emmet_handle_syscmd(struct emmet_raw_rsp *raw_rsp, Emmet__SysCmd *syscmd);

void emmet_handle_lwipcmd(struct emmet_raw_rsp *raw_rsp, Emmet__LwipCmd *lwipcmd);

void emmet_handle_wlancmd(struct emmet_raw_rsp *raw_rsp, Emmet__WlanCmd *wlancmd);

void emmet_handle_capicmd(struct emmet_raw_rsp *raw_rsp, Emmet__CapiCmd *capicmd);

void emmet_handle_cfgcmd(struct emmet_raw_rsp *raw_rsp, Emmet__CfgCmd *cfgcmd);

void emmet_handle_filecmd(struct emmet_raw_rsp *raw_rsp, Emmet__FileCmd *filecmd);

void emmet_handle_halcmd(struct emmet_raw_rsp *raw_rsp, Emmet__HalCmd *halcmd);


typedef void (
    *emmet_transport_rx_fn_t)(struct emmet_raw_rsp *raw_rsp, const uint8_t *cmd, size_t cmd_len);

void emmet_transport_init(emmet_transport_rx_fn_t rx_handler);

void emmet_transport_start(void);

int emmet_ip6addr_aton(const char *cp, uint32_t *addr);
