/*
 * Some constants, hardware definitions and comments taken from ST's HAL driver
 * library, COPYRIGHT(c) 2015 STMicroelectronics.
 */

/*
 * FreeRTOS+TCP V4.3.1
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "FreeRTOS_DNS.h"
#include "FreeRTOS_ARP.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#include "phyHandling.h"
#include "FreeRTOS_Routing.h"

#include "stm32fxx_hal_eth.h"

/* ST includes. */
#if defined( STM32F7xx )
    #include "stm32f7xx_hal.h"
    #define CACHE_LINE_SIZE    32u
#elif defined( STM32F4xx )
    #include "stm32f4xx_hal.h"
#elif defined( STM32F2xx )
    #include "stm32f2xx_hal.h"
#elif defined( STM32F1xx )
    #include "stm32f1xx_hal.h"
#elif !defined( _lint ) /* Lint does not like an #error */
    #error What part?
#endif /* if defined( STM32F7xx ) */


/* Interrupt events to process.  Currently only the Rx event is processed
 * although code for other events is included to allow for possible future
 * expansion. */
#define EMAC_IF_RX_EVENT        1UL
#define EMAC_IF_TX_EVENT        2UL
#define EMAC_IF_ERR_EVENT       4UL
#define EMAC_IF_ALL_EVENT       ( EMAC_IF_RX_EVENT | EMAC_IF_TX_EVENT | EMAC_IF_ERR_EVENT )

/* Calculate the maximum packet size that the DMA can receive. */
#define EMAC_DMA_BUFFER_SIZE    ( ( uint32_t ) ( ETH_MAX_PACKET_SIZE - ipBUFFER_PADDING ) )

#define ETH_DMA_ALL_INTS                                                  \
    ( ETH_DMA_IT_TST | ETH_DMA_IT_PMT | ETH_DMA_IT_MMC | ETH_DMA_IT_NIS | \
      ETH_DMA_IT_AIS | ETH_DMA_IT_ER | ETH_DMA_IT_FBE | ETH_DMA_IT_RWT |  \
      ETH_DMA_IT_RPS | ETH_DMA_IT_RBU | ETH_DMA_IT_R | ETH_DMA_IT_TU |    \
      ETH_DMA_IT_RO | ETH_DMA_IT_TJT | ETH_DMA_IT_TPS | ETH_DMA_IT_T )

#ifndef NETWORK_BUFFER_HEADER_SIZE
    #define NETWORK_BUFFER_HEADER_SIZE    ( ipBUFFER_PADDING )
#endif

#ifndef niEMAC_HANDLER_TASK_PRIORITY
    #define niEMAC_HANDLER_TASK_PRIORITY    configMAX_PRIORITIES - 1
#endif

#if ( ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 ) || ( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM == 0 ) )
    #if ( ipconfigPORT_SUPPRESS_WARNING == 0 )
        #warning Consider enabling checksum offloading
    #endif
#endif

#ifndef niDESCRIPTOR_WAIT_TIME_MS
    #define niDESCRIPTOR_WAIT_TIME_MS    250uL
#endif

/*
 * Most users will want a PHY that negotiates about
 * the connection properties: speed, MDIX and duplex.
 * On some rare cases, you want to select what is being
 * advertised, properties like MDIX and duplex.
 */

#if !defined( ipconfigETHERNET_AN_ENABLE )
    /* Enable auto-negotiation */
    #define ipconfigETHERNET_AN_ENABLE    1
#endif

#if !defined( ipconfigETHERNET_AUTO_CROSS_ENABLE )
    #define ipconfigETHERNET_AUTO_CROSS_ENABLE    1
#endif

#if ( ipconfigETHERNET_AN_ENABLE == 0 )

/*
 * The following three defines are only used in case there
 * is no auto-negotiation.
 */
    #if !defined( ipconfigETHERNET_CROSSED_LINK )
        #define ipconfigETHERNET_CROSSED_LINK    1
    #endif

    #if !defined( ipconfigETHERNET_USE_100MB )
        #define ipconfigETHERNET_USE_100MB    1
    #endif

    #if !defined( ipconfigETHERNET_USE_FULL_DUPLEX )
        #define ipconfigETHERNET_USE_FULL_DUPLEX    1
    #endif
#endif /* ipconfigETHERNET_AN_ENABLE == 0 */

/* Default the size of the stack used by the EMAC deferred handler task to twice
 * the size of the stack used by the idle task - but allow this to be overridden in
 * FreeRTOSConfig.h as configMINIMAL_STACK_SIZE is a user definable constant. */
#ifndef configEMAC_TASK_STACK_SIZE
    #define configEMAC_TASK_STACK_SIZE    ( 2 * configMINIMAL_STACK_SIZE )
#endif

/* Two choices must be made: RMII versus MII,
 * and the index of the PHY in use ( between 0 and 31 ). */
#ifndef ipconfigUSE_RMII
    #ifdef STM32F7xx
        #define ipconfigUSE_RMII    1
        #if ( ipconfigPORT_SUPPRESS_WARNING == 0 )
            #warning Using RMII, make sure if this is correct
        #endif
    #else
        #define ipconfigUSE_RMII    0
        #if ( ipconfigPORT_SUPPRESS_WARNING == 0 )
            #warning Using MII, make sure if this is correct
        #endif
    #endif /* STM32F7xx */
#endif /* ipconfigUSE_RMII */

typedef enum
{
    eMACInit,   /* Must initialise MAC. */
    eMACPass,   /* Initialisation was successful. */
    eMACFailed, /* Initialisation failed. */
} eMAC_INIT_STATUS_TYPE;

static eMAC_INIT_STATUS_TYPE xMacInitStatus = eMACInit;

/*-----------------------------------------------------------*/

/*
 * A deferred interrupt handler task that processes
 */
static void prvEMACHandlerTask( void * pvParameters );

/*
 * Force a negotiation with the Switch or Router and wait for LS.
 */
static void prvEthernetUpdateConfig( BaseType_t xForce );

/*
 * See if there is a new packet and forward it to the IP-task.
 */
static BaseType_t prvNetworkInterfaceInput( void );


/*
 * For LLMNR, an extra MAC-address must be configured to
 * be able to receive the multicast messages.
 */
static void prvMACAddressConfig( ETH_HandleTypeDef * heth,
                                 uint32_t ulIndex,
                                 uint8_t * Addr );

/* FreeRTOS+TCP/multi :
 * Each network device has 3 access functions:
 * - Initialise the device
 * - Output a network packet
 * - Return the PHY Link-Status (LS)
 * They can be defined as static because the function addresses
 * will be stored in struct NetworkInterface_t.  The latter will be done in
 * the function pxSTM32Fxx_FillInterfaceDescriptor(). */

static BaseType_t xSTM32F_NetworkInterfaceInitialise( NetworkInterface_t * pxInterface );

static BaseType_t xSTM32F_NetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                                  NetworkBufferDescriptor_t * const pxBuffer,
                                                  BaseType_t bReleaseAfterSend );

static BaseType_t xSTM32F_GetPhyLinkStatus( NetworkInterface_t * pxInterface );

NetworkInterface_t * pxSTM32Fxx_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                         NetworkInterface_t * pxInterface );

