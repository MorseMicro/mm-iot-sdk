/*
 * Copyright 2023 Morse Micro.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "mmosal.h"
#include "mmwlan.h"
#include "mmutils.h"
#include "lwip_mmwlan.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "mbedtls/net.h"
#include "core_sntp_config.h"
#include "core_sntp_client.h"
#include "backoff_algorithm.h"
#include "mmhal.h"

/* Implementation of SntpDnsResolve_t interface. */
static bool resolveDns(const SntpServerInfo_t * pServerAddr,
                       uint32_t * pIpV4Addr )
{
    bool status = false;

#if LWIP_IPV4
    int32_t dnsStatus = -1;
    struct addrinfo hints = { 0 };
    struct addrinfo * pListHead = NULL;

    hints.ai_family = AF_INET;

    hints.ai_socktype = ( int32_t ) SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    dnsStatus = getaddrinfo( pServerAddr->pServerName, NULL, &hints, &pListHead );

    if( dnsStatus == 0 )
    {
        struct sockaddr_in * pAddrInfo = ( struct sockaddr_in * ) pListHead->ai_addr;
        *pIpV4Addr = ntohl(pAddrInfo->sin_addr.s_addr);
        status = true;
    }

    freeaddrinfo( pListHead );
#endif

    return status;
}

/* Definition of NetworkContext_t for UDP socket operations. */
struct NetworkContext
{
    int udpSocket;
};

/* Implementation of the UdpTransportSendTo_t function of UDP transport interface. */
static int32_t UdpTransport_Send(NetworkContext_t * pNetworkContext,
                                 uint32_t serverAddr,
                                 uint16_t serverPort,
                                 const void * pBuffer,
                                 uint16_t bytesToSend )
{
    int32_t bytesSent = -1;

#if LWIP_IPV4
    int32_t pollStatus = 1;
    struct pollfd pollFds;
    pollFds.events = POLLOUT | POLLPRI;
    pollFds.revents = 0;
    pollFds.fd = pNetworkContext->udpSocket;

    /* Check if there is data to read from the socket. */
    pollStatus = poll( &pollFds, 1, 0 );

    if (pollStatus > 0)
    {
        struct sockaddr_in addrInfo;
        addrInfo.sin_family = AF_INET;
        addrInfo.sin_port = htons(serverPort);
        addrInfo.sin_addr.s_addr = htonl(serverAddr);

        bytesSent = sendto(pNetworkContext->udpSocket,
                           pBuffer,
                           bytesToSend, 0,
                           (const struct sockaddr *) &addrInfo,
                           sizeof(addrInfo));
    }
    else if (pollStatus == 0)
    {
        bytesSent = 0;
    }
#else
    MM_UNUSED(pNetworkContext);
    MM_UNUSED(serverAddr);
    MM_UNUSED(serverPort);
    MM_UNUSED(pBuffer);
    MM_UNUSED(bytesToSend);
#endif

    return bytesSent;
}

/* Implementation of the UdpTransportRecvFrom_t function of UDP transport interface. */
static int32_t UdpTransport_Recv(NetworkContext_t * pNetworkContext,
                                 uint32_t serverAddr,
                                 uint16_t serverPort,
                                 void * pBuffer,
                                 uint16_t bytesToRecv)
{
    int32_t bytesReceived = -1;

#if LWIP_IPV4
    int32_t pollStatus = 1;
    struct pollfd pollFds;

    pollFds.events = POLLIN | POLLPRI;
    pollFds.revents = 0;
    pollFds.fd = pNetworkContext->udpSocket;

    /* Check if there is data to read from the socket. */
    pollStatus = poll( &pollFds, 1, 0 );

    if (pollStatus > 0)
    {
        struct sockaddr_in addrInfo;
        addrInfo.sin_family = AF_INET;
        addrInfo.sin_port = htons( serverPort );
        addrInfo.sin_addr.s_addr = htonl( serverAddr );
        socklen_t addrLen = sizeof( addrInfo );

        bytesReceived = recvfrom(pNetworkContext->udpSocket, pBuffer,
                                 bytesToRecv, 0,
                                 (struct sockaddr *) &addrInfo,
                                 &addrLen );
    }
    else if (pollStatus == 0)
    {
        bytesReceived = 0;
    }
#else
    MM_UNUSED(pNetworkContext);
    MM_UNUSED(serverAddr);
    MM_UNUSED(serverPort);
    MM_UNUSED(pBuffer);
    MM_UNUSED(bytesToRecv);
#endif

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
    SntpStatus_t status = Sntp_ConvertToUnixTime( pServerTime, &unixSecs, &unixMs );

    assert( status == SntpSuccess );

    mmhal_set_time(unixSecs);
}

/* Implementation of the SntpGetTime_t interface for POSIX platforms. */
static void sntpClient_GetTime(SntpTimestamp_t * pCurrentTime)
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
    udpContext.udpSocket = socket( AF_INET, SOCK_DGRAM, 0 );
    if (udpContext.udpSocket < 0)
    {
        return SntpErrorNetworkFailure;
    }

    /* Setup list of time servers. */
    SntpServerInfo_t pTimeServers[] =
    {
        {
            .port = SNTP_DEFAULT_SERVER_PORT,
            .pServerName = server_name,
            .serverNameLen = strlen( server_name )
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

    close(udpContext.udpSocket);

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
