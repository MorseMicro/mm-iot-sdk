/*
 * Copyright 2021-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmekh05_hal_common.h"
#include "stm32u5xx_ll_sdmmc.h"


#ifndef ENABLE_SDIO_4BIT
#define ENABLE_SDIO_4BIT (1)
#endif

#ifndef ENABLE_WLAN_HAL_LOG_ERR
#define ENABLE_WLAN_HAL_LOG_ERR (1)
#endif

#ifndef LOG_ERR
#if ENABLE_WLAN_HAL_LOG_ERR
#define LOG_ERR(...) printf(__VA_ARGS__)
#else
#define LOG_ERR(...) do { } while (0)
#endif
#endif

/* SDIO clock frequency to use during early initialization. */
#define SD_INIT_FREQ_HZ                     400000U

/** Binary Semaphore for indicating DMA transaction complete */
static struct mmosal_semb *dma_semb_handle;
/** Maximum time we expect a DMA transaction to take (for the maximum block size) in ms. */
#define DMA_WAIT_TMO (100)

/** SPI hw interrupt handler. Must be set before enabling irq */
static mmhal_irq_handler_t spi_irq_handler = NULL;

/** busy interrupt handler. Must be set before enabling irq */
static mmhal_irq_handler_t busy_irq_handler = NULL;

/** SDIO result is stashed here by IRQ handler. */
static volatile uint32_t sdio_result = 0;

/** Inserts a delay during XTAL init phase */
extern void morse_xtal_init_delay(void);

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

int mmhal_wlan_sdio_cmd(uint8_t cmd_idx, uint32_t arg, uint32_t *rsp)
{
    uint32_t sta_reg;
    uint32_t rsp_;

    SDMMC_CmdInitTypeDef sdmmc_cmdinit = {
        .CmdIndex = cmd_idx,
        .Argument = arg,
        .Response = SDMMC_RESPONSE_SHORT,
        .WaitForInterrupt = SDMMC_WAIT_NO,
        .CPSM = SDMMC_CPSM_ENABLE
    };

    (void)SDMMC_SendCommand(WLAN_SDMMC, &sdmmc_cmdinit);

    morse_xtal_init_delay();
    uint32_t timeout_at_ms = mmosal_get_time_ms() + SDMMC_CMDTIMEOUT;

    do
    {
        if (mmosal_time_has_passed(timeout_at_ms))
        {
            return MMHAL_SDIO_CMD_TIMEOUT;
        }

        sta_reg = WLAN_SDMMC->STA;
    } while (((sta_reg & SDMMC_STATIC_CMD_FLAGS) == 0) ||
             ((sta_reg & SDMMC_FLAG_CMDACT) != 0U));

    if (__SDMMC_GET_FLAG(WLAN_SDMMC, SDMMC_FLAG_CTIMEOUT))
    {
        __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_FLAG_CTIMEOUT);

        return MMHAL_SDIO_CMD_TIMEOUT;
    }
    else if (__SDMMC_GET_FLAG(WLAN_SDMMC, SDMMC_FLAG_CCRCFAIL))
    {
        __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_FLAG_CCRCFAIL);

        return MMHAL_SDIO_CMD_CRC_ERROR;
    }

    /* Clear all the static flags */
    __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_CMD_FLAGS);

    rsp_ = SDMMC_GetResponse(WLAN_SDMMC, SDMMC_RESP1);
    if (rsp != NULL)
    {
        *rsp = rsp_;
    }

    return 0;
}

