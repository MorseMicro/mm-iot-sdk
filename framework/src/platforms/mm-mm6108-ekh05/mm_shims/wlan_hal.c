/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmekh05_hal_common.h"


/** 10x8bit training seq */
#define BYTE_TRAIN 16

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
    {}
    LL_SPI_TransmitData8(SPI_PERIPH, data);
    while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
    {}
    uint8_t readval = LL_SPI_ReceiveData8(SPI_PERIPH);
    return readval;
}

static void mmhal_wlan_spi_read_buf_dma(uint8_t *buf, unsigned len)
{
    uint32_t spi_tx_data = 0xFF; /* Write dummy data */

    /* Configure DMA */
    LL_DMA_SetDestIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, LL_DMA_DEST_INCREMENT);
    LL_DMA_SetSrcIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, LL_DMA_SRC_FIXED);

    /* Configure Transaction */
    LL_DMA_SetDestAddress(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, (uint32_t)buf);
    LL_DMA_SetBlkDataLength(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, (uint32_t)len);

    LL_DMA_SetSrcAddress(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, (uint32_t)&spi_tx_data);
    LL_DMA_SetBlkDataLength(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, (uint32_t)len);

    /* Enable DMA Channel */
    LL_DMA_EnableChannel(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL);
    LL_DMA_EnableChannel(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL);

    /* Enable SPI Peripheral DMA */
    LL_SPI_EnableDMAReq_RX(SPI_PERIPH);
    LL_SPI_EnableDMAReq_TX(SPI_PERIPH);

    bool ok = mmosal_semb_wait(dma_semb_handle, DMA_WAIT_TMO);
    MMOSAL_ASSERT(ok);

    LL_SPI_DisableDMAReq_RX(SPI_PERIPH);
    LL_SPI_DisableDMAReq_TX(SPI_PERIPH);

    LL_DMA_DisableChannel(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL);
    LL_DMA_DisableChannel(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL);
}


void mmhal_wlan_spi_read_buf(uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        unsigned ii;
        for (ii = 0; ii < len; ii++)
        {
            while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
            {}
            LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
            while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
            {}
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

    /* Configure DMA */
    LL_DMA_SetDestIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, LL_DMA_DEST_FIXED);
    LL_DMA_SetSrcIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, LL_DMA_SRC_INCREMENT);

    /* Configure Transaction */
    LL_DMA_SetSrcAddress(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, (uint32_t)&SPI_PERIPH->RXDR);
    LL_DMA_SetDestAddress(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, (uint32_t)&spi_rx_data);
    LL_DMA_SetBlkDataLength(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL, (uint32_t)len);

    LL_DMA_SetSrcAddress(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, (uint32_t)buf);
    LL_DMA_SetDestAddress(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, (uint32_t)&SPI_PERIPH->TXDR);
    LL_DMA_SetBlkDataLength(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL, (uint32_t)len);

    LL_SPI_EnableDMAReq_RX(SPI_PERIPH);

    /* Enable DMA Channel */
    LL_DMA_EnableChannel(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL);
    LL_DMA_EnableChannel(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL);

    /* Enable SPI Peripheral DMA */
    LL_SPI_EnableDMAReq_TX(SPI_PERIPH);

    LL_SPI_Enable(SPI_PERIPH);
    LL_SPI_StartMasterTransfer(SPI_PERIPH);

    bool ok = mmosal_semb_wait(dma_semb_handle, DMA_WAIT_TMO);
    MMOSAL_ASSERT(ok);

    LL_SPI_DisableDMAReq_RX(SPI_PERIPH);
    LL_SPI_DisableDMAReq_TX(SPI_PERIPH);

    LL_DMA_DisableChannel(SPI_DMA_PERIPH, SPI_TX_DMA_CHANNEL);
    LL_DMA_DisableChannel(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL);
}


void mmhal_wlan_spi_write_buf(const uint8_t *buf, unsigned len)
{
    if (len < DMA_TRANSFER_MIN_LENGTH)
    {
        unsigned ii;
        for (ii = 0; ii < len; ii++)
        {
            while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
            {}
            LL_SPI_TransmitData8(SPI_PERIPH, *buf++);
            while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
            {}
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
    {}
    LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
    for (i = 0; i < BYTE_TRAIN; i++)
    {
        while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
        {}
        LL_SPI_TransmitData8(SPI_PERIPH, 0xFF);
        while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
        {}
        LL_SPI_ReceiveData8(SPI_PERIPH);
    }
    while (!LL_SPI_IsActiveFlag_RXP(SPI_PERIPH))
    {}
    LL_SPI_ReceiveData8(SPI_PERIPH);

    while (!LL_SPI_IsActiveFlag_TXP(SPI_PERIPH))
    {}

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
    if (LL_EXTI_IsActiveRisingFlag_0_31(BUSY_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearRisingFlag_0_31(BUSY_IRQ_LINE);
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
    if (LL_EXTI_IsActiveFallingFlag_0_31(SPI_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearFallingFlag_0_31(SPI_IRQ_LINE);
        if (spi_irq_handler != NULL)
        {
            spi_irq_handler();
        }
    }
}

/**
  * @brief This function handles GPDMA1 Channel 14 global interrupt.
  */
void GPDMA1_Channel14_IRQHandler(void)
{
    LL_DMA_ClearFlag_TC(SPI_DMA_PERIPH, SPI_RX_DMA_CHANNEL);
    mmosal_semb_give_from_isr(dma_semb_handle);
}

/**
  * @brief This function handles GPDMA1 Channel 15 global interrupt.
  */
void GPDMA1_Channel15_IRQHandler(void)
{
    /* We do not expect to get interrupts on the SPI TX DMA channel. */
    MMOSAL_ASSERT(false);
}
