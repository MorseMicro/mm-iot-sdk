/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_wlan.h"
#include "mmosal.h"
#include "mmconfig.h"
#include "mmutils.h"
#include "main.h"

/**
 * The number of bytes to send as part of @ref mmhal_wlan_send_training_seq for SD over SPI.
 * This must be at least 74 bits (>= 10 bytes), see section 6.4.1.1 of the SD spec
 * "Physical Layer Simplified Specification Version 9.10" for more detail.
 */
#define BYTE_TRAIN 16

MM_STATIC_ASSERT((BYTE_TRAIN >= 10), "BYTE_TRAIN must be at least 10.");

/** Binary Semaphore for indicating DMA transaction complete */
static struct mmosal_semb *dma_semb_handle;
/** Maximum time we expect a DMA transaction to take (for the maximum block size) in ms. */
#define DMA_WAIT_TMO (10)

/** Minimum length of data to be transferred we require before we do a DMA transfer. */
#define DMA_TRANSFER_MIN_LENGTH (16)

/** SPI hw interrupt handler. Must be set before enabling irq */
static mmhal_irq_handler_t spi_irq_handler = NULL;

/** busy interrupt handler. Must be set before enabling irq */
static mmhal_irq_handler_t busy_irq_handler = NULL;

void mmhal_wlan_hard_reset(void)
{
    LL_GPIO_ResetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    mmosal_task_sleep(5);
    LL_GPIO_SetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    mmosal_task_sleep(20);
}

#if defined(ENABLE_EXT_XTAL_INIT) && ENABLE_EXT_XTAL_INIT
bool mmhal_wlan_ext_xtal_init_is_required(void)
{
    return true;
}
#endif

void mmhal_wlan_spi_cs_assert(void)
{
    LL_GPIO_ResetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
}

void mmhal_wlan_spi_cs_deassert(void)
{
    LL_GPIO_SetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
}

uint8_t mmhal_wlan_spi_rw(uint8_t data)
{
    while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
    {
    }
    LL_SPI_TransmitData8(SPI_PERIPH, data);
    while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
    {
    }
    uint8_t readval = LL_SPI_ReceiveData8(SPI_PERIPH);
    return readval;
}

static void mmhal_wlan_spi_read_buf_dma(uint8_t *buf, unsigned len)
{
    uint32_t spi_tx_data = 0xFF; /* Write dummy data */

    /* Ensure DMA Channels are disabled before configuration */
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);

    while (LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM))
    {
    }

    while (LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM))
    {
    }

    /* Configure DMA */
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_MEMORY_NOINCREMENT);

    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH,
                           SPI_RX_DMA_STREAM,
                           LL_SPI_DMA_GetRxRegAddr(SPI_PERIPH),
                           (uint32_t)buf,
                           LL_DMA_GetDataTransferDirection(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM));
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, (uint32_t)len);

    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH,
                           SPI_TX_DMA_STREAM,
                           (uint32_t)&spi_tx_data,
                           LL_SPI_DMA_GetTxRegAddr(SPI_PERIPH),
                           LL_DMA_GetDataTransferDirection(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM));
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, (uint32_t)len);

    /* Enable SPI Peripheral DMA */
    LL_SPI_EnableDMAReq_RX(SPI_PERIPH);
    LL_SPI_EnableDMAReq_TX(SPI_PERIPH);

    /* Enable DMA Channel */
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);

    LL_SPI_Enable(SPI_PERIPH);
    LL_SPI_StartMasterTransfer(SPI_PERIPH);

    bool ok = mmosal_semb_wait(dma_semb_handle, DMA_WAIT_TMO);
    MMOSAL_ASSERT(ok);

    LL_SPI_DisableDMAReq_RX(SPI_PERIPH);
    LL_SPI_DisableDMAReq_TX(SPI_PERIPH);

    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);
}

void mmhal_wlan_spi_read_buf(uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        unsigned ii;
        for (ii = 0; ii < len; ii++)
        {
            while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
            {
            }
            LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
            while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
            {
            }
            *buf++ = LL_SPI_ReceiveData8(SPI_PERIPH);
        }
    }
    else
    {
        mmhal_wlan_spi_read_buf_dma(buf, len);
    }
}

