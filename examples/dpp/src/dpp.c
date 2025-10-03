/*
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief DPP example application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include <stdio.h>
#include <string.h>
#include "mmutils.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mm_app_common.h"


/**
 * Convert a DPP push button result code to its string representation.
 *
 * @param result    The enumeration value of type @c enum mmwlan_dpp_pb_result to be converted.
 *
 * @return          A constant string describing the result. If the input is not a recognized
 *                  enumeration value, the string "Unknown Result" is returned.
 */
const char* mmwlan_dpp_pb_result_to_string(enum mmwlan_dpp_pb_result result)
{
    switch (result)
    {
        case MMWLAN_DPP_PB_RESULT_SUCCESS:
            return "Success";
        case MMWLAN_DPP_PB_RESULT_ERROR:
            return "Error";
        case MMWLAN_DPP_PB_RESULT_SESSION_OVERLAP:
            return "Session Overlap";
        default:
            return "Unknown Result";
    }
}

/**
 * Function to handle dpp events.
 *
 * @param dpp_event Reference to dpp event argument structure
 * @param arg       User argument that was registered when @c mmwlan_dpp_start() was called.
 */
static void dpp_event_handler(const struct mmwlan_dpp_cb_args *dpp_event, void *arg)
{
    struct mmosal_semb *semb = (struct mmosal_semb *)arg;
    if (dpp_event->event != MMWLAN_DPP_EVT_PB_RESULT)
    {
        mmosal_printf("Unsupported event %lu\n", dpp_event->event);
        return;
    }

    mmosal_printf("DPP Result: %s\n",
                  mmwlan_dpp_pb_result_to_string(dpp_event->args.pb_result.result));

    if (dpp_event->args.pb_result.result != MMWLAN_DPP_PB_RESULT_SUCCESS)
    {
        return;
    }

    if ((dpp_event->args.pb_result.ssid == NULL) ||
        (dpp_event->args.pb_result.passphrase == NULL) ||
        (dpp_event->args.pb_result.ssid_len > MMWLAN_SSID_MAXLEN - 1))
    {
        mmosal_printf("Invalid/incomplete credentials provided\n");
        return;
    }

    mmosal_printf("SSID %*s, PWD %s\n",
                  dpp_event->args.pb_result.ssid_len,
                  dpp_event->args.pb_result.ssid,
                  dpp_event->args.pb_result.passphrase);


    char ssid[MMWLAN_SSID_MAXLEN];
    memcpy(ssid, dpp_event->args.pb_result.ssid, dpp_event->args.pb_result.ssid_len);
    ssid[dpp_event->args.pb_result.ssid_len] = '\0';
    mmconfig_write_string("wlan.ssid", ssid);
    mmconfig_write_string("wlan.password", dpp_event->args.pb_result.passphrase);

    (void)mmosal_semb_give(semb);
}


/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nSTA DPP Connect Example (Built " __DATE__ " " __TIME__ ")\n\n");

    struct mmosal_semb *semb = mmosal_semb_create("dpp");
    MMOSAL_ASSERT(semb);

    app_wlan_init();

    struct mmwlan_dpp_args dpp_args = {.dpp_event_cb = dpp_event_handler, .dpp_event_cb_arg = semb};

    mmosal_printf("DPP Start\n");
    enum mmwlan_status status = mmwlan_dpp_start(&dpp_args);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    bool ok = mmosal_semb_wait(semb, 200*1000);
    MMOSAL_ASSERT(ok);

    mmwlan_dpp_stop();

    app_wlan_start();
}

