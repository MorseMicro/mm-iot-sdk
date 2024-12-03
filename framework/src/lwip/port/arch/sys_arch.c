/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "sys_arch.h"

sys_thread_t sys_tcpip_thread = NULL;

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    *mutex = mmosal_mutex_create("lwip");
    if ((*mutex) == NULL)
    {
        SYS_STATS_INC(mutex.err);
        return ERR_MEM;
    }

    SYS_STATS_INC_USED(mutex);
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    bool ok = mmosal_mutex_get(*mutex, UINT32_MAX);
    LWIP_ASSERT("mutex_get failed", ok);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
    bool ok = mmosal_mutex_release(*mutex);
    LWIP_ASSERT("mutex_release failed", ok);
}

void sys_mutex_free(sys_mutex_t *mutex)
{
    mmosal_mutex_delete(*mutex);
    *mutex = NULL;
    SYS_STATS_DEC(mutex.used);
}

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    // Maybe should use semb??
    *sem = mmosal_sem_create(1, count, "lwip");
    if (*sem == NULL)
    {
        SYS_STATS_INC(sem.err);
        return ERR_MEM;
    }

    SYS_STATS_INC_USED(sem);
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem)
{
    bool ok = mmosal_sem_give(*sem);
    LWIP_ASSERT("sem_give failed", ok);
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    if (timeout == 0)
    {
        timeout = UINT32_MAX;
    }

    bool ok = mmosal_sem_wait(*sem, timeout);
    return ok ? 0 : SYS_ARCH_TIMEOUT;
}

void sys_sem_free(sys_sem_t *sem)
{
    mmosal_sem_delete(*sem);
    *sem = NULL;
    SYS_STATS_DEC(sem.used);
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    *mbox = mmosal_queue_create(size, sizeof(void *), "lwip");
    if (*mbox == NULL)
    {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }

    SYS_STATS_INC_USED(mbox);
    return ERR_OK;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    bool ok = mmosal_queue_push(*mbox, msg, UINT32_MAX);
    LWIP_ASSERT("post failed", ok);
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    bool ok = mmosal_queue_push(*mbox, &msg, 0); //??
    if (!ok)
    {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    // Not supported
    (void)mbox;
    (void)msg;
    LWIP_PLATFORM_ASSERT("unsupported");
    return ERR_ARG;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    bool ok = mmosal_queue_pop(*mbox, msg, timeout);
    return ok ? 0 : SYS_ARCH_TIMEOUT;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    mmosal_queue_delete(*mbox);
    *mbox = NULL;
    SYS_STATS_DEC(mbox.used);
}

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread,
                            void *arg, int stacksize, int prio)
{
    LWIP_ASSERT("Invalid prio",
                prio >= MMOSAL_TASK_PRI_MIN && prio <= MMOSAL_TASK_PRI_HIGH);
    enum mmosal_task_priority prio_ = (enum mmosal_task_priority)prio;
    return mmosal_task_create(thread, arg, prio_, stacksize/4, name);
}

void sys_init(void)
{
    /* No initiliasation necessary (but mmosal_init() must have previously been called). */
}


u32_t sys_now(void)
{
    return mmosal_get_time_ms();
}

sys_prot_t sys_arch_protect(void)
{
    mmosal_task_enter_critical();
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    mmosal_task_exit_critical();
}