int mmhal_wlan_sdio_startup(void)
{
    uint32_t rsp;
    int ret;
    SDMMC_InitTypeDef Init = {
        .ClockEdge           = SDMMC_CLOCK_EDGE_RISING,
        .ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE,
        .BusWide             = SDMMC_BUS_WIDE_1B,
        .HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE,
        .ClockDiv = 0,
    };
    SD_HandleTypeDef hsd = {
        .Instance = WLAN_SDMMC,
    };

    uint32_t sdmmc_clk_freq;
    uint32_t sdmmc_periph_clk_freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC);
    if (sdmmc_periph_clk_freq == 0)
    {
        return MMHAL_SDIO_HW_ERROR;
    }

    Init.ClockDiv = sdmmc_periph_clk_freq / (2u * SD_INIT_FREQ_HZ);

    HAL_SD_MspInit(&hsd);

    ret = SDMMC_Init(WLAN_SDMMC, Init);
        if (ret != HAL_OK)
    {
        LOG_ERR("%s:%d: SDMMC_Init failed\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = SDMMC_PowerState_ON(WLAN_SDMMC);
    if (ret != HAL_OK)
    {
        LOG_ERR("%s:%d: %s failed\n", __FUNCTION__, __LINE__, "PowerState");
        return ret;
    }

    /* Wait 74 clock cycles before starting the SD initialization sequence. */
    sdmmc_clk_freq = sdmmc_periph_clk_freq / (2u * Init.ClockDiv);
    HAL_Delay(1u + (74u * 1000u / (sdmmc_clk_freq)));

    /* CMD0: GO_IDLE_STATE */
    ret = SDMMC_CmdGoIdleState(WLAN_SDMMC);
    if (ret != HAL_OK)
    {
        LOG_ERR("%s:%d: %s failed\n", __FUNCTION__, __LINE__, "CMD0");
        return ret;
    }

    /* CMD8: SEND_IF_COND: Command available only on V2.0 cards */
    ret = SDMMC_CmdOperCond(WLAN_SDMMC);
    if (ret != HAL_OK)
    {
        LOG_ERR("%s:%d: %s failed\n", __FUNCTION__, __LINE__, "CMD8");
        return ret;
    }

    /* CMD5 */
    ret = mmhal_wlan_sdio_cmd(5, 0xfc0000, &rsp);
    if (ret != HAL_OK)
    {
        LOG_ERR("%s:%d: %s failed\n", __FUNCTION__, __LINE__, "CMD8");
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(3, 0, &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD3 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(7, rsp & 0xffff0000, &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD7 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

#if ENABLE_SDIO_4BIT != 0
    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_0, 0x07, 0x02), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }
#endif

    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_0, 0x02, 0x06), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_0, 0x04, 0x07), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_1, 0x10000, 0x05), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_1, 0x10001, 0x10), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = mmhal_wlan_sdio_cmd(
        52, mmhal_make_cmd52_arg(MMHAL_SDIO_WRITE, MMHAL_SDIO_FUNCTION_1, 0x10002, 0x02), &rsp);
    if (ret != 0)
    {
        LOG_ERR("%s:%d: CMD52 error\n", __FUNCTION__, __LINE__);
        return ret;
    }

    /* Reinitialize the SD Controller with higher clock rate and 4 bit mode if enabled. */
#if ENABLE_SDIO_4BIT != 0
    Init.BusWide = SDMMC_BUS_WIDE_4B;
#endif
    Init.ClockDiv = 2;
    ret = SDMMC_Init(WLAN_SDMMC, Init);

    return ret;
}

void mmhal_wlan_register_spi_irq_handler(mmhal_irq_handler_t handler)
{
    spi_irq_handler = handler;
}

bool mmhal_wlan_spi_irq_is_asserted(void)
{
    return __SDMMC_GET_IT(WLAN_SDMMC, SDMMC_IT_SDIOIT);
}

void mmhal_wlan_set_spi_irq_enabled(bool enabled)
{
    if (enabled)
    {
        __SDMMC_ENABLE_IT(WLAN_SDMMC, SDMMC_IT_SDIOIT);

        /* Kick the SDIO controller in case the interrupt line is low but the interrupt
         * flag is not set. */
        __SDMMC_OPERATION_DISABLE(WLAN_SDMMC);
        __SDMMC_OPERATION_ENABLE(WLAN_SDMMC);
    }
    else
    {
        __SDMMC_DISABLE_IT(WLAN_SDMMC, SDMMC_IT_SDIOIT);
    }
}


void mmhal_wlan_init(void)
{
    /* WLAN_SDMMC interrupt Init */
    HAL_NVIC_SetPriority(WLAN_SDMMC_IRQ, 15, 0);
    HAL_NVIC_EnableIRQ(WLAN_SDMMC_IRQ);

    dma_semb_handle = mmosal_semb_create("dma_semb");
    /* Raise the RESET_N line to enable the WLAN transceiver. */
    LL_GPIO_SetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
}

