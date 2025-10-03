/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdatomic.h>
#include "mmhal.h"
#include "mmosal.h"
#include "main.h"
#include "mmconfig.h"
#include "mmwlan.h"
#include "mm_hal_common.h"
#include "mmutils.h"

const uint32_t mmhal_system_clock = 480000000;
static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

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
    UNUSED(data);
    UNUSED(length);
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
    UNUSED(data);
    UNUSED(length);
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

        int ret = sscanf(strval, "%x:%x:%x:%x:%x:%x",
                         &temp[0], &temp[1], &temp[2],
                         &temp[3], &temp[4], &temp[5]);
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
            LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
        }
        break;

    case LED_GREEN:
        if (level)
        {
            LL_GPIO_SetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
        }
        break;

    case LED_BLUE:
        /* The h753 doesn't have a blue led. For hal completeness, yellow is treated as blue. */
        if (level)
        {
            LL_GPIO_SetOutputPin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin);
        }
        else
        {
            LL_GPIO_ResetOutputPin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin);
        }
        break;

    default:
        break;
    }
}

void mmhal_set_error_led(bool state)
{
    if (state)
    {
        LL_GPIO_SetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
    else
    {
        LL_GPIO_ResetOutputPin(LED_RED_GPIO_Port, LED_RED_Pin);
    }
}

enum mmhal_button_state mmhal_get_button(enum mmhal_button_id button_id)
{
    /* Not implemented on this platform */
    UNUSED(button_id);
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

void mmhal_set_deep_sleep_veto(uint8_t veto_id)
{
    UNUSED(veto_id);
}

void mmhal_clear_deep_sleep_veto(uint8_t veto_id)
{
    UNUSED(veto_id);
}


void mmhal_sleep_abort(enum mmhal_sleep_state sleep_state)
{
    UNUSED(sleep_state);
}

enum mmhal_sleep_state mmhal_sleep_prepare(uint32_t expected_idle_time_ms)
{
    UNUSED(expected_idle_time_ms);
    return MMHAL_SLEEP_DISABLED;
}

uint32_t mmhal_sleep(enum mmhal_sleep_state sleep_state, uint32_t expected_idle_time_ms)
{
    UNUSED(sleep_state);
    UNUSED(expected_idle_time_ms);

    return 0;
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

bool mmhal_get_hardware_version(char * version_buffer, size_t version_buffer_length)
{
    /* Note: You need to identify the correct hardware and or version
     *       here using whatever means available (GPIO's, version number stored in EEPROM, etc)
     *       and return the correct string here. */
    return !mmosal_safer_strcpy(version_buffer, MMHAL_HARDWARE_VERSION, version_buffer_length);
}
