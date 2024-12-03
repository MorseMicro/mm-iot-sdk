/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example bootloader to boot and update application securely.
 *
 * Firmware Update Procedure
 * =========================
 * The software update is done in 2 stages.
 *
 * Stage 1: Software download and Authentication
 * ---------------------------------------------
 * This step is performed by the application and is application specific.
 * The application downloads the software image using any means possible and then
 * authenticates it using any means possible - this is all application specific.
 * The update image is in the morse @c MBIN file format. Once the software image has been
 * successfully downloaded and authenticated, the application software needs to signal
 * the bootloader that a new image is available for flashing. it does so by simply writing
 * the path of the software image to config store key UPDATE_IMAGE.  For added security
 * the application also writes a SHA256 hash of the image to IMAGE_SIGNATURE key. Once
 * this is done, the application simply reboots the CPU which causes the bootloader to execute.
 *
 * To build a .mbin file for any application simply issue the following make command in the
 * applications folder:
 * @code
 * make mbin -j4
 * @endcode
 * The result of the above command will be a .mbin file in the `build/` directory for the
 * application in addition to the .elf file.
 * @note You cannot generate a .mbin file for the bootloader itself.
 *
 *
 * Stage 2: Flashing the Software image
 * ------------------------------------
 * This code implements stage 2. Once rebooted by the application, the CPU automatically
 * executes the bootloader (this program) which is in the boot region of flash. The bootloader
 * sees that there is an UPDATE_IMAGE key in config store and proceeds to verify the
 * image pointed to by the key using the SHA256 algorithm and compares the result with
 * the hash in the IMAGE_SIGNATURE key. If the hash matches it then proceeds with
 * flashing the image into the application code area of the flash. In case the checksum
 * does not match the bootloader writes an error code into the config store and jumps back
 * into the old application image without performing the software update. Once the image
 * has been successfully flashed the bootloader transfers control to the new application image.
 *
 * If no UPDATE_IMAGE key was found, the bootloader simply jumps to the current application
 * image in flash.
 *
 * In addition to the above the bootloader performs some sanity checks like checking how many
 * times the update was attempted and failed in order to prevent the bootloader from getting
 * stuck in an infinite update loop in the event of a faulty hardware (or software image).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <endian.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include "sha256.h"
#include "mmhal_flash.h"
#include "mmconfig.h"
#include "mmhal.h"
#include "mmosal.h"
#include "puff.h"
#include "mbin.h"


/** Maximum number of times we attempt to update before giving up */
#define MAX_UPDATE_ATTEMPTS     10

/** Maximum size of a segment, set to 32768 */
#define MAX_SEGMENT_SIZE        32768

/** Bootloader return codes */
enum bootloader_return_codes
{
    BOOTLOADER_OK = 0,
    BOOTLOADER_ERR_FILE_NOT_FOUND,
    BOOTLOADER_ERR_SIGNATURE_NOT_FOUND,
    BOOTLOADER_ERR_FILE_VERIFICATION_FAILED,
    BOOTLOADER_ERR_INVALID_FILE,
    BOOTLOADER_ERR_FILE_CORRUPT,
    BOOTLOADER_ERR_ERASE_FAILED,
    BOOTLOADER_ERR_PROGRAM_FAILED,
    BOOTLOADER_ERR_TOO_MANY_ATTEMPTS,
    BOOTLOADER_ERR_FILE_DECOMPRESSION,
};

/** Start of Application region in flash. */
extern uint8_t application_start;

/** End of Application region in flash. */
extern uint8_t application_end;

/** Temporary buffer for loading segments */
static uint8_t segment_buffer[MAX_SEGMENT_SIZE];

/** Temporary buffer for deflating segments */
static uint8_t deflate_buffer[MAX_SEGMENT_SIZE];


/**
 * Implements a delay of approximately the duration specified.
 *
 * @param approx_delay_ms The delay in ms.
 */
static void delay_ms(uint32_t approx_delay_ms)
{
    uint32_t i;
    for (i = 0; i < approx_delay_ms; i++)
    {
        uint32_t tick = mmosal_get_time_ticks();
        while (tick == mmosal_get_time_ticks())
        {
        }
    }
}

/**
 * Blinks the hardware error LED to indicate the error code.
 *
 * @param code The error code to blink.
 */
static void blink_error_code(int code)
{
    int ii;
    /* Flash error LED code times */

    for (ii = 0; ii < code; ii++)
    {
        mmhal_set_error_led(true);
        delay_ms(100);
        mmhal_set_error_led(false);
        delay_ms(100);
    }
}

