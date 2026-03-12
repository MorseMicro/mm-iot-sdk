/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mm_hal_common.h"
#include "mmhal_os.h"
#include "mmosal.h"
#include "mmutils.h"
#include "main.h"

/**
 * Minimum time in milliseconds required to enter deep sleep. The task that takes the most amount of
 * time is synchronizing values between the APB clock domain and the kernel clock domain in the
 * LPTIM. This takes ~2-3 clock cycles of the LPTIM.
 */
#define MIN_DEEP_SLEEP_TIME_MS          (5)

/**
 * Maximum time allowed to be in deep sleep. This is set so that we do not set a value in the
 * LPTIM_CMP register that is greater than or equal to the LPTIM_ARR register with enough
 * margin for delays.
 */
#define MAX_POSSIBLE_SUPPRESSED_TICKS   (64000)

/** Ticks per second, dependent of the LPTIM clock source. */
#define LPTIM_TICKS_PER_SECOND 8192

/** Macro to transform milliseconds into LPTIM ticks. */
#define MS_TO_LPTIM_TICKS(x) (((x) * LPTIM_TICKS_PER_SECOND) / 1000)

/** Macro to transform LPTIM ticks into milliseconds.  */
#define LPTIM_TICKS_TO_MS(x) (((x) * 1000) / LPTIM_TICKS_PER_SECOND)

const uint32_t mmhal_system_clock = 160000000;

void SystemClock_Config(void);

void mmhal_early_init(void)
{
    mmosal_disable_interrupts();
}

void mmhal_init(void)
{
    mmosal_enable_interrupts();
    mmhal_random_init();
}

enum mmhal_isr_state mmhal_get_isr_state(void)
{
    if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
    {
        return MMHAL_IN_ISR;
    }
    else
    {
        return MMHAL_NOT_IN_ISR;
    }
}

/* Log output via UART. Included by default, unless DISABLE_UART_LOG is defined. */
#ifndef DISABLE_UART_LOG

/** Size of the debug log buffer used by mmhal_log_write(). */
#define LOG_BUF_SIZE (512)
/**
 * Debug log buffer data structure. This is a simple circular buffer where empty is indicated
 * by (wr_idx == rd_idx) and full by (wr_idx == rd_idx-1) [mod LOG_BUF_SIZE].
 */
struct log_buf
{
    /** The place where the data goes. */
    uint8_t buf[LOG_BUF_SIZE];
    /** Write index into buf. */
    volatile size_t wr_idx;
    /** Read index into buf. */
    volatile size_t rd_idx;
    /** Flag to indicate if we've already sent a '\r' for the current '\n' */
    volatile bool newline_converted;
};

/** Log buffer instance. */
static struct log_buf log_buf = { 0 };

/**
 * Function to output a char from the a log buffer.
 *
 * @param buf Reference to log buffer to take the char from
 *
 * @note This will automatically convert any instances of @c \n to @c \r\n
 *
 * @warning A @c LL_USART_IsActiveFlag_TXE_TXFNF(LOG_USART) call must be made between consecutive
 *          calls of this function.
 */
static void mmhal_log_print_char(struct log_buf *buf)
{
    /* If we encounter a newline insert a carriage return first to maximize compatibility */
    if (buf->buf[buf->rd_idx] == '\n' && !buf->newline_converted)
    {
        buf->newline_converted = true;
        LL_USART_TransmitData8(LOG_USART, '\r');
        return;
    }
    buf->newline_converted = false;

    LL_USART_TransmitData8(LOG_USART, buf->buf[buf->rd_idx]);
    buf->rd_idx++;
    if (buf->rd_idx >= LOG_BUF_SIZE)
    {
        buf->rd_idx = 0;
    }
}

