/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <endian.h>

#include "errno.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmipal.h"
#include "FreeRTOS_DNS.h"
#include "FreeRTOS_Sockets.h"
#include "netdb.h"
#include "core_sntp_config.h"
#include "core_sntp_client.h"
#include "backoff_algorithm.h"
#include "mmhal.h"

/* Implementation of SntpDnsResolve_t interface. */
static bool resolveDns(const SntpServerInfo_t *pServerAddr, uint32_t *pIpV4Addr)
{
    bool status = false;
    int32_t dnsStatus = -1;
    struct freertos_addrinfo hints = { 0 };
    struct freertos_addrinfo * pListHead = NULL;

    hints.ai_family = FREERTOS_AF_INET;
    hints.ai_socktype = FREERTOS_SOCK_STREAM;
    hints.ai_protocol = FREERTOS_IPPROTO_TCP;

    dnsStatus = FreeRTOS_getaddrinfo(pServerAddr->pServerName, NULL, &hints, &pListHead);

    if (dnsStatus == 0)
    {
        *pIpV4Addr = be32toh(pListHead->ai_addr->sin_address.ulIP_IPv4);
        status = true;
    }

    freeaddrinfo(pListHead);

    return status;
}

/* Definition of NetworkContext_t for UDP socket operations. */
struct NetworkContext
{
    Socket_t udpSocket;
};

/* Implementation of the UdpTransportSendTo_t function of UDP transport interface. */
static int32_t UdpTransport_Send(NetworkContext_t * pNetworkContext,
                                 uint32_t serverAddr, uint16_t serverPort,
                                 const void *pBuffer, uint16_t bytesToSend)
{
    int32_t bytesSent;
    struct freertos_sockaddr addr = { };
    addr.sin_family = FREERTOS_AF_INET;
    addr.sin_port = htobe16(serverPort);
    addr.sin_address.ulIP_IPv4 = htobe32(serverAddr);
    addr.sin_len = sizeof(addr);

    bytesSent = FreeRTOS_sendto(pNetworkContext->udpSocket,
                                pBuffer, bytesToSend, 0,
                                &addr, sizeof(addr));

    return bytesSent;
}

/* Implementation of the UdpTransportRecvFrom_t function of UDP transport interface. */
static int32_t UdpTransport_Recv(NetworkContext_t * pNetworkContext,
                                 uint32_t serverAddr, uint16_t serverPort,
                                 void *pBuffer, uint16_t bytesToRecv)
{
    int32_t bytesReceived;
    struct freertos_sockaddr addr = { };
    addr.sin_family = FREERTOS_AF_INET;
    addr.sin_port = htobe16(serverPort);
    addr.sin_address.ulIP_IPv4 = htobe32(serverAddr);
    socklen_t addrLen = sizeof(addr);

    bytesReceived = FreeRTOS_recvfrom(pNetworkContext->udpSocket, pBuffer,
                                      bytesToRecv, 0, &addr, &addrLen);

    return bytesReceived;
}

/* Implementation of the SntpSetTime_t interface for POSIX platforms. */
static void sntpClient_SetTime(const SntpServerInfo_t * pTimeServer,
                               const SntpTimestamp_t * pServerTime,
                               int64_t clockOffsetMs,
                               SntpLeapSecondInfo_t leapSecondInfo)
{
    uint32_t unixSecs;
    uint32_t unixMs;
    SntpStatus_t status = Sntp_ConvertToUnixTime(pServerTime, &unixSecs, &unixMs);

    assert(status == SntpSuccess);

    mmhal_set_time(unixSecs);
}

/* Implementation of the SntpGetTime_t interface for POSIX platforms. */
static void sntpClient_GetTime( SntpTimestamp_t * pCurrentTime )
{
    pCurrentTime->seconds = mmhal_get_time();
    pCurrentTime->fractions = 0;
}

/* Sync local time to NTP, see header file for doxygen */
int sntp_sync(char * server_name, int timeout_ms)
{
    /* Memory for network buffer. */
    uint8_t networkBuffer[ SNTP_PACKET_BASE_SIZE ];

    /* Create UDP socket. */
    NetworkContext_t udpContext;
    udpContext.udpSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, 0 );
    if (!xSocketValid(udpContext.udpSocket))
    {
        return SntpErrorNetworkFailure;
    }

    /* Setup list of time servers. */
    SntpServerInfo_t pTimeServers[] =
    {
        {
            .port = SNTP_DEFAULT_SERVER_PORT,
            .pServerName = server_name,
            .serverNameLen = strlen(server_name)
        }
    };

    /* Set the UDP transport interface object. */
    UdpTransportInterface_t udpTransportIntf;
    udpTransportIntf.pUserContext = &udpContext;
    udpTransportIntf.sendTo = UdpTransport_Send;
    udpTransportIntf.recvFrom = UdpTransport_Recv;

    /* Context variable. */
    SntpContext_t context;

    /* Initialize context. */
    SntpStatus_t status = Sntp_Init(&context,
                                    pTimeServers,
                                    sizeof( pTimeServers ) / sizeof( SntpServerInfo_t ),
                                    timeout_ms,
                                    networkBuffer,
                                    SNTP_PACKET_BASE_SIZE,
                                    resolveDns,
                                    sntpClient_GetTime,
                                    sntpClient_SetTime,
                                    &udpTransportIntf,
                                    NULL);

    /* Send NTP request */
    if (status == SntpSuccess)
    {
       status = Sntp_SendTimeRequest(&context, mmhal_random_u32(0, UINT32_MAX), timeout_ms);
    }

    /* Wait for NTP response */
    if (status == SntpSuccess)
    {
        status = Sntp_ReceiveTimeResponse(&context, timeout_ms);
    }

    FreeRTOS_closesocket(udpContext.udpSocket);

    return status;
}

/* Sync local time to NTP with back-off, see header file for doxygen */
int sntp_sync_with_backoff(char * server_name, int timeout_ms, uint32_t min_backoff,
                           uint16_t min_jitter, uint16_t max_jitter, uint32_t max_attempts)
{
    BackoffAlgorithmContext_t backoff_params = { 0 };
    int sntpstatus = 0;
    uint16_t sntp_backoff_jitter_ms;
    BackoffAlgorithm_InitializeParams(&backoff_params,
                                      min_jitter,
                                      max_jitter,
                                      max_attempts);

    /* Try till we succeed */
    while ((sntpstatus = sntp_sync(server_name, timeout_ms)) != 0)
    {
        if (BackoffAlgorithm_GetNextBackoff(&backoff_params, mmhal_random_u32(0, UINT32_MAX),
                                            &sntp_backoff_jitter_ms) != BackoffAlgorithmSuccess)
        {
            break;
        }
        printf("NTP Sync failed, backing off %ld...\n",
               min_backoff + (uint32_t)sntp_backoff_jitter_ms);
        mmosal_task_sleep(min_backoff + (uint32_t)sntp_backoff_jitter_ms);
    }
    return sntpstatus;
}
