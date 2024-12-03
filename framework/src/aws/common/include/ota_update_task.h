/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * OTA Update task API
 */

#pragma once

/**
 * Prototype for callback function invoked before initiating an OTA update.
 *
 * This callback function is called from the context of the OTA update task.
 * Ensure you protect any shared data access with suitable critical sections.
 * The purpose of this callback is to signal to the application that an OTA
 * update is about to start. The application may perform any cleanup here to
 * free up space in the file system in preparation for the OTA update. This may
 * include deleting log and temporary files, uploading any data to the cloud
 * and shutting down any activities. The application may delay the update by
 * blocking in this function till it is ready. If the application does not want
 * to perform OTA update at any time then it should use @c vSuspendOTAUpdate()
 * and @c vResumeOTAUpdate() in advance to signal its readiness to receive updates.
 *
 * @return Return true if the OTA update can proceed, false to defer the update.
 */
typedef void (*ota_preupdate_cb_fn_t)(void);

/**
 * Prototype for callback function invoked after an OTA update.
 *
 * This callback function is called from the context of the OTA update task.
 * Ensure you protect any shared data access with suitable critical sections.
 * The purpose of this callback is to signal to the application that an OTA
 * update has completed - either successfully or in failure. The application may
 * perform any logging to note the event and also to migrate any data from the
 * older version if required. The application may also use this callback to
 * restore any files and data it had uploaded to the cloud prior to the update
 * starting. If the update file contains a BCF or firmware image, it can be used to
 * update BCF and firmware atomically in this function.
 *
 * @param update_file Path to the update file - use this to update firmware or BCF if required.
 * @param status      0 on success, loader error code on failure.
 */
typedef void (*ota_postupdate_cb_fn_t)(const char *update_file, int status);


/**
 * Suspends the OTA update
 */
void vSuspendOTAUpdate(void);

/**
 * Resumes an OTA update
 */
void vResumeOTAUpdate(void);

/**
 * The function initializes the OTA update task.
 *
 * @param[in] preupdate_cb  A callback function to call before initiating an OTA update.
 *                          Set to NULL if not needed.
 * @param[in] postupdate_cb A callback function to call after an OTA update.
 *                          Set to NULL if not needed.
 */
void start_ota_update_task(ota_preupdate_cb_fn_t preupdate_cb,
                           ota_postupdate_cb_fn_t postupdate_cb);