void LOG_USART_IRQ_HANDLER(void)
{
    /* Keep pumping data into the UART until it is not empty or we have no data left. */
    while (LL_USART_IsActiveFlag_TXE_TXFNF(LOG_USART))
    {
        if (log_buf.wr_idx != log_buf.rd_idx)
        {
            mmhal_log_print_char(&log_buf);
        }
        else
        {
            break;
        }
    }

    /* If we emptied the buffer then we no longer want to be interrupted. It will be reenabled
     * once more data is added to the buffer. */
    if (log_buf.wr_idx == log_buf.rd_idx)
    {
        LL_USART_DisableIT_TXE_TXFNF(LOG_USART);
    }
}

static void mmhal_log_flush_uart(void)
{
    /* We don't know whether interrupts will be enabled or not, so let's just disable and do
     * this the manual way. */
    LL_USART_DisableIT_TXE_TXFNF(LOG_USART);
    while (log_buf.wr_idx != log_buf.rd_idx)
    {
        while (!LL_USART_IsActiveFlag_TXE_TXFNF(LOG_USART))
        { }

        mmhal_log_print_char(&log_buf);
    }
}

static void mmhal_log_write_uart(const uint8_t *data, size_t length)
{
    while (length > 0)
    {
        size_t rd_idx = log_buf.rd_idx;
        size_t wr_idx = log_buf.wr_idx;

        /* Calculate maximum length we can copy in one go. */
        size_t space;
        if (wr_idx < rd_idx)
        {
            space = rd_idx - wr_idx - 1;
        }
        else
        {
            space = LOG_BUF_SIZE - wr_idx;
            /* Constrain to size-1 so we don't wrap. */
            if (!rd_idx)
            {
                space--;
            }
        }

        MMOSAL_ASSERT(space < LOG_BUF_SIZE);

        size_t copy_len = space < length ? space : length;
        memcpy(log_buf.buf + log_buf.wr_idx, data, copy_len);
        log_buf.wr_idx += copy_len;
        if (log_buf.wr_idx >= LOG_BUF_SIZE)
        {
            log_buf.wr_idx = 0;
        }
        data += copy_len;
        length -= copy_len;

        /* Enable TX empty interrupt. */
        LL_USART_EnableIT_TXE_TXFNF(LOG_USART);
        NVIC_EnableIRQ(LOG_USART_IRQ);
    }
}

#else

static void mmhal_log_write_uart(const uint8_t *data, size_t length)
{
    /* UART logging is disabled... send the message to a black hole. */
    MM_UNUSED(data);
    MM_UNUSED(length);
}

static void mmhal_log_flush_uart(void)
{
    /* UART logging is disabled... no action required. */
}

#endif

/* Log output via ITM/SWO. Only enabled if ENABLE_ITM_LOG is defined. */
#ifdef ENABLE_ITM_LOG

static void mmhal_log_write_itm(const uint8_t *data, size_t length)
{
    while (length-- > 0)
    {
        ITM_SendChar(*data++);
    }
}

#else

/* ITM/SWO logging is disabled... send the message to a black hole. */
static void mmhal_log_write_itm(const uint8_t *data, size_t length)
{
    MM_UNUSED(data);
    MM_UNUSED(length);
}

#endif

void mmhal_log_write(const uint8_t *data, size_t length)
{
    mmhal_log_write_uart(data, length);
    mmhal_log_write_itm(data, length);
}

void mmhal_log_flush(void)
{
    mmhal_log_flush_uart();
}

void mmhal_reset(void)
{
    HAL_NVIC_SystemReset();
    while (1)
    { }
}

void LPTIM1_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    /* Check whether Compare match 1 interrupt is pending */
    if (LL_LPTIM_IsActiveFlag_CC1(LPTIM1))
    {
        LL_LPTIM_ClearFLAG_CC1(LPTIM1);
    }
}

/**
 * Function to configure the timer to wake us up after the desired sleep period.
 *
 * @param  sleep_time_ms Time in milliseconds that we wish to sleep for.
 *
 * @note If @c sleep_time_ms results in a tick count greater than or equal to
 *       @ref MAX_POSSIBLE_SUPPRESSED_TICKS the timer will be configured to elapse
 *       after @ref MAX_POSSIBLE_SUPPRESSED_TICKS
 *
 * @return               The starting tick count from the timer when the compare register is set.
 */
