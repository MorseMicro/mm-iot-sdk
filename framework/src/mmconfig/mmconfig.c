/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * This File implements the MMCONFIG Persistent Store API.
 */

#include <ctype.h>
#include "mmosal.h"
#include "mmhal_flash.h"
#include "mmconfig.h"

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif

/** @c MMCS (Morse Micro Config Store) in little endian notation */
#define MMCONFIG_SIGNATURE  0x53434D4D

/** Starting seed value for the Xorshift PRNG, arbitrarily chosen */
#define XORHASH_SEED        0xdfb7f3e1

/* This value marks the end of a Key Value List */
#define LIST_TERMINATOR     MMHAL_FLASH_ERASE_VALUE

/** Mutex to protect MMCONFIG API */
static struct mmosal_mutex *mmconfig_mutex = NULL;

struct PACKED mmconfig_partition_header
{
    uint32_t signature;     /**< Contains signature @ref MMCONFIG_SIGNATURE */
    uint32_t version;       /**< Version number increments on every update */
    uint32_t checksum;      /**< Checksum of all data in partition up to the end marker */
    uint8_t data[];         /**< Partition data */
};

struct PACKED mmconfig_key_header
{
    uint8_t key_len;        /**< Length of the key in bytes */
    char    key[];          /**< Key name, not NULL terminated */
};

struct PACKED mmconfig_data_header
{
    uint16_t data_len;      /**< Length of the data in bytes */
    uint8_t  data[];        /**< Data blob */
};

/** Pointer to primary MMCONFIG partition */
static struct mmconfig_partition_header *mmconfig_primary_image = NULL;

/** Pointer to secondary MMCONFIG partition */
static struct mmconfig_partition_header *mmconfig_secondary_image = NULL;

/** Length of each partition in bytes */
static uint32_t mmconfig_partition_size = 0;

/**
 * This function converts an unsigned integer to string
 *
 * @param buf       The buffer to place the converted string in
 * @param bufsize   The length of the buffer
 * @param val       The unsigned integer to convert to string
 * @return          Pointer to the converted string or NULL on error
 */
static char* mmconfig_uint_to_str(char *buf, size_t bufsize, uint32_t val)
{
    uint32_t len;

    /* Figure out number of digits, this lets us start in the correct spot in the string */
    if (val < 10)
    {
        len = 1;
    }
    else if (val < 100)
    {
        len = 2;
    }
    else if (val < 1000)
    {
        len = 3;
    }
    else if (val < 10000)
    {
        len = 4;
    }
    else if (val < 100000)
    {
        len = 5;
    }
    else if (val < 1000000)
    {
        len = 6;
    }
    else if (val < 10000000)
    {
        len = 7;
    }
    else if (val < 100000000)
    {
        len = 8;
    }
    else if (val < 1000000000)
    {
        len = 9;
    }
    else
    {
        len = 10;
    }

    /* Do we have enough space in the buffer? */
    if (bufsize <= len)
        return NULL;

    char *tmp = buf + len;

    /* Place NULL terminator */
    *tmp-- = 0;

    /* Then create the string from the end */
    do {
        char digit = (char)('0' + (val % 10));
        *tmp-- = digit;
        val = val / 10;
    } while ((val != 0) && (tmp >= buf));

    MMOSAL_ASSERT(val == 0);

    return buf;
}

/**
 * This function converts an unsigned integer to string
 *
 * @param buf       The buffer to place the converted string in
 * @param bufsize   The length of the buffer
 * @param val       The signed integer to convert to string
 * @return          Pointer to the converted string or NULL on error
 */
static char* mmconfig_int_to_str(char *buf, size_t bufsize, int32_t val)
{
    if (val >= 0)
    {
        return mmconfig_uint_to_str(buf, bufsize, (uint32_t)val);
    }
    else
    {
        if (bufsize > 2)
        {
            *buf = '-';
            if (mmconfig_uint_to_str(&buf[1], bufsize - 1, (uint32_t)-val) == NULL)
            {
                /* We got an error */
                return NULL;
            }
            else
                return buf;
        }
    }
    return NULL;
}

/**
 * This function converts a string representation of a decimal or hexadecimal
 * number to an unsigned integer.
 *
 * @param str       The string representation of a decimal or hexadecimal number
 * @param val       The unsigned integer to return the value in, unchanged on error
 * @return          MMCONFIG_OK on success or an error code on failure
 */
