/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmagic_core_utils.h"

enum mmagic_status mmagic_mmipal_status_to_mmagic_status(enum mmipal_status status)
{
    switch (status)
    {
    case MMIPAL_SUCCESS:
        return MMAGIC_STATUS_OK;

    case MMIPAL_INVALID_ARGUMENT:
        return MMAGIC_STATUS_INVALID_ARG;

    case MMIPAL_NO_MEM:
        return MMAGIC_STATUS_NO_MEM;

    case MMIPAL_NO_LINK:
        return MMAGIC_STATUS_NO_LINK;

    case MMIPAL_NOT_SUPPORTED:
        return MMAGIC_STATUS_NOT_SUPPORTED;

        /* Note that we do not add a default case here. The compiler should therefore give us a
         * warning if new fields get added to mmipal_status and we do not handle them. */
    }

    return MMAGIC_STATUS_ERROR;
}

enum mmagic_status mmagic_mmwlan_status_to_mmagic_status(enum mmwlan_status status)
{
    switch (status)
    {
    case MMWLAN_SUCCESS:
        return MMAGIC_STATUS_OK;

    case MMWLAN_ERROR:
        return MMAGIC_STATUS_ERROR;

    case MMWLAN_INVALID_ARGUMENT:
        return MMAGIC_STATUS_INVALID_ARG;

    case MMWLAN_UNAVAILABLE:
        return MMAGIC_STATUS_UNAVAILABLE;

    case MMWLAN_CHANNEL_LIST_NOT_SET:
        return MMAGIC_STATUS_CHANNEL_LIST_NOT_SET;

    case MMWLAN_NO_MEM:
        return MMAGIC_STATUS_NO_MEM;

    case MMWLAN_TIMED_OUT:
        return MMAGIC_STATUS_TIMEOUT;

    case MMWLAN_SHUTDOWN_BLOCKED:
        return MMAGIC_STATUS_SHUTDOWN_BLOCKED;

    case MMWLAN_CHANNEL_INVALID:
        return MMAGIC_STATUS_CHANNEL_INVALID;

    case MMWLAN_NOT_FOUND:
        return MMAGIC_STATUS_NOT_FOUND;

    case MMWLAN_NOT_RUNNING:
        return MMAGIC_STATUS_NOT_RUNNING;

        /* Note that we do not add a default case here. The compiler should therefore give us a
         * warning if new fields get added to mmwlan_status and we do not handle them. */
    }

    return MMAGIC_STATUS_ERROR;
}
