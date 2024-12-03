/*
 * Copyright 2021-2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"

/*
 * Note: This function is defined in unistd.h
 * It is used by libc to determine the heap size for malloc().
 */
void *_sbrk(ptrdiff_t p)
{
    (void)p;
    /* Since we do not want libc to be doing any memory allocation, we do an assert here. */
    MMOSAL_ASSERT(0);

    return NULL;
}

#if !(defined(LIBC_PROVIDES__EXIT) && LIBC_PROVIDES__EXIT)
/* The following stub is not required if libc provides _exit(). In most cases libc does
 * provide _exit(), but it is not provided by @c libc-nano that we use by default.
 */

/*
 * Note: This function is defined in unistd.h
 * It is called when main() exits.
 */
void _exit(int status)
{
    (void) status;
    while (1)
    {
    }
}
#endif

/*
 * Note: This function is defined in unistd.h
 * It is used to kill a thread.
 */
void _kill(pid_t pid)
{
    (void)pid;
}

/*
 * Note: This function is defined in unistd.h
 * It is used to get the current PID.
 */
pid_t _getpid(void)
{
    return 0;
}
