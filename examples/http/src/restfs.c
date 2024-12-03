/*
 * Copyright 2022-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "restfs.h"

#include <stdarg.h>
#include <string.h>

#include "lwip/apps/fs.h"
#include "lwip/mem.h"

#include "mmosal.h"

/**
 * Opaque object used for writing REST output data.
 */
struct restfs_file {
    /** lwIP @c httpd custom file object */
    struct fs_file *fs_file;
};

static const struct rest_endpoint *rest_endpoints;
static uint16_t num_rest_endpoints = 0;

void rest_init_endpoints(const struct rest_endpoint* endpoints, uint16_t num_endpoints)
{
    rest_endpoints = endpoints;
    num_rest_endpoints = num_endpoints;
}

int restfs_printf(struct restfs_file *rest_file, const char *fmt, ...)
{
    struct fs_file *file = rest_file->fs_file;
    MMOSAL_ASSERT(file->pextension != NULL);

    char *data = (char *)file->pextension;
    int bytes_written;
    va_list args;

    va_start(args, fmt);
    bytes_written = vsnprintf(data + file->index, (file->len - file->index), fmt, args);
    va_end(args);

    file->index += bytes_written;
    return bytes_written;
}

int restfs_write(struct restfs_file *rest_file, const uint8_t *buff, uint16_t len)
{
    struct fs_file *file = rest_file->fs_file;

    MMOSAL_ASSERT(file->pextension != NULL);

    char *data = (char *)file->pextension;
    uint16_t bytes_written = (len < (file->len - file->index) ? len : (file->len - file->index));

    memcpy(data + file->index, buff, bytes_written);
    file->index += bytes_written;

    return bytes_written;
}

int restfs_alloc_buffer(struct restfs_file *rest_file, uint16_t size)
{
    struct fs_file *file = rest_file->fs_file;

    MMOSAL_ASSERT((file->pextension == NULL) && (file->data == NULL));

    file->pextension = mem_malloc(size);

    if (file->pextension != NULL)
    {
        memset(file->pextension, 0, size);
        file->data = (const char *) file->pextension;
        file->len = size;
        return ERR_OK;
    }
    return ERR_MEM;
}

void restfs_write_const(struct restfs_file *rest_file, const char* str)
{
    struct fs_file *file = rest_file->fs_file;

    MMOSAL_ASSERT((file->pextension == NULL) && (file->data == NULL));

    file->data = str;
    file->index = strlen(str);
}

char* restfs_claim_raw_buffer(struct restfs_file *rest_file)
{
    struct fs_file *file = rest_file->fs_file;

    MMOSAL_ASSERT((file->pextension != NULL) && (file->data != NULL));

    char *buff = (char *)file->pextension;
    file->pextension = NULL; /* Prevent access while raw buffer is aquired */
    return buff + file->index;
}

void restfs_release_raw_buffer(struct restfs_file *rest_file, uint16_t wr_len)
{
    struct fs_file *file = rest_file->fs_file;

    MMOSAL_ASSERT((file->pextension == NULL) && (file->data != NULL));

    file->index += wr_len;
    file->pextension = (char*) file->data;
}

/**
 * Leverage custom file-system features to generate REST responses.
 */
int fs_open_custom(struct fs_file *file, const char *name)
{
    for (int i = 0; i < num_rest_endpoints; i++)
    {
        if (!strcmp(name, rest_endpoints[i].uri))
        {
            struct restfs_file rest_file = {
                .fs_file = file
            };
            memset(file, 0, sizeof(*file));

            rest_endpoints[i].user_function(&rest_file);

            file->len = file->index;

            /* Persistent header flag will force lwIP to add a content-length header */
            file->flags |= FS_FILE_FLAGS_HEADER_PERSISTENT;

            return 1;
        }
    }
    return 0;
}

void fs_close_custom(struct fs_file *file)
{
    if (file && file->pextension)
    {
        mem_free(file->pextension);
        file->pextension = NULL;
    }
}

int fs_read_custom(struct fs_file *file, char *buffer, int count)
{
    (void) (file);
    (void) (buffer);
    (void) (count);
    /* Empty for now. */
    return FS_READ_EOF;
}
