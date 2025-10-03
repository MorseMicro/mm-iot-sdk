/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "mmhal.h"

void vApplicationMallocFailedHook(void)
{
    /* This hook function that will be called if configUSE_MALLOC_FAILED_HOOK is set to 1 and
     * a call to pvPortMalloc() fails. */
    configABORT();
}

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /* This hook function will be called if configCHECK_FOR_STACK_OVERFLOW is set to 1 or 2
     * and a stack overflow is detected. */
    configABORT();
}

void vApplicationIdleHook(void)
{
    /* This hook function will be called on each iteration of the idle task if
     * configUSE_IDLE_HOOK is set to 1. */
}

void vApplicationTickHook(void)
{
    /* This hook function will be called on each tick interrupt (from interrupt context)
     * if configUSE_TICK_HOOK is set to 1. */
}

#if (configUSE_TICKLESS_IDLE == 1)

/**
 * Custom implementation of the FreeRTOS @c vPortSuppressTicksAndSleep function. This is used for
 * Tickless Idle. This implementation allows us to halt the systick and enter ultra low power modes.
 *
 * @param expected_idle_time_ms Expected time to sleep in milliseconds.
 *
 * @retval None
 */
void vPortSuppressTicksAndSleep(uint32_t expected_idle_time_ms)
{
    enum mmhal_sleep_state sleep_state;
    uint32_t elapsed_ms = 0;

    while (elapsed_ms < expected_idle_time_ms)
    {
        sleep_state = mmhal_sleep_prepare(expected_idle_time_ms - elapsed_ms);
        if (sleep_state == MMHAL_SLEEP_DISABLED)
        {
            return;
        }

        /* If a context switch is pending or a task is waiting for the scheduler
         * to be unsuspended then abandon the low power entry. */
        if (eTaskConfirmSleepModeStatus() == eAbortSleep)
        {
            mmhal_sleep_abort(sleep_state);
            return;
        }

        uint32_t slept_ms = mmhal_sleep(sleep_state, expected_idle_time_ms - elapsed_ms);
        elapsed_ms += slept_ms;

        if (sleep_state == MMHAL_SLEEP_DEEP)
        {
            /* Step the FreeRTOS tick to account for any tick periods that elapsed. */
            vTaskStepTick(slept_ms);
        }

        /* If a context switch is pending or a task is waiting for the scheduler
         * to be unsuspended then abandon the low power entry. */
        if (eTaskConfirmSleepModeStatus() == eAbortSleep)
        {
            mmhal_sleep_abort(sleep_state);
            return;
        }

        mmhal_sleep_cleanup();
    }
}

#endif
