/*
 * Copyright 2021-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>

#include "mmekh05_hal_common.h"
#include "mmconfig.h"
#include "mmwlan.h"
#include "mmutils.h"


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
#define MS_TO_LPTIM_TICKS(x) (((x)*LPTIM_TICKS_PER_SECOND)/1000)

/** Macro to transform LPTIM ticks into milliseconds.  */
#define LPTIM_TICKS_TO_MS(x) (((x)*1000)/LPTIM_TICKS_PER_SECOND)

static volatile atomic_uint_fast32_t deep_sleep_vetos = 0;

const uint32_t mmhal_system_clock = 160000000;
static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

void SystemClock_Config(void);

void mmhal_early_init(void)
{
    mmosal_disable_interrupts();
}

void mmhal_init(void)
{
    mmosal_enable_interrupts();
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
 * @warning A @c LL_LPUART_IsActiveFlag_TXE_TXFNF(LOG_USART) call must be made between consecutive
 *          calls of this function.
 */
static void mmhal_log_print_char(struct log_buf *buf)
{
    /* If we encounter a newline insert a carriage return first to maximize compatibility */
    if (buf->buf[buf->rd_idx] == '\n' && !buf->newline_converted)
    {
        buf->newline_converted = true;
        LL_LPUART_TransmitData8(LOG_USART, '\r');
        return;
    }
    buf->newline_converted = false;

    LL_LPUART_TransmitData8(LOG_USART, buf->buf[buf->rd_idx]);
    buf->rd_idx++;
    if (buf->rd_idx >= LOG_BUF_SIZE)
    {
        buf->rd_idx = 0;
    }
}

void LOG_USART_IRQ_HANDLER(void)
{
    /* Keep pumping data into the UART until it is not empty or we have no data left. */
    while (LL_LPUART_IsActiveFlag_TXE_TXFNF(LOG_USART))
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
        LL_LPUART_DisableIT_TXE_TXFNF(LOG_USART);
    }
}