static int mmconfig_str_to_uint(char *str, uint32_t *val)
{
    uint64_t num = 0;
    int ii;

    /* Is it a hexadecimal string? */
    if ((str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X')))
    {
        for (ii = 2; str[ii] != '\0'; ii++) {
            if ((str[ii] >= '0') && (str[ii] <= '9'))
            {
                num = num * 16 + (str[ii] - '0');
            }
            else if ((str[ii] >= 'a') && (str[ii] <= 'f'))
            {
                num = num * 16 + (str[ii] - 'a' + 10);
            }
            else if ((str[ii] >= 'A') && (str[ii] <= 'F'))
            {
                num = num * 16 + (str[ii] - 'A' + 10);
            }
            else
            {
                /* Invalid code, return error */
                return MMCONFIG_ERR_INCORRECT_TYPE;
            }
        }
    }
    else
    {
        for (ii = 0; str[ii] != '\0'; ii++) {
            if ((str[ii] < '0') || (str[ii] > '9'))
            {
                /* Invalid code, return error */
                return MMCONFIG_ERR_INCORRECT_TYPE;
            }

            num = num * 10 + (str[ii] - '0');
        }
    }

    if ((ii > 10) || (num > 4294967295))
    {
        /* Overflow */
        return MMCONFIG_ERR_INCORRECT_TYPE;
    }

    *val = (uint32_t)num;

    return MMCONFIG_OK;
}

/**
 * This function converts a string representation of a signed decimal or hexadecimal
 * number to an signed integer.
 *
 * @param str       The string representation of a signed decimal or hexadecimal number
 * @param val       The signed integer to return the value in, unchanged on error
 * @return          MMCONFIG_OK on success or an error code on failure
 */
static int mmconfig_str_to_int(char *str, int *val)
{
    uint32_t num = 0;
    int sign = 1;

    if (str[0] == '-')
    {
        sign = -1;
        str++;
    }

    int ret = mmconfig_str_to_uint(str, &num);
    if (ret == MMCONFIG_OK)
    {
        /* Check for overflows */
        if ((sign == -1) && (num > 0x80000000))
        {
            return MMCONFIG_ERR_INCORRECT_TYPE;
        }
        if ((sign == 1) && (num > 0x7FFFFFFF))
        {
            return MMCONFIG_ERR_INCORRECT_TYPE;
        }
        *val = sign * num;
    }

    return ret;
}

/**
 * This function updates the provided checksum with the data provided.
 *
 * The checksum is literally a sum of all the bytes in the data.
 *
 * @param checksum  A pointer to the current checksum which is then updated.
 * @param data      A pointer to the data.
 * @param size      The length of the data in bytes.
 */
static void mmconfig_update_checksum(uint32_t *checksum, const uint8_t *data, size_t size)
{
    while (size--)
    {
        *checksum += *data++;

        /* Now do an Xorshift to shuffle the bits */
        *checksum ^= *checksum << 13;
        *checksum ^= *checksum >> 17;
        *checksum ^= *checksum << 5;
    }
}

/**
 * Does a case insensitive compare of the requested and found keys.
 *
 * @param requested_key     The requested key.
 * @param requested_key_len The length of the requested key.
 * @param found_key         The key found in flash.
 * @param found_key_len     The length of the key found in flash.
 * @return                  True if keys match.
 */
static bool mmconfig_key_match(const char *requested_key, size_t requested_key_len,
                               const char *found_key, size_t found_key_len)
{
    uint32_t i;

    MMOSAL_ASSERT(found_key_len <= MMCONFIG_MAX_KEYLEN);
    if (requested_key_len < 1)
    {
        return false;
    }

    /* Compare sizes unless requested_key contains trailing asterisk */
    if (requested_key[requested_key_len - 1] != '*')
    {
        /* Dissimilar sizes? Of course they don't match */
        if (requested_key_len != found_key_len)
            return false;
    }
    else
    {
        /* If the found_key is shorter than the non-wildcard portion of the requested_key,
         * it cannot be a match either
         */
        if ((requested_key_len - 1) > found_key_len)
            return false;
    }

    for (i = 0; i < found_key_len; i++)
    {
        if (requested_key[i] == '*')
        {
            /* Matched up to trailing asterisk, so success */
            return true;
        }

        if (toupper(requested_key[i]) != toupper(found_key[i]))
        {
            /* We had a mismatch before we reached the end */
            return false;
        }
    }
    return true;
}

int mmconfig_validate_key_character(char character)
{
    if (!isalnum(character) && (character != '_') && (character != '.'))
    {
        return MMCONFIG_ERR_INVALID_KEY;
    }

    return MMCONFIG_OK;
}

int mmconfig_validate_key(const char *key)
{
    uint32_t i;

    /* Validate key length */
    size_t key_len = strnlen(key, MMCONFIG_MAX_KEYLEN + 1);
    if ((key_len > MMCONFIG_MAX_KEYLEN) || (key_len == 0))
    {
        return MMCONFIG_ERR_INVALID_KEY;
    }

    /* Validate first char is alpha */
    if (!isalpha((int)key[0]))
    {
        return MMCONFIG_ERR_INVALID_KEY;
    }

    /* Validate rest of key */
    for (i = 1; i < key_len; i++)
    {
        if (mmconfig_validate_key_character(key[i]) != MMCONFIG_OK)
        {
            if ((key[i] == '*') && (i == (key_len - 1)))
            {
                /* Asterisk wildcard currently only supported at end of key */
                return MMCONFIG_ERR_WILDCARD_KEY;
            }
            return MMCONFIG_ERR_INVALID_KEY;
        }
    }

    /* All good */
    return MMCONFIG_OK;
}


/**
 * Validates the provided partition.
 * @param partition The partition to validate.
 * @return          MMCONFIG_OK if partition is valid and checksum passes.
 */
static int mmconfig_validate_partition(struct mmconfig_partition_header *partition)
{
    uint32_t checksum = XORHASH_SEED;

    /* Verify header signature */
    if (partition->signature != MMCONFIG_SIGNATURE)
    {
        return MMCONFIG_ERR_INVALID_PARTITION;
    }

    /* Find the first keyheader which is at the end of mmconfig_partition_header */
    struct mmconfig_key_header *keyheader_ptr =
            (struct mmconfig_key_header*) (partition->data);

    /* Loop till we find a terminator marked by key_len of @c LIST_TERMINATOR or 0 */
    while ((keyheader_ptr->key_len != LIST_TERMINATOR) &&
           (keyheader_ptr->key_len != 0) &&
           ((uint32_t) keyheader_ptr < (uint32_t) partition + mmconfig_partition_size))
    {
        struct mmconfig_data_header *dataheader_ptr =
                (struct mmconfig_data_header *) (keyheader_ptr->key + keyheader_ptr->key_len);

        /* Break out if we run into erased flash.  The Flash writing is done by
         * mmconfig_buffered_write() in chunks of 128 bytes. So it is possible that the lower
         * byte of data_len was written but not the higher byte (eg: power removed during a write
         * operation). In which case the data_len will contain @c MMHAL_FLASH_ERASE_VALUE in the upper byte,
         * so check for this. We don't repeat this check in mmconfig_read_data() as we have already
         * validated the partition here.
         */
        if ((dataheader_ptr->data_len >> 8) == MMHAL_FLASH_ERASE_VALUE)
            return MMCONFIG_ERR_INVALID_PARTITION;

        /* Compute number of bytes in key & data */
        size_t bytecount = sizeof(struct mmconfig_key_header) + keyheader_ptr->key_len +
                           sizeof(struct mmconfig_data_header) + dataheader_ptr->data_len;

        mmconfig_update_checksum(&checksum, (uint8_t*) keyheader_ptr, bytecount);

        /* Move to next record */
        keyheader_ptr = (struct mmconfig_key_header*)
                (dataheader_ptr->data + dataheader_ptr->data_len);
    }

    /* Verify checksum matches */
    if (checksum != partition->checksum)
    {
        return MMCONFIG_ERR_INVALID_PARTITION;
    }

    return MMCONFIG_OK;
}

/**
 * Erases the blocks of flash pointed to by the address and the specified size.
 *
 * Erasing the flash block shall reset all bytes in the flash block to @c MMHAL_FLASH_ERASE_VALUE.
 *
 * @param partition A pointer to the partition start.
 * @param size      The size of the partition in bytes.
 * @return          MMCONFIG_OK on success, negative number on error.
 */
static int mmconfig_erase_partition(struct mmconfig_partition_header *partition, size_t size)
{
    uint32_t block_address = (uint32_t)partition;
    uint32_t end_address = block_address + size;

    while (block_address < end_address)
    {
        /* Sanity check */
        uint32_t block_size = mmhal_flash_getblocksize(block_address);
        MMOSAL_ASSERT(block_size != 0);

        /* This function is expected to erase all bytes in the flash block to @c MMHAL_FLASH_ERASE_VALUE. */
        mmhal_flash_erase(block_address);
        block_address += block_size;
    }

    return MMCONFIG_OK;
}

/** A temporary staging buffer for flash writes. We choose a staging buffer
 *  size of 128 bytes to get the best burst performance from all supported platforms.
 */
static uint8_t mmconfig_staging_buffer[128];

/** An index into the 256 byte buffer showing us where the fill the next few bytes from.*/
static uint32_t mmconfig_buffer_index;

/** A pointer into flash showing us where to flash the staging buffer to next. */
static uint32_t mmconfig_flashing_address;

/**
 * Pushes the string of bytes to staging buffer, if the staging buffer is full
 * then contents of staging buffer are written to flash.
 *
 * @param data  The data to push.
 * @param size  The length of the data in bytes.
 */
static void mmconfig_buffered_write(const uint8_t *data, size_t size)
{
    /* Write to flash if we have atleast 1 full staging buffer */
    while (size >= sizeof(mmconfig_staging_buffer) - mmconfig_buffer_index)
    {
        /* We have enough data to flush the staging buffer to flash */
        memcpy(&mmconfig_staging_buffer[mmconfig_buffer_index], data,
                sizeof(mmconfig_staging_buffer) - mmconfig_buffer_index);

        mmhal_flash_write((uint32_t) mmconfig_flashing_address,
                           mmconfig_staging_buffer, sizeof(mmconfig_staging_buffer));
        data += sizeof(mmconfig_staging_buffer) - mmconfig_buffer_index;
        size -= sizeof(mmconfig_staging_buffer) - mmconfig_buffer_index;
        mmconfig_flashing_address += sizeof(mmconfig_staging_buffer);
        mmconfig_buffer_index = 0;
    }

    /* Write any leftover bytes to staging buffer but not to flash */
    if (size > 0)
    {
        /* Sanity check */
        MMOSAL_ASSERT(mmconfig_buffer_index + size < sizeof(mmconfig_staging_buffer));

        /* Copy unwritten data to the staging area and return */
        memcpy(&mmconfig_staging_buffer[mmconfig_buffer_index], data, size);
        mmconfig_buffer_index += size;
    }
}

/**
 * A wrapper for mmconfig_buffered_write which presents the same parameter list as
 * mmconfig_update_checksum() for use by mmconfig_process_existing_storage().
 *
 * @param checksum Unused
 * @param data  The data to push.
 * @param size  The length of the data in bytes.
 */
static void mmconfig_buffered_write_wrapper(uint32_t *checksum, const uint8_t *data, size_t size)
{
    (void) checksum;

    mmconfig_buffered_write(data, size);
}

/**
 * Initializes the staging buffer and partition header prior to streaming data to flash.
 *
 * @param partition The partition to write to.
 * @param version   The version number of the partition.
 * @param checksum  The pre-computed checksum.
 */
static void mmconfig_start_flashing(struct mmconfig_partition_header *partition,
                                    uint32_t version, uint32_t checksum)
{
    /* Initialise Flashing buffer with erase values */
    memset(mmconfig_staging_buffer, MMHAL_FLASH_ERASE_VALUE, sizeof(mmconfig_staging_buffer));
    mmconfig_buffer_index = 0;
    mmconfig_flashing_address = (uint32_t) partition;

    /* Write partition header */
    struct mmconfig_partition_header partition_header = {
        .signature = MMCONFIG_SIGNATURE,            /* MMCS in little endian */
        .checksum = checksum,                       /* The computed checksum */
        .version = version,                         /* The version number */
    };
    mmconfig_buffered_write((uint8_t*) &partition_header, sizeof(partition_header));
}

/**
 * Flushes any remaining data in staging buffer to flash.
 */
static void mmconfig_end_flashing(void)
{
    /* Write any unwritten data */
    mmhal_flash_write((uint32_t) mmconfig_flashing_address,
                      mmconfig_staging_buffer, mmconfig_buffer_index);
}

/**
 * Works through all keys stored in the flash to identify those unaffected by proposed update.
 *
 * This function contains the common code for comparing each key in the flash against each key in
 * the update list.  If there is no match in the update list then the key and data will be retained
 * unchanged when the update is applied.  Otherwise, the key/data in the flash is skipped over as
 * it is invalidated by the update.
 *
 * The resulting action is taken by the given @c retain_entry_fn function. In practice, this is either:
 *  - mmconfig_update_checksum - to add the key and associated data to the new checksum being
 *    calculated.  This may be a prelude to a write, for which the checksum is needed up front for
 *    the partition header.  Alternatively it may be for the side effect of calculating how much
 *    space will be freed if the update is applied (output in @c keyheader_ptr_out and
 *    @c skipped_key_space_out).
 *
 *  - mmconfig_buffered_write_wrapper - to write the key and associated data to the new partition.
 *    In this case only the write itself is of interest and the checksum is unlikely to be
 *    calculated as it has already been written into the header.  skipped_key_space_out is unlikely
 *    to be of interest at this stage.
 *
 *  In both cases the data pointer and size provided to @c retain_entry_fn is the start of the raw data
 *  in the current flash partition, including the key header, key, data header and data.
 *
 * This function deals only with filtering out keys in the flash that are also in the update list.
 * It is up to the caller to actually add the keys in the update list.
 *
 * @param retain_entry_fn       Function to call when key/data in flash is not in update list
 * @param node_list             Pointer to a linked list of nodes comprising a potential update
 * @param keyheader_ptr_out     Output pointer to the first byte after the final key in the
 *                              current flash partition.  May be used by the caller in conjunction
 *                              with skipped_key_space_out to determine how much space is available
 *                              in the new partition
 * @param checksum_out          Output pointer to the calculated checksum, if supported by
 *                              @c retain_entry_fn
 * @param skipped_key_space_out The number of bytes skipped over because the key was found in the
 *                              update list.
 */
static void mmconfig_process_existing_storage(void(*retain_entry_fn)(uint32_t *checksum,
                                                                     const uint8_t *data,
                                                                     size_t size),
                                              const struct mmconfig_update_node *node_list,
                                              struct mmconfig_key_header **keyheader_ptr_out,
                                              uint32_t *checksum_out,
                                              uint32_t *skipped_key_space_out)
{
    uint32_t skipped_key_space = 0;

    /* Initialise checksum although it may not be computed, depending on retain_entry_fn() */
    uint32_t checksum = XORHASH_SEED;

    /* Provide limited protection against mmconfig_primary_image changing under us */
    struct mmconfig_partition_header *mmconfig_current_image = mmconfig_primary_image;

    /* Find the first keyheader which is at the end of mmconfig_partition_header */
    struct mmconfig_key_header *keyheader_ptr = (struct mmconfig_key_header*)
                                                            (mmconfig_current_image->data);

    /* Loop till we find a terminator marked by key_len of @c LIST_TERMINATOR or 0 */
    while ((keyheader_ptr->key_len != LIST_TERMINATOR) &&
           (keyheader_ptr->key_len != 0) &&
           ((uint32_t) keyheader_ptr < (uint32_t) mmconfig_current_image + mmconfig_partition_size))
    {
        /* Find data header and data */
        struct mmconfig_data_header *dataheader_ptr = (struct mmconfig_data_header *)
                                                    (keyheader_ptr->key + keyheader_ptr->key_len);
        uint8_t *data_ptr = dataheader_ptr->data;

        const struct mmconfig_update_node *node = node_list;

        while (node != NULL)
        {
            /* If the key in the flash matches any node in the update list, exclude it */
            if (mmconfig_key_match(node->key, strlen(node->key),
                                   keyheader_ptr->key, keyheader_ptr->key_len))
            {
                skipped_key_space += sizeof(struct mmconfig_key_header) + keyheader_ptr->key_len +
                                     sizeof(struct mmconfig_data_header) + dataheader_ptr->data_len;
                break;
            }
            node = node->next;
        }

        if (node == NULL)
        {
            /* Key in flash does not match any update node, so compute number of bytes to retain */
            size_t bytecount = sizeof(struct mmconfig_key_header) + keyheader_ptr->key_len +
                               sizeof(struct mmconfig_data_header) + dataheader_ptr->data_len;

            /* Retain key header, key name, data header and data */
            retain_entry_fn(&checksum, (uint8_t*) keyheader_ptr, bytecount);
        }

        /* Move to next record */
        keyheader_ptr = (struct mmconfig_key_header*) (data_ptr + dataheader_ptr->data_len);
    }

    *keyheader_ptr_out = keyheader_ptr;
    *skipped_key_space_out = skipped_key_space;
    *checksum_out = checksum;
}

/**
 * Predicts the checksum after a new key has been added.
 *
 * @param node_list Pointer to a linked list of nodes to be included in the checksum.
 * @param checksum The computed checksum is returned in this.
 * @param bytes_used The number of bytes that are required to store all the data after updates
 * @param bytes_remaining The number of bytes that will still be available after update is applied
 * @return         Returns MMCONFIG_OK on success or an error code.
 *                  For @c MMCONFIG_ERR_FULL, bytes_used and bytes_remaining will be valid
 *                      @c MMCONFIG_ERR_INVALID_KEY if a key in node_list is invalid
 */
static uint32_t mmconfig_compute_new_checksum(const struct mmconfig_update_node *node_list,
                                              uint32_t *checksum,
                                              uint32_t *bytes_used, int32_t *bytes_remaining)
{
    /* Check if we have the space allocated for config store */
    const struct mmhal_flash_partition_config *mmconfig_partition =
        mmhal_get_mmconfig_partition();

    if ((mmconfig_partition == NULL) || (mmconfig_partition->partition_size == 0))
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    struct mmconfig_key_header *keyheader_ptr;
    uint32_t skipped_key_space;

    /* Process existing storage to checksum items to be retained and count up bytes freed
     * for items that will be deleted or replaced.
     */
    mmconfig_process_existing_storage(mmconfig_update_checksum,
                                      node_list, &keyheader_ptr, checksum, &skipped_key_space);

    /* Checksum new and updated data items and check there is sufficient space to store them all */
    const struct mmconfig_update_node *node = node_list;
    size_t required_space = 0;

    while (node != NULL)
    {
        int result = mmconfig_validate_key(node->key);
        if ((result != MMCONFIG_OK) && (result != MMCONFIG_ERR_WILDCARD_KEY))
        {
            return MMCONFIG_ERR_INVALID_KEY;
        }

        if (node->data)
        {
            struct mmconfig_key_header keyheader = {
                .key_len = strlen(node->key),
            };
            struct mmconfig_data_header dataheader = {
                .data_len = node->size,
            };

            required_space += sizeof(keyheader) + keyheader.key_len +
                              sizeof(dataheader) + dataheader.data_len;

            /* Checksum new keyheader and key */
            mmconfig_update_checksum(checksum, (uint8_t*) &keyheader, sizeof(keyheader));
            mmconfig_update_checksum(checksum, (const uint8_t*) node->key, keyheader.key_len);

            /* Checksum new dataheader and data */
            mmconfig_update_checksum(checksum, (uint8_t*) &dataheader, sizeof(dataheader));
            mmconfig_update_checksum(checksum, (const uint8_t*) node->data, node->size);
        }
        node = node->next;
    }

    /* Check that we won't exceed available space in partition */
    uint32_t space_required = ((uint32_t) keyheader_ptr - (uint32_t) mmconfig_primary_image) +
                              (required_space - skipped_key_space);
    uint32_t space_available =  mmconfig_partition_size;

    if (bytes_remaining != NULL)
    {
        *bytes_remaining = space_available - space_required;
    }

    if (bytes_used != NULL)
    {
        *bytes_used = space_required;
    }

    if (space_available < space_required)
    {
        return MMCONFIG_ERR_FULL;
    }

    return MMCONFIG_OK;
}

/**
 * This is where all the magic happens.
 *
 * This function copies the primary image to the secondary image replacing
 * any data that matches the key. If the data pointer is NULL then the key
 * is deleted if found. The version is incremented.
 *
 * @param node_list     Pointer to a linked list of nodes to be included in the
 *                      update.
 * @return      MMCONFIG_OK on success.
 */
static int mmconfig_update_secondary_image(const struct mmconfig_update_node *node_list)
{
    uint32_t checksum = 0;
    uint32_t bytes_used;
    int32_t bytes_remaining;
    int retval;

    /* Compute new checksum, this also checks if we have enough space */
    retval = mmconfig_compute_new_checksum(node_list, &checksum, &bytes_used, &bytes_remaining);
    if (retval != MMCONFIG_OK)
    {
        return retval;
    }

    /* Erase secondary partition */
    mmconfig_erase_partition(mmconfig_secondary_image, mmconfig_partition_size);

    /* Setup new flash partition */
    mmconfig_start_flashing(mmconfig_secondary_image,
                            mmconfig_primary_image->version + 1,
                            checksum);

    struct mmconfig_key_header *keyheader_ptr;
    uint32_t skipped_key_space;
    uint32_t rechecksum;

    /* Process existing storage to copy unchanged items to the secondary partition */
    mmconfig_process_existing_storage(mmconfig_buffered_write_wrapper,
                                      node_list, &keyheader_ptr, &rechecksum, &skipped_key_space);

    /* We have copied primary partition to secondary excluding deleted or updated keys.
     * Now add the new key data effectively replacing the old key data. Don't do anything
     * if data is NULL - this effectively deletes the key.
     */
    const struct mmconfig_update_node *node = node_list;

    while (node != NULL)
    {
        if (node->data)
        {
            struct mmconfig_key_header keyheader = {
                .key_len = strlen(node->key),
            };
            struct mmconfig_data_header dataheader = {
                .data_len = node->size,
            };

            /* Push new keyheader and key */
            mmconfig_buffered_write((uint8_t*) &keyheader, sizeof(keyheader));
            mmconfig_buffered_write((const uint8_t*) node->key, keyheader.key_len);

            /* Push new dataheader and data */
            mmconfig_buffered_write((uint8_t*) &dataheader, sizeof(dataheader));
            mmconfig_buffered_write((const uint8_t*) node->data, node->size);
        }

        node = node->next;
    }

    /* Write 1 byte of LIST_TERMINATOR in case Flash does not use Erase value of 0xFF */
    uint8_t terminator = LIST_TERMINATOR;
    mmconfig_buffered_write(&terminator, sizeof(terminator));

    /* All done, now finalise the operation */
    mmconfig_end_flashing();

    /* Now check that everything was written correctly */
    if (mmconfig_validate_partition(mmconfig_secondary_image) == MMCONFIG_OK)
    {
        /* Now swap partitions as the data we wrote is the latest */
        struct mmconfig_partition_header *tmp_partition = mmconfig_secondary_image;
        mmconfig_secondary_image = mmconfig_primary_image;
        mmconfig_primary_image = tmp_partition;
    }

    return MMCONFIG_OK;
}

/**
 * Initialize the internal structures of the mmconfig API.
 *
 * This will also perform a validity check of the flash contents at the provided address
 * and erase the config store using @ref mmconfig_eraseall() if a valid signature
 * and matching checksum could not be found in either of the 2 copies of data.
 *
 * @return @c MMCONFIG_OK if an existing persistent store was found in flash.
 *         @c MMCONFIG_DATA_ERASED if the flash was newly initialized, in which case
 *         the application may, for example, choose to pre-load the persistent store
 *         with some initial data.
 */
static int mmconfig_init(void)
{
    int retval = MMCONFIG_OK;

    /* If we have already been initialized, just return */
    if (mmconfig_mutex != NULL)
    {
        return retval;
    }

    const struct mmhal_flash_partition_config *mmconfig_partition =
        mmhal_get_mmconfig_partition();

    /* Check if we have the space allocated for config store */
    if ((mmconfig_partition == NULL) || (mmconfig_partition->partition_size == 0))
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    /* Ensure we have valid flash at the address range passed */
    MMOSAL_ASSERT(mmhal_flash_getblocksize(mmconfig_partition->partition_start) != 0);
    MMOSAL_ASSERT(mmhal_flash_getblocksize(mmconfig_partition->partition_start +
                                           mmconfig_partition->partition_size - 1) != 0);

    /* We only support memory mapped flash for now */
    MMOSAL_ASSERT(mmconfig_partition->not_memory_mapped == false);

    /* Create a mutex */
    mmconfig_mutex = mmosal_mutex_create("MMCONFIG_MUTEX");

    /* We keep 2 logical 'partitions' of MMCONFIG in the allocated flash partition */
    mmconfig_partition_size = mmconfig_partition->partition_size / 2;

    /* Start off assigning the first partition as primary partition */
    mmconfig_primary_image =
        (struct mmconfig_partition_header *) mmconfig_partition->partition_start;
    mmconfig_secondary_image =
        (struct mmconfig_partition_header *) (mmconfig_partition->partition_start +
                                              mmconfig_partition_size);

    if (mmconfig_validate_partition(mmconfig_primary_image) == MMCONFIG_OK)
    {
        /* First partition is good, validate second */
        if (mmconfig_validate_partition(mmconfig_secondary_image) == MMCONFIG_OK)
        {
            /* Both partitions good, see which is newer */
            /* Don't worry about rollover, flash will die long before we reach 2^32 writes */
            if (mmconfig_secondary_image->version > mmconfig_primary_image->version)
            {
                /* Ooh, secondary is actually newer, swap them */
                mmconfig_primary_image = mmconfig_secondary_image;
                mmconfig_secondary_image =
                    (struct mmconfig_partition_header *) mmconfig_partition->partition_start;
            }
        }
        /* At this point nobody cares if secondary partition is corrupt */
    }
    else if (mmconfig_validate_partition(mmconfig_secondary_image) == MMCONFIG_OK)
    {
        /* Primary is corrupt, but secondary is good, so swap them */
        mmconfig_primary_image = mmconfig_secondary_image;
        mmconfig_secondary_image =
            (struct mmconfig_partition_header *) mmconfig_partition->partition_start;
    }
    else
    {
        /* No Valid partitions found, so initialise both */
        mmconfig_eraseall();
        retval = MMCONFIG_DATA_ERASED;
    }
    return retval;
}

int mmconfig_eraseall(void)
{
    /* Stage initial data to write to erased flash */
    struct mmconfig_partition_header partition_header = {
        .signature = MMCONFIG_SIGNATURE,
        .version = 0,
        .checksum = XORHASH_SEED,
    };

    /* Ensure MMCONFIG subsystem is initialized */
    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    /* If we are not initialised yet, things are really bad */
    MMOSAL_ASSERT(mmconfig_mutex != NULL);

    /* Take the mutex, who knows what other tasks are doing... */
    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    /* Erase primary partition */
    mmconfig_erase_partition(mmconfig_primary_image, mmconfig_partition_size);

    /* Erase secondary partition */
    mmconfig_erase_partition(mmconfig_secondary_image, mmconfig_partition_size);

    /* Write to both partitions */
    mmhal_flash_write((uint32_t)mmconfig_primary_image,
                      (uint8_t*)&partition_header, sizeof(partition_header));
    mmhal_flash_write((uint32_t)mmconfig_secondary_image,
                      (uint8_t*)&partition_header, sizeof(partition_header));

    /* All done, release mutex */
    mmosal_mutex_release(mmconfig_mutex);

    return MMCONFIG_OK;
}

/**
 * Reads data identified by the supplied key from persistent memory returning a pointer.
 * @note This is an internal function.
 *
 * @param key           Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param data          Returns a live pointer to the data in flash memory.  It is the callers
 *                      responsibility to consume it immediately or take a copy as this pointer
 *                      will be invalidated on the next config store write.
 *                      Returns NULL on any error.
 * @return              Returns number of bytes read and allocated on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          Other negative number for other errors.
 */
static int mmconfig_read_data(const char *key, void **data)
{
    /* Ensure MMCONFIG subsystem is initialized */
    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    /* mmconfig_mutex should have already been acquired before calling this internal function */

    /* Ensure key is valid */
    if (mmconfig_validate_key(key) != MMCONFIG_OK)
    {
        return MMCONFIG_ERR_INVALID_KEY;
    }

    /* Provide limited protection against mmconfig_primary_image changing under us */
    struct mmconfig_partition_header *mmconfig_current_image = mmconfig_primary_image;

    /* Find the first keyheader which is at the end of mmconfig_partition_header */
    struct mmconfig_key_header *keyheader_ptr =
            (struct mmconfig_key_header*) (mmconfig_current_image->data);

    /* Loop till we find a terminator marked by key_len of @c LIST_TERMINATOR */
    while ((keyheader_ptr->key_len != LIST_TERMINATOR) &&
           (keyheader_ptr->key_len != 0) &&
           ((uint32_t) keyheader_ptr < (uint32_t) mmconfig_current_image + mmconfig_partition_size))
    {
        uint8_t *data_ptr;
        char *keyname_ptr = keyheader_ptr->key;

        struct mmconfig_data_header *dataheader_ptr =
                (struct mmconfig_data_header *) (keyname_ptr + keyheader_ptr->key_len);
        data_ptr = dataheader_ptr->data;

        if (mmconfig_key_match(key, strlen(key), keyname_ptr, keyheader_ptr->key_len))
        {
            if (data)
            {
                *data = (void *) data_ptr;
            }
            return dataheader_ptr->data_len;
        }

        /* Move to next record */
        keyheader_ptr = (struct mmconfig_key_header*) (data_ptr + dataheader_ptr->data_len);
    }

    return MMCONFIG_ERR_NOT_FOUND;
}

int mmconfig_alloc_and_load(const char *key, void **data)
{
    int length;
    void *livedata;

    /* So we return NULL on error */
    *data = NULL;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    /* Look for the key in config store */
    length = mmconfig_read_data(key, &livedata);

    if (length < 0)
    {
        /* We got an error */
        goto mmconfig_alloc_and_load_cleanup;
    }

    /* Allocate memory */
    *data = mmosal_malloc(length);
    if (*data == NULL)
    {
        length = MMCONFIG_ERR_INSUFFICIENT_MEMORY;
        goto mmconfig_alloc_and_load_cleanup;
    }

    memcpy(*data, livedata, length);

mmconfig_alloc_and_load_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return length;
}

int mmconfig_write_data(const char *key, const void *data, size_t size)
{
    void *currvalue = NULL;

    /* Ensure MMCONFIG subsystem is initialized */
    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    /* Ensure key is valid */
    int result = mmconfig_validate_key(key);

    switch (result)
    {
    case MMCONFIG_OK:
        /* Skip write if key already exists and has the same data */
        mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);
        result = mmconfig_read_data(key, &currvalue);
        mmosal_mutex_release(mmconfig_mutex);

        if ((result >= 0) && ((size_t)result == size))
        {
            if ((size == 0) || (memcmp(currvalue, data, size) == 0))
            {
                return MMCONFIG_OK;
            }
        }
        break;

    case MMCONFIG_ERR_WILDCARD_KEY:
        if (data == NULL)
        {
            /* WILDCARD_KEY is acceptable only for deletions */
            break;
        }
        return MMCONFIG_ERR_INVALID_KEY;

    default:
        return MMCONFIG_ERR_INVALID_KEY;
    }

    /* Create a temporary node for this single write operation */
    struct mmconfig_update_node node;
    /* Need to discard "const" here but mmconfig_write_update_node_list() does
     * nothing other than copy the data to flash
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    node.key = (char *) key;
    node.data = (void *) data;
#pragma GCC diagnostic pop
    node.size = size;
    node.next = NULL;

    return mmconfig_write_update_node_list(&node);
}

int mmconfig_write_update_node_list(const struct mmconfig_update_node *node_list)
{
    int retval = MMCONFIG_OK;

    /* Ensure MMCONFIG subsystem is initialized */
    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    if (node_list == NULL)
    {
        /* Nothing to do */
        return retval;
    }

    /* Take the mutex, who knows what other tasks are doing... */
    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    /* Update the secondary image, this switches primary and secondary images */
    retval = mmconfig_update_secondary_image(node_list);

    /* Uncomment this line to update both partition images on each write,
     * this ensures that if the primary gets corrupt, the latest value is in secondary too.
     * The downside of this is that flash life is effectively halved.
     */
    // retval = mmconfig_update_secondary_image(node_list);

    /* All done, release mutex */
    mmosal_mutex_release(mmconfig_mutex);

    return retval;
}

int mmconfig_read_bytes(const char *key, void *buffer,  uint32_t buffsize, uint32_t offset)
{
    int length;
    uint8_t *data;
    int result;
    int copied;

    result = mmconfig_init();
    if (result == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    /* Look for the key in config store */
    length = mmconfig_read_data(key, (void**)&data);

    if (length < 0)
    {
        /* We got an error */
        result = length;
        goto mmconfig_read_bytes_cleanup;
    }

    if (offset > (uint32_t)length)
    {
        result = MMCONFIG_ERR_OUT_OF_BOUNDS;
        goto mmconfig_read_bytes_cleanup;
    }


    /* If buffer is NULL, just return the length required */
    if (buffer == NULL)
    {
        result = length;
        goto mmconfig_read_bytes_cleanup;
    }

    copied = buffsize < (length - offset) ? buffsize : length - offset;
    memcpy(buffer, &data[offset], copied);
    result = copied;

mmconfig_read_bytes_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return result;
}

int mmconfig_write_string(const char *key, const char *value)
{
    /* Treat the string as raw data including the NULL terminator */
    return mmconfig_write_data(key, (const void *) value, strlen(value) + 1);
}

int mmconfig_read_string(const char *key, char *buffer, int bufsize)
{
    char *value;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);
    int retval =  mmconfig_read_data(key, (void**) &value);

    /* Check for error */
    if (retval < 0)
    {
        goto mmconfig_read_string_cleanup;
    }

    /* Treat a NULL data (0 length) as invalid, (not same as a NULL string "") */
    if (retval == 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_string_cleanup;
    }

    /* Check for NULL termination */
    if (value[retval - 1] != 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_string_cleanup;
    }

    /* If buffer is NULL just return the number of bytes required */
    if (buffer)
    {
        /* Check for sufficient buffer */
        if (retval > bufsize)
        {
            retval = MMCONFIG_ERR_INSUFFICIENT_MEMORY;
            goto mmconfig_read_string_cleanup;
        }

        /* All good, do it */
        memcpy(buffer, value, retval);
    }

mmconfig_read_string_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return retval;
}

int mmconfig_write_int(const char *key, int value)
{
    char str[16];

    /* For maximum compatibility, we are going to represent the integer as a string */
    return mmconfig_write_string(key, mmconfig_int_to_str(str, sizeof(str), value));
}

int mmconfig_read_int(const char *key, int *value)
{
    /* For maximum compatibility, we are going to represent the integer as a string */
    char *data;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);
    int retval =  mmconfig_read_data(key, (void**) &data);

    /* Check for error */
    if (retval < 0)
    {
        goto mmconfig_read_int_cleanup;
    }

    /* Treat a NULL data (0 length) as invalid */
    if (retval == 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_int_cleanup;
    }

    /* Check for NULL termination */
    if (data[retval - 1] != 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_int_cleanup;
    }

    /* Try to convert to int */
    if (mmconfig_str_to_int(data, value) == MMCONFIG_OK)
    {
        retval = MMCONFIG_OK;
        goto mmconfig_read_int_cleanup;
    }

    retval = MMCONFIG_ERR_INCORRECT_TYPE;

mmconfig_read_int_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return retval;
}