/**
 * Called when multiple attempts to update failed, must not return.
 *
 * This function *could* try rebooting after a fixed delay and the
 * update process can be retried by the bootloader. However the delay should
 * be sufficiently large (several minutes at least) as it is likely that
 * whatever caused us to fail will happen again, so spamming the flash
 * with multiple attempts is not a good idea.
 *
 * We usually end up in this function if we had a hardware failure, like
 * a flash failure. So the problem is likely to happen again and will
 * possibly get worse with each attempt.
 *
 * We do not end up in this function if sanity checks (Like image checksum)
 * prior to updating failed. If that happens we simply continue to the existing
 * application with an error code stored in config store. Once the flash is
 * erased, any failure will get us here.
 *
 * @param code The error code, gets written to config store and blinked.
 */
static void update_failed(int code)
{
    /* Write error code to config store */
    mmconfig_write_int("BOOTLOADER_ERROR", code);
    while (true)
    {
        /* We repeatedly blink the error code here */
        blink_error_code(code);
        delay_ms(1000);
    }
}

/**
 * This function erases the entire application flash region.
 *
 * We erase everything in one go as we don't want to deal with overlapping segments
 * in the MBIN file. If a segment overlaps we need to take care that we don't erase
 * the same flash block twice (or we lose the data data for the previous segment due
 * to the overlap). We avoid this issue by erasing everything in one go at the start.
 */
static void erase_application_area(void)
{
    uint32_t block_address = (uint32_t) &application_start;
    uint32_t end_address = (uint32_t) &application_end;

    while (block_address < end_address)
    {
        /* Erase block */
        if (mmhal_flash_erase(block_address) != 0)
        {
            /* We had a failure erasing flash, try again */
            if (mmhal_flash_erase(block_address) != 0)
            {
                /* We had a second consecutive failure, give up as the flash is likely worn out */
                update_failed(BOOTLOADER_ERR_ERASE_FAILED);
            }
        }

        /*
         * No need to sanity check this as everything between @c application_start
         * and @c application_end should be in valid flash or the linker script is wrong.
         */
        block_address += mmhal_flash_getblocksize(block_address);
    }
}


/**
 * Loads the segment header from file
 *
 * @param fd           The file descriptor to read from
 * @param seg_hdr      Pointer to segment header to read data into
 * @return             BOOTLOADER_OK on success, else error code
 */
static int read_segment_header(int fd, struct mbin_tlv_hdr *seg_hdr)
{
    struct mbin_tlv_hdr hdr_overlay;

    int ret = read(fd, &hdr_overlay, sizeof(hdr_overlay));
    if (ret != sizeof(hdr_overlay))
    {
        return BOOTLOADER_ERR_FILE_CORRUPT;
    }

    seg_hdr->type = le16toh(hdr_overlay.type);
    seg_hdr->len = le16toh(hdr_overlay.len);

    return BOOTLOADER_OK;
}

/**
 * Loads a 32 bit integer from file and converts to host endian
 *
 * @param fd           The file descriptor to read from
 * @param val          Pointer to variable to read data into
 * @return             BOOTLOADER_OK on success, else error code
 */
static int read_uint32(int fd, uint32_t *val)
{
    uint32_t tmp;

    int ret = read(fd, &tmp, sizeof(tmp));
    if (ret != sizeof(tmp))
    {
        return BOOTLOADER_ERR_FILE_CORRUPT;
    }

    *val = le32toh(tmp);

    return BOOTLOADER_OK;
}

/**
 * Loads and flashes the image
 *
 * @param fname        MBIN file to load
 * @param make_changes If make_changes is false then flash will not be written to
 * @return             BOOTLOADER_OK on success, else error code
 */
