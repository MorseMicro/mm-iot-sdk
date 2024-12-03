/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Implements the OTA Platform Abstraction Layer.
 */

#include <stdio.h>
#include "mmosal.h"
#include "mmconfig.h"
#include "ota_config.h"
#include "ota_pal.h"
#include "version.h"
#include "ota_appversion32.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/x509_crt.h"
#include "ota_update_task.h"

/**
 * This is the maximum size of any placeholder files, we determine the file is a dummy
 * if it is less than this size.
 */
#define DUMMY_FILE_SIZE                       100

/** Firmware version. */
const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = APP_VERSION_MAJOR,
    .u.x.minor = APP_VERSION_MINOR,
    .u.x.build = APP_VERSION_BUILD,
};

/** Initialize the source key for JSON files. */
const char OTA_JsonFileSignatureKey[ OTA_FILE_SIG_KEY_STR_MAX_LENGTH ] = "sig-sha256-ecdsa";

/** The current state of the OTA PAL */
static OtaPalImageState_t prvImageState = OtaPalImageStateUnknown;

/** Callback to call prior to starting an update */
ota_preupdate_cb_fn_t ota_pal_preupdate_callback = NULL;

OtaPalStatus_t otaPal_Abort(OtaFileContext_t * const pFileContext)
{
    (void)pFileContext;

    LogInfo(("Aborting..."));

    prvImageState = OtaPalImageStateUnknown;
    if (pFileContext->pFile)
    {
        /* Close file */
        fclose(pFileContext->pFile);
    }

    return OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);
}

OtaPalStatus_t otaPal_CreateFileForRx(OtaFileContext_t * const pFileContext)
{
    OtaPalStatus_t ret = OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);

    LogInfo(("Creating file %s...", (char*)pFileContext->pFilePath));

    pFileContext->pFile = fopen((char*)pFileContext->pFilePath, "wb+");
    if (pFileContext->pFile == NULL)
    {
        ret = OTA_PAL_COMBINE_ERR(OtaPalRxFileCreateFailed, 0);
    }

    /* Mark the file for deletion after succesful or failed update */
    mmconfig_write_string("DELETE_FILE", (char*) pFileContext->pFilePath);

    /* Notify user application that an OTA update is about to start */
    if (ota_pal_preupdate_callback)
    {
        ota_pal_preupdate_callback();
    }

    prvImageState = OtaPalImageStateUnknown;

    return ret;
}

OtaPalStatus_t otaPal_CloseFile(OtaFileContext_t * const pFileContext)
{
    OtaPalStatus_t uxOtaStatus = OTA_PAL_COMBINE_ERR(OtaPalFileClose, 0);
    unsigned char pucHashBuffer[MBEDTLS_MD_MAX_SIZE];

    /* Initialize mbedtls API */
    mbedtls_x509_crt crt;
    mbedtls_md_context_t ctx;
    mbedtls_x509_crt_init(&crt);
    mbedtls_md_init(&ctx);
    MMOSAL_ASSERT(mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0) == 0);
    MMOSAL_ASSERT(mbedtls_md_starts(&ctx) == 0);

    prvImageState = OtaPalImageStateInvalid;

    do
    {
        /* Perform SHA256 hash on the file */
        size_t n;
        static unsigned char buf[1024];
        MMOSAL_ASSERT(fseek(pFileContext->pFile, 0, SEEK_SET) == 0);
        while ((n = fread(buf, 1, sizeof(buf), pFileContext->pFile)) > 0)
        {
            MMOSAL_ASSERT(mbedtls_md_update(&ctx, buf, n) == 0);
        }
        MMOSAL_ASSERT(mbedtls_md_finish(&ctx, pucHashBuffer) == 0);

        /* Close file */
        MMOSAL_ASSERT(fclose(pFileContext->pFile) == 0);

        /* Check the size of the OTA public key */
        int len =  mmconfig_read_bytes(AWS_KEY_OTA_CERTIFICATE, NULL, 0, 0);
        if (len < DUMMY_FILE_SIZE)
        {
            /* No valid OTA keys found */
            break;
        }

        /* Looks like we have a valid key */
        uint8_t *certificate = (uint8_t*) mmosal_malloc(len + 1);
        MMOSAL_ASSERT(certificate != NULL);
        /* Now read the bytes in */
        MMOSAL_ASSERT(mmconfig_read_bytes(AWS_KEY_OTA_CERTIFICATE, certificate, len, 0) > 0);
        /* Add NULL terminator as MbedTLS expects this, we already allocated +1 bytes */
        certificate[len] = 0;

        if (mbedtls_x509_crt_parse(&crt, certificate, len + 1) != 0)
        {
            /* Invalid public key */
            break;
        }

        if (mbedtls_pk_verify(&crt.pk, MBEDTLS_MD_SHA256,
                              pucHashBuffer, 0,
                              pFileContext->pSignature->data,
                              pFileContext->pSignature->size) != 0)
        {
            /* Signature did not match */
            break;
        }

        /* Succesful verification */
        uxOtaStatus = OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);
        prvImageState = OtaPalImageStatePendingCommit;

        /* Write the SHA256 hash for the loader to verify */
        MMOSAL_ASSERT(mmconfig_write_data("IMAGE_SIGNATURE", pucHashBuffer, 32) == MMCONFIG_OK);
    } while (false);

    LogInfo(("Closing, status %d", (int)uxOtaStatus));

    /* Cleanup mbedtls */
    mbedtls_md_free(&ctx);
    mbedtls_x509_crt_free(&crt);

    return uxOtaStatus;
}

int16_t otaPal_WriteBlock(OtaFileContext_t * const pFileContext,
                          uint32_t ulOffset,
                          uint8_t * const pData,
                          uint32_t ulBlockSize)
{
    LogInfo(("Writing %lu bytes at %lu to %s...", ulBlockSize, ulOffset, pFileContext->pFilePath));

    if (fseek(pFileContext->pFile, ulOffset, SEEK_SET) == 0)
    {
        return (int16_t) fwrite(pData, 1, ulBlockSize, pFileContext->pFile);
    }
    else
    {
        LogError(("Could not write to file at offset %lu!", ulOffset));
        return -1;
    }
}

OtaPalStatus_t otaPal_ActivateNewImage(OtaFileContext_t * const pFileContext)
{
    LogInfo(("Activating..."));

    /* Notify loader an update is available */
    mmconfig_write_string("UPDATE_IMAGE", (char*) pFileContext->pFilePath);

    /* Reset to loader */
    mmhal_reset();

    /* Shouldn't get here */
    return OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);
}

OtaPalStatus_t otaPal_SetPlatformImageState(OtaFileContext_t * const pFileContext,
                                            OtaImageState_t eState)
{
    (void)pFileContext;
    (void)eState;

    LogInfo(("OTA State is now %d", (int) eState));

    return OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);
}

OtaPalImageState_t otaPal_GetPlatformImageState(OtaFileContext_t * const pFileContext)
{
    (void)pFileContext;

    return prvImageState;
}

OtaPalStatus_t otaPal_ResetDevice(OtaFileContext_t * const pFileContext)
{
    (void)pFileContext;

    LogInfo(("Resetting..."));

    mmhal_reset();

    return OTA_PAL_COMBINE_ERR(OtaPalSuccess, 0);
}
