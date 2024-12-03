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

#include "bsp.h"
#include "stm32f4xx_ll_utils.h"

const uint32_t mmhal_system_clock = 180000000;
static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

void mmhal_early_init(void)
{
    mmosal_disable_interrupts();
}

void mmhal_init(void)
{
    HAL_Init();

    bsp_clock_init();
    bsp_peripherals_init();

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
 * @warning A @c LL_USART_IsActiveFlag_TXE(USART3) call must be made between consecutive
 *          calls of this function.
 */
static void mmhal_log_print_char(struct log_buf *buf)
{
    /* If we encounter a newline insert a carriage return first to maximize compatibility */
    if (buf->buf[buf->rd_idx] == '\n' && !buf->newline_converted)
    {
        buf->newline_converted = true;
        LL_USART_TransmitData8(USART3, '\r');
        return;
    }
    buf->newline_converted = false;

    LL_USART_TransmitData8(USART3, buf->buf[buf->rd_idx]);
    buf->rd_idx++;
    if (buf->rd_idx >= LOG_BUF_SIZE)
    {
        buf->rd_idx = 0;
    }
}

void USART3_IRQHandler(void)
{
    /* Keep pumping data into the UART until it is not empty or we have no data left. */
    while (LL_USART_IsActiveFlag_TXE(USART3))
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
        LL_USART_DisableIT_TXE(USART3);
    }
}

void mmhal_log_write(const uint8_t *data, size_t length)
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
        LL_USART_EnableIT_TXE(USART3);
        NVIC_EnableIRQ(USART3_IRQn);
    }
}

void mmhal_log_flush(void)
{
    /* We don't know whether interrupts will be enabled or not, so let's just disable and do
     * this the manual way. */
    LL_USART_DisableIT_TXE(USART3);
    while (log_buf.wr_idx != log_buf.rd_idx)
    {
        while (!LL_USART_IsActiveFlag_TXE(USART3))
        { }

        mmhal_log_print_char(&log_buf);
    }
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
    static uint32_t prev = 0;
    static bool prev_valid = false;
    uint32_t err_cnt = 0;
    uint32_t rnd;
    uint32_t sr;
    bool rnd_valid;

    /* Put a limit on the number of attempts so we can take more drastic action if necessary. */
    for (err_cnt = 0; err_cnt < 1000; err_cnt++)
    {
        /* Wait until valid random data is available or an error is detected. */
        while (!(RNG->SR & (RNG_SR_DRDY | RNG_SR_CEIS | RNG_SR_SEIS)))
        {}

        /* Check there is no clock error preventing RNG */
        MMOSAL_ASSERT(!LL_RNG_IsActiveFlag_CEIS(RNG));

        /* Per RM0090 Rev 19, Section 24.3.2, if we encounter a seed error we should
         * clear the SEIS bit, then clear and set RNGEN to reinitialise and restart
         * the RNG. */
        if (LL_RNG_IsActiveFlag_SEIS(RNG))
        {
            /* Disable interrupts whilst we handle the seed error */
            mmosal_disable_interrupts();
            LL_RNG_ClearFlag_SEIS(RNG);
            LL_RNG_Disable(RNG);
            LL_RNG_Enable(RNG);
            prev_valid = false;
            mmosal_enable_interrupts();
            continue;
        }

        err_cnt = 0;

        /* Disable interrupts and double check the data ready bit at the same time we read the
         * random number to make sure that another thread doesn't get in first. */
        mmosal_disable_interrupts();
        sr = RNG->SR;
        rnd = LL_RNG_ReadRandData32(RNG);
        mmosal_enable_interrupts();

        if ((sr & RNG_SR_DRDY) == 0)
        {
            continue;
        }

        /*
         * Per RM0090 Rev 19, Section 24.3.1:
         * > As required by the FIPS PUB (Federal Information Processing Standard Publication)
         * > 140-2, the first random number generated after setting the RNGEN bit should not be
         * > used, but saved for comparison with the next generated random number. Each subsequent
         * > generated random number has to be compared with the previously generated number. The
         * > test fails if any two compared numbers are equal (continuous random number generator
         * > test).
         */
        rnd_valid = prev_valid && rnd != prev;

        prev = rnd;
        prev_valid = true;

        if (!rnd_valid)
        {
            continue;
        }

        if (min != 0 || max != UINT32_MAX)
        {
            /* Warning: this does not guarantee uniformly distributed random numbers */
            rnd = rnd % (max-min+1) + min;
        }
        return rnd;
    }

    /* If we got here then we weren't able to generate a valid random number in a reasonable
     * number of attempts. */
    MMOSAL_ASSERT(false);

    /* Keep compiler happy */
    return 0;
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

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

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
    return !mmosal_safer_strcpy(version_buffer, "MM-EKH08-F429 V1.0", version_buffer_length);
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
