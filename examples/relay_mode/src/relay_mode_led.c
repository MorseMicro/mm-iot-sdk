/*
 * Copyright 2026 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

#include "relay_mode_led.h"

#include "mmhal_app.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmwlan.h"

/** Duration of each LED on/off phase of the slow blue flash shown while connecting. */
#define CONNECTING_BLINK_PERIOD_MS 1000

/** Duration of each quick green pulse, used to indicate relay depth. */
#define DEPTH_PULSE_MS 150

/** Pause after the last pulse of a depth count, before the count repeats. */
#define DEPTH_PULSE_PAUSE_MS 4000

/** How often @c led_task_main() checks state while a solid LED is shown. */
#define LED_TASK_IDLE_POLL_MS 200

/** High level LED state */
enum relay_led_mode
{
    /** Blue slow flash. */
    RELAY_LED_MODE_CONNECTING,
    /** Solid blue (depth unknown), solid green (root, depth 0), or green pulses (depth > 0). */
    RELAY_LED_MODE_CONNECTED,
    /** Solid red. */
    RELAY_LED_MODE_FAILED,
};

static struct mmosal_task *led_task;

/** Current high level state */
static enum relay_led_mode led_mode = RELAY_LED_MODE_CONNECTING;

/** Current relay depth */
static uint8_t led_depth = MMWLAN_RELAY_DEPTH_UNKNOWN;

/**
 * Main loop for the LED task.
 * Solid LED states are applied immediately by @c relay_led_set_*() methods below
 */
static void led_task_main(void *arg)
{
    MM_UNUSED(arg);

    while (true)
    {
        /* Clear LED for ease of state management */
        mmhal_set_led(LED_RED, LED_OFF);
        mmhal_set_led(LED_GREEN, LED_OFF);
        mmhal_set_led(LED_BLUE, LED_OFF);

        switch (led_mode)
        {
            case RELAY_LED_MODE_CONNECTING:
                mmhal_set_led(LED_BLUE, LED_ON);
                mmosal_task_sleep(CONNECTING_BLINK_PERIOD_MS);
                mmhal_set_led(LED_BLUE, LED_OFF);
                mmosal_task_sleep(CONNECTING_BLINK_PERIOD_MS);
                break;

            case RELAY_LED_MODE_CONNECTED:
                if (led_depth == MMWLAN_RELAY_DEPTH_UNKNOWN)
                {
                    if (mmwlan_get_sta_state() != MMWLAN_STA_CONNECTED)
                    {
                        relay_led_set_connecting();
                        break;
                    }
                    mmhal_set_led(LED_BLUE, LED_ON);
                    mmosal_task_sleep(LED_TASK_IDLE_POLL_MS);
                }
                else if (led_depth == 0)
                {
                    mmhal_set_led(LED_GREEN, LED_ON);
                    mmosal_task_sleep(LED_TASK_IDLE_POLL_MS);
                }
                else
                {
                    uint8_t depth = led_depth;

                    for (size_t i = 0; i < depth && led_mode == RELAY_LED_MODE_CONNECTED; i++)
                    {
                        mmhal_set_led(LED_GREEN, LED_ON);
                        mmosal_task_sleep(DEPTH_PULSE_MS);
                        mmhal_set_led(LED_GREEN, LED_OFF);
                        mmosal_task_sleep(DEPTH_PULSE_MS);
                    }
                    mmosal_task_sleep(DEPTH_PULSE_PAUSE_MS);
                }
                break;

            case RELAY_LED_MODE_FAILED:
            default:
                mmhal_set_led(LED_RED, LED_ON);
                mmosal_task_sleep(LED_TASK_IDLE_POLL_MS);
                break;
        }
    }
}

void relay_led_init(void)
{
    MMOSAL_ASSERT(led_task == NULL);
    led_task = mmosal_task_create(led_task_main, NULL, MMOSAL_TASK_PRI_LOW, 128, "relay_led");
    MMOSAL_ASSERT(led_task != NULL);
}

void relay_led_set_connecting(void)
{
    led_mode = RELAY_LED_MODE_CONNECTING;
}

void relay_led_set_connected(void)
{
    led_mode = RELAY_LED_MODE_CONNECTED;
}

void relay_led_set_failed(void)
{
    led_mode = RELAY_LED_MODE_FAILED;
}

void relay_led_set_depth(uint8_t depth)
{
    led_depth = depth;
    relay_led_set_connected();
}
