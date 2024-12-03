/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 *
 * Function prototypes derived from ota_os_interface.h (MIT license).
 */

/**
 * @file
 * @brief Function declarations for the example OTA OS Functional interface for MMOSAL.
 */

#pragma once

/* Standard library include. */
#include <stdint.h>
#include <string.h>

/* OTA library interface include. */
#include "ota_os_interface.h"

/**
 * Initialize the OTA events.
 * This function initializes the OTA events mechanism for Morse Micro platforms.
 * @param[pEventCtx]     Pointer to the OTA event context.
 * @return               @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaInitEvent_MMOSAL(OtaEventContext_t *pEventCtx);

/**
 * Sends an OTA event.
 *
 * This function sends an event to OTA library event handler on Morse Micro platforms.
 *
 * @param[pEventCtx]     Pointer to the OTA event context.
 * @param[pEventMsg]     Event to be sent to the OTA handler.
 * @param[timeout]       The maximum amount of time (milliseconds) the task should block.
 * @return               @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaSendEvent_MMOSAL(OtaEventContext_t *pEventCtx,
                                  const void *pEventMsg,
                                  unsigned int timeout);

/**
 * Receive an OTA event.
 *
 * This function receives next event from the pending OTA events on Morse Micro platforms.
 *
 * @param[pEventCtx]     Pointer to the OTA event context.
 * @param[pEventMsg]     Pointer to store message.
 * @param[timeout]       The maximum amount of time the task should block.
 * @return               @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaReceiveEvent_MMOSAL(OtaEventContext_t *pEventCtx,
                                     void *pEventMsg,
                                     uint32_t timeout);

/**
 * @brief Deinitialize the OTA Events mechanism.
 *
 * This function deinitialize the OTA events mechanism and frees any resources
 * used on Morse Micro platforms.
 *
 * @param[pEventCtx]     Pointer to the OTA event context.
 * @return               @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaDeinitEvent_MMOSAL(OtaEventContext_t *pEventCtx);


/**
 * Start timer.
 *
 * This function starts the timer or resets it if it is already started on Morse Micro platforms.
 *
 * @param[otaTimerId]       Timer ID of type @c otaTimerId_t.
 * @param[pTimerName]       Timer name.
 * @param[timeout]          Timeout for the timer.
 * @param[callback]         Callback to be called when timer expires.
 * @return                  @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaStartTimer_MMOSAL(OtaTimerId_t otaTimerId,
                                   const char * const pTimerName,
                                   const uint32_t timeout,
                                   OtaTimerCallback_t callback);

/**
 * Stop timer.
 *
 * This function stops the timer on Morse Micro platforms.
 *
 * @param[otaTimerId]     Timer ID of type @c otaTimerId_t.
 * @return                @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaStopTimer_MMOSAL(OtaTimerId_t otaTimerId);

/**
 * Delete a timer.
 *
 * This function deletes a timer for Morse Micro platforms.
 *
 * @param[otaTimerId]       Timer ID of type @c otaTimerId_t.
 * @return                  @c OtaOsSuccess if success , other error code on failure.
 */
OtaOsStatus_t OtaDeleteTimer_MMOSAL(OtaTimerId_t otaTimerId);

/**
 * Allocate memory.
 *
 * This function allocates the requested memory and returns a pointer to it on Morse Micro platforms.
 *
 * @param[size]        This is the size of the memory block, in bytes..
 * @return             This function returns a pointer to the allocated memory, or NULL if
 *                     the request fails.
 */

void* Malloc_MMOSAL(size_t size);

/**
 * Free memory.
 *
 * This function de-allocates the memory previously allocated by a call to allocation
 * function of type @c OtaMalloc_t on Morse Micro platforms.
 *
 * @param[ptr]         This is the pointer to a memory block previously allocated with function
 *                     of type @c OtaMalloc_t. If a null pointer is passed as an argument, no action occurs.
 */

void Free_MMOSAL(void *ptr);