static uint32_t mmhal_sleep_configure_timer(uint32_t sleep_time_ms)
{
    uint32_t timeout_count = mmhal_system_clock;
    uint32_t start_time_ticks = LL_LPTIM_GetCounter(LPTIM1);

    uint32_t sleep_time_ticks = MS_TO_LPTIM_TICKS(sleep_time_ms);
    if (sleep_time_ticks >= MAX_POSSIBLE_SUPPRESSED_TICKS)
    {
        sleep_time_ticks = MAX_POSSIBLE_SUPPRESSED_TICKS;
    }

    uint32_t wakeup_time_ticks = start_time_ticks + sleep_time_ticks;

    /* Limit to 16 bit */
    wakeup_time_ticks &= 0xFFFF;

    LL_LPTIM_ClearFlag_CMP1OK(LPTIM1);
    LL_LPTIM_OC_SetCompareCH1(LPTIM1, wakeup_time_ticks);
    /* Wait until the compare value written to the register domain synchronizes with the LPTIM */
    while ((LL_LPTIM_IsActiveFlag_CMP1OK(LPTIM1) != 1UL) && timeout_count)
    {
        timeout_count--;
    }

    /* We don't actually expect to ever hit this, just placed here as a sanity check. */
    MMOSAL_ASSERT(timeout_count);

    return start_time_ticks;
}

void mmhal_sleep_abort(enum mmhal_sleep_state sleep_state)
{
    if (sleep_state == MMHAL_SLEEP_DEEP)
    {
        /* Restart the SysTick */
        SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
        HAL_ResumeTick();
    }

    /* Re-enable interrupts */
    __enable_irq();
}

/**
 * Function to initialize any peripherals upon exiting deep sleep. It is expected that a
 * corresponding call to @ref mmhal_sleep_deinit_peripherals was made upon entry to deep sleep.
 */
static void mmhal_sleep_init_peripherals(void)
{
    /* USART pins */
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;

    if (LOG_USART_TX_GPIO_Port == LOG_USART_RX_GPIO_Port)
    {
        GPIO_InitStruct.Pin = LOG_USART_TX_Pin | LOG_USART_RX_Pin;
        LL_GPIO_Init(LOG_USART_TX_GPIO_Port, &GPIO_InitStruct);
    }
    else
    {
        GPIO_InitStruct.Pin = LOG_USART_TX_Pin;
        LL_GPIO_Init(LOG_USART_TX_GPIO_Port, &GPIO_InitStruct);
        GPIO_InitStruct.Pin = LOG_USART_RX_Pin;
        LL_GPIO_Init(LOG_USART_RX_GPIO_Port, &GPIO_InitStruct);
    }
}

/**
 * Function to de-initialize any peripherals upon entering deep sleep. It is expected that a
 * corresponding call to @ref mmhal_sleep_init_peripherals was made upon exit of deep sleep. The
 * main purpose is to prevent any unnecessary leakage when in deep sleep.
 */
static void mmhal_sleep_deinit_peripherals(void)
{
    /* UASART1 pins */
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    LL_GPIO_StructInit(&GPIO_InitStruct);

    /* Only TX pin needs to be de-initialized */
    GPIO_InitStruct.Pin = LOG_USART_TX_Pin;
    LL_GPIO_Init(LOG_USART_TX_GPIO_Port, &GPIO_InitStruct);
}

enum mmhal_sleep_state mmhal_sleep_prepare(uint32_t expected_idle_time_ms)
{
    enum mmhal_sleep_state sleep_state;