/*
 * Check if a given packet should be accepted.
 */
static BaseType_t xMayAcceptPacket( uint8_t * pucEthernetBuffer );

/*
 * Initialise the TX descriptors.
 */
static void prvDMATxDescListInit( void );

/*
 * Initialise the RX descriptors.
 */
static void prvDMARxDescListInit( void );

/* After packets have been sent, the network
 * buffers will be released. */
static void vClearTXBuffers( void );

/*-----------------------------------------------------------*/

#if ( ipconfigUSE_LLMNR == 1 )
static const uint8_t xLLMNR_MACAddress[] = { 0x01, 0x00, 0x5E, 0x00, 0x00, 0xFC };
#endif

#if ( ipconfigUSE_MDNS == 1 )
static const uint8_t xMDNS_MACAddressIPv4[] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb };
#endif

static EthernetPhy_t xPhyObject;

/* Ethernet handle. */
static ETH_HandleTypeDef xETH;

/* xTXDescriptorSemaphore is a counting semaphore with
 * a maximum count of ETH_TXBUFNB, which is the number of
 * DMA TX descriptors. */
static SemaphoreHandle_t xTXDescriptorSemaphore = NULL;

/*
 * Note: it is advised to define both
 *
 *     #define  ipconfigZERO_COPY_RX_DRIVER   1
 *     #define  ipconfigZERO_COPY_TX_DRIVER   1
 *
 * The method using memcpy is slower and probably uses more RAM memory.
 * The possibility is left in the code just for comparison.
 *
 * It is advised to define ETH_TXBUFNB at least 4. Note that no
 * TX buffers are allocated in a zero-copy driver.
 */
/* MAC buffers: ---------------------------------------------------------*/

/* Put the DMA descriptors in '.first_data'.
 * This is important for STM32F7, which has an L1 data cache.
 * The first 64KB of the SRAM is not cached.
 * See README.TXT in this folder. */

/* Ethernet Rx MA Descriptor */
__attribute__( ( aligned( 32 ) ) )
#if defined( STM32F7xx )
    __attribute__( ( section( ".first_data" ) ) )
#endif
ETH_DMADescTypeDef DMARxDscrTab[ ETH_RXBUFNB ];

#if ( ipconfigZERO_COPY_RX_DRIVER == 0 )
    /* Ethernet Receive Buffer */
    __ALIGN_BEGIN uint8_t Rx_Buff[ ETH_RXBUFNB ][ ETH_RX_BUF_SIZE ] __ALIGN_END;
#endif

/* Ethernet Tx DMA Descriptor */
__attribute__( ( aligned( 32 ) ) )
#if defined( STM32F7xx )
    __attribute__( ( section( ".first_data" ) ) )
#endif
ETH_DMADescTypeDef DMATxDscrTab[ ETH_TXBUFNB ];

#if ( ipconfigZERO_COPY_TX_DRIVER == 0 )
    /* Ethernet Transmit Buffer */
    __ALIGN_BEGIN uint8_t Tx_Buff[ ETH_TXBUFNB ][ ETH_TX_BUF_SIZE ] __ALIGN_END;
#endif

/* DMATxDescToClear points to the next TX DMA descriptor
 * that must be cleared by vClearTXBuffers(). */
static __IO ETH_DMADescTypeDef * DMATxDescToClear;

static NetworkInterface_t * pxMyInterface = NULL;

/* Holds the handle of the task used as a deferred interrupt processor.  The
 * handle is used so direct notifications can be sent to the task for all EMAC/DMA
 * related interrupts. */
static TaskHandle_t xEMACTaskHandle = NULL;

/* For local use only: describe the PHY's properties: */
const PhyProperties_t xPHYProperties =
{
    #if ( ipconfigETHERNET_AN_ENABLE != 0 )
        .ucSpeed      = PHY_SPEED_AUTO,
        .ucDuplex     = PHY_DUPLEX_AUTO,
    #else
        #if ( ipconfigETHERNET_USE_100MB != 0 )
            .ucSpeed  = PHY_SPEED_100,
        #else
            .ucSpeed  = PHY_SPEED_10,
        #endif

        #if ( ipconfigETHERNET_USE_FULL_DUPLEX != 0 )
            .ucDuplex = PHY_DUPLEX_FULL,
        #else
            .ucDuplex = PHY_DUPLEX_HALF,
        #endif
    #endif /* if ( ipconfigETHERNET_AN_ENABLE != 0 ) */

    #if ( ipconfigETHERNET_AN_ENABLE != 0 ) && ( ipconfigETHERNET_AUTO_CROSS_ENABLE != 0 )
        .ucMDI_X      = PHY_MDIX_AUTO,
    #elif ( ipconfigETHERNET_CROSSED_LINK != 0 )
        .ucMDI_X      = PHY_MDIX_CROSSED,
    #else
        .ucMDI_X      = PHY_MDIX_DIRECT,
    #endif
};

/*-----------------------------------------------------------*/

BaseType_t xNetworkInterfaceInitialise( NetworkInterface_t * pxInterface )
{
    return xSTM32F_NetworkInterfaceInitialise( pxInterface );
}

BaseType_t xNetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                    NetworkBufferDescriptor_t * const pxBuffer,
                                    BaseType_t bReleaseAfterSend )
{
    return xSTM32F_NetworkInterfaceOutput( pxInterface, pxBuffer, bReleaseAfterSend );
}

#if ( ipconfigIPv4_BACKWARD_COMPATIBLE != 0 )

/* Do not call the following function directly. It is there for downward compatibility.
 * The function FreeRTOS_IPInit() will call it to initialice the interface and end-point
 * objects.  See the description in FreeRTOS_Routing.h. */

    NetworkInterface_t * pxFillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                    NetworkInterface_t * pxInterface )
    {
        return pxSTM32Fxx_FillInterfaceDescriptor( xEMACIndex, pxInterface );
    }

#endif

BaseType_t xGetPhyLinkStatus( NetworkInterface_t * pxInterface )
{
    return xSTM32F_GetPhyLinkStatus( pxInterface );
}

