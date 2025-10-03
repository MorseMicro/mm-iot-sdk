/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * This file provides the implementation of system level libc file I/O functions.
 * Libc requires us to provide namespace clean versions of the system calls
 * by prefixing them with '_' (_open, _close, etc.). These are then mapped
 * correctly to the actual function names (open, close, etc.) in the respective
 * header files.
 *
 * These function calls map to the corresponding function calls in LittleFS to
 * transparently provide native File I/O using LittleFS. LittleFS is a lightweight
 * file-system with fail-safe and wear leveling features for micro-controllers. It is designed
 * to tolerate random power failures and recover from such situations gracefully.
 *
 * To initialize LittleFS, we first call @c mmhal_get_littlefs_config() which is defined in
 * the HAL layer to retrieve a pointer to @c struct @c lfs_config which contains hardware
 * parameters as well as pointers to HAL functions to read and write from the hardware.
 * We initialize then LittleFS with this structure and thereafter LittleFS can directly call
 * the HAL functions to read and write to the hardware.
 *
 * See @c README.md in the @c src/littlefs folder for detailed information on LittleFS.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "mmosal.h"
#include "mmhal.h"
#include "mmutils.h"

#include "lfs.h"

/** The maximum number of files that may be open at any one time */
#define MAX_FILES       4

/** The first 3 file descriptors are reserved by libc for @c STDOUT, @c STDIN & @c STDERR */
#define STDIO_OFFSET    3

/** A macro to convert from file descriptor to a slot in the file table */
#define FD_TO_SLOT(fd)  ((fd) - STDIO_OFFSET)

/** A macro to convert from a slot number to file descriptor */
#define SLOT_TO_FD(fd)  ((slot) + STDIO_OFFSET)

/** A pointer to the file-system.  This is initialized in @c littlefs_init. */
static lfs_t *lfs_filesystem = NULL;

/** A file table that contains the @c LFS file data for each file slot.
 * This is initialized in @c littlefs_init.  */
static lfs_file_t *file_table = NULL;

/** A zero filled structure for comparison */
static const lfs_file_t zero_file = { 0 };

/* These pointers are set to the corresponding LFS functions when _open() is first called
 * This prevents these functions being linked if we never call open() in our application. */
static lfs_ssize_t (*lfs_file_read_ptr)(
    lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size) = NULL;
#ifndef LFS_READONLY
static lfs_ssize_t (*lfs_file_write_ptr)(
    lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size) = NULL;
#endif
static int (*lfs_file_close_ptr)(
    lfs_t *lfs, lfs_file_t *file) = NULL;
static lfs_soff_t (*lfs_file_seek_ptr)(
    lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence) = NULL;

/**
 * Initializes the LittleFS subsystem.
 *
 * We split the LittleFS initialization calls into @c mmhal_get_littlefs_config
 * to fetch the hardware configuration for LittleFS from MMHAL. If LittleFS is
 * not supported @c mmhal_get_littlefs_config returns NULL.
 */
static void littlefs_init(void)
{
    /* This is static */
    static lfs_t lfs = { 0 };

    /* Ensure we do initialization only once on platforms that support it */
    if (lfs_filesystem == NULL)
    {
        const struct lfs_config *littlefs_config = mmhal_get_littlefs_config();

        /* Check if supported by the platform */
        if (littlefs_config == NULL)
        {
#ifndef MMOSAL_NO_DEBUGLOG
            printf("LittleFS is not supported by current configuration\n");
#endif
            return;
        }

        file_table = (lfs_file_t *)mmosal_calloc(MAX_FILES, sizeof(lfs_file_t));
        if (file_table == NULL)
        {
#ifndef MMOSAL_NO_DEBUGLOG
            printf("Failed to allocate memory for file_table (%u*%u bytes)\n",
                   MAX_FILES, sizeof(lfs_file_t));
#endif
            return;
        }

        /* mount the filesystem */
        int err = lfs_mount(&lfs, littlefs_config);

#ifndef LFS_READONLY
        /* Reformat if we can't mount the filesystem
         * this should only happen on the first boot */
        if (err)
        {
            printf("Could not find LFS partition, formatting...");
            lfs_format(&lfs, littlefs_config);
            err = lfs_mount(&lfs, littlefs_config);
            MMOSAL_ASSERT(err == LFS_ERR_OK);
            printf("Done!\n");
        }
#endif

        if (!err)
        {
            /* No errors, setup the function pointers */
            lfs_file_read_ptr = lfs_file_read;
#ifndef LFS_READONLY
            lfs_file_write_ptr = lfs_file_write;
#endif
            lfs_file_close_ptr = lfs_file_close;
            lfs_file_seek_ptr = lfs_file_seek;

            /* Set the lfs_filesystem pointer */
            lfs_filesystem = &lfs;
        }
    }
}

/**
 * Find an empty slot in the file table.
 *
 * @return An empty file slot or -1 if full
 */
