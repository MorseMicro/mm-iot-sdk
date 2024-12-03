/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmiperf_freertosplustcp.h"

void iperf_freertosplustcp_session_start_common(struct mmiperf_state *base,
                                                const struct freertos_sockaddr *local_addr,
                                                const struct freertos_sockaddr *remote_addr)
{
    struct mmiperf_report *report = &base->report;

    memset(report, 0, sizeof(*report));
    report->report_type = MMIPERF_INTERRIM_REPORT;

#if ipconfigUSE_IPv4
    if (local_addr->sin_family == FREERTOS_AF_INET)
    {
        (void)FreeRTOS_inet_ntop4(&local_addr->sin_address.ulIP_IPv4,
                                  report->local_addr, sizeof(report->local_addr));
    }
    if (remote_addr->sin_family == FREERTOS_AF_INET)
    {
        (void)FreeRTOS_inet_ntop4(&remote_addr->sin_address.ulIP_IPv4,
                                  report->remote_addr, sizeof(report->remote_addr));
    }
#endif
#if ipconfigUSE_IPv6

    if (local_addr->sin_family == FREERTOS_AF_INET6)
    {
        (void)FreeRTOS_inet_ntop6(&local_addr->sin_address.xIP_IPv6.ucBytes,
                                  report->local_addr, sizeof(report->local_addr));
    }
    if (remote_addr->sin_family == FREERTOS_AF_INET6)
    {
        (void)FreeRTOS_inet_ntop6(&remote_addr->sin_address.xIP_IPv6.ucBytes,
                                  report->remote_addr, sizeof(report->remote_addr));
    }
#endif
    report->local_port = FreeRTOS_ntohs(local_addr->sin_port);
    report->remote_port = FreeRTOS_ntohs(remote_addr->sin_port);
    report->bytes_transferred = 0;
    report->duration_ms = 0;

    base->time_started_ms = mmosal_get_time_ms();
}
