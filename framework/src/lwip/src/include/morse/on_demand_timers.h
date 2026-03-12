/*
 * Copyright 2025 Morse Micro
 */
#pragma once

#include "lwip/opt.h"

#ifndef MORSE_LWIP_TIMERS_ON_DEMAND
#define MORSE_LWIP_TIMERS_ON_DEMAND 0
#endif

#if MORSE_LWIP_TIMERS_ON_DEMAND

#include "lwip/arch.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/api.h"

/* Enabling this might make timer executions unstable because parts of sleep_ticks will
 * work on outdated tick because of the printf calls */
#ifndef DEBUG_SLEEP_TICKS_ENABLE
#define DEBUG_SLEEP_TICKS_ENABLE 0
#endif

#if DEBUG_SLEEP_TICKS_ENABLE
#define DEBUG_SLEEP_TICKS_GENERIC(res, prot) \
  LWIP_PLATFORM_DIAG(("%6"PRIu32": " prot " ST: %"PRIu32" line %"PRIu32"\n", sys_now(), (u32_t)res, (u32_t)__LINE__))
#else
#define DEBUG_SLEEP_TICKS_GENERIC(res, prot) do {} while (0)
#endif

enum morse_timer_index
{
  MORSE_ON_DEMAND_TIMER_ARP = 0,
  MORSE_ON_DEMAND_TIMER_TCP_FAST,
  MORSE_ON_DEMAND_TIMER_TCP_SLOW,

  MORSE_ON_DEMAND_TIMER_NUM
};

/* TIMEOUTS */

/**
 * @brief Schedule an evaluation of the need to run timers.
 * Internally creates a timeout with a very short delay that will call the sleep_tick functions
 * @param index the protocol that needs its timer reevaluated
 */
void on_demand_timer_eval_schedule(enum morse_timer_index index);

/* PROTOCOLS */

/*
 * Each protocol defines 4 functions:
 * - x_timer:
 * Timer timeout handler. Calls x_tmr.
 * - x_update_tick:
 * Updates the ticks that should be updated by x_tmr, taking into account delay between executions.
 * - x_timer_needed:
 * Requests a reevaluation of the need to run the timer.
 * - x_sleep_ticks:
 * Evaluates the need to run the timer. Returns a number of ticks before the
 * next time there will be work required.
 */

#define ETHARP_TIMER_NEEDED() etharp_timer_needed()
#define ETHARP_UPDATE_TICK() etharp_update_tick()
void etharp_timer(void* arg);
void etharp_update_tick(void);
void etharp_timer_needed(void);
u32_t etharp_sleep_ticks(void);

#define TCP_TIMER_NEEDED() tcp_timer_needed()
#define TCP_UPDATE_TICK() tcp_update_tick()
void tcpip_tcp_fast_timer(void *arg);
void tcp_update_tick(void);
/* tcp_timer_needed is already in tcp_priv.h */
u32_t tcp_fast_sleep_ticks(void);

void tcpip_tcp_slow_timer(void *arg);
/* TCP slow timer uses the same tcp_update_tick as fast timer */
/* TCP slow timer uses the same tcp_timer_needed as fast timer */
u32_t tcp_slow_sleep_ticks(void);

/* Netconn is a special case in the TCP timer
 * The timer, update_tick and timer_needed functions are the TCP ones */
u32_t netconn_sleep_ticks(struct netconn *conn);

#else /* MORSE_LWIP_TIMERS_ON_DEMAND */

#define ETHARP_TIMER_NEEDED() do {} while (0)
#define ETHARP_UPDATE_TICK() do {} while (0)

#define TCP_TIMER_NEEDED() do {} while (0)
#define TCP_UPDATE_TICK() do {} while (0)

#endif /* MORSE_LWIP_TIMERS_ON_DEMAND */