static void mmhal_log_flush_uart(void)
{
    /* We don't know whether interrupts will be enabled or not, so let's just disable and do
     * this the manual way. */
    LL_LPUART_DisableIT_TXE_TXFNF(LOG_USART);
    while (log_buf.wr_idx != log_buf.rd_idx)
    {
        while (!LL_LPUART_IsActiveFlag_TXE_TXFNF(LOG_USART))
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
        LL_LPUART_EnableIT_TXE_TXFNF(LOG_USART);
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

void mmhal_read_mac_addr(uint8_t *mac_addr)
{
    /*
     * MAC address is determined by the following order of precedence:
     * 1. The value returned in mac_addr by this function (if non-zero)
     *    a. If MMHAL_MAC_ADDR_OVERRIDE_ENABLED is defined then mac_addr will be set to
     *       a unique number derived from the chip unique ID.
     *    b. If wlan.macaddr is set in persistent store, then mac_addr will be set to this value.
     * 2. If this function returns an all zero mac_addr (as neither setting above was found), then:
     *    a. If the tranceiver has a MAC address, then that is used by the firmware.
     *    b. Otherwise a randomly generated MAC address is used by the firmware.
     */

#ifdef MMHAL_MAC_ADDR_OVERRIDE_ENABLED
    /* Shorten the UID */
    uint32_t uid = LL_GetUID_Word0() ^ LL_GetUID_Word1() ^ LL_GetUID_Word2();

    /* Set MAC address as locally administered */
    mac_addr[0] = 0x02;
    mac_addr[1] = 0x00;
    memcpy(&mac_addr[2], &uid, sizeof(uid));
#else
    /* Load MAC Address */
    char strval[32];
    if (mmconfig_read_string("wlan.macaddr", strval, sizeof(strval)) > 0)
    {
        /* Need to provide an array of ints to sscanf otherwise it will overflow */
        int temp[MMWLAN_MAC_ADDR_LEN];
        int i;

        int ret = sscanf(strval, "%x:%x:%x:%x:%x:%x",
                         &temp[0], &temp[1], &temp[2],
                         &temp[3], &temp[4], &temp[5]);
        if (ret == MMWLAN_MAC_ADDR_LEN)
        {
            for (i = 0; i < MMWLAN_MAC_ADDR_LEN; i++)
            {
                if (temp[i] > UINT8_MAX || temp[i] < 0)
                {
                    /* Invalid value, ignore and reset to default */
                    printf("Invalid MAC address found in [wlan.macaddr], rejecting!\n");
                    memset(mac_addr, 0, MMWLAN_MAC_ADDR_LEN);
                    break;
                }

                mac_addr[i] = (uint8_t)temp[i];
            }
        }
    }
#endif
}

extern RNG_HandleTypeDef hrng;
uint32_t mmhal_random_u32(uint32_t min, uint32_t max)
{
#define RNG_MAX_GENERATE_ATTEMPTS   (100)
    uint32_t rndm;
    uint32_t ii;
    HAL_StatusTypeDef status = HAL_ERROR;
    for (ii = 0; ii < RNG_MAX_GENERATE_ATTEMPTS; ii++)
    {
        status = HAL_RNG_GenerateRandomNumber(&hrng, &rndm);
        if (status == HAL_OK)
        {
            break;
        }
        else
        {
            printf("%lu HAL_RNG_GenerateRandomNumber failed with status %u; retrying...\n",
                   ii, status);
        }
    }
    MMOSAL_ASSERT(status == HAL_OK);

    /* Caution: this does not guarantee uniformly distributed random numbers. */
    if (min == 0 && max == UINT32_MAX)
    {
        return rndm;
    }
    else
    {
        return rndm % (max-min+1) + min;
    }
}

void mmhal_set_led(uint8_t led, uint8_t level)
{
    switch (led)
    {
    case LED_RED:
        if (level)
        {
            LL_GPIO_SetOutputPin(GPIO_LED_RED_GPIO_Port, GPIO_LED_RED_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(GPIO_LED_RED_GPIO_Port, GPIO_LED_RED_Pin);
        }
        break;

    case LED_GREEN:
        if (level)
        {
            LL_GPIO_SetOutputPin(GPIO_LED_GREEN_GPIO_Port, GPIO_LED_GREEN_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(GPIO_LED_GREEN_GPIO_Port, GPIO_LED_GREEN_Pin);
        }
        break;

    case LED_BLUE:
        if (level)
        {
            LL_GPIO_SetOutputPin(GPIO_LED_BLUE_GPIO_Port, GPIO_LED_BLUE_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(GPIO_LED_BLUE_GPIO_Port, GPIO_LED_BLUE_Pin);
        }
        break;

    default:
        break;
    }
}

void mmhal_set_error_led(bool state)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);

    GPIO_InitStruct.Pin = GPIO_LED_BLUE_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIO_LED_BLUE_GPIO_Port, &GPIO_InitStruct);

    if (state)
    {
        LL_GPIO_SetOutputPin(GPIO_LED_BLUE_GPIO_Port, GPIO_LED_BLUE_Pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(GPIO_LED_BLUE_GPIO_Port, GPIO_LED_BLUE_Pin);
    }
}

enum mmhal_button_state mmhal_get_button(enum mmhal_button_id button_id)
{
    /* Not implemented on this platform */
    MM_UNUSED(button_id);
    return BUTTON_RELEASED;
}

bool mmhal_set_button_callback(enum mmhal_button_id button_id,
                               mmhal_button_state_cb_t button_state_cb)
{
    if (button_id != BUTTON_ID_USER0)
    {
        return false;
    }

    mmhal_button_state_cb = button_state_cb;
    return true;
}

mmhal_button_state_cb_t mmhal_get_button_callback(enum mmhal_button_id button_id)
{
    if (button_id != BUTTON_ID_USER0)
    {
        return NULL;
    }

    return mmhal_button_state_cb;
}

time_t mmhal_get_time()
{
    struct tm t;

    /* Time should be read before date, this locks DR till it is read */
    uint32_t time = LL_RTC_TIME_Get(RTC);
    uint32_t date = LL_RTC_DATE_Get(RTC);

    t.tm_sec = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(time));
    t.tm_min = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(time));
    t.tm_hour = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_HOUR(time));

    t.tm_mday = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(date));
    t.tm_mon = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(date));
    t.tm_year = __LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(date)) + 100;

    /* ignored */
    t.tm_yday = 0;
    t.tm_isdst = 0;
    t.tm_yday = 0;

    /* Now convert to epoch and return */
    return mktime(&t);
}

