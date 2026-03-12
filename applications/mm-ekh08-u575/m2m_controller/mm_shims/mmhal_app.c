/*
 * Copyright 2021-2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmhal_app.h"
#include "mm_hal_common.h"
#include "mmosal.h"
#include "mmutils.h"

#include "main.h"

static mmhal_button_state_cb_t mmhal_button_state_cb = NULL;

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

time_t mmhal_get_time(void)
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
