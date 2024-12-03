/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Example implementation of the OTA OS Functional Interface for the @ref MMOSAL API.
 */

/* OTA OS POSIX Interface Includes */
#include "ota_os_mmosal.h"

/* MMOSAL includes */
#include "mmosal.h"

/* OTA Library include */
#include "ota.h"
#include "ota_private.h"

#ifndef MAX_MESSAGES
/** OTA Event queue depth */
#define MAX_MESSAGES    20
#endif

/** OTA Event queue size */
#define MAX_MSG_SIZE    sizeof(OtaEventMsg_t)

/** The queue control handle */
static struct mmosal_queue *otaEventQueue;

/** OTA App Timer callback */
static OtaTimerCallback_t otaTimerCallbackPtr;

/** OTA Timer handles */
static struct mmosal_timer *otaTimer[OtaNumOfTimers];

/* OTA Timer callbacks.*/
static void requestTimerCallback(struct mmosal_timer *);
static void selfTestTimerCallback(struct mmosal_timer *);

/** Template for timer callback */
void (*timerCallback[OtaNumOfTimers])(struct mmosal_timer *) =
{
    requestTimerCallback,
    selfTestTimerCallback
};

OtaOsStatus_t OtaInitEvent_MMOSAL(OtaEventContext_t * pEventCtx)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    (void) pEventCtx;

    otaEventQueue = mmosal_queue_create(MAX_MESSAGES, MAX_MSG_SIZE, "OTA_Q");
    if (otaEventQueue == NULL)
    {
        otaOsStatus = OtaOsEventQueueCreateFailed;
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaSendEvent_MMOSAL(OtaEventContext_t * pEventCtx,
                                  const void * pEventMsg,
                                  unsigned int timeout)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    (void) pEventCtx;

    /* Send the event to OTA event queue.*/
    if (!mmosal_queue_push(otaEventQueue, pEventMsg, timeout))
    {
        otaOsStatus = OtaOsEventQueueSendFailed;
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaReceiveEvent_MMOSAL(OtaEventContext_t * pEventCtx,
                                     void * pEventMsg,
                                     uint32_t timeout)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    (void) pEventCtx;

    if (!mmosal_queue_pop(otaEventQueue, pEventMsg, timeout))
    {
        otaOsStatus = OtaOsEventQueueReceiveFailed;
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaDeinitEvent_MMOSAL(OtaEventContext_t * pEventCtx)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    (void) pEventCtx;

    /* Remove the event queue.*/
    if (otaEventQueue != NULL)
    {
        mmosal_queue_delete(otaEventQueue);
    }

    return otaOsStatus;
}

/**
 * Timer callback
 *
 * @param T A reference to the timer that fired
 */
static void selfTestTimerCallback(struct mmosal_timer * T)
{
    (void) T;

    if (otaTimerCallbackPtr != NULL)
    {
        otaTimerCallbackPtr(OtaSelfTestTimer);
    }
}

/**
 * Timer callback
 *
 * @param T A reference to the timer that fired
 */
static void requestTimerCallback(struct mmosal_timer * T)
{
    (void) T;

    if (otaTimerCallbackPtr != NULL)
    {
        otaTimerCallbackPtr(OtaRequestTimer);
    }
}

OtaOsStatus_t OtaStartTimer_MMOSAL(OtaTimerId_t otaTimerId,
                                   const char * const pTimerName,
                                   const uint32_t timeout,
                                   OtaTimerCallback_t callback)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    MMOSAL_ASSERT(callback != NULL);
    MMOSAL_ASSERT(pTimerName != NULL);

    /* Set OTA lib callback. */
    otaTimerCallbackPtr = callback;

    /* If timer is not created.*/
    if (otaTimer[otaTimerId] == NULL)
    {
        /* Create the timer. */
        otaTimer[otaTimerId] = mmosal_timer_create(pTimerName, timeout, false, "OTA_TMR",
                                                   timerCallback[otaTimerId]);

        if (otaTimer[otaTimerId] == NULL)
        {
            otaOsStatus = OtaOsTimerCreateFailed;
        }
        else
        {
            /* Start the timer. */
            if (!mmosal_timer_start(otaTimer[otaTimerId]))
            {
                otaOsStatus = OtaOsTimerStartFailed;
            }
        }
    }
    else
    {
        /* Reset the timer. */
        if (!mmosal_timer_start(otaTimer[otaTimerId]))
        {
            otaOsStatus = OtaOsTimerRestartFailed;
        }
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaStopTimer_MMOSAL(OtaTimerId_t otaTimerId)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    if ((otaTimerId < OtaNumOfTimers) && (otaTimer[ otaTimerId ] != NULL))
    {
        /* Stop the timer. */
        if (!mmosal_timer_stop(otaTimer[otaTimerId]))
        {
            otaOsStatus = OtaOsTimerStopFailed;
        }
    }
    else
    {
        otaOsStatus = OtaOsTimerStopFailed;
    }

    return otaOsStatus;
}

OtaOsStatus_t OtaDeleteTimer_MMOSAL(OtaTimerId_t otaTimerId)
{
    OtaOsStatus_t otaOsStatus = OtaOsSuccess;

    if ((otaTimerId < OtaNumOfTimers) && (otaTimer[ otaTimerId ] != NULL))
    {
        /* Delete the timer. */
        mmosal_timer_delete(otaTimer[otaTimerId]);
    }
    else
    {
        otaOsStatus = OtaOsTimerDeleteFailed;
    }

    return otaOsStatus;
}

void * Malloc_MMOSAL(size_t size)
{
    return mmosal_malloc(size);
}

void Free_MMOSAL(void * ptr)
{
    mmosal_free(ptr);
}
