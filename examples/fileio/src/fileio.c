/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Example file I/O application using standard C API.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * @ref fileio.c is an example application that demonstrates how to use standard C file I/O
 * routines to access files. We use standard POSIX @c open(), @c close(), @c read() and
 * @c write() function calls to open, close, read and write to files. Other functions
 * supported are @c lseek(), @c mkdir() and @c remove(). Yes, sub-directories are supported.
 * For details on these functions see their definitions in @c sys/unistd.h. If the platform
 * does not support file I/O then calling these functions will return an error.  See
 * @c sys/errno.h for a list of possible error codes that will be returned in @c errno.
 *
 * The file I/O module is automatically included and initialized in the application binary
 * only if @c open() is called from the application. This saves space in applications that
 * do not use this functionality.
 */

#include <stdio.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/errno.h>

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nFile I/O Example (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Open a file for reading - calling open() the first time automatically initializes
     * The file I/O subsystem and links in the required modules. */
    int fd = open("test.txt", O_RDONLY);
    if (fd < 0)
    {
        /* We got an error, check for @c EINVAL which signifies File I/O is not supported */
        if (errno == ENOENT)
        {
            /* We got a file not found error, so simply create the file */
            printf("File test.txt not found, creating...");
            fd = open("test.txt", O_RDWR | O_CREAT);
            if (fd >= 0)
            {
                char message[] = "G'day World!\n";
                int count = write(fd, message, sizeof(message));
                close(fd);
                if (count >= 0)
                {
                    printf("%d bytes written!\n"
                        "Run application again to see what was written.\n", count);
                }
                else
                {
                    printf("Error %d!\n Failed to write to file!\n", errno);
                }
            }
            else
            {
                printf("Failed!\n");
            }
        }
        else
        {
            printf("File I/O is currently supported only on the mm-ekh08-f429"
                   " and mm-ekh08-u575 platforms!\n");
        }
    }
    else
    {
        /* File successfully opened, read contents */
        char buffer[64];
        int count = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        if (count >= 0)
        {
            /* Ensure buffer is null terminated */
            buffer[count] = 0;
            printf("%d bytes read!\nContents: %s\n", count, buffer);
        }
        else
        {
            printf("Error %d!\nFailed to read from file!\n", errno);
        }
    }
}