void HAL_ETH_RxCpltCallback( ETH_HandleTypeDef * heth )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ( void ) heth;

    /* Pass an RX-event and wakeup the prvEMACHandlerTask. */
    if( xEMACTaskHandle != NULL )
    {
        xTaskNotifyFromISR( xEMACTaskHandle, EMAC_IF_RX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
/*-----------------------------------------------------------*/

void HAL_ETH_TxCpltCallback( ETH_HandleTypeDef * heth )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ( void ) heth;

    /* Pass a TX-event and wakeup the prvEMACHandlerTask. */
    if( xEMACTaskHandle != NULL )
    {
        xTaskNotifyFromISR( xEMACTaskHandle, EMAC_IF_TX_EVENT, eSetBits, &( xHigherPriorityTaskWoken ) );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
}
/*-----------------------------------------------------------*/

static void vClearTXBuffers()
{
    __IO ETH_DMADescTypeDef * txLastDescriptor = xETH.TxDesc;
    size_t uxCount = ( ( UBaseType_t ) ETH_TXBUFNB ) - uxSemaphoreGetCount( xTXDescriptorSemaphore );

    #if ( ipconfigZERO_COPY_TX_DRIVER != 0 )
        NetworkBufferDescriptor_t * pxNetworkBuffer;
        uint8_t * ucPayLoad;
    #endif

    /* This function is called after a TX-completion interrupt.
     * It will release each Network Buffer used in xNetworkInterfaceOutput().
     * 'uxCount' represents the number of descriptors given to DMA for transmission.
     * After sending a packet, the DMA will clear the 'ETH_DMATXDESC_OWN' bit. */
    while( ( uxCount > 0 ) && ( ( DMATxDescToClear->Status & ETH_DMATXDESC_OWN ) == 0 ) )
    {
        if( ( DMATxDescToClear == txLastDescriptor ) && ( uxCount != ETH_TXBUFNB ) )
        {
            break;
        }

        #if ( ipconfigZERO_COPY_TX_DRIVER != 0 )
        {
            ucPayLoad = ( uint8_t * ) DMATxDescToClear->Buffer1Addr;

            if( ucPayLoad != NULL )
            {
                pxNetworkBuffer = pxPacketBuffer_to_NetworkBuffer( ucPayLoad );

                if( pxNetworkBuffer != NULL )
                {
                    vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
                }

                DMATxDescToClear->Buffer1Addr = ( uint32_t ) 0u;
            }
        }
        #endif /* ipconfigZERO_COPY_TX_DRIVER */

        DMATxDescToClear = ( ETH_DMADescTypeDef * ) ( DMATxDescToClear->Buffer2NextDescAddr );

        uxCount--;
        /* Tell the counting semaphore that one more TX descriptor is available. */
        xSemaphoreGive( xTXDescriptorSemaphore );
    }
}
/*-----------------------------------------------------------*/

BaseType_t xSTM32F_NetworkInterfaceInitialise( NetworkInterface_t * pxInterface )
{
    HAL_StatusTypeDef hal_eth_init_status;
    BaseType_t xResult;
    NetworkEndPoint_t * pxEndPoint;

    BaseType_t xMACEntry = ETH_MAC_ADDRESS1; /* ETH_MAC_ADDRESS0 reserved for the primary MAC-address. */

    if( xMacInitStatus == eMACInit )
    {
        xTXDescriptorSemaphore = xSemaphoreCreateCounting( ( UBaseType_t ) ETH_TXBUFNB, ( UBaseType_t ) ETH_TXBUFNB );

        if( xTXDescriptorSemaphore == NULL )
        {
            xMacInitStatus = eMACFailed;
        }
        else
        {
            /* Initialise ETH */
            pxMyInterface = pxInterface;
            pxEndPoint = FreeRTOS_FirstEndPoint( pxInterface );
            configASSERT( pxEndPoint != NULL );

            xETH.Instance = ETH;
            xETH.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
            xETH.Init.Speed = ETH_SPEED_100M;
            xETH.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
            /* Value of PhyAddress doesn't matter, will be probed for. */
            xETH.Init.PhyAddress = 0;

            xETH.Init.MACAddr = ( uint8_t * ) pxEndPoint->xMACAddress.ucBytes;
            xETH.Init.RxMode = ETH_RXINTERRUPT_MODE;

            #if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 0 )
            {
                /* using the ETH_CHECKSUM_BY_HARDWARE option:
                 * both the IP and the protocol checksums will be calculated
                 * by the peripheral. */
                xETH.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
            }
            #else
            {
                xETH.Init.ChecksumMode = ETH_CHECKSUM_BY_SOFTWARE;
            }
            #endif

            #if ( ipconfigUSE_RMII != 0 )
            {
                xETH.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
            }
            #else
            {
                xETH.Init.MediaInterface = ETH_MEDIA_INTERFACE_MII;
            }
            #endif /* ipconfigUSE_RMII */

            hal_eth_init_status = HAL_ETH_Init( &xETH );

            /* Only for inspection by debugger. */
            ( void ) hal_eth_init_status;

            /* Set the TxDesc and RxDesc pointers. */
            xETH.TxDesc = DMATxDscrTab;
            xETH.RxDesc = DMARxDscrTab;

            /* Make sure that all unused fields are cleared. */
            memset( &DMATxDscrTab, '\0', sizeof( DMATxDscrTab ) );
            memset( &DMARxDscrTab, '\0', sizeof( DMARxDscrTab ) );

            /* Initialize Tx Descriptors list: Chain Mode */
            DMATxDescToClear = DMATxDscrTab;

            /* Initialise TX-descriptors. */
            prvDMATxDescListInit();

            /* Initialise RX-descriptors. */
            prvDMARxDescListInit();

            #if ( ipconfigUSE_MDNS == 1 )
            {
                /* Program the MDNS address. */
                prvMACAddressConfig( &xETH, xMACEntry, ( uint8_t * ) xMDNS_MACAddressIPv4 );
                xMACEntry += 8;
            }
            #endif
            #if ( ( ipconfigUSE_MDNS == 1 ) && ( ipconfigUSE_IPv6 != 0 ) )
            {
                prvMACAddressConfig( &xETH, xMACEntry, ( uint8_t * ) xMDNS_MacAddressIPv6.ucBytes );
                xMACEntry += 8;
            }
            #endif
            #if ( ipconfigUSE_LLMNR == 1 )
            {
                /* Program the LLMNR address. */
                prvMACAddressConfig( &xETH, xMACEntry, ( uint8_t * ) xLLMNR_MACAddress );
                xMACEntry += 8;
            }
            #endif
            #if ( ( ipconfigUSE_LLMNR == 1 ) && ( ipconfigUSE_IPv6 != 0 ) )
            {
                prvMACAddressConfig( &xETH, xMACEntry, ( uint8_t * ) xLLMNR_MacAddressIPv6.ucBytes );
                xMACEntry += 8;
            }
            #endif

            {
                /* The EMAC address of the first end-point has been registered in HAL_ETH_Init(). */
                for( ;
                     pxEndPoint != NULL;
                     pxEndPoint = FreeRTOS_NextEndPoint( pxMyInterface, pxEndPoint ) )
                {
                    #if ( ipconfigUSE_IPv6 != 0 )
                        if( pxEndPoint->bits.bIPv6 != pdFALSE_UNSIGNED )
                        {
                            uint8_t ucMACAddress[ 6 ] = { 0x33, 0x33, 0xff, 0, 0, 0 };

                            ucMACAddress[ 3 ] = pxEndPoint->ipv6_settings.xIPAddress.ucBytes[ 13 ];
                            ucMACAddress[ 4 ] = pxEndPoint->ipv6_settings.xIPAddress.ucBytes[ 14 ];
                            ucMACAddress[ 5 ] = pxEndPoint->ipv6_settings.xIPAddress.ucBytes[ 15 ];
                            prvMACAddressConfig( &xETH, xMACEntry, ucMACAddress );
                            xMACEntry += 8;
                        }
                        else
                    #else /* if ( ipconfigUSE_IPv6 != 0 ) */
                    {
                        if( xETH.Init.MACAddr != ( uint8_t * ) pxEndPoint->xMACAddress.ucBytes )
                        {
                            prvMACAddressConfig( &xETH, xMACEntry, pxEndPoint->xMACAddress.ucBytes );
                            xMACEntry += 8;
                        }
                    }
                    #endif /* if ( ipconfigUSE_IPv6 != 0 ) */
                    if( xMACEntry > ( BaseType_t ) ETH_MAC_ADDRESS3 )
                    {
                        /* No more locations available. */
                        break;
                    }
                }
            }
            #if ( ipconfigUSE_IPv6 != 0 )
            {
                if( xMACEntry <= ( BaseType_t ) ETH_MAC_ADDRESS3 )
                {
                    /* 33:33:00:00:00:01 */
                    uint8_t ucMACAddress[ 6 ] = { 0x33, 0x33, 0, 0, 0, 0x01 };

                    prvMACAddressConfig( &xETH, xMACEntry, ucMACAddress );
                    xMACEntry += 8;
                }
            }
            #endif /* ( ipconfigUSE_IPv6 != 0 ) */

            /* Force a negotiation with the Switch or Router and wait for LS. */
            prvEthernetUpdateConfig( pdTRUE );

            /* The deferred interrupt handler task is created at the highest
             * possible priority to ensure the interrupt handler can return directly
             * to it.  The task's handle is stored in xEMACTaskHandle so interrupts can
             * notify the task when there is something to process. */
            if( xTaskCreate( prvEMACHandlerTask, "EMAC", configEMAC_TASK_STACK_SIZE, NULL, niEMAC_HANDLER_TASK_PRIORITY, &xEMACTaskHandle ) == pdPASS )
            {
                /* The xTXDescriptorSemaphore and the task are created successfully. */
                xMacInitStatus = eMACPass;
            }
            else
            {
                xMacInitStatus = eMACFailed;
            }
        }
    } /* if( xMacInitStatus == eMACInit ) */

    if( xMacInitStatus != eMACPass )
    {
        /* EMAC initialisation failed, return pdFAIL. */
        xResult = pdFAIL;
    }
    else
    {
        if( xPhyObject.ulLinkStatusMask != 0U )
        {
            xETH.Instance->DMAIER |= ETH_DMA_ALL_INTS;
            xResult = pdPASS;
            FreeRTOS_printf( ( "Link Status is high\n" ) );
        }
        else
        {
            /* For now pdFAIL will be returned. But prvEMACHandlerTask() is running
             * and it will keep on checking the PHY and set 'ulLinkStatusMask' when necessary. */
            xResult = pdFAIL;
        }
    }

    /* When returning non-zero, the stack will become active and
     * start DHCP (in configured) */
    return xResult;
}
/*-----------------------------------------------------------*/

static void prvDMATxDescListInit()
{
    ETH_DMADescTypeDef * pxDMADescriptor;
    BaseType_t xIndex;

    /* Get the pointer on the first member of the descriptor list */
    pxDMADescriptor = DMATxDscrTab;

    /* Fill each DMA descriptor with the right values */
    for( xIndex = 0; xIndex < ETH_TXBUFNB; xIndex++, pxDMADescriptor++ )
    {
        /* Set Second Address Chained bit */
        pxDMADescriptor->Status = ETH_DMATXDESC_TCH;

        #if ( ipconfigZERO_COPY_TX_DRIVER == 0 )
        {
            /* Set Buffer1 address pointer */
            pxDMADescriptor->Buffer1Addr = ( uint32_t ) ( Tx_Buff[ xIndex ] );
        }
        #endif

        if( xETH.Init.ChecksumMode == ETH_CHECKSUM_BY_HARDWARE )
        {
            /* Set the DMA Tx descriptors checksum insertion for TCP, UDP, and ICMP */
            pxDMADescriptor->Status |= ETH_DMATXDESC_CHECKSUMTCPUDPICMPFULL;
        }
        else
        {
            pxDMADescriptor->Status &= ~( ( uint32_t ) ETH_DMATXDESC_CHECKSUMTCPUDPICMPFULL );
        }

        /* Initialize the next descriptor with the Next Descriptor Polling Enable */
        if( xIndex < ETH_TXBUFNB - 1 )
        {
            /* Set next descriptor address register with next descriptor base address */
            pxDMADescriptor->Buffer2NextDescAddr = ( uint32_t ) ( pxDMADescriptor + 1 );
        }
        else
        {
            /* For last descriptor, set next descriptor address register equal to the first descriptor base address */
            pxDMADescriptor->Buffer2NextDescAddr = ( uint32_t ) DMATxDscrTab;
        }
    }

    /* Set Transmit Descriptor List Address Register */
    xETH.Instance->DMATDLAR = ( uint32_t ) DMATxDscrTab;
}
/*-----------------------------------------------------------*/

static void prvDMARxDescListInit()
{
    ETH_DMADescTypeDef * pxDMADescriptor;
    BaseType_t xIndex;

    /*
     * RX-descriptors.
     */

    /* Get the pointer on the first member of the descriptor list */
    pxDMADescriptor = DMARxDscrTab;

    /* Fill each DMA descriptor with the right values */
    for( xIndex = 0; xIndex < ETH_RXBUFNB; xIndex++, pxDMADescriptor++ )
    {
        /* Set Buffer1 size and Second Address Chained bit */
        pxDMADescriptor->ControlBufferSize = ETH_DMARXDESC_RCH | EMAC_DMA_BUFFER_SIZE;

        #if ( ipconfigZERO_COPY_RX_DRIVER != 0 )
        {
            /* Set Buffer1 address pointer */
            NetworkBufferDescriptor_t * pxBuffer;

            pxBuffer = pxGetNetworkBufferWithDescriptor( EMAC_DMA_BUFFER_SIZE, 100ul );

            /* If the assert below fails, make sure that there are at least 'ETH_RXBUFNB'
             * Network Buffers available during start-up ( ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ) */
            configASSERT( pxBuffer != NULL );

            if( pxBuffer != NULL )
            {
                pxDMADescriptor->Buffer1Addr = ( uint32_t ) pxBuffer->pucEthernetBuffer;
                pxDMADescriptor->Status = ETH_DMARXDESC_OWN;
            }
        }
        #else /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */
        {
            /* Set Buffer1 address pointer */
            pxDMADescriptor->Buffer1Addr = ( uint32_t ) ( Rx_Buff[ xIndex ] );
            /* Set Own bit of the Rx descriptor Status */
            pxDMADescriptor->Status = ETH_DMARXDESC_OWN;
        }
        #endif /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */

        /* Initialize the next descriptor with the Next Descriptor Polling Enable */
        if( xIndex < ETH_RXBUFNB - 1 )
        {
            /* Set next descriptor address register with next descriptor base address */
            pxDMADescriptor->Buffer2NextDescAddr = ( uint32_t ) ( pxDMADescriptor + 1 );
        }
        else
        {
            /* For last descriptor, set next descriptor address register equal to the first descriptor base address */
            pxDMADescriptor->Buffer2NextDescAddr = ( uint32_t ) DMARxDscrTab;
        }
    }

    /* Set Receive Descriptor List Address Register */
    xETH.Instance->DMARDLAR = ( uint32_t ) DMARxDscrTab;
}
/*-----------------------------------------------------------*/


static void prvMACAddressConfig( ETH_HandleTypeDef * heth,
                                 uint32_t ulIndex,
                                 uint8_t * Addr )
{
    uint32_t ulTempReg;

    ( void ) heth;

    /* Calculate the selected MAC address high register. */
    ulTempReg = 0x80000000ul | ( ( uint32_t ) Addr[ 5 ] << 8 ) | ( uint32_t ) Addr[ 4 ];

    /* Load the selected MAC address high register. */
    ( *( __IO uint32_t * ) ( ( uint32_t ) ( ETH_MAC_ADDR_HBASE + ulIndex ) ) ) = ulTempReg;

    /* Calculate the selected MAC address low register. */
    ulTempReg = ( ( uint32_t ) Addr[ 3 ] << 24 ) | ( ( uint32_t ) Addr[ 2 ] << 16 ) | ( ( uint32_t ) Addr[ 1 ] << 8 ) | Addr[ 0 ];

    /* Load the selected MAC address low register */
    ( *( __IO uint32_t * ) ( ( uint32_t ) ( ETH_MAC_ADDR_LBASE + ulIndex ) ) ) = ulTempReg;
}

/*-----------------------------------------------------------*/

static BaseType_t xSTM32F_NetworkInterfaceOutput( NetworkInterface_t * pxInterface,
                                                  NetworkBufferDescriptor_t * const pxDescriptor,
                                                  BaseType_t bReleaseAfterSend )
{
    BaseType_t xReturn = pdFAIL;
    uint32_t ulTransmitSize = 0;
    __IO ETH_DMADescTypeDef * pxDmaTxDesc;
/* Do not wait too long for a free TX DMA buffer. */
    const TickType_t xBlockTimeTicks = pdMS_TO_TICKS( 50u );

    /* As there is only a single instance of the EMAC, there is only one pxInterface object. */
    ( void ) pxInterface;

    /* Open a do {} while ( 0 ) loop to be able to call break. */
    do
    {
        #if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 0 )
        {
            const IPPacket_t * pxIPPacket;

            pxIPPacket = ( const IPPacket_t * ) pxDescriptor->pucEthernetBuffer;
            #if ( ipconfigUSE_IPv6 != 0 )
                if( pxIPPacket->xEthernetHeader.usFrameType == ipIPv6_FRAME_TYPE )
                {
                    const IPHeader_IPv6_t * pxIPPacket_IPv6;

                    pxIPPacket_IPv6 = ( const IPHeader_IPv6_t * ) &( pxDescriptor->pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER ] );

                    if( pxIPPacket_IPv6->ucNextHeader == ( uint8_t ) ipPROTOCOL_ICMP_IPv6 )
                    {
                        ICMPHeader_IPv6_t * pxICMPHeader_IPv6;

                        pxICMPHeader_IPv6 = ( ICMPHeader_IPv6_t * ) &( pxDescriptor->pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER ] );
                        pxICMPHeader_IPv6->usChecksum = 0U;
                    }
                }
                else
            #endif /* if ( ipconfigUSE_IPv6 != 0 ) */

            if( pxIPPacket->xEthernetHeader.usFrameType == ipIPv4_FRAME_TYPE )
            {
                if( pxIPPacket->xIPHeader.ucProtocol == ( uint8_t ) ipPROTOCOL_ICMP )
                {
                    ICMPHeader_t * pxICMPHeader;

                    pxICMPHeader = ( ICMPHeader_t * ) &( pxDescriptor->pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv4_HEADER ] );
                    pxICMPHeader->usChecksum = ( uint16_t ) 0U;
                }
            }
        }
        #endif /* ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM */

        if( xPhyObject.ulLinkStatusMask != 0 )
        {
            if( xSemaphoreTake( xTXDescriptorSemaphore, xBlockTimeTicks ) != pdPASS )
            {
                /* Time-out waiting for a free TX descriptor. */
                break;
            }

            /* This function does the actual transmission of the packet. The packet is
             * contained in 'pxDescriptor' that is passed to the function. */
            pxDmaTxDesc = xETH.TxDesc;

            /* Is this buffer available? */
            configASSERT( ( pxDmaTxDesc->Status & ETH_DMATXDESC_OWN ) == 0 );

            {
                /* Is this buffer available? */
                /* Get bytes in current buffer. */
                ulTransmitSize = pxDescriptor->xDataLength;

                if( ulTransmitSize > EMAC_DMA_BUFFER_SIZE )
                {
                    ulTransmitSize = EMAC_DMA_BUFFER_SIZE;
                }

                #if ( ipconfigZERO_COPY_TX_DRIVER == 0 )
                {
                    /* Copy the bytes. */
                    memcpy( ( void * ) pxDmaTxDesc->Buffer1Addr, pxDescriptor->pucEthernetBuffer, ulTransmitSize );
                }
                #else
                {
                    configASSERT( bReleaseAfterSend != 0 );

                    /* Move the buffer. */
                    pxDmaTxDesc->Buffer1Addr = ( uint32_t ) pxDescriptor->pucEthernetBuffer;
                    /* The Network Buffer has been passed to DMA, no need to release it. */
                    bReleaseAfterSend = pdFALSE_UNSIGNED;
                }
                #endif /* ipconfigZERO_COPY_TX_DRIVER */

                /* Ask to set the IPv4 checksum.
                 * Also need an Interrupt on Completion so that 'vClearTXBuffers()' will be called.. */
                #if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 0 )
                {
                    pxDmaTxDesc->Status |= ETH_DMATXDESC_CIC_TCPUDPICMP_FULL | ETH_DMATXDESC_IC;
                }
                #else
                {
                    pxDmaTxDesc->Status &= ~( ( uint32_t ) ETH_DMATXDESC_CIC );
                    pxDmaTxDesc->Status |= ETH_DMATXDESC_IC;
                }
                #endif


                /* Prepare transmit descriptors to give to DMA. */

                /* Set LAST and FIRST segment */
                pxDmaTxDesc->Status |= ETH_DMATXDESC_FS | ETH_DMATXDESC_LS;
                /* Set frame size */
                pxDmaTxDesc->ControlBufferSize = ( ulTransmitSize & ETH_DMATXDESC_TBS1 );

                #if ( NETWORK_BUFFERS_CACHED != 0 )
                {
                    BaseType_t xlength = CACHE_LINE_SIZE * ( ( ulTransmitSize + NETWORK_BUFFER_HEADER_SIZE + CACHE_LINE_SIZE - 1 ) / CACHE_LINE_SIZE );
                    uint32_t * pulBuffer = ( uint32_t ) ( pxDescriptor->pucEthernetBuffer - NETWORK_BUFFER_HEADER_SIZE );
                    cache_clean_invalidate_by_addr( pulBuffer, xlength );
                }
                #endif

                /* Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA */
                pxDmaTxDesc->Status |= ETH_DMATXDESC_OWN;

                /* Point to next descriptor */
                xETH.TxDesc = ( ETH_DMADescTypeDef * ) ( xETH.TxDesc->Buffer2NextDescAddr );
                /* Ensure completion of memory access */
                __DSB();
                /* Resume DMA transmission*/
                xETH.Instance->DMATPDR = 0;
                iptraceNETWORK_INTERFACE_TRANSMIT();
                xReturn = pdPASS;
            }
        }
        else
        {
            /* The PHY has no Link Status, packet shall be dropped. */
        }
    } while( 0 );

    /* The buffer has been sent so can be released. */
    if( bReleaseAfterSend != pdFALSE )
    {
        vReleaseNetworkBufferAndDescriptor( pxDescriptor );
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t xMayAcceptPacket( uint8_t * pucEthernetBuffer )
{
    const ProtocolPacket_t * pxProtPacket = ( const ProtocolPacket_t * ) pucEthernetBuffer;

    switch( pxProtPacket->xTCPPacket.xEthernetHeader.usFrameType )
    {
        case ipARP_FRAME_TYPE:
            /* Check it later. */
            return pdTRUE;

            #if ( ipconfigUSE_IPv6 != 0 )
                case ipIPv6_FRAME_TYPE:
                    /* Check it later. */
                    return pdTRUE;
            #endif
        case ipIPv4_FRAME_TYPE:
            /* Check it here. */
            break;

        default:
            /* Refuse the packet. */
            return pdFALSE;
    }

    #if ( ipconfigETHERNET_DRIVER_FILTERS_PACKETS == 1 )
    {
        const IPHeader_t * pxIPHeader = &( pxProtPacket->xTCPPacket.xIPHeader );
        uint32_t ulDestinationIPAddress;

        /* Ensure that the incoming packet is not fragmented (only outgoing packets
         * can be fragmented) as these are the only handled IP frames currently. */
        if( ( pxIPHeader->usFragmentOffset & ipFRAGMENT_OFFSET_BIT_MASK ) != 0U )
        {
            return pdFALSE;
        }

        /* HT: Might want to make the following configurable because
         * most IP messages have a standard length of 20 bytes */

        /* 0x45 means: IPv4 with an IP header of 5 x 4 = 20 bytes
         * 0x47 means: IPv4 with an IP header of 7 x 4 = 28 bytes */
        if( ( pxIPHeader->ucVersionHeaderLength < 0x45 ) || ( pxIPHeader->ucVersionHeaderLength > 0x4F ) )
        {
            return pdFALSE;
        }

        ulDestinationIPAddress = pxIPHeader->ulDestinationIPAddress;

        if( pxIPHeader->ucProtocol == ipPROTOCOL_UDP )
        {
            #if ( ipconfigUSE_LLMNR == 1 ) || ( ipconfigUSE_MDNS == 1 ) || ( ipconfigUSE_NBNS == 1 ) || ( ipconfigUSE_DNS == 1 )
                uint16_t usSourcePort = FreeRTOS_ntohs( pxProtPacket->xUDPPacket.xUDPHeader.usSourcePort );
                uint16_t usDestinationPort = FreeRTOS_ntohs( pxProtPacket->xUDPPacket.xUDPHeader.usDestinationPort );
            #endif

            if( ( xPortHasUDPSocket( pxProtPacket->xUDPPacket.xUDPHeader.usDestinationPort ) == pdFALSE )
                #if ipconfigUSE_LLMNR == 1
                    && ( usDestinationPort != ipLLMNR_PORT ) &&
                    ( usSourcePort != ipLLMNR_PORT )
                #endif
                #if ipconfigUSE_MDNS == 1
                    && ( usDestinationPort != ipMDNS_PORT ) &&
                    ( usSourcePort != ipMDNS_PORT )
                #endif
                #if ipconfigUSE_NBNS == 1
                    && ( usDestinationPort != ipNBNS_PORT ) &&
                    ( usSourcePort != ipNBNS_PORT )
                #endif
                #if ipconfigUSE_DNS == 1
                    && ( usSourcePort != ipDNS_PORT )
                #endif
                )
            {
                /* Drop this packet, not for this device. */
                /* FreeRTOS_printf( ( "Drop: UDP port %d -> %d\n", usSourcePort, usDestinationPort ) ); */
                return pdFALSE;
            }
        }
    }
    #endif /* ipconfigETHERNET_DRIVER_FILTERS_PACKETS */
    return pdTRUE;
}
/*-----------------------------------------------------------*/

static void prvPassEthMessages( NetworkBufferDescriptor_t * pxDescriptor )
{
    IPStackEvent_t xRxEvent;

    xRxEvent.eEventType = eNetworkRxEvent;
    xRxEvent.pvData = ( void * ) pxDescriptor;

    if( xSendEventStructToIPTask( &xRxEvent, ( TickType_t ) 1000 ) != pdPASS )
    {
        /* The buffer could not be sent to the stack so must be released again.
         * This is a deferred handler task, not a real interrupt, so it is ok to
         * use the task level function here. */
        #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
        {
            do
            {
                NetworkBufferDescriptor_t * pxNext = pxDescriptor->pxNextBuffer;
                vReleaseNetworkBufferAndDescriptor( pxDescriptor );
                pxDescriptor = pxNext;
            } while( pxDescriptor != NULL );
        }
        #else
        {
            vReleaseNetworkBufferAndDescriptor( pxDescriptor );
        }
        #endif /* ipconfigUSE_LINKED_RX_MESSAGES */
        iptraceETHERNET_RX_EVENT_LOST();
        FreeRTOS_printf( ( "prvPassEthMessages: Can not queue return packet!\n" ) );
    }
    else
    {
        iptraceNETWORK_INTERFACE_RECEIVE();
    }
}

static BaseType_t prvNetworkInterfaceInput( void )
{
    #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
        NetworkBufferDescriptor_t * pxFirstDescriptor = NULL;
        NetworkBufferDescriptor_t * pxLastDescriptor = NULL;
    #endif
    BaseType_t xReceivedLength = 0;
    __IO ETH_DMADescTypeDef * pxDMARxDescriptor;
    const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS( niDESCRIPTOR_WAIT_TIME_MS );
    uint8_t * pucBuffer;

    pxDMARxDescriptor = xETH.RxDesc;

    while( ( pxDMARxDescriptor->Status & ETH_DMARXDESC_OWN ) == 0u )
    {
        NetworkBufferDescriptor_t * pxCurDescriptor;
        NetworkBufferDescriptor_t * pxNewDescriptor = NULL;
        BaseType_t xAccepted = pdTRUE;

        /* Get the Frame Length of the received packet: subtract 4 bytes of the CRC */
        xReceivedLength = ( ( pxDMARxDescriptor->Status & ETH_DMARXDESC_FL ) >> ETH_DMARXDESC_FRAMELENGTHSHIFT ) - 4;

        pucBuffer = ( uint8_t * ) pxDMARxDescriptor->Buffer1Addr;

        /* Update the ETHERNET DMA global Rx descriptor with next Rx descriptor */
        /* Chained Mode */
        /* Selects the next DMA Rx descriptor list for next buffer to read */
        xETH.RxDesc = ( ETH_DMADescTypeDef * ) pxDMARxDescriptor->Buffer2NextDescAddr;

        /* In order to make the code easier and faster, only packets in a single buffer
         * will be accepted.  This can be done by making the buffers large enough to
         * hold a complete Ethernet packet, minus ipBUFFER_PADDING.
         * Therefore, two sanity checks: */
        configASSERT( xReceivedLength <= EMAC_DMA_BUFFER_SIZE );

        if( ( pxDMARxDescriptor->Status & ( ETH_DMARXDESC_CE | ETH_DMARXDESC_IPV4HCE | ETH_DMARXDESC_FT ) ) != ETH_DMARXDESC_FT )
        {
            /* Not an Ethernet frame-type or a checksum error. */
            xAccepted = pdFALSE;
        }
        else
        {
            /* See if this packet must be handled. */
            xAccepted = xMayAcceptPacket( pucBuffer );
        }

        if( xAccepted != pdFALSE )
        {
            /* The packet will be accepted, but check first if a new Network Buffer can
             * be obtained. If not, the packet will still be dropped. */
            pxNewDescriptor = pxGetNetworkBufferWithDescriptor( EMAC_DMA_BUFFER_SIZE, xDescriptorWaitTime );

            if( pxNewDescriptor == NULL )
            {
                /* A new descriptor can not be allocated now. This packet will be dropped. */
                xAccepted = pdFALSE;
            }
        }

        #if ( ipconfigZERO_COPY_RX_DRIVER != 0 )
        {
            /* Find out which Network Buffer was originally passed to the descriptor. */
            pxCurDescriptor = pxPacketBuffer_to_NetworkBuffer( pucBuffer );
            configASSERT( pxCurDescriptor != NULL );
            configASSERT( pxCurDescriptor->pucEthernetBuffer != NULL );
        }
        #else
        {
            /* In this mode, the two descriptors are the same. */
            pxCurDescriptor = pxNewDescriptor;

            if( pxNewDescriptor != NULL )
            {
                /* The packet is accepted and a new Network Buffer was created,
                 * copy data to the Network Buffer. */
                memcpy( pxNewDescriptor->pucEthernetBuffer, pucBuffer, xReceivedLength );
            }
        }
        #endif /* if ( ipconfigZERO_COPY_RX_DRIVER != 0 ) */

        if( xAccepted != pdFALSE )
        {
            pxCurDescriptor->xDataLength = xReceivedLength;
            pxCurDescriptor->pxInterface = pxMyInterface;
            pxCurDescriptor->pxEndPoint = FreeRTOS_MatchingEndpoint( pxMyInterface, pxCurDescriptor->pucEthernetBuffer );

            #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
            {
                pxCurDescriptor->pxNextBuffer = NULL;

                if( pxFirstDescriptor == NULL )
                {
                    /* Becomes the first message */
                    pxFirstDescriptor = pxCurDescriptor;
                }
                else if( pxLastDescriptor != NULL )
                {
                    /* Add to the tail */
                    pxLastDescriptor->pxNextBuffer = pxCurDescriptor;
                }

                pxLastDescriptor = pxCurDescriptor;
            }
            #else /* if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 ) */
            {
                prvPassEthMessages( pxCurDescriptor );
            }
            #endif /* if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 ) */
        }

        /* Release descriptors to DMA */
        #if ( ipconfigZERO_COPY_RX_DRIVER != 0 )
        {
            /* Set Buffer1 address pointer */
            if( pxNewDescriptor != NULL )
            {
                pxDMARxDescriptor->Buffer1Addr = ( uint32_t ) pxNewDescriptor->pucEthernetBuffer;
            }
            else
            {
                /* The packet was dropped and the same Network
                 * Buffer will be used to receive a new packet. */
            }
        }
        #endif /* ipconfigZERO_COPY_RX_DRIVER */

        /* Set Buffer1 size and Second Address Chained bit */
        pxDMARxDescriptor->ControlBufferSize = ETH_DMARXDESC_RCH | EMAC_DMA_BUFFER_SIZE;
        pxDMARxDescriptor->Status = ETH_DMARXDESC_OWN;

        /* Ensure completion of memory access */
        __DSB();

        /* When Rx Buffer unavailable flag is set clear it and resume
         * reception. */
        if( ( xETH.Instance->DMASR & ETH_DMASR_RBUS ) != 0 )
        {
            /* Clear RBUS ETHERNET DMA flag. */
            xETH.Instance->DMASR = ETH_DMASR_RBUS;

            /* Resume DMA reception. */
            xETH.Instance->DMARPDR = 0;
        }

        pxDMARxDescriptor = xETH.RxDesc;
    }

    #if ( ipconfigUSE_LINKED_RX_MESSAGES != 0 )
    {
        if( pxFirstDescriptor != NULL )
        {
            prvPassEthMessages( pxFirstDescriptor );
        }
    }
    #endif /* ipconfigUSE_LINKED_RX_MESSAGES */

    return( xReceivedLength > 0 );
}
/*-----------------------------------------------------------*/


BaseType_t xSTM32_PhyRead( BaseType_t xAddress,
                           BaseType_t xRegister,
                           uint32_t * pulValue )
{
    uint16_t usPrevAddress = xETH.Init.PhyAddress;
    BaseType_t xResult;
    HAL_StatusTypeDef xHALResult;

    xETH.Init.PhyAddress = xAddress;
    xHALResult = HAL_ETH_ReadPHYRegister( &xETH, ( uint16_t ) xRegister, pulValue );
    xETH.Init.PhyAddress = usPrevAddress;

    if( xHALResult == HAL_OK )
    {
        xResult = 0;
    }
    else
    {
        xResult = -1;
    }

    return xResult;
}
/*-----------------------------------------------------------*/

BaseType_t xSTM32_PhyWrite( BaseType_t xAddress,
                            BaseType_t xRegister,
                            uint32_t ulValue )
{
    uint16_t usPrevAddress = xETH.Init.PhyAddress;
    BaseType_t xResult;
    HAL_StatusTypeDef xHALResult;

    xETH.Init.PhyAddress = xAddress;
    xHALResult = HAL_ETH_WritePHYRegister( &xETH, ( uint16_t ) xRegister, ulValue );
    xETH.Init.PhyAddress = usPrevAddress;

    if( xHALResult == HAL_OK )
    {
        xResult = 0;
    }
    else
    {
        xResult = -1;
    }

    return xResult;
}
/*-----------------------------------------------------------*/

void vMACBProbePhy( void )
{
    vPhyInitialise( &xPhyObject, xSTM32_PhyRead, xSTM32_PhyWrite );
    xPhyDiscover( &xPhyObject );
    xPhyConfigure( &xPhyObject, &xPHYProperties );
}
/*-----------------------------------------------------------*/

static void prvEthernetUpdateConfig( BaseType_t xForce )
{
    FreeRTOS_printf( ( "prvEthernetUpdateConfig: LS mask %02lX Force %d\n",
                       xPhyObject.ulLinkStatusMask,
                       ( int ) xForce ) );

    if( ( xForce != pdFALSE ) || ( xPhyObject.ulLinkStatusMask != 0 ) )
    {
        /* Restart the auto-negotiation. */
        if( xETH.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE )
        {
            xPhyStartAutoNegotiation( &xPhyObject, xPhyGetMask( &xPhyObject ) );

            /* Configure the MAC with the Duplex Mode fixed by the
             * auto-negotiation process. */
            if( xPhyObject.xPhyProperties.ucDuplex == PHY_DUPLEX_FULL )
            {
                xETH.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
            }
            else
            {
                xETH.Init.DuplexMode = ETH_MODE_HALFDUPLEX;
            }

            /* Configure the MAC with the speed fixed by the
             * auto-negotiation process. */
            if( xPhyObject.xPhyProperties.ucSpeed == PHY_SPEED_10 )
            {
                xETH.Init.Speed = ETH_SPEED_10M;
            }
            else
            {
                xETH.Init.Speed = ETH_SPEED_100M;
            }
        }
        else /* AutoNegotiation Disable */
        {
            /* Check parameters */
            assert_param( IS_ETH_SPEED( xETH.Init.Speed ) );
            assert_param( IS_ETH_DUPLEX_MODE( xETH.Init.DuplexMode ) );

            if( xETH.Init.DuplexMode == ETH_MODE_FULLDUPLEX )
            {
                xPhyObject.xPhyPreferences.ucDuplex = PHY_DUPLEX_HALF;
            }
            else
            {
                xPhyObject.xPhyPreferences.ucDuplex = PHY_DUPLEX_FULL;
            }

            if( xETH.Init.Speed == ETH_SPEED_10M )
            {
                xPhyObject.xPhyPreferences.ucSpeed = PHY_SPEED_10;
            }
            else
            {
                xPhyObject.xPhyPreferences.ucSpeed = PHY_SPEED_100;
            }

            xPhyObject.xPhyPreferences.ucMDI_X = PHY_MDIX_AUTO;

            /* Use predefined (fixed) configuration. */
            xPhyFixedValue( &xPhyObject, xPhyGetMask( &xPhyObject ) );
        }

        /* ETHERNET MAC Re-Configuration */
        HAL_ETH_ConfigMAC( &xETH, ( ETH_MACInitTypeDef * ) NULL );

        /* Optionally, pass all multicast */
        #if 0
            xETH.Instance->MACFFR |= ETH_MACFFR_PAM;
        #endif

        /* Restart MAC interface */
        HAL_ETH_Start( &xETH );
    }
    else
    {
        /* Stop MAC interface */
        HAL_ETH_Stop( &xETH );
    }
}
/*-----------------------------------------------------------*/

static BaseType_t xSTM32F_GetPhyLinkStatus( NetworkInterface_t * pxInterface )
{
    BaseType_t xReturn;

    ( void ) pxInterface;

    if( xPhyObject.ulLinkStatusMask != 0 )
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/* Uncomment this in case BufferAllocation_1.c is used. */

void vNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
{
    static
    #if defined( STM32F7xx )
        __attribute__( ( section( ".first_data" ) ) )
    #endif
    uint8_t ucNetworkPackets[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS * ETH_MAX_PACKET_SIZE ] __attribute__( ( aligned( 32 ) ) );
    uint8_t * ucRAMBuffer = ucNetworkPackets;
    uint32_t ul;

    for( ul = 0; ul < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; ul++ )
    {
        pxNetworkBuffers[ ul ].pucEthernetBuffer = ucRAMBuffer + ipBUFFER_PADDING;
        *( ( unsigned * ) ucRAMBuffer ) = ( unsigned ) ( &( pxNetworkBuffers[ ul ] ) );
        ucRAMBuffer += ETH_MAX_PACKET_SIZE;
    }
}

/*-----------------------------------------------------------*/

NetworkInterface_t * pxSTM32Fxx_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                         NetworkInterface_t * pxInterface )
{
    static char pcName[ 17 ];

/* This function pxSTM32Fxx_FillInterfaceDescriptor() adds a network-interface.
 * Make sure that the object pointed to by 'pxInterface'
 * is declared static or global, and that it will remain to exist. */

    snprintf( pcName, sizeof( pcName ), "eth%u", ( unsigned ) xEMACIndex );

    memset( pxInterface, '\0', sizeof( *pxInterface ) );
    pxInterface->pcName = pcName;                    /* Just for logging, debugging. */
    pxInterface->pvArgument = ( void * ) xEMACIndex; /* Has only meaning for the driver functions. */
    pxInterface->pfInitialise = xSTM32F_NetworkInterfaceInitialise;
    pxInterface->pfOutput = xSTM32F_NetworkInterfaceOutput;
    pxInterface->pfGetPhyLinkStatus = xSTM32F_GetPhyLinkStatus;

    FreeRTOS_AddNetworkInterface( pxInterface );

    return pxInterface;
}
/*-----------------------------------------------------------*/

static void prvEMACHandlerTask( void * pvParameters )
{
    UBaseType_t uxCurrentCount;
    BaseType_t xResult;
    const TickType_t ulMaxBlockTime = pdMS_TO_TICKS( 100UL );
    uint32_t ulISREvents = 0U;

    /* Remove compiler warnings about unused parameters. */
    ( void ) pvParameters;

    for( ; ; )
    {
        xResult = 0;

        #if ( ipconfigHAS_PRINTF != 0 )
        {
            /* Call a function that monitors resources: the amount of free network
             * buffers and the amount of free space on the heap.  See FreeRTOS_IP.c
             * for more detailed comments. */
            vPrintResourceStats();
        }
        #endif /* ( ipconfigHAS_PRINTF != 0 ) */

        if( xTXDescriptorSemaphore != NULL )
        {
            static UBaseType_t uxLowestSemCount = ( UBaseType_t ) ETH_TXBUFNB - 1;

            uxCurrentCount = uxSemaphoreGetCount( xTXDescriptorSemaphore );

            if( uxLowestSemCount > uxCurrentCount )
            {
                uxLowestSemCount = uxCurrentCount;
                FreeRTOS_printf( ( "TX DMA buffers: lowest %lu\n", uxLowestSemCount ) );
            }
        }

        /* Wait for a new event or a time-out. */
        xTaskNotifyWait( 0U,                /* ulBitsToClearOnEntry */
                         EMAC_IF_ALL_EVENT, /* ulBitsToClearOnExit */
                         &( ulISREvents ),  /* pulNotificationValue */
                         ulMaxBlockTime );

        if( ( ulISREvents & EMAC_IF_RX_EVENT ) != 0 )
        {
            xResult = prvNetworkInterfaceInput();
        }

        if( ( ulISREvents & EMAC_IF_TX_EVENT ) != 0 )
        {
            /* Code to release TX buffers in case zero-copy is used. */
            /* Check if DMA packets have been delivered. */
            vClearTXBuffers();
        }

        if( ( ulISREvents & EMAC_IF_ERR_EVENT ) != 0 )
        {
            /* Future extension: logging about errors that occurred. */
        }

        if( xPhyCheckLinkStatus( &xPhyObject, xResult ) != 0 )
        {
            /* Something has changed to a Link Status, need re-check. */
            prvEthernetUpdateConfig( pdFALSE );

            #if ( ipconfigSUPPORT_NETWORK_DOWN_EVENT != 0 )
            {
                if( xGetPhyLinkStatus( pxMyInterface ) == pdFALSE )
                {
                    FreeRTOS_NetworkDown( pxMyInterface );
                }
            }
            #endif /* ( ipconfigSUPPORT_NETWORK_DOWN_EVENT != 0 ) */
        }
    }
}
/*-----------------------------------------------------------*/

void ETH_IRQHandler( void )
{
    HAL_ETH_IRQHandler( &xETH );
}