static int load_and_flash_mbin(const char *fname, bool make_changes)
{
    struct mbin_tlv_hdr seg_hdr;
    uint32_t mbin_magic = 0;
    uint32_t load_address, load_size;
    unsigned long compressed_size, puffed_size;   // NOLINT(runtime/int) match puff API
    int ret;
    bool eof = false;
    bool modified = false;

    /* Open file */
    int fd = open(fname, O_RDONLY);
    if (fd < 0)
    {
        /* Could not open file */
        return BOOTLOADER_ERR_FILE_NOT_FOUND;
    }

    /* Verify MBIN magic */
    ret = read_segment_header(fd, &seg_hdr);
    if (ret != BOOTLOADER_OK)
    {
        ret = BOOTLOADER_ERR_FILE_CORRUPT;
        goto bailout;
    }
    if (seg_hdr.type != FIELD_TYPE_MAGIC)
    {
        ret = BOOTLOADER_ERR_INVALID_FILE;
        goto bailout;
    }
    if (seg_hdr.len != sizeof(mbin_magic))
    {
        ret = BOOTLOADER_ERR_FILE_CORRUPT;
        goto bailout;
    }
    ret = read_uint32(fd, &mbin_magic);
    if (MBIN_SW_MAGIC_NUMBER != mbin_magic)
    {
        ret = BOOTLOADER_ERR_INVALID_FILE;
        goto bailout;
    }

    /* Iterate through TLV headers */
    while (!eof)
    {
        ret = read_segment_header(fd, &seg_hdr);
        if (ret != BOOTLOADER_OK)
        {
            break;
        }

        switch (seg_hdr.type)
        {
        case FIELD_TYPE_SW_SEGMENT:
            ret = read_uint32(fd, &load_address);
            if (ret != BOOTLOADER_OK)
            {
                /* We had a failure reading MBIN, give up */
                goto bailout;
            }

            load_size = seg_hdr.len - sizeof(load_address);
            if (load_size > MAX_SEGMENT_SIZE)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }

            ret = read(fd, segment_buffer, load_size);
            if (ret != (int)load_size)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }

            /* Does the load address fall within the application area? */
            if ((load_address >= (uint32_t) &application_start) &&
                (load_address + load_size <= (uint32_t) &application_end))
            {
                modified = true;
                if (make_changes)
                {
                    ret = mmhal_flash_write(load_address, segment_buffer, load_size);
                    if (ret != 0)
                    {
                        /* We had a failure while flashing, give up as flash state is unknown */
                        ret = BOOTLOADER_ERR_PROGRAM_FAILED;
                        goto bailout;
                    }
                }
            }
            else
            {
                /* We attempted to write outside the valid area */
                ret = BOOTLOADER_ERR_INVALID_FILE;
                goto bailout;
            }
            break;

        case FIELD_TYPE_SW_SEGMENT_DEFLATED:
            ret = read_uint32(fd, &load_address);
            if (ret != BOOTLOADER_OK)
            {
                /* We had a failure reading MBIN, give up */
                goto bailout;
            }

            ret = read_uint32(fd, &load_size);
            if (ret != BOOTLOADER_OK)
            {
                /* We had a failure reading MBIN, give up */
                goto bailout;
            }

            if (load_size > MAX_SEGMENT_SIZE)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }

            compressed_size = seg_hdr.len - sizeof(load_address) - sizeof(load_size);
            if (compressed_size > MAX_SEGMENT_SIZE)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }

            ret = read(fd, deflate_buffer, compressed_size);
            if (ret != (int)compressed_size)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }

            /* Decompress the segment */
            puffed_size = load_size;
            puff(segment_buffer, &puffed_size, deflate_buffer, &compressed_size);
            if (puffed_size != load_size)
            {
                /* We had a failure decompressing, give up */
                ret = BOOTLOADER_ERR_FILE_DECOMPRESSION;
                goto bailout;
            }

            /* Does the load address fall within the application area? */
            if ((load_address >= (uint32_t) &application_start) &&
                (load_address + load_size <= (uint32_t) &application_end))
            {
                modified = true;
                if (make_changes)
                {
                    ret = mmhal_flash_write(load_address, segment_buffer, load_size);
                    if (ret!= 0)
                    {
                        /* We had a failure while flashing, give up as flash state is unknown */
                        ret = BOOTLOADER_ERR_PROGRAM_FAILED;
                        goto bailout;
                    }
                }
            }
            else
            {
                /* We attempted to write outside the valid area */
                ret = BOOTLOADER_ERR_INVALID_FILE;
                goto bailout;
            }
            break;

        case FIELD_TYPE_EOF_WITH_SIGNATURE:
            /* We validate this signature in the application and generate the
             * simplified IMAGE_SIGNATURE hash in config store for the bootloader.
             * So the bootloader just treats this as an EOF. */
        case FIELD_TYPE_EOF:
            eof = true;
            break;

        /* Skip these fields since they are not used for the software update process */
        case FIELD_TYPE_FW_SEGMENT:
        case FIELD_TYPE_FW_SEGMENT_DEFLATED:
        case FIELD_TYPE_FW_TLV_BCF_ADDR:
        case FIELD_TYPE_BCF_BOARD_CONFIG:
        case FIELD_TYPE_BCF_REGDOM:
            if (seg_hdr.len > MAX_SEGMENT_SIZE)
            {
                /* We had a failure reading MBIN, give up */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }
             /* Skip over */
            ret = read(fd, segment_buffer, seg_hdr.len);
            if (ret != seg_hdr.len)
            {
                /* We had an error reading the file */
                ret = BOOTLOADER_ERR_FILE_CORRUPT;
                goto bailout;
            }
            break;

        case FIELD_TYPE_MAGIC:
        default:
            /* These are unexpected */
            ret = BOOTLOADER_ERR_FILE_CORRUPT;
            goto bailout;
        }
    }

    if (!eof)
    {
        /* File ended unexpectedly */
        ret = BOOTLOADER_ERR_FILE_CORRUPT;
        goto bailout;
    }

    if (!modified)
    {
        /* No attempts were made to write to the application region of flash. */
        /* Maybe this file was meant for another device? */
        ret = BOOTLOADER_ERR_INVALID_FILE;
        goto bailout;
    }

    ret = BOOTLOADER_OK;