void mmhal_wlan_deinit(void)
{
    HAL_NVIC_DisableIRQ(WLAN_SDMMC_IRQ);
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

void mmhal_wlan_clear_spi_irq(void)
{
    __SDMMC_CLEAR_IT(WLAN_SDMMC, SDMMC_IT_SDIOIT);
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
  * @brief This function handles Busy pin interrupt.
  */
void BUSY_IRQ_HANDLER(void)
{
    if (LL_EXTI_IsActiveRisingFlag_0_31(BUSY_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearRisingFlag_0_31(BUSY_IRQ_LINE);
        if (busy_irq_handler != NULL)
        {
            busy_irq_handler();
            /* Kick the SDIO controller in case the interrupt line is low but the interrupt
             * flag is not set. */
            __SDMMC_OPERATION_DISABLE(WLAN_SDMMC);
            __SDMMC_OPERATION_ENABLE(WLAN_SDMMC);
        }
    }
}

int mmhal_wlan_sdio_cmd53_write(const struct mmhal_wlan_sdio_cmd53_write_args *args)
{
    uint32_t transfer_mode = SDMMC_TRANSFER_MODE_BLOCK;
    uint32_t block_size = SDMMC_DATABLOCK_SIZE_512B;
    uint32_t transfer_length_bytes = args->transfer_length;
    uint32_t rsp;
    int ret = 0;

    if (args->block_size != 0)
    {
        transfer_length_bytes = (uint32_t)args->transfer_length * (uint32_t)args->block_size;
    }

    /** Transfer must be 32-bit word aligned. */
    if ((transfer_length_bytes & 0x03) != 0)
    {
        LOG_ERR("SDIO Transfer length must be a multiple of 4\n");
        return MMHAL_SDIO_INVALID_ARGUMENT;
    }

    if (((uint32_t)(args->data) & 0x03) != 0)
    {
        LOG_ERR("SDIO data buffer must be 32 bit word aligned\n");
        return MMHAL_SDIO_INVALID_ARGUMENT;
    }

    if (args->block_size == 8)
    {
        block_size = SDMMC_DATABLOCK_SIZE_8B;
    }
    else if (args->block_size != 512 && args->block_size != 0)
    {
        MMOSAL_ASSERT(false);
    }

    if (args->block_size == 0)
    {
        /* Byte mode transfer */
        transfer_mode = SDMMC_DCTRL_DTMODE_0;
    }

    WLAN_SDMMC->DTIMER = SDMMC_DATATIMEOUT;
    WLAN_SDMMC->DLEN = transfer_length_bytes;

    __SDMMC_OPERATION_DISABLE(WLAN_SDMMC);
    WLAN_SDMMC->DCTRL =
        SDMMC_DCTRL_SDIOEN | block_size | SDMMC_TRANSFER_DIR_TO_CARD |
        transfer_mode | SDMMC_DPSM_DISABLE;

    __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);
    __SDMMC_CMDTRANS_ENABLE(WLAN_SDMMC);

    WLAN_SDMMC->IDMABASER = (uint32_t)args->data;
    WLAN_SDMMC->IDMACTRL = SDMMC_ENABLE_IDMA_SINGLE_BUFF;

    sdio_result = 0;
    ret = mmhal_wlan_sdio_cmd(53, args->sdio_arg, &rsp);
    if (ret != 0)
    {
        __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_CMD_FLAGS);
        __SDMMC_CMDTRANS_DISABLE(WLAN_SDMMC);
        return ret;
    }

    __SDMMC_ENABLE_IT(WLAN_SDMMC,
                      SDMMC_IT_DATAEND | SDMMC_IT_DTIMEOUT |
                      SDMMC_IT_TXUNDERR | SDMMC_FLAG_DCRCFAIL);
    ret = 0;

    mmosal_semb_wait(dma_semb_handle, 750);

    __SDMMC_DISABLE_IT(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);
    __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);

    if (sdio_result & SDMMC_FLAG_DATAEND)
    {
        /* Re-enable SDIO operation (enables SDIO interrupt) */
        __SDMMC_OPERATION_ENABLE(WLAN_SDMMC);
        return 0;
    }
    else if (sdio_result == 0 || sdio_result & SDMMC_FLAG_DTIMEOUT)
    {
        return MMHAL_SDIO_DATA_TIMEOUT;
    }
    else if (sdio_result & SDMMC_FLAG_DCRCFAIL)
    {
        return MMHAL_SDIO_DATA_CRC_ERROR;
    }
    else if (sdio_result & SDMMC_FLAG_TXUNDERR)
    {
        return MMHAL_SDIO_DATA_UNDERFLOW;
    }
    else
    {
        return MMHAL_SDIO_OTHER_ERROR;
    }
}

