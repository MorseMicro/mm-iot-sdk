/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "mmosal.h"
#include "mmutils.h"

/** Approximate number of loops we can execute per ms in the bootloader - used for rough timing */
#define LOOPS_PER_MS            1000

/* See mmosal.h for declarations */

int mmosal_main(mmosal_app_init_cb_t app_init_cb)
{
    app_init_cb();
    return 0;
}

void *mmosal_malloc_(size_t size)
{
    return malloc(size);
}

void *mmosal_malloc_dbg(size_t size, const char *name, unsigned line_number)
{
    (void)name;
    (void)line_number;
    return malloc(size);
}

void *mmosal_calloc(size_t nitems, size_t size)
{
    return calloc(nitems, size);
}

void mmosal_free(void *p)
{
    free(p);
}

/* --------------------------------------------------------------------------------------------- */

struct mmosal_mutex *mmosal_mutex_create(const char *name)
{
    MM_UNUSED(name);
    return NULL;
}

void mmosal_mutex_delete(struct mmosal_mutex *mutex)
{
    MM_UNUSED(mutex);
}

bool mmosal_mutex_get(struct mmosal_mutex *mutex, uint32_t timeout_ms)
{
    MM_UNUSED(mutex);
    MM_UNUSED(timeout_ms);
    return true;
}

bool mmosal_mutex_release(struct mmosal_mutex *mutex)
{
    MM_UNUSED(mutex);
    return true;
}

/* --------------------------------------------------------------------------------------------- */

struct mmosal_semb *mmosal_semb_create(const char *name)
{
    MM_UNUSED(name);
    return NULL;
}

void mmosal_semb_delete(struct mmosal_semb *semb)
{
    MM_UNUSED(semb);
}

bool mmosal_semb_give(struct mmosal_semb *semb)
{
    MM_UNUSED(semb);
    return true;
}

bool mmosal_semb_give_from_isr(struct mmosal_semb *semb)
{
    MM_UNUSED(semb);
    return true;
}

bool mmosal_semb_wait(struct mmosal_semb *semb, uint32_t timeout_ms)
{
    MM_UNUSED(semb);
    MM_UNUSED(timeout_ms);
    return true;
}

/* --------------------------------------------------------------------------------------------- */

void mmosal_impl_assert(void)
{
    while (1)
    {}
}

void mmosal_log_failure_info(const struct mmosal_failure_info *info)
{
    MM_UNUSED(info);
}

/* --------------------------------------------------------------------------------------------- */

uint32_t mmosal_get_time_ms(void)
{
    return mmosal_get_time_ticks();
}

uint32_t mmosal_get_time_ticks(void)
{
    static uint32_t tick = 0;
    static uint16_t microtick = 0;

    if (microtick++ == LOOPS_PER_MS)
    {
        tick++;
        microtick = 0;
    }
    return tick;
}

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

/**
 * Pointer to the current high watermark of the heap usage
 */
static uint8_t *__sbrk_heap_end = NULL;

/*
 * Note: This function is defined in unistd.h
 * It is used by libc to determine the heap size for malloc().
 */
void *_sbrk(ptrdiff_t incr)
{
    /* The bootloader needs a basic malloc() implementation */
    extern uint8_t _end; /* Symbol defined in the linker script */
    extern uint8_t _estack; /* Symbol defined in the linker script */
    extern uint32_t _Min_Stack_Size; /* Symbol defined in the linker script */
    const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
    const uint8_t *max_heap = (uint8_t *)stack_limit;
    uint8_t *prev_heap_end;

    /* Initialize heap end at first call */
    if (NULL == __sbrk_heap_end)
    {
        __sbrk_heap_end = &_end;
    }

    /* Protect heap from growing into the reserved MSP stack */
    if (__sbrk_heap_end + incr > max_heap)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    prev_heap_end = __sbrk_heap_end;
    __sbrk_heap_end += incr;

    return (void *)prev_heap_end;
}
