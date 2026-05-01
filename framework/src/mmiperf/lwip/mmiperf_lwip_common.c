/*
 * Copyright 2026 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmiperf.h"
#include "mmlog.h"
#include "../common/mmiperf_private.h"

#include "lwip/tcpip.h"

#if LWIP_TCP && LWIP_CALLBACK_API
#include "mmiperf_tcp_private.h"
#endif
#if LWIP_UDP && LWIP_CALLBACK_API
extern bool iperf_udp_client_stop(mmiperf_handle_t handle);
extern bool iperf_udp_server_stop(mmiperf_handle_t handle);
#endif

bool mmiperf_stop(mmiperf_handle_t handle)
{
    struct mmiperf_state *state = iperf_list_get(handle);
    if (state == NULL)
    {
        MMLOG_WRN("Invalid iperf handle\n");
        return false;
    }

#if LWIP_TCP && LWIP_CALLBACK_API
    if (state->tcp)
    {
        return iperf_tcp_stop(handle);
    }
#endif
#if LWIP_UDP && LWIP_CALLBACK_API
    if (state->server)
    {
        return iperf_udp_server_stop(handle);
    }
    return iperf_udp_client_stop(handle);
#else
    return false;
#endif
}