static void mmhal_wlan_spi_write_buf_dma(const uint8_t *buf, unsigned len)
{
    uint32_t spi_rx_data; /* Dummy Read */

    /* Ensure DMA Channels are disabled before configuration */
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);

    while (LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM))
    {
    }

    while (LL_DMA_IsEnabledStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM))
    {
    }

    /* Configure DMA */
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_MEMORY_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);

    /* Configure Transaction */
    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH,
                           SPI_RX_DMA_STREAM,
                           LL_SPI_DMA_GetRxRegAddr(SPI_PERIPH),
                           (uint32_t)&spi_rx_data,
                           LL_DMA_GetDataTransferDirection(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM));
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, (uint32_t)len);

    LL_DMA_ConfigAddresses(SPI_DMA_PERIPH,
                           SPI_TX_DMA_STREAM,
                           (uint32_t)buf,
                           LL_SPI_DMA_GetTxRegAddr(SPI_PERIPH),
                           LL_DMA_GetDataTransferDirection(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM));
    LL_DMA_SetDataLength(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, (uint32_t)len);

    /* Enable SPI Peripheral DMA */
    LL_SPI_EnableDMAReq_RX(SPI_PERIPH);
    LL_SPI_EnableDMAReq_TX(SPI_PERIPH);

    /* Enable DMA Channel */
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_EnableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);

    LL_SPI_Enable(SPI_PERIPH);
    LL_SPI_StartMasterTransfer(SPI_PERIPH);

    bool ok = mmosal_semb_wait(dma_semb_handle, DMA_WAIT_TMO);
    MMOSAL_ASSERT(ok);

    LL_SPI_DisableDMAReq_RX(SPI_PERIPH);
    LL_SPI_DisableDMAReq_TX(SPI_PERIPH);

    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_DisableStream(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);
}

void mmhal_wlan_spi_write_buf(const uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        unsigned ii;
        for (ii = 0; ii < len; ii++)
        {
            while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
            {
            }
            LL_SPI_TransmitData8(SPI_PERIPH, *buf++);
            while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
            {
            }
            LL_SPI_ReceiveData8(SPI_PERIPH);
        }
    }
    else
    {
        mmhal_wlan_spi_write_buf_dma(buf, len);
    }
}

