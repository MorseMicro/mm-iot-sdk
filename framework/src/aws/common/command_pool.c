/*
 * FreeRTOS V202104.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Copyright 2023 Morse Micro
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file
 * This file implements a simple message queue API.
 * This allows any thread to post a command to the message queue to be executed
 * in the context of a message processing thread. The message processing thread
 * retrieves the queued commands and executes them.
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>

/* Header include. */
#include "command_pool.h"

/* Morse includes */
#include "mmosal.h"

/**
 * The pool of command structures used to hold information on commands (such
 * as PUBLISH or SUBSCRIBE) between the command being created by an API call and
 * completion of the command by the execution of the command's callback.
 */
static MQTTAgentCommand_t commandStructurePool[ MQTT_COMMAND_CONTEXTS_POOL_SIZE ];

static struct mmosal_queue * xCommandPoolQueue = NULL;

/**
 * Initializes the command queue
 */
void Agent_InitializePool(void)
{
    if (xCommandPoolQueue == NULL)
    {
        xCommandPoolQueue = mmosal_queue_create(MQTT_COMMAND_CONTEXTS_POOL_SIZE,
                                         sizeof(MQTTAgentCommand_t *), "MQTT_COMMAND_POOL");

        /* Populate the queue with pointers to each command structure. */
        for (uint32_t ulIdx = 0; ulIdx < MQTT_COMMAND_CONTEXTS_POOL_SIZE; ulIdx++)
        {
            MQTTAgentCommand_t * pCommand = &commandStructurePool[ ulIdx ];

            (void) mmosal_queue_push(xCommandPoolQueue, &pCommand, 0U);
        }
    }
}

/**
 * Retrieves next command to execute from the queue
 * @param ulBlockTimeMs Milliseconds to wait before timing out
 * @return Returns a pointer to the command to execute or NULL if timed out
 */
MQTTAgentCommand_t * Agent_GetCommand(uint32_t ulBlockTimeMs)
{
    MQTTAgentCommand_t * pxCommandStruct = NULL;

    if (xCommandPoolQueue)
    {
        if (!mmosal_queue_pop(xCommandPoolQueue, &pxCommandStruct, ulBlockTimeMs))
        {
            printf("ERR:No command structure available.\n");
        }
    }
    else
    {
        printf("ERR:Command pool not initialized.\n");
    }

    return pxCommandStruct;
}

/**
 * Posts the command to execute to the queue
 * @param pCommandToRelease Command to post in the queue
 * @return Returns true if successfully posted in the queue
 */
bool Agent_ReleaseCommand(MQTTAgentCommand_t * pCommandToRelease)
{
    bool bStructReturned = false;

    if (!xCommandPoolQueue)
    {
        printf("ERR:Command pool not initialized.\n");
    }
    /* See if the structure being returned is actually from the pool. */
    else if ((pCommandToRelease < commandStructurePool) ||
             (pCommandToRelease > (commandStructurePool + MQTT_COMMAND_CONTEXTS_POOL_SIZE)))
    {
        printf("ERR:Provided pointer: %p does not belong to the command pool.\n",
               pCommandToRelease);
    }
    else
    {
        bStructReturned = mmosal_queue_push(xCommandPoolQueue, &pCommandToRelease, 0U);
    }

    return bStructReturned;
}
