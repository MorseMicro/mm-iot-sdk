/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup MMCONFIG  Morse Micro Persistent Configuration Store
 *
 * This provides key/value based persistent storage. The @ref MMCONFIG_API is provided to access the
 * values stored in the @ref MMCONFIG. An additional set of @ref LOADCONFIG simplify loading from
 * persistent store into commonly used data structures.
 *
 * Storage format
 * ==============
 * Two equal sized copies of persistent storage are stored in separate flash partitions. One is
 * designated primary - this is the highest versioned valid partition. Whenever data needs to be
 * written, the secondary is updated and then the pointers are switched to make the now updated
 * secondary the primary. This ensures that primary always points to a valid and latest copy. For
 * redundancy, this process is done twice so that both primary and secondary are up to date. Flash
 * life can be doubled by doing this only once with the disadvantage that if primary partition gets
 * corrupted then the secondary data will be stale.
 *
 * Partition Header
 * ----------------
 * Each Flash partition consists of a header containing the following data:
 * @code
 * |---------------------|------------------------------------------------------------------|
 * | Signature (32 bits) | This is the text ‘MMCS’ in little Endian format                  |
 * |---------------------|------------------------------------------------------------------|
 * | Version (32 bits)   | This is the version number of the store. It starts at 0 and is   |
 * |                     | incremented with every update.                                   |
 * |---------------------|------------------------------------------------------------------|
 * | Checksum (32 bits)  | This is a bytewise sum of every byte following the checksum to   |
 * |                     | the last item in the list.                                       |
 * |---------------------|------------------------------------------------------------------|
 * @endcode
 *
 * The partition header is immediately followed by a sequential list of key value pairs as shown
 * below. The list is terminated when a @c 0xFF is read instead of a key value pair.
 * @code
 * |-------|-----------|---------|----------|---------------------------|-------------------|
 * |       | Signature | Version | Checksum | Key/Value Pairs (repeats) | End Marker (0xFF) |
 * |-------|-----------|---------|----------|---------------------------|-------------------|
 * | Bytes |    4      |    4    |     4    |             n             |          1        |
 * |-------|-----------|---------|----------|---------------------------|-------------------|
 * @endcode
 *
 * Key-Value pair
 * --------------
 * ### Keys
 * Keys are unique in each copy of the persistent store and are case insensitive. A key must start
 * with an alphabet [A..Z,a..z]. Subsequent letters in the key may contain alphanumeric characters,
 * underscores or periods[A..Z,a..z,0..9,_,.]. The reasoning for this is to allow them to be used as
 * well defined identifiers in a future scripting or CLI implementation.
 *
 * A key ending with an asterisk '*' is accepted for deletions only (i.e. the data is NULL).  Any
 * key matching up to the asterisk will be deleted.
 *
 * A key starts with a single byte indicating the length of the key in bytes followed by the
 * characters of the key itself. The key must not be null terminated.
 *
 * A length value of @c 0xFF indicates end of the list. The @c 0xFF is excluded from the checksum
 * calculation.
 *
 * ### Values
 * Values are streams of raw data bytes and may be opaque binary data or NULL terminated strings.
 * The Data may be up to 65279 bytes in length. (A length of @c 0xFFxx could mean a partially
 * programmed flash so is prohibited as it may cause buffer overruns)
 *
 * Values start with a 16bit value indicating the length of the data stream followed immediately by
 * the raw data bytes.
 *
 * Supported data types
 * --------------------
 * Internally the persistent store stores all data as raw bytes of opaque binary data. However,
 * for user convenience, we provide helper functions that can convert from common C data types
 * to the binary format and back. Some of the data types we support are described below.
 *
 * ### Binary data
 * The @ref mmconfig_read_bytes() and @ref mmconfig_write_data() function calls can read and
 * write arbitrary binary data like arrays and C data structures into the persistent store.
 * However, please be aware that if you store data structures in raw binary format there is a
 * risk that future versions of the software will be incompatible if the data structure changes.
 *
 * ### Strings
 * The @ref mmconfig_read_string() and @ref mmconfig_write_string() function calls can be used
 * to read and write NULL terminated strings into the persistent store. @ref mmconfig_read_string()
 * will return an error if attempting to read any data from the persistent store whose last byte
 * is not 0 as the function determines that this is not a valid NULL terminated string.
 *
 * ### Unsigned integers
 * Unsigned integers are stored as a string for maximum compatibility - this removes confusion
 * about word width and endianness. The API can be expanded to cater to wider integers and
 * even floating point numbers if needed. The functions @ref mmconfig_read_uint32() and
 * @ref mmconfig_write_uint32() allow the user to read and write 32 bit (and smaller) unsigned
 * integers to/from the persistent store. The @ref mmconfig_read_uint32() will return an error if
 * the data being read is not a NULL terminated string or contains characters that are invalid
 * in an unsigned decimal or hexadecimal number. This function supports reading integers in
 * hexadecimal format if it detects a string starting with @c 0x.
 *
 * ### Signed integers
 * Signed integers are stored as a string for maximum compatibility. @ref mmconfig_read_int()
 * and @ref mmconfig_write_int() allow the user to read and write signed integers to/from
 * the persistent store. These functions simply test for a negative number and then call the
 * unsigned versions of the conversion functions above after correcting for the sign.
 * The @ref mmconfig_read_int() will return an error if the data being read is not
 * a NULL terminated string or contains characters that are invalid in a signed  decimal or
 * hexadecimal number. This function supports reading integers in hexadecimal format if it
 * detects a string starting with @c 0x or @c -0x.
 *
 * ### Boolean
 * @ref mmconfig_read_bool() and @ref mmconfig_write_bool() functions can be used to read and
 * write boolean data types the persistent store. Once again for maximum compatibility the data
 * is represented as strings in persistent store. @ref mmconfig_write_bool() simply writes the
 * strings `true` or `false` to the persistent store depending on the boolean value being written.
 * @ref mmconfig_read_bool() is a bit more accommodating and in addition to the case insensitive
 * strings `true` and `false` also returns true for a non zero numeric string.
 *
 * @note Since the above functions with the exception of the binary data functions all store the
 * data in a string representation, it is possible to write the data in one format and read it in
 * another. For example you can use @ref mmconfig_write_int() to write an integer and then use
 * @ref mmconfig_read_string() to read it back as a string and vice versa. You can also choose not
 * to use these conversion functions at all and use only the raw @ref mmconfig_read_bytes() and
 * @ref mmconfig_write_data() functions, in which case the string conversion functions will
 * be optimized out giving you considerable code size savings provided you also exclude the helper
 * functions in @ref mm_app_loadconfig.h which are used in @c mm_app_common.c.
 *
 *
 * Operations
 * ==========
 *
 * Initialization
 * --------------
 *
 * On startup the system will scan both partitions looking for the signature value. If found, it
 * will then perform a checksum calculation by summing all bytes after the partition header till if
 * finds a @c 0xFF key length, it then compares this computed checksum with the checksum stored in
 * the header.
 *
 * If both partitions contain a valid signature and matching checksum, then the partition with the
 * higher version number is designated the primary partition and the other is designated the
 * secondary. If only one is valid then that is designated the primary partition and the other is
 * designated the secondary. If neither are valid then both partitions are erased and the header is
 * written to both with version number 0. Since the first byte following the header is @c 0xFF this
 * is treated as an empty list.
 *
 * Writing a new Key-Value pair
 * ----------------------------
 *
 * To write a new key value pair the system first erases the secondary partition. Then it copies all
 * key-value pairs from the primary partition to the secondary partition. If a key with the same
 * name as the new key is found then it is excluded from the copy. The system then appends the new
 * Key-Value pair to the end of the list. If the new data is NULL then it skips writing the new
 * Key-Value pair effectively deleting the named key. Once this is done, the new checksum is
 * computed and the header is written after incrementing the version number by 1. The written data
 * is then validated and if correct the partitions are swapped and the newly written partition
 * becomes the primary.
 *
 * Reading Data
 * ------------
 *
 * To read data we simply scan the primary partition Key-Value by Key-Value till we find the
 * requested key or run into the @c 0xFF marker signifying end of the list.
 *
 * Programming the config store from a host PC {#MMCONFIG_PROGRAMMING}
 * ===========================================
 *
 * This section provides instructions for programming the config store from a host PC via the
 * command line. Alternatively the config store may be programmed through the Platform IO UI.
 *
 * - Connect the target on USB and start OpenOCD in a terminal by running the following command from
 *   the `framework` directory. If OpenOCD starts successfully it will print some information
 *   messages and then pause waiting for a connection. If OpenOCD exits then this indicates that an
 *   error was encountered while starting (for example, the device was not connected or another
 *   OpenOCD instance was already running).
 *   @code
 *   openocd -f src/platforms/mm-ekh08-u575/openocd.cfg
 *   @endcode
 *   Replace @c mm-ekh08-u575 with the actual name of your platform.
 *
 * - In a separate terminal, also from the `framework` directory, run the following command to load
 *   the keys to the device:
 *   @code
 *   pipenv run ./tools/platform/program-configstore.py -H localhost -p mm-ekh08-u575 write-json
 *   <config.hjson>
 *   @endcode
 *   Replace @c mm-ekh08-u575 with the actual name of your platform, and substitute `<config.hjson>`
 *   with the path to the configuration file (e.g., `../examples/<app>/config.hjson` where `<app>`
 *   is the name of the appropriate application directory).
 *
 * - You can now stop OpenOCD by returning to the original terminal and pressing @c CTRL-C.
 */

/**
 * @ingroup MMCONFIG
 * @defgroup MMCONFIG_API Morse Micro Persistent Configuration Store API
 *
 * This API provides functionality for managing configuration data in a key/value store
 * with a flash backend.
 *
 * @{
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The maximum length of a key in bytes */
#define MMCONFIG_MAX_KEYLEN             32

/** Return & error codes */
enum mmconfig_result
{
    MMCONFIG_OK                      = 0,   /**< Operation completed successfully */
    MMCONFIG_DATA_ERASED             = 1,   /**< Partition was erased */
    MMCONFIG_ERR_INVALID_KEY         = -1,  /**< Key provided was invalid */
    MMCONFIG_ERR_FULL                = -2,  /**< Config store is full */
    MMCONFIG_ERR_NOT_FOUND           = -3,  /**< Requested key was not found */
    MMCONFIG_ERR_INCORRECT_TYPE      = -4,  /**< Requested data type did not match found data */
    MMCONFIG_ERR_INVALID_PARTITION   = -5,  /**< Valid partition was not found */
    MMCONFIG_ERR_INSUFFICIENT_MEMORY = -6,  /**< Insufficient memory */
    MMCONFIG_ERR_OUT_OF_BOUNDS       = -7,  /**< Offset was out of bounds */
    MMCONFIG_ERR_NOT_SUPPORTED       = -8,  /**< Operation not supported */
    MMCONFIG_ERR_WILDCARD_KEY        = -9   /**< Key contains wildcard valid only for deletion */
};

/**
 * Update node structure
 *
 * Used to make a list of raw updates to write to the flash in a single operation.
 */
struct mmconfig_update_node
{
    /** The key, which should be pre-validated using mmconfig_validate_key(),
     * unless it is to be used for multiple deletions and so ends in an
     * asterisk '*'
     */
    char *key;

    /** Pointer to the data. May be NULL to indicate deletion.
     */
    void *data;

    /** Size of the data. May be zero to indicate deletion. */
    size_t size;

    /** Pointer to the next node in the list */
    struct mmconfig_update_node *next;
};

/**
 * Erases all flash blocks allocated to persistent storage and write the signature
 * at the 2 copies in flash.
 *
 * @return It returns 0 on success or an error code on failure.
 */
int mmconfig_eraseall(void);

/**
 * Writes the raw data to persistent store location identified by key.
 *
 * If there is already data with the same key (ignoring case) then it will be replaced.
 *
 * @param  key  Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  data The binary data to be written, can contain nulls and need not be null
 *                      terminated. Can be a pointer to a structure or any arbitrary data to
 *                      be written.
 * @param  size The size of the binary data to be written.
 * @return      Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_FULL if insufficient space to write data
 *                          Other negative number for other errors.
 */
int mmconfig_write_data(const char *key, const void *data, size_t size);

/**
 * Deletes the specified key(s) from persistent store.
 *
 * @param  key Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      For deletion a wildcard (*) may be specified at the end to delete
 *                      multiple keys in one go. Must be a null terminated string.
 * @return     Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_FULL if insufficient space to write data
 *                          Other negative number for other errors.
 */
static inline int mmconfig_delete_key(const char *key)
{
    return mmconfig_write_data(key, NULL, 0);
}

/**
 * Writes all updates from the update node list to persistent store.
 *
 * If there is already data with the same key (ignoring case) then it will be replaced.
 * If data is NULL then it will be deleted.
 *
 * @note It is up to the caller to ensure that there is only one non-NULL data
 * item per key, otherwise both entries will be written.  The caller retains
 * ownership of the list and is responsible for freeing the nodes after the
 * write.
 *
 * @param  node_list Pointer to a linked list of nodes to be included in the
 *                      update.
 *
 * @return           Returns @c MMCONFIG_OK on success. On error, no update
 *                      takes place and the return code is:
 *                          @c MMCONFIG_ERR_INVALID_KEY if a key in the list is invalid
 *                          @c MMCONFIG_ERR_FULL if insufficient space to write data
 *                          Other negative number for other errors.
 */
int mmconfig_write_update_node_list(const struct mmconfig_update_node *node_list);

/**
 * Writes the null terminated string to persistent store location identified by key.
 *
 * If there is already data with the same key (ignoring case) then it will be replaced.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value The data to be written, must be a null terminated string.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_FULL if insufficient space to write data
 *                          Other negative number for other errors.
 */
int mmconfig_write_string(const char *key, const char *value);

/**
 * Returns the persistent store string value identified by the key.
 *
 * @note If you use @c mmconfig_read_string() to read data written by @c mmconfig_write_data()
 *       then it could fail if the data is not null terminated.
 *
 * @param  key     Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  buffer  Buffer to read the string into.
 *                      On error, the buffer is untouched, so can be preloaded with a default value.
 * @param  bufsize Length of buffer.
 * @return         Returns length of string read on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_INCORRECT_TYPE if the data pointed to by the key is not
 *                          a null terminated string
 *                          @c MMCONFIG_ERR_INSUFFICIENT_MEMORY if the buffer size is insufficient
 *                          Other negative number for other errors.
 */
int mmconfig_read_string(const char *key, char *buffer, int bufsize);

/**
 * Allocates memory and loads the data from persistent memory into it returning a pointer.
 *
 * It is the responsibility of the caller to free the memory when it is no longer required.
 *
 * @param  key  Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  data Returns a pointer to allocated memory loaded with the key value.
 *                      Returns NULL on any error.
 * @return      Returns number of bytes read and allocated on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_INSUFFICIENT_MEMORY if memory could not be allocated
 *                          Other negative number for other errors.
 */
int mmconfig_alloc_and_load(const char *key, void **data);

/**
 * Converts the given integer to a string and writes to persistent store.
 *
 * If there is already data with the same key then it will be replaced.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value The integer to be written, it is converted to string and written.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          Other negative number for other errors.
 */
int mmconfig_write_int(const char *key, int value);

/**
 * Returns the integer stored in persistent store identified by the key.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value Returns the integer in this, it is converted from string.
 *                      On error, the value is untouched, so can be preloaded with a default.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_INCORRECT_TYPE if the data pointed to by the key is not
 *                          an integer represented as a string
 */
int mmconfig_read_int(const char *key, int *value);

/**
 * Converts the given unsigned integer to a string and writes to persistent store.
 *
 * If there is already data with the same key then it will be replaced.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value The unsigned integer to be written, it is converted to string and written.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          Other negative number for other errors.
 */
int mmconfig_write_uint32(const char *key, uint32_t value);

/**
 * Returns the unsigned integer stored in persistent store identified by the key.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value Returns the unsigned integer in this, it is converted from string.
 *                      On error, the value is untouched, so can be preloaded with a default.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_INCORRECT_TYPE if the data pointed to by the key is not
 *                          an unsigned integer or hexadecimal number represented as a string
 */
int mmconfig_read_uint32(const char *key, uint32_t *value);

/**
 * Converts the given boolean to a string and writes to persistent store.
 *
 * If there is already data with the same key then it will be replaced.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value The boolean value to be written, it is converted to string and written.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          Other negative number for other errors.
 */
int mmconfig_write_bool(const char *key, bool value);

/**
 * Returns the boolean value stored in persistent store identified by the key.
 *
 * @param  key   Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  value Returns the boolean value stored, it is converted from string but may even be
 *                      a single byte raw boolean value.  The string representation may be
 *                      "true"/"false" or a "0"/non zero integer string.
 *                      On error, the value is untouched, so can be preloaded with a default.
 * @return       Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_INCORRECT_TYPE if the data pointed to by the key is not
 *                          an integer represented as a string
 */
int mmconfig_read_bool(const char *key, bool *value);

/**
 * Returns the persistent store data identified by the key.
 * @param  key      Identifies the data element in persistent storage and is a
 *                      case insensitive alphanumeric (plus underscore) string starting
 *                      with an alpha. Same rules as a C variable name, but case insensitive.
 *                      Must be a null terminated string.
 * @param  buffer   A pointer to a pre-allocated buffer to return the data address in.
 * @param  buffsize The length of the buffer.
 * @param  offset   An offset into the source from which to copy the data into the buffer.
 * @return          Returns length of data copied on success. On error returns:
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 *                          @c MMCONFIG_ERR_NOT_FOUND if the specified key was not found
 *                          @c MMCONFIG_ERR_OUT_OF_BOUNDS if the offset is out of bounds
 *                          Other negative number for other errors.
 */
int mmconfig_read_bytes(const char *key, void *buffer, uint32_t buffsize, uint32_t offset);

/**
 * Validates an entire key intended for data storage
 *
 * @param  key The key to validate
 * @return     Returns @c MMCONFIG_OK if the key is valid, or
 *                          @c MMCONFIG_ERR_INVALID_KEY if key is invalid
 * @note If the key ends in an asterisk '*' it will fail this validation.
 *       However, it is still valid for deleting multiple keys (where any key
 *       matching up to the asterisk will be deleted).
 */
int mmconfig_validate_key(const char *key);

/**
 * Validates a single character intended to make up a key for data storage
 *
 * @param  character The character to validate
 * @return           Returns @c MMCONFIG_OK if the character is valid, or
 *                          @c MMCONFIG_ERR_INVALID_KEY if character is invalid
 *
 * @note Additional restrictions apply to the first character of a key
 *       If the character is an asterisk '*' it will fail this validation.
 *       '*' is still valid for deleting multiple keys (where any key
 *       matching up to the asterisk will be deleted).
 */
int mmconfig_validate_key_character(char character);

/**
 * Calculates the number of bytes that would be needed to write the given
 * updates (if not NULL) and the number of bytes that would remain free in
 * the persistent store afterwards.
 *
 * @param  node_list       Pointer to a linked list of nodes to be sized.
 *                     May be NULL if there is nothing to store
 * @param  bytes_used      Pointer for the size of current data plus node_list
 * @param  bytes_remaining Pointer for the number of bytes that would remain free if
 *                     node_list was stored.  May be negative if insufficient storage is available.
 * @return                 Returns @c MMCONFIG_OK on success. On error returns:
 *                          @c MMCONFIG_ERR_NOT_SUPPORTED if there is no persistent store
 */
int mmconfig_check_usage(const struct mmconfig_update_node *node_list,
                         uint32_t *bytes_used, int32_t *bytes_remaining);

/**
 * Loads and applies any other @c mmwlan settings specified in config store.
 *
 * Specifically looks for @c wlan.subbands_enabled, @c wlan.sgi_enabled,
 * @c wlan.ampdu_enabled, @c wlan.fragment_threshold and @c wlan.rts_threshold.
 */
void load_mmwlan_settings(void);

#ifdef __cplusplus
}
#endif

/** @} */