static int find_empty_slot()
{
    int i;

    if (file_table == NULL)
    {
        return -1;
    }

    for (i = 0; i < MAX_FILES; i++)
    {
        if (memcmp((const void *)&file_table[i],
                   (const void *)&zero_file, sizeof(lfs_file_t)) == 0)
        {
            return i;
        }
    }

    /* No empty slot found */
    return -1;
}

/**
 * Returns 1 (true) if the specified file descriptor is a terminal.
 *
 * @param  fd The file descriptor to query.
 * @return    1 if a terminal, 0 otherwise.
 */
int _isatty(int fd)
{
    MM_UNUSED(fd);

    /* For simplicity we asssume we have no TTY */
    return 0;
}

/**
 * Standard POSIX file open, See open().
 *
 * @param  pathname The path of the file to open.
 * @param  flags    Open flags, see open()
 * @return          The file descriptor of the opened file.
 */
int _open(const char *pathname, int flags, ...)
{
    /* The following code ensures LittleFS is linked in only if
     * we call open() to use file I/O anywhere in our application. */
    littlefs_init();

    if (!lfs_filesystem)
    {
        /* No filesystem available, apologize and bail out */
        errno = EINVAL;
        return -1;
    }

    int slot = find_empty_slot();

    if (slot == -1)
    {
        /* No free slots */
        errno = EMFILE;
        return -1;
    }

    /* Convert to LFS flags */
    int lfs_flags = LFS_O_RDONLY;
#ifdef LFS_READONLY
    MM_UNUSED(flags);
#else
    if (flags & O_WRONLY)
    {
        lfs_flags |= LFS_O_WRONLY;
    }
    if (flags & O_RDWR)
    {
        lfs_flags |= LFS_O_RDWR;
    }
    if (flags & O_CREAT)
    {
        lfs_flags |= LFS_O_CREAT;
    }
    if (flags & O_TRUNC)
    {
        lfs_flags |= LFS_O_TRUNC;
    }
    if (flags & O_APPEND)
    {
        lfs_flags |= LFS_O_APPEND;
    }
    if (flags & O_EXCL)
    {
        lfs_flags |= LFS_O_EXCL;
    }
#endif

    /* Now open the file */
    int err = lfs_file_open(lfs_filesystem, &file_table[slot], pathname, lfs_flags);
    if (err == LFS_ERR_OK)
    {
        /* Success, return fd */
        return SLOT_TO_FD(slot);
    }

    /* LFS errors map directly to errno but opposite sign */
    errno = -err;
    return -1;
}

/**
 * Standard POSIX file close, See close().
 *
 * @param  fd The file descriptor to close.
 * @return    0 on success, -1 on error.
 */
int _close(int fd)
{
    /* Basic sanity checks */
    if (!lfs_filesystem)
    {
        /* No filesystem available, apologize and bail out */
        errno = EINVAL;
        return -1;
    }
    else if ((fd < 0) || (fd >= STDIO_OFFSET + MAX_FILES))
    {
        errno = EBADF;
        return -1;
    }
    else if (fd < STDIO_OFFSET)
    {
        /* STDIO, just ignore and pretend we closed it */
        return 0;
    }
    else if (memcmp((const void *)&file_table[FD_TO_SLOT(fd)],
                    (const void *)&zero_file, sizeof(lfs_file_t)) == 0)
    {
        /* File not open */
        errno = EBADF;
        return -1;
    }

    /* All good, now close the file */
    int err = lfs_file_close_ptr(lfs_filesystem, &file_table[FD_TO_SLOT(fd)]);
    if (err != LFS_ERR_OK)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -err;
        return -1;
    }

    /* Now clear the slot */
    memset((void *)&file_table[FD_TO_SLOT(fd)], 0, sizeof(lfs_file_t));

    return 0;
}

/**
 * Standard POSIX file read, see read().
 *
 * @param  fd    The file descriptor to read from.
 * @param  buf   The buffer to read into.
 * @param  count Maximum number of bytes to read.
 * @return       The number of bytes read, 0 on end-of-file, or -1 on error.
 */
ssize_t _read(int fd, void *buf, size_t count)
{
    /* Basic sanity checks */
    if ((fd == STDOUT_FILENO) || (fd == STDERR_FILENO))
    {
        /* Nothing to read from STDOUT/STDERR */
        return 0;
    }
    else if (fd == STDIN_FILENO)
    {
        /* We currently don's support console input */
        return 0;
    }
    else if (!lfs_filesystem)
    {
        /* No filesystem available, apologize and bail out */
        errno = EINVAL;
        return -1;
    }
    else if ((fd < STDIO_OFFSET) || (fd >= STDIO_OFFSET + MAX_FILES))
    {
        errno = EBADF;
        return -1;
    }
    else if (memcmp((const void *)&file_table[FD_TO_SLOT(fd)],
                    (const void *)&zero_file, sizeof(lfs_file_t)) == 0)
    {
        /* File not open */
        errno = EBADF;
        return -1;
    }

    /* All good, now read from the file */
    int ret = lfs_file_read_ptr(lfs_filesystem, &file_table[FD_TO_SLOT(fd)], buf, count);
    if (ret < 0)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -ret;
        return -1;
    }

    return ret;
}