void mmhal_set_time(time_t epoch)
{
    struct tm *t;
    struct tm result;

    LL_RTC_TimeTypeDef RTC_TimeStruct = {0};
    LL_RTC_DateTypeDef RTC_DateStruct = {0};

    /* Convert to broken-down time format */
    t = gmtime_r(&epoch, &result);

    /* Set the Time and Date */
    RTC_TimeStruct.Hours = t->tm_hour;
    RTC_TimeStruct.Minutes = t->tm_min;
    RTC_TimeStruct.Seconds = t->tm_sec;
    LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_TimeStruct);

    RTC_DateStruct.WeekDay = t->tm_wday;
    RTC_DateStruct.Month = t->tm_mon;
    RTC_DateStruct.Day = t->tm_mday;
    RTC_DateStruct.Year = t->tm_year - 100;
    LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_DateStruct);
}

void mmhal_reset(void)
{
    HAL_NVIC_SystemReset();
    while (1)
    { }
}

bool mmhal_get_hardware_version(char * version_buffer, size_t version_buffer_length)
{
    /* Note: You need to identify the correct hardware and or version
     *       here using whatever means available (GPIO's, version number stored in EEPROM, etc)
     *       and return the correct string here. */
    return !mmosal_safer_strcpy(version_buffer, "MM-MM6108-EKH05-SDIO V3.0", version_buffer_length);
}

void LPTIM1_IRQHandler(void)
{
    /* Check whether Compare match 1 interrupt is pending */
    if (LL_LPTIM_IsActiveFlag_CC1(LPTIM1))
    {
        LL_LPTIM_ClearFLAG_CC1(LPTIM1);
        NVIC_ClearPendingIRQ(LPTIM1_IRQn);
    }
}

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    MMOSAL_ASSERT(veto_id < 32);
    atomic_fetch_or(&deep_sleep_vetos, 1ul << veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    MMOSAL_ASSERT(veto_id < 32);
    atomic_fetch_and(&deep_sleep_vetos, ~(1ul << veto_id));
}

/**
 * Function to configure the timer to wake us up after the desired sleep period.
 *
 * @param sleep_time_ms Time in milliseconds that we wish to sleep for.
 *
 * @note If @c sleep_time_ms results in a tick count greater than or equal to
 *       @ref MAX_POSSIBLE_SUPPRESSED_TICKS the timer will be configured to elapse
 *       after @ref MAX_POSSIBLE_SUPPRESSED_TICKS
 *
 * @return The starting tick count from the timer when the compare register is set.
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

    if (deep_sleep_vetos || (expected_idle_time_ms < MIN_DEEP_SLEEP_TIME_MS))
    {
        sleep_state = MMHAL_SLEEP_SHALLOW;
    }
    else if (!LL_LPUART_IsActiveFlag_TXE(LOG_USART) ||
             !LL_LPUART_IsActiveFlag_TC(LOG_USART) ||
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

void mmhal_set_debug_pins(uint32_t mask, uint32_t values)
{
    if (mask & MMHAL_DEBUG_PIN(0))
    {
        if (values & MMHAL_DEBUG_PIN(0))
        {
            LL_GPIO_SetOutputPin(MM_DEBUG_0_GPIO_Port, MM_DEBUG_0_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(MM_DEBUG_0_GPIO_Port, MM_DEBUG_0_Pin);
        }
    }

    if (mask & MMHAL_DEBUG_PIN(1))
    {
        if (values & MMHAL_DEBUG_PIN(1))
        {
            LL_GPIO_SetOutputPin(MM_DEBUG_1_GPIO_Port, MM_DEBUG_1_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(MM_DEBUG_1_GPIO_Port, MM_DEBUG_1_Pin);
        }
    }
}

const struct mmhal_flash_partition_config* mmhal_get_mmconfig_partition(void)
{
    /** Start of MMCONFIG region in flash. */
    extern uint8_t mmconfig_start;

    /** End of MMCONFIG region in flash. */
    extern uint8_t mmconfig_end;

    static struct mmhal_flash_partition_config mmconfig_partition =
        MMHAL_FLASH_PARTITION_CONFIG_DEFAULT;

    mmconfig_partition.partition_start = (uint32_t) &mmconfig_start;
    mmconfig_partition.partition_size = (uint32_t) (&mmconfig_end - &mmconfig_start);
    mmconfig_partition.not_memory_mapped = false;

    return &mmconfig_partition;
}
