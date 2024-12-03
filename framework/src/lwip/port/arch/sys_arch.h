/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "mmosal.h"
#include "mmhal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mmosal_sem   *sys_sem_t;
typedef struct mmosal_mutex *sys_mutex_t;
typedef struct mmosal_queue *sys_mbox_t;
typedef struct mmosal_task  *sys_thread_t;

#define sys_mutex_valid(mutex)              ((*(mutex)) != NULL)
#define sys_mutex_set_invalid(mutex)        ((*(mutex)) = NULL)
#define sys_sem_valid(sem)                  ((*(sem)) != NULL)
#define sys_sem_set_invalid(sem)            ((*(sem)) = NULL)
#define sys_msleep(ms)                      (mmosal_task_sleep(ms))
#define sys_arch_mbox_tryfetch(mbox, msg)   (sys_arch_mbox_fetch((mbox), (msg), 0))
#define sys_mbox_valid(mbox)                ((*(mbox)) != NULL)
#define sys_mbox_set_invalid(mbox)          ((*(mbox)) = NULL)
#define sys_jiffies()                       (mmosal_get_time_ticks())


extern sys_mutex_t lock_tcpip_core;
extern sys_thread_t sys_tcpip_thread;

#define sys_mark_tcpip_thread() (sys_tcpip_thread = mmosal_task_get_active())
#define sys_assert_core_locked()                                                            \
    LWIP_ASSERT("tcpiplock",                                                                \
                (mmhal_get_isr_state() != MMHAL_IN_ISR) &&                                  \
                (!sys_tcpip_thread || mmosal_mutex_is_held_by_active_task(lock_tcpip_core)))

#ifdef __cplusplus
}
#endif
