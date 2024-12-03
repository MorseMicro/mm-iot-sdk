/*
 * Copyright 2021-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal.h"
#include "mmosal.h"
#include "mmconfig.h"
#include "mmwlan.h"
#include "mmutils.h"

#include "main.h"
#include "stm32wbxx_hal_flash.h"

const uint32_t mmhal_system_clock = 64000000;
static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

void mmhal_early_init(void)
{
    mmosal_disable_interrupts();

    /* This prevents the CPU2 (M0+) to disable the HSI48 oscillator, needed for RNG module */
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_CLK48_CONFIG_SEMID))
    {}
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
 * @warning A @c LL_USART_IsActiveFlag_TXE(LOG_USART) call must be made between consecutive
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
    while (LL_USART_IsActiveFlag_TXE(LOG_USART))
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
        LL_USART_DisableIT_TXE(LOG_USART);
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
            log_buf.wr_idx -= LOG_BUF_SIZE;
        }
        data += copy_len;
        length -= copy_len;

        /* Enable TX empty interrupt. */
        LL_USART_EnableIT_TXE(LOG_USART);
        NVIC_EnableIRQ(LOG_USART_IRQ);
    }
}

static void mmhal_log_flush_uart(void)
{
    /* We don't know whether interrupts will be enabled or not, so let's just disable and do
     * this the manual way. */
    LL_USART_DisableIT_TXE(LOG_USART);
    while (log_buf.wr_idx != log_buf.rd_idx)
    {
        while (!LL_USART_IsActiveFlag_TXE(LOG_USART))
        { }

        mmhal_log_print_char(&log_buf);
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

uint32_t mmhal_random_u32(uint32_t min, uint32_t max)
{
    /* Attempt to obtain the RNG Semaphore */
    while (LL_HSEM_1StepLock(HSEM, CFG_HW_RNG_SEMID))
    {}

    LL_RCC_SetRNGClockSource(LL_RCC_RNG_CLKSOURCE_CLK48);
    LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);

    /* Peripheral clock enable */
    LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_RNG);

    LL_RNG_Enable(RNG);

    /* Wait until valid random data is available */
    while (!LL_RNG_IsActiveFlag_DRDY(RNG))
    {
        /* Check if error occurs */
        if (   (LL_RNG_IsActiveFlag_CECS(RNG) )
            || (LL_RNG_IsActiveFlag_SECS(RNG))  )
        {
            LL_RNG_ClearFlag_CEIS(RNG);
            // MMOSAL_ASSERT(false);
        }
    }

    uint32_t rand = LL_RNG_ReadRandData32(RNG);

    LL_RNG_Disable(RNG);

    /* Release RNG Semaphore */
    LL_HSEM_ReleaseLock(HSEM, CFG_HW_RNG_SEMID, 0);

    if (min != 0 || max != UINT32_MAX)
    {
        /* Warning: this does not guarantee uniformly distributed random numbers */
        rand = rand % (max-min) + min;
    }

    return rand;
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
    NVIC_SystemReset();
    while (1)
    { }
}

bool mmhal_get_hardware_version(char * version_buffer, size_t version_buffer_length)
{
    /* Note: You need to identify the correct hardware and or version
     *       here using whatever means available (GPIO's, version number stored in EEPROM, etc)
     *       and return the correct string here. */
    return !mmosal_safer_strcpy(version_buffer, "MM-EKH08-WB55-BLE V1.0", version_buffer_length);
}

/*
 * ---------------------------------------------------------------------------------------------
 *                                      HAL Sleep Functions
 * ---------------------------------------------------------------------------------------------
 */

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    MM_UNUSED(veto_id);
}

void mmhal_set_debug_pins(uint32_t mask, uint32_t values)
{
    /* Not implemented for this platform. */
    MM_UNUSED(mask);
    MM_UNUSED(values);
}

/*
 * ---------------------------------------------------------------------------------------------
 *                                    LittleFS Stubs
 * ---------------------------------------------------------------------------------------------
 */
const struct lfs_config* mmhal_get_littlefs_config(void)
{
    return NULL;
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