int mmconfig_write_uint32(const char *key, uint32_t value)
{
    char str[16];

    /* For maximum compatibility, we are going to represent the integer as a string */
    return mmconfig_write_string(key, mmconfig_uint_to_str(str, sizeof(str), value));
}

int mmconfig_read_uint32(const char *key, uint32_t *value)
{
    /* For maximum compatibility, we are going to represent the value as a string */
    char *data;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    int retval =  mmconfig_read_data(key, (void**) &data);

    /* Check for error */
    if (retval < 0)
    {
        goto mmconfig_read_uint32_cleanup;
    }

    /* Treat a NULL data (0 length) as invalid */
    if (retval == 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_uint32_cleanup;
    }

    /* Check for NULL termination */
    if (data[retval - 1] != 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_uint32_cleanup;
    }

    /* Is it a hexadecimal or plain numeric string */
    if (mmconfig_str_to_uint(data, value) == MMCONFIG_OK)
    {
        retval = MMCONFIG_OK;
        goto mmconfig_read_uint32_cleanup;
    }

    retval = MMCONFIG_ERR_INCORRECT_TYPE;

mmconfig_read_uint32_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return retval;
}

int mmconfig_write_bool(const char *key, bool value)
{
    /* For maximum compatibility, we are going to represent the integer as a string */
    if (value)
    {
        return mmconfig_write_string(key, "true");
    }
    else
    {
        return mmconfig_write_string(key, "false");
    }
}