    if (mmhal_get_deep_sleep_veto() || (expected_idle_time_ms < MIN_DEEP_SLEEP_TIME_MS))
    {
        sleep_state = MMHAL_SLEEP_SHALLOW;
    }
    else if (!LL_USART_IsActiveFlag_TXE(LOG_USART) ||
             !LL_USART_IsActiveFlag_TC(LOG_USART) ||
             LL_DMA_GetBlkDataLength(GPDMA1, LL_DMA_CHANNEL_14) ||
             LL_DMA_GetBlkDataLength(GPDMA1, LL_DMA_CHANNEL_15))
    {
        /* Go to normal sleep if there is any pending UART or
         * SPI DMA data to be transmitted. */
        sleep_state = MMHAL_SLEEP_SHALLOW;
    }
    else
    {
        /* Stop the SysTick */
        SysTick->CTRL &= ~(SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
        HAL_SuspendTick();

        sleep_state = MMHAL_SLEEP_DEEP;
    }

    if (sleep_state != MMHAL_SLEEP_DISABLED)
    {
        /* Enter a critical section but don't use the taskENTER_CRITICAL()
         * method as that will mask interrupts that should exit sleep mode. */
        __disable_irq();
        __DSB();
        __ISB();
    }

    return sleep_state;
}

/**
 * Function to stop the clocks before entering stop 2 and stop 3 modes. This is a workaround
 * described in the STM32 U5 errata (ES0499 - Rev 9, section 2.2.5).
 *
 * > Device hang-up can occur when wakeup request received a few clock cycles before entering
 * > Stop 2 or Stop 3 mode.
 */
static void mmhal_sleep_u5_stop_prepare(void)
{
    LL_RCC_PLL2_Disable();
    LL_RCC_PLL3_Disable();
    LL_RCC_HSI48_Disable();
    LL_RCC_SHSI_Disable();

    while (LL_RCC_PLL2_IsReady())
    {}
    while (LL_RCC_PLL3_IsReady())
    {}
    while (LL_RCC_HSI48_IsReady())
    {}
    while (LL_RCC_SHSI_IsReady())
    {}
}

extern RNG_HandleTypeDef hrng;

uint32_t mmhal_sleep(enum mmhal_sleep_state sleep_state, uint32_t expected_idle_time_ms)
{
    uint32_t elapsed_ms = 0;
    uint32_t elapsed_ticks = 0;
    uint32_t start_time_ticks;

    /* Disable Random Number Generator */
    __HAL_RNG_DISABLE(&hrng);

    if (sleep_state == MMHAL_SLEEP_DEEP)
    {
        mmhal_sleep_deinit_peripherals();

        mmhal_sleep_u5_stop_prepare();

        start_time_ticks = mmhal_sleep_configure_timer(expected_idle_time_ms);
        /* Clear wake-up flags */
        LL_PWR_ClearFlag_WU();
        /* Enable Deep Sleep Mode */
        LL_LPM_EnableDeepSleep();
        /* Enter Stop Mode 2 */
        LL_PWR_SetPowerMode(LL_PWR_STOP2_MODE);

        /* Wait for any interrupts */
        __WFI();

        /* Calculate the time that elapsed whilst we were asleep. */
        if (LL_LPTIM_GetCounter(LPTIM1) < start_time_ticks)
        {
            elapsed_ticks += 0x10000 + LL_LPTIM_GetCounter(LPTIM1) - start_time_ticks;
            elapsed_ms = LPTIM_TICKS_TO_MS(elapsed_ticks);
        }
        else
        {
            elapsed_ticks += LL_LPTIM_GetCounter(LPTIM1) - start_time_ticks;
            elapsed_ms = LPTIM_TICKS_TO_MS(elapsed_ticks);
        }

        /* Configure the system clock after waking up from stop mode. */
        SystemClock_Config();

        if (elapsed_ms > expected_idle_time_ms)
        {
            elapsed_ms = expected_idle_time_ms;
        }

        /* Restart the SysTick */
        SysTick->CTRL |= (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
        HAL_ResumeTick();

        mmhal_sleep_init_peripherals();
    }
    else if (sleep_state == MMHAL_SLEEP_SHALLOW)
    {
        /* Clear wake-up flags */
        LL_PWR_ClearFlag_WU();
        LL_LPM_EnableSleep();

        /* Wait for any interrupts */
        __WFI();
    }

    /* Re-enable Random Number Generator */
    __HAL_RNG_ENABLE(&hrng);

    return elapsed_ms;
}

void mmhal_sleep_cleanup(void)
{
    /* Re-enable interrupts */
    __enable_irq();
}