int mmhal_wlan_sdio_cmd53_read(const struct mmhal_wlan_sdio_cmd53_read_args *args)
{
    int ret = 0;
    uint32_t rsp;
    uint32_t transfer_length_bytes = args->transfer_length;
    uint32_t transfer_mode = SDMMC_TRANSFER_MODE_BLOCK;
    uint32_t block_size = SDMMC_DATABLOCK_SIZE_512B;

    if (args->block_size != 0)
    {
        transfer_length_bytes *= args->block_size;
    }

    if (args->block_size == 8)
    {
        block_size = SDMMC_DATABLOCK_SIZE_8B;
    }
    else if (args->block_size != 512 && args->block_size != 0)
    {
        MMOSAL_ASSERT(false);
    }

    if (args->block_size == 0)
    {
        transfer_mode = SDMMC_DCTRL_DTMODE_0;
    }

    /** Transfer must be 32-bit word aligned. */
    MMOSAL_ASSERT((transfer_length_bytes & 0x03) == 0);
    MMOSAL_ASSERT(((uint32_t)(args->data) & 0x03) == 0);

    WLAN_SDMMC->DTIMER = SDMMC_DATATIMEOUT;
    WLAN_SDMMC->DLEN = transfer_length_bytes;

    __SDMMC_OPERATION_DISABLE(WLAN_SDMMC);
    MODIFY_REG(WLAN_SDMMC->DCTRL, DCTRL_CLEAR_MASK,
               SDMMC_DCTRL_SDIOEN | block_size | SDMMC_TRANSFER_DIR_TO_SDMMC |
               transfer_mode | SDMMC_DPSM_DISABLE);

    __SDMMC_CMDTRANS_ENABLE(WLAN_SDMMC);
    WLAN_SDMMC->IDMABASER = (uint32_t)args->data;
    WLAN_SDMMC->IDMACTRL = SDMMC_ENABLE_IDMA_SINGLE_BUFF;

    sdio_result = 0;

    ret = mmhal_wlan_sdio_cmd(53, args->sdio_arg, &rsp);
    if (ret != 0)
    {
        __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_CMD_FLAGS);
        __SDMMC_CMDTRANS_DISABLE(WLAN_SDMMC);
        return ret;
    }

    __SDMMC_ENABLE_IT(WLAN_SDMMC,
                      SDMMC_IT_DCRCFAIL | SDMMC_IT_DTIMEOUT |
                      SDMMC_IT_RXOVERR | SDMMC_IT_DATAEND);
    (void)mmosal_semb_wait(dma_semb_handle, 1000);

    __SDMMC_DISABLE_IT(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);
    __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);

    if (sdio_result & SDMMC_FLAG_DATAEND)
    {
        /* Re-enable SDIO operation (enables SDIO interrupt */
        __SDMMC_OPERATION_ENABLE(WLAN_SDMMC);
        return 0;
    }
    if (sdio_result == 0 || sdio_result & SDMMC_FLAG_DTIMEOUT)
    {
        return MMHAL_SDIO_DATA_TIMEOUT;
    }
    else if (sdio_result & SDMMC_FLAG_DCRCFAIL)
    {
        return MMHAL_SDIO_DATA_CRC_ERROR;
    }
    else if (sdio_result & SDMMC_FLAG_RXOVERR)
    {
        return MMHAL_SDIO_DATA_OVERRUN;
    }
    else
    {
        return MMHAL_SDIO_OTHER_ERROR;
    }
}


void WLAN_SDMMC_IRQHandler(void)
{
    if (__SDMMC_GET_FLAG(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS))
    {
        sdio_result = WLAN_SDMMC->STA & SDMMC_STATIC_DATA_FLAGS;
        __SDMMC_CLEAR_FLAG(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);
        __SDMMC_DISABLE_IT(WLAN_SDMMC, SDMMC_STATIC_DATA_FLAGS);

        __SDMMC_CMDTRANS_DISABLE(WLAN_SDMMC);

        WLAN_SDMMC->DLEN = 0;
        WLAN_SDMMC->DCTRL = 0;
        WLAN_SDMMC->IDMACTRL = SDMMC_DISABLE_IDMA;

        mmosal_semb_give_from_isr(dma_semb_handle);
    }

    /* Check whether SDIO interrupt is enabled and asserted */
    if (WLAN_SDMMC->STA & WLAN_SDMMC->MASK & SDMMC_IT_SDIOIT)
    {
        if (spi_irq_handler != NULL)
        {
            spi_irq_handler();
        }
    }
}
