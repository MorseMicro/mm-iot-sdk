/*
 * Copyright 2022-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#if !defined(LWIP_HTTPD_CUSTOM_FILES)
#error "http_rest requires LWIP HTTPD custom files"
#endif

/**
 * Opaque data object. Users should not use this directly.
 * Use @c restfs_write() / @c restfs_printf() instead.
 */
struct restfs_file;

/**
 * Function prototype of REST endpoint callback
 */
typedef void (*rest_endpoint_handler_t)(struct restfs_file *);

/**
 * A REST endpoint
 */
struct rest_endpoint {
    /** URI of endpoint */
    char* uri;
    /** User defined function to call when this endpoint is requested by a client */
    rest_endpoint_handler_t user_function;
};

/**
 * Allocate bytes for REST response
 * Must call this function BEFORE writing any data out.
 *
 * @note Can NOT be used with @c restfs_write_const()
 *
 * @param rest_file - Opaque file object
 * @param size - number of bytes to allocate
 * @return @c ERR_OK if succeeded, @c ERR_MEM if failed
 */
int restfs_alloc_buffer(struct restfs_file *rest_file, uint16_t size);

/**
 * Use a constant string as the REST response.
 * This can NOT be used with any other @c restfs functions.
 * Use this function for constant, predefined responses that do not require an allocated
 * buffer (e.g. an embedded, constant html string).
 *
 * @note Can NOT be used with @c restfs_alloc_buffer()
 *
 * @param rest_file - Opaque file object
 * @param str - Pointer to constant string
 */
void restfs_write_const(struct restfs_file *rest_file, const char* str);

/**
 * Write data into REST response
 *
 * @param rest_file - Opaque file object
 * @param buff - data to write
 * @param len - length to write
 * @return Number of bytes written
 */
int restfs_write(struct restfs_file *rest_file, const uint8_t *buff, uint16_t len);

/**
 * Formatted print into REST response
 *
 * @param rest_file - Opaque file object
 * @param fmt - Format string
 * @param ... - variable arguments
 * @return Number of bytes written
 */
int restfs_printf(struct restfs_file *rest_file, const char *fmt, ...);

/**
 * Get access to a pointer where one can write a REST response. This pointer is now owned by the
 * caller, and must be released by a call to @c restfs_release_raw_buffer().
 *
 * @warning This function makes no checks for whether the data will fit. Make sure you allocate
 *          enough space in the initial @c restfs_alloc_buffer()
 *
 * @param rest_file - opaque file object
 * @return Pointer to raw buffer
 */
char* restfs_claim_raw_buffer(struct restfs_file *rest_file);

/**
 * Release access to raw buffer after a previous call to @c restfs_claim_raw_buffer()
 *
 * @param rest_file - Opaque file object
 * @param wr_len - Amount written to buffer (must be accurate as it is used to update the offset)
 */
void restfs_release_raw_buffer(struct restfs_file *rest_file, uint16_t wr_len);


/**
 * Initialize REST endpoints and their handlers
 *
 * @param endpoints - Array of rest endpoints
 * @param num_endpoints - number of elements in the array
 */
void rest_init_endpoints(const struct rest_endpoint* endpoints, uint16_t num_endpoints);