/**
 * Standard POSIX file write, see write().
 *
 * @param  fd    The file descriptor to read from.
 * @param  buf   The buffer to write from.
 * @param  count Number of bytes to write.
 * @return       The number of bytes written or -1 on error.
 */
ssize_t _write(int fd, const void *buf, size_t count)
{
#ifdef LFS_READONLY
    MM_UNUSED(fd);
    MM_UNUSED(buf);
    MM_UNUSED(count);
    return -1;
#else
    /* Basic sanity checks */
    if ((fd == STDOUT_FILENO) || (fd == STDERR_FILENO))
    {
        /* Dump to console */
        uint32_t ii;
        uint8_t *c = (uint8_t *)buf;
        for (ii = 0; ii < count; ii++)
        {
            putchar((int)c[ii]);
        }
        return count;
    }
    else if (fd == STDIN_FILENO)
    {
        /* Can't write to STDIN */
        errno = EPERM;
        return -1;
    }
    else if (!lfs_filesystem)
    {
        /* No filesystem available, apologize and bail out */
        errno = EINVAL;
        return -1;
    }
    else if ((fd < STDIO_OFFSET) || (fd >= STDIO_OFFSET + MAX_FILES))
    {
        errno = EBADF;
        return -1;
    }
    else if (memcmp((const void *)&file_table[FD_TO_SLOT(fd)],
                    (const void *)&zero_file, sizeof(lfs_file_t)) == 0)
    {
        /* File not open */
        errno = EBADF;
        return -1;
    }

    /* All good, now write to the file */
    int ret = lfs_file_write_ptr(lfs_filesystem, &file_table[FD_TO_SLOT(fd)], buf, count);
    if (ret < 0)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -ret;
        return -1;
    }

    return ret;
#endif
}

/**
 * Standard POSIX file seek, see @c lseek().
 *
 * @param  fd     The file descriptor to seek within.
 * @param  offset The offset to seek to.
 * @param  whence Where to offset from, see @c lseek().
 * @return        On success returns the absolute offset from the beginning
 *                  of the file, or -1 on error.
 */
off_t _lseek(int fd, off_t offset, int whence)
{
    /* Basic sanity checks */
    if (!lfs_filesystem)
    {
        /* No filesystem available, apologize and bail out */
        errno = EINVAL;
        return -1;
    }
    else if ((fd < 0) || (fd >= STDIO_OFFSET + MAX_FILES))
    {
        errno = EBADF;
        return -1;
    }
    else if (fd < STDIO_OFFSET)
    {
        /* STDIO, can't seek */
        errno = EINVAL;
        return -1;
    }
    else if (memcmp((const void *)&file_table[FD_TO_SLOT(fd)],
                    (const void *)&zero_file, sizeof(lfs_file_t)) == 0)
    {
        /* File not open */
        errno = EBADF;
        return -1;
    }

    /* All good, now seek */
    int ret = lfs_file_seek_ptr(lfs_filesystem, &file_table[FD_TO_SLOT(fd)], offset, whence);
    if (ret < 0)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -ret;
        return -1;
    }

    return ret;
}

/**
 * Standard POSIX file status, see @c fstat().
 *
 * @param  fd  The file descriptor whose status is requested.
 * @param  buf A pointer to a struct stat to return the status in.
 * @return     0 on success, -1 on error.
 */
int _fstat(int fd, struct stat *buf)
{
    MM_UNUSED(fd);
    MM_UNUSED(buf);

    /* Not supported for now */
    errno = EINVAL;
    return -1;
}

/**
 * Standard POSIX file/directory delete, see @c unlink().
 *
 * @param  pathname The file/directory to delete.
 * @return          0 on success, -1 on error.
 */
int _unlink(const char *pathname)
{
#ifdef LFS_READONLY
    MM_UNUSED(pathname);
    return -1;
#else
    /* It is possible for the user to call remove() without calling open() first
     * so, ensure we are initialized. */
    littlefs_init();

    int err = lfs_remove(lfs_filesystem, pathname);
    if (err != LFS_ERR_OK)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -err;
        return -1;
    }
    return 0;
#endif
}

/**
 * Standard POSIX make directory, see @c mkdir().
 *
 * @param  pathname The directory to create.
 * @param  mode     The permissions/mode - ignored.
 * @return          0 on success, -1 on error.
 */
int _mkdir(const char *pathname, mode_t mode)
{
    MM_UNUSED(mode);

#ifdef LFS_READONLY
    MM_UNUSED(pathname);
    return -1;
#else
    /* It is possible for the user to call mkdir() without calling open() first
     * so, ensure we are initialized. */
    littlefs_init();

    int err = lfs_mkdir(lfs_filesystem, pathname);
    if (err != LFS_ERR_OK)
    {
        /* LFS errors map directly to errno but opposite sign */
        errno = -err;
        return -1;
    }
    return 0;
#endif
}
