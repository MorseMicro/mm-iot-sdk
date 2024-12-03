/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file shadow_device_task.h Device Shadow task API.
 */

#pragma once

/** Enumeration of shadow update statuses. */
enum shadow_update_status
{
    /** Update resulted in a state change, subscribers use this to change state. */
    UPDATE_DELTA,

    /** Update was accepted, publishers use this to confirm reception of message. */
    UPDATE_ACCEPTED,

    /** Update was rejected, this is to notify publishers of an error condition. */
    UPDATE_REJECTED
};

/**
 * Prototype for callback function invoked on link status changes.
 *
 * @param json       The JSON update message, not NULL terminated.
 * @param json_len   The length of the JSON update message.
 * @param status     The status of the update.
 */
typedef void (*shadow_update_cb_fn_t)(char *json, size_t json_len,
                                      enum shadow_update_status status);

/**
 * Publishes to the shadow update topic: @c "$aws/things/thingName/shadow/update"
 *
 * @param pcShadowName Name of the shadow, NULL or empty string for classic shadow.
 * @param json         The JSON document to publish.
 * @return             true on success.
 */
bool aws_publish_shadow(char *pcShadowName, char* json);

/**
 * Creates the AWS Shadow device task for the specified shadow.
 * You may call this multiple times for multiple shadows.
 *
 * @param pcShadowName        Name of the named shadow, pass NULL if using classic shadow.
 * @param pfnUpdateCallback   Callback for shadow updates.
 * @return                    true on success.
 */
bool aws_create_shadow(char *pcShadowName, shadow_update_cb_fn_t pfnUpdateCallback);

/**
 * Releases the AWS Shadow resources for the specified shadow.
 *
 * @param pcShadowName       Name of the named shadow, pass NULL if using classic shadow.
 * @param pfnUpdateCallback  Callback for shadow updates, required to match the correct resource.
 * @return                   true on success.
 */
void aws_close_shadow(char *pcShadowName, shadow_update_cb_fn_t pfnUpdateCallback);