void mmhal_wlan_send_training_seq(void)
{
    uint8_t i;

    mmhal_wlan_spi_cs_deassert();

    /* SPI MOSI line to GPIO out*/
    LL_GPIO_SetPinMode(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetOutputPin(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin);

    LL_SPI_Enable(SPI_PERIPH);
    LL_SPI_StartMasterTransfer(SPI_PERIPH);
    /* Send >74 clock pulses to card to stabilize CLK.
     * This method of stacking up the TX data is described in RM0090 rev 19 Figure 253.
     * It results is a reduction in the time between bytes of ~85% (316ns -> 48ns).
     * Could not get this to work for the other transactions however.
     */
    while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
    {
    }
    LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
    for (i = 0; i < BYTE_TRAIN; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
        {
        }
        LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
        while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
        {
        }
        LL_SPI_ReceiveData8(SPI_PERIPH);
    }
    while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
    {
    }
    LL_SPI_ReceiveData8(SPI_PERIPH);

    while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
    {
    }

    /* Configure SPI MOSI line back to orginal config */
    LL_GPIO_ResetOutputPin(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin);
    LL_GPIO_SetPinMode(SPI_MOSI_GPIO_Port, SPI_MOSI_Pin, LL_GPIO_MODE_ALTERNATE);
}

void mmhal_wlan_register_spi_irq_handler(mmhal_irq_handler_t handler)
{
    spi_irq_handler = handler;
}

bool mmhal_wlan_spi_irq_is_asserted(void)
{
    return !LL_GPIO_IsInputPinSet(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin);
}

void mmhal_wlan_set_spi_irq_enabled(bool enabled)
{
    if (enabled)
    {
        LL_EXTI_EnableIT_0_31(SPI_IRQ_LINE);
        LL_EXTI_EnableEvent_0_31(SPI_IRQ_LINE);

        /* The transiver will hold the IRQ line low if there is additional information
         * to be retrived. Ideally the interrupt pin would be configured as a low level
         * interrupt.
         */
        if (mmhal_wlan_spi_irq_is_asserted())
        {
            if (spi_irq_handler != NULL)
            {
                spi_irq_handler();
            }
        }
        NVIC_EnableIRQ(SPI_IRQn);
    }
    else
    {
        NVIC_DisableIRQ(SPI_IRQn);

        LL_EXTI_DisableIT_0_31(SPI_IRQ_LINE);
        LL_EXTI_DisableEvent_0_31(SPI_IRQ_LINE);
    }
}

void mmhal_wlan_init(void)
{
    dma_semb_handle = mmosal_semb_create("dma_semb_handle");
    /* Raise the RESET_N line to enable the WLAN transceiver. */
    LL_GPIO_SetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
}

void mmhal_wlan_deinit(void)
{
    mmosal_semb_delete(dma_semb_handle);
    /* Lower the RESET_N line to disable the WLAN transceiver. This will put the transceiver in its
     * lowest power state. */
    LL_GPIO_ResetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
}

void mmhal_wlan_wake_assert(void)
{
    LL_GPIO_SetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
}

void mmhal_wlan_wake_deassert(void)
{
    LL_GPIO_ResetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
}

bool mmhal_wlan_busy_is_asserted(void)
{
    return LL_GPIO_IsInputPinSet(BUSY_GPIO_Port, BUSY_Pin);
}

void mmhal_wlan_register_busy_irq_handler(mmhal_irq_handler_t handler)
{
    busy_irq_handler = handler;
}

void mmhal_wlan_set_busy_irq_enabled(bool enabled)
{
    if (enabled)
    {
        NVIC_EnableIRQ(BUSY_IRQn);
    }
    else
    {
        NVIC_DisableIRQ(BUSY_IRQn);
    }
}

/**
 * @brief This function handles BUSY interrupt.
 */
void BUSY_IRQ_HANDLER(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(BUSY_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearFlag_0_31(BUSY_IRQ_LINE);
        if (busy_irq_handler != NULL)
        {
            busy_irq_handler();
        }
    }
}

/**
 * @brief This function handles SPI IRQ interrupts.
 */
void SPI_IRQ_HANDLER(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(SPI_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearFlag_0_31(SPI_IRQ_LINE);
        if (spi_irq_handler != NULL)
        {
            spi_irq_handler();
        }
    }
}

/**
 * @brief This function handles DMA1 Stream 1 global interrupt.
 */
void DMA1_Stream1_IRQHandler(void)
{
    LL_DMA_ClearFlag_TC1(SPI_DMA_PERIPH);
    mmosal_semb_give_from_isr(dma_semb_handle);
}

/**
 * @brief This function handles DMA1 Stream 0 global interrupt.
 */
void DMA1_Stream0_IRQHandler(void)
{
    /* We do not expect to get interrupts on the SPI TX DMA channel. */
    LL_DMA_ClearFlag_TC0(SPI_DMA_PERIPH);
}

/**
 * Generate a stable, device-unique MAC address based on the MCU UID. This address is not globally
 * unique, but is consistent across boots on the same device and marked as locally administered
 * (0x02 prefix).
 *
 * @param mac_addr Location where the MAC address will be stored.
 */
static void generate_stable_mac_addr_from_uid(uint8_t *mac_addr)
{
    /* Shorten the UID */
    uint32_t uid = LL_GetUID_Word0() ^ LL_GetUID_Word1() ^ LL_GetUID_Word2();

    /* Set MAC address as locally administered */
    mac_addr[0] = 0x02;
    mac_addr[1] = 0x00;
    memcpy(&mac_addr[2], &uid, sizeof(uid));
}

/**
 * Attempts to read a MAC address from the "wlan.macaddr" key in mmconfig persistent configuration.
 *
 * @param mac_addr Location where the MAC address will be stored if there is a valid MAC address in
 *                 mmconfig persistent storage.
 */
static void get_mmconfig_mac_addr(uint8_t *mac_addr)
{
    char strval[32];
    if (mmconfig_read_string("wlan.macaddr", strval, sizeof(strval)) > 0)
    {
        /* Need to provide an array of ints to sscanf otherwise it will overflow */
        int temp[MMWLAN_MAC_ADDR_LEN];
        uint8_t validated_mac[MMWLAN_MAC_ADDR_LEN];
        int i;

        int ret = sscanf(strval,
                         "%x:%x:%x:%x:%x:%x",
                         &temp[0],
                         &temp[1],
                         &temp[2],
                         &temp[3],
                         &temp[4],
                         &temp[5]);
        if (ret == MMWLAN_MAC_ADDR_LEN)
        {
            for (i = 0; i < MMWLAN_MAC_ADDR_LEN; i++)
            {
                if (temp[i] > UINT8_MAX || temp[i] < 0)
                {
                    /* Invalid value, ignore and exit without updating mac_addr */
                    printf("Invalid MAC address found in [wlan.macaddr], rejecting!\n");
                    return;
                }
                validated_mac[i] = (uint8_t)temp[i];
            }
            /* We only override the value in mac_addr once the entire mmconfig MAC has been
             * validated in case mac_addr already contains a MAC address. */
            memcpy(mac_addr, validated_mac, MMWLAN_MAC_ADDR_LEN);
        }
    }
}

void mmhal_read_mac_addr(uint8_t *mac_addr)
{
    /*
     * MAC address is determined using the following precedence:
     *
     * 1. The value of the `wlan.macaddr` setting in persistent storage, if present and valid.
     *
     * 2. The MAC address in transceiver OTP (i.e., the value of mac_addr passed into this function,
     *    if non-zero).
     *
     * 3. A stable MAC address generated from the MCU’s hardware UID. This value is consistent
     *    across boots for the same device, but unique to each MCU.
     *
     * 4. Failing all of the above, the value of mac_addr will remain zero on return from this
     *    function, in which case the driver will generate a random MAC address.
     */

    get_mmconfig_mac_addr(mac_addr);

    if (!mm_mac_addr_is_zero(mac_addr))
    {
        return;
    }

    generate_stable_mac_addr_from_uid(mac_addr);
}

const struct mmhal_chip *mmhal_get_chip(void)
{
    /* This is a define that is set by the build system in the platform-xxx.mk file to select the
     * Morse chip type. See the API documentation for mmhal_get_chip() for more information. */
    return &MMHAL_CHIP_TYPE;
}