bailout:
    close(fd);
    return ret;
}

/**
 * Verifies the signature of the file
 *
 * @param fname   MBIN file to verify
 * @return        BOOTLOADER_OK on success, else error code
 */
static int verify_signature(const char *fname)
{
    /* Stub to compute memory usage of sha256 */
    BYTE data[64];
    BYTE hash[32];
    SHA256_CTX ctx;
    sha256_init(&ctx);

    BYTE compare[32];
    if (mmconfig_read_bytes("IMAGE_SIGNATURE", compare, sizeof(compare), 0) != sizeof(compare))
    {
        /* Error reading hash, bail */
        return BOOTLOADER_ERR_SIGNATURE_NOT_FOUND;
    }

    /* Open file */
    int fd = open(fname, O_RDONLY);
    if (fd < 0)
    {
        /* Could not open file */
        return BOOTLOADER_ERR_FILE_NOT_FOUND;
    }

    int bytesread;
    do
    {
        bytesread = read(fd, data, sizeof(data));
        if (bytesread <= 0)
        {
            /* break out on error */
            break;
        }
        sha256_update(&ctx, data, bytesread);
    } while (bytesread == sizeof(data));

    sha256_final(&ctx, hash);
    close(fd);

    if (memcmp(hash, compare, sizeof(hash)) != 0)
    {
        /* Verification failed */
        return BOOTLOADER_ERR_FILE_VERIFICATION_FAILED;
    }
    return BOOTLOADER_OK;
}

/**
 * Check for an update, if an update is found, perform it.
 */
static void check_update()
{
    static char update_path[128];

    /* Check if UPDATE_IMAGE is set in config store */
    int ret = mmconfig_read_string("UPDATE_IMAGE", update_path, sizeof(update_path));
    if (ret <= 0)
    {
        /* Could not find UPDATE_IMAGE, bail to application, this is the normal use case */
        return;
    }

    ret = verify_signature(update_path);
    if (ret != 0)
    {
        /* Signature check failed, bail to application */
        mmconfig_write_int("BOOTLOADER_ERROR", ret);
        blink_error_code(ret);
        return;
    }

    /* Do a dry run to ensure MBIN file is ok */
    ret = load_and_flash_mbin(update_path, false);
    if (ret != BOOTLOADER_OK)
    {
        /* Update failed early, write error code and bail to application */
        mmconfig_write_int("BOOTLOADER_ERROR", ret);
        blink_error_code(ret);
        return;
    }

    /* MBIN looks good, increment update attempts */
    int update_attempts = 0;
    mmconfig_read_int("UPDATE_ATTEMPTS", &update_attempts);
    if (update_attempts >= MAX_UPDATE_ATTEMPTS)
    {
        /* We tried too many times, give up */
        update_failed(BOOTLOADER_ERR_TOO_MANY_ATTEMPTS);
    }
    mmconfig_write_int("UPDATE_ATTEMPTS", ++update_attempts);

    /* No turning back, any failures past this point are critical */
    erase_application_area();

    /* Start the flashing process for real */
    ret = load_and_flash_mbin(update_path, true);
    if (ret != BOOTLOADER_OK)
    {
        /* We failed once, so erase and try again */
        erase_application_area();
        ret = load_and_flash_mbin(update_path, true);
        if (ret != BOOTLOADER_OK)
        {
            /* We failed a second time, situation is unrecoverable, give up */
            update_failed(ret);
        }
    }

    /* We successfully did an update, so cleanup and reboot now */
    mmconfig_delete_key("UPDATE_IMAGE");
    mmconfig_delete_key("UPDATE_ATTEMPTS");
    mmconfig_delete_key("IMAGE_SIGNATURE");
    mmconfig_delete_key("BOOTLOADER_ERROR");
    mmhal_reset();
}

/**
 * The bootloader entry point.
 * @return Does not return
 */
int main(void)
{
    /* This writes the bootloader version into config store for applicatiopn to read.
     * Note: This function returns without writing anything if the key is already present
     * and has the same value - so no need to worry about flash wear and tear. */
    mmconfig_write_string("BOOTLOADER_VERSION", BOOTLOADER_VERSION);

    /* Check for updates */
    check_update();

    /* Jump to application. The address to jump to is the second entry
     * in the vector table located at the start of application area.
     * Note: we ignore the first entry (Stack pointer) as this is usually the same
     * for the bootloader and set to end of RAM - requires messy assembly to change. */
    uint32_t go_address = (*((volatile uint32_t*) (((uint32_t)&application_start) + 4)));
    void (*jump_to_app)(void) = (void (*)(void)) go_address;

    /* No updates found, jump to application */
    jump_to_app();

    /* We should not get here */
    MMOSAL_ASSERT(false);
}