int mmconfig_read_bool(const char *key, bool *value)
{
    /* For maximum compatibility, we are going to represent the integer as a string */
    char *data;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);

    int retval =  mmconfig_read_data(key, (void**) &data);

    /* Check for error */
    if (retval < 0)
    {
        goto mmconfig_read_bool_cleanup;
    }

    /* Treat a NULL data (0 length) as invalid */
    if (retval == 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_bool_cleanup;
    }

    /* Single byte encoded bool */
    if (retval == 1)
    {
        /* Non zero is true */
        *value = (data[0] != 0);
        goto mmconfig_read_bool_cleanup;
    }

    /* Length > 1, must be a string, check for NULL termination */
    if (data[retval - 1] != 0)
    {
        retval = MMCONFIG_ERR_INCORRECT_TYPE;
        goto mmconfig_read_bool_cleanup;
    }

    if (strcasecmp(data, "true") == 0)
    {
        *value = true;
        retval = MMCONFIG_OK;
        goto mmconfig_read_bool_cleanup;
    }

    if (strcasecmp(data, "false") == 0)
    {
        *value = false;
        retval = MMCONFIG_OK;
        goto mmconfig_read_bool_cleanup;
    }

    /* Try to convert to int */
    int tmp;
    if (mmconfig_str_to_int(data, &tmp) == MMCONFIG_OK)
    {
        *value = (tmp != 0);
        retval = MMCONFIG_OK;
        goto mmconfig_read_bool_cleanup;
    }

    /* Didn't match anything we could interpret as bool */
    retval = MMCONFIG_ERR_INCORRECT_TYPE;

mmconfig_read_bool_cleanup:
    mmosal_mutex_release(mmconfig_mutex);

    return retval;
}

int mmconfig_check_usage(const struct mmconfig_update_node *node_list,
                         uint32_t *bytes_used, int32_t *bytes_remaining)
{
    uint32_t checksum;
    int result;

    if (mmconfig_init() == MMCONFIG_ERR_NOT_SUPPORTED)
    {
        return MMCONFIG_ERR_NOT_SUPPORTED;
    }

    mmosal_mutex_get(mmconfig_mutex, UINT32_MAX);
    /* Use checksum computation to find how much space is needed. node_list
     * may be NULL if caller only wants to know how much space is currently free
     */
    result = mmconfig_compute_new_checksum(node_list, &checksum, bytes_used, bytes_remaining);
    mmosal_mutex_release(mmconfig_mutex);
    return result;
}
