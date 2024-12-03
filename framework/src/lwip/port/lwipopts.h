/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "mmosal.h"
#include "mmhal.h"
#include "mmipal.h"
#include "arch/sys_arch.h"

/*
   ------------------------------------
   ----------- Core locking -----------
   ------------------------------------
*/

/**
 * LWIP_TCPIP_CORE_LOCKING_INPUT: when LWIP_TCPIP_CORE_LOCKING is enabled,
 * this lets tcpip_input() grab the mutex for input packets as well,
 * instead of allocating a message and passing it to tcpip_thread.
 *
 * ATTENTION: this does not work when tcpip_input() is called from
 * interrupt context!
 */
#define LWIP_TCPIP_CORE_LOCKING_INPUT   (0)

/**
 * Macro/function to check whether lwIP's threading/locking
 * requirements are satisfied during current function call.
 * This macro usually calls a function that is implemented in the OS-dependent
 * sys layer and performs the following checks:
 * - Not in ISR (this should be checked for NO_SYS==1, too!)
 * - If @ref LWIP_TCPIP_CORE_LOCKING = 1: TCPIP core lock is held
 * - If @ref LWIP_TCPIP_CORE_LOCKING = 0: function is called from TCPIP thread
 * @see @ref multithreading
 */
#define LWIP_ASSERT_CORE_LOCKED() sys_assert_core_locked()

/**
 * Called as first thing in the lwIP TCPIP thread. Can be used in conjunction
 * with @ref LWIP_ASSERT_CORE_LOCKED to check core locking.
 * @see @ref multithreading
 */
#define LWIP_MARK_TCPIP_THREAD() sys_mark_tcpip_thread()

/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/

/**
 * MEM_LIBC_MALLOC==1: Use malloc/free/realloc provided by your C-library
 * instead of the lwip internal allocator. Can save code size if you
 * already use it.
 */
#ifndef MEM_LIBC_MALLOC
#define MEM_LIBC_MALLOC                 (1)
#endif

/**
 * MEMP_MEM_INIT==1: Force use of memset to initialize pool memory.
 * Useful if pool are moved in uninitialized section of memory. This will ensure
 * default values in pcbs struct are well initialized in all conditions.
 */
#ifndef MEMP_MEM_INIT
#define MEMP_MEM_INIT                   (1)
#endif

/**
 * MEM_ALIGNMENT: should be set to the alignment of the CPU
 *    4 byte alignment -> \#define MEM_ALIGNMENT 4
 *    2 byte alignment -> \#define MEM_ALIGNMENT 2
 */
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT                   (4)
#endif

/**
 * MEM_SIZE: the size of the heap memory. If the application will send
 * a lot of data that needs to be copied, this should be set high.
 */
#ifndef MEM_SIZE
#define MEM_SIZE                        (4096)
#endif

/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/

/**
 * MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
 * per active UDP "connection".
 * (requires the LWIP_UDP option)
 */
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB                (5)
#endif

/**
 * MEMP_NUM_TCPIP_MSG_INPKT: the number of struct tcpip_msg, which are used
 * for incoming packets.
 * (only needed if you use tcpip.c)
 */
#ifndef MEMP_NUM_TCPIP_MSG_INPKT
#define MEMP_NUM_TCPIP_MSG_INPKT        16
#endif

/**
 * PBUF_POOL_SIZE: the number of buffers in the pbuf pool.
 *
 * Note: we can have up to 20 packets queued for transmit at a time.
 */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE                  20
#endif

/**
 * MEMP_NUM_SYS_TIMEOUT: the number of simultaneously active timeouts.
 */
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 2)
#endif

/*
   ---------------------------------
   ---------- ARP options ----------
   ---------------------------------
*/

/*
   --------------------------------
   ---------- IP options ----------
   --------------------------------
*/

/**
 * LWIP_IPV4==1: Enable IPv4
 *
 * @warning IPv4 must be *ENABLED* if using mmiperf, mmping, or emmet.
 */
#ifndef LWIP_IPV4
#define LWIP_IPV4                       (1)
#endif

/*
   ----------------------------------
   ---------- ICMP options ----------
   ----------------------------------
*/

/*
   ---------------------------------
   ---------- RAW options ----------
   ---------------------------------
*/
#ifndef LWIP_RAW
#define LWIP_RAW                        (1)
#endif

/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/

/**
 * LWIP_DHCP==1: Enable DHCP module.
 */
#ifndef LWIP_DHCP
#define LWIP_DHCP                       (1)
#endif

/*
   ------------------------------------
   ---------- AUTOIP options ----------
   ------------------------------------
*/

/*
   ----------------------------------
   ----- SNMP MIB2 support      -----
   ----------------------------------
*/

/*
   ----------------------------------
   -------- Multicast options -------
   ----------------------------------
*/

/*
   ----------------------------------
   ---------- IGMP options ----------
   ----------------------------------
*/
#ifndef LWIP_IGMP
#define LWIP_IGMP                       (1)
#endif

/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/

/**
 * LWIP_DNS==1: Turn on DNS module. UDP must be available for DNS
 * transport.
 */
#ifndef LWIP_DNS
#define LWIP_DNS                        (1)
#endif

/*
   ---------------------------------
   ---------- UDP options ----------
   ---------------------------------
*/

/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/

/**
 * TCP_WND: The size of a TCP window.  This must be at least
 * (2 * TCP_MSS) for things to work well.
 * ATTENTION: when using TCP_RCV_SCALE, TCP_WND is the total size
 * with scaling applied. Maximum window value in the TCP header
 * will be TCP_WND >> TCP_RCV_SCALE
 */
#ifndef TCP_WND
#define TCP_WND                         (10 * TCP_MSS)
#endif

/**
 * TCP_MSS: TCP Maximum segment size. (default is 536, a conservative default,
 * you might want to increase this.)
 * For the receive side, this MSS is advertised to the remote side
 * when opening a connection. For the transmit size, this MSS sets
 * an upper limit on the MSS advertised by the remote host.
 */
#ifndef TCP_MSS
#define TCP_MSS                         (1460)
#endif

/**
 * TCP_SND_BUF: TCP sender buffer space (bytes).
 * To achieve good performance, this should be at least 2 * TCP_MSS.
 */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                     (6 * TCP_MSS)
#endif


/**
 * TCP_SND_QUEUELEN: TCP sender buffer space (pbufs). This must be at least
 * as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work.
 */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN                (12)
#endif


/*
   ----------------------------------
   ---------- Pbuf options ----------
   ----------------------------------
*/

/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/

/**
 * LWIP_SINGLE_NETIF==1: use a single netif only. This is the common case for
 * small real-life targets. Some code like routing etc. can be left out.
 */
#ifndef LWIP_SINGLE_NETIF
#define LWIP_SINGLE_NETIF               (0)
#endif

/**
 * LWIP_NETIF_STATUS_CALLBACK==1: Support a callback function whenever an interface
 * changes its up/down status (i.e., due to DHCP IP acquisition)
 */
#ifndef LWIP_NETIF_STATUS_CALLBACK
#define LWIP_NETIF_STATUS_CALLBACK      (1)
#endif

/**
 * LWIP_NETIF_LINK_CALLBACK==1: Support a callback function from an interface
 * whenever the link changes (i.e., link down)
 */
#ifndef LWIP_NETIF_LINK_CALLBACK
#define LWIP_NETIF_LINK_CALLBACK        (1)
#endif

/**
 * LWIP_NUM_NETIF_CLIENT_DATA: Number of clients that may store
 * data in client_data member array of struct netif (max. 256).
 */
#ifndef LWIP_NUM_NETIF_CLIENT_DATA
#if defined(LWIP_MDNS_RESPONDER) && LWIP_MDNS_RESPONDER != 0
#define LWIP_NUM_NETIF_CLIENT_DATA      (1)
#endif
#endif

/*
   ------------------------------------
   ---------- LOOPIF options ----------
   ------------------------------------
*/


/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
*/


/**
 * TCPIP_THREAD_STACKSIZE: The stack size used by the main tcpip thread.
 * The stack size value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#ifndef TCPIP_THREAD_STACKSIZE
#define TCPIP_THREAD_STACKSIZE          (4096)
#endif

/**
 * TCPIP_THREAD_PRIO: The priority assigned to the main tcpip thread.
 * The priority value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#ifndef TCPIP_THREAD_PRIO
#define TCPIP_THREAD_PRIO               (MMOSAL_TASK_PRI_NORM)
#endif

/**
 * TCPIP_MBOX_SIZE: The mailbox size for the tcpip thread messages
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when tcpip_init is called.
 */
#ifndef TCPIP_MBOX_SIZE
#define TCPIP_MBOX_SIZE                 (10)
#endif


/**
 * DEFAULT_THREAD_STACKSIZE: The stack size used by any other lwIP thread.
 * The stack size value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#ifndef DEFAULT_THREAD_STACKSIZE
#define DEFAULT_THREAD_STACKSIZE        (1024)
#endif

/**
 * DEFAULT_THREAD_PRIO: The priority assigned to any other lwIP thread.
 * The priority value itself is platform-dependent, but is passed to
 * sys_thread_new() when the thread is created.
 */
#ifndef DEFAULT_THREAD_PRIO
#define DEFAULT_THREAD_PRIO             (MMOSAL_TASK_PRI_NORM)
#endif

/**
 * DEFAULT_RAW_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_RAW. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#ifndef DEFAULT_RAW_RECVMBOX_SIZE
#define DEFAULT_RAW_RECVMBOX_SIZE       (10)
#endif

/**
 * DEFAULT_UDP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_UDP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#ifndef DEFAULT_UDP_RECVMBOX_SIZE
#define DEFAULT_UDP_RECVMBOX_SIZE       (10)
#endif

/**
 * DEFAULT_TCP_RECVMBOX_SIZE: The mailbox size for the incoming packets on a
 * NETCONN_TCP. The queue size value itself is platform-dependent, but is passed
 * to sys_mbox_new() when the recvmbox is created.
 */
#ifndef DEFAULT_TCP_RECVMBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE       (10)
#endif

/**
 * DEFAULT_ACCEPTMBOX_SIZE: The mailbox size for the incoming connections.
 * The queue size value itself is platform-dependent, but is passed to
 * sys_mbox_new() when the acceptmbox is created.
 */
#ifndef DEFAULT_ACCEPTMBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE         (10)
#endif

/*
   ----------------------------------------------
   ---------- Sequential layer options ----------
   ----------------------------------------------
*/

/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/

/**
 * LWIP_TCP_KEEPALIVE==1: Enable TCP_KEEPIDLE, TCP_KEEPINTVL and TCP_KEEPCNT
 * options processing. Note that TCP_KEEPIDLE and TCP_KEEPINTVL have to be set
 * in seconds. (does not require sockets.c, and will affect tcp.c)
 */
#ifndef LWIP_TCP_KEEPALIVE
#define LWIP_TCP_KEEPALIVE              (1)
#endif

/**
 * LWIP_SO_RCVTIMEO==1: Enable receive timeout for sockets/netconns and
 * SO_RCVTIMEO processing.
 */
#ifndef LWIP_SO_RCVTIMEO
#define LWIP_SO_RCVTIMEO                (1)
#endif

/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/

/* Disable stats by default to save space. */
#ifndef LWIP_STATS
#define LWIP_STATS                      (0)
#endif

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

/**
 * LWIP_IPV6==1: Enable IPv6
 *
 * @warning IPv6 must be *ENABLED* if using mmiperf, mmping, or emmet.
 */
#ifndef LWIP_IPV6
#define LWIP_IPV6 (1)
#endif

/**
 * LWIP_IPV6_SCOPES==1: Enable support for IPv6 address scopes, ensuring that
 * e.g. link-local addresses are really treated as link-local. Disable this
 * setting only for single-interface configurations.
 * This affects the layout of the ip6_addr and ip_addr_t structures.
 */
#ifndef LWIP_IPV6_SCOPES
#define LWIP_IPV6_SCOPES (LWIP_IPV6 && !LWIP_SINGLE_NETIF)
#endif

/**
 * LWIP_IPV6_DHCP6==1: enable DHCPv6 stateful/stateless address autoconfiguration.
 */
#ifndef LWIP_IPV6_DHCP6
#define LWIP_IPV6_DHCP6                 (LWIP_IPV6)
#endif

/**
 * LWIP_IPV6_NUM_ADDRESSES: Number of IPv6 addresses per netif.
 */
#ifndef LWIP_IPV6_NUM_ADDRESSES
#define LWIP_IPV6_NUM_ADDRESSES         (MMIPAL_MAX_IPV6_ADDRESSES)
#endif



/*
   ---------------------------------------
   ---------- Hook options ---------------
   ---------------------------------------
*/

/*
   ---------------------------------------
   ---------- Debugging options ----------
   ---------------------------------------
*/

#ifdef LWIP_DEBUG

#ifndef LWIP_DBG_MIN_LEVEL
/* Default to warning level, since "all" is a bit too verbose. */
#define LWIP_DBG_MIN_LEVEL  (LWIP_DBG_LEVEL_WARNING)
#endif

#define LWIP_DBG_TYPES_ON   (LWIP_DBG_ON)
#define SYS_DEBUG           (LWIP_DBG_ON)
#define MEM_DEBUG           (LWIP_DBG_ON)
#define MEMP_DEBUG          (LWIP_DBG_ON)
#define ETHARP_DEBUG        (LWIP_DBG_ON)
#define NETIF_DEBUG         (LWIP_DBG_ON)
#define PBUF_DEBUG          (LWIP_DBG_ON)
#define SOCKETS_DEBUG       (LWIP_DBG_ON)
#define API_MSG_DEBUG       (LWIP_DBG_ON)
#define API_LIB_DEBUG       (LWIP_DBG_ON)
#define INET_DEBUG          (LWIP_DBG_ON)
#define RAW_DEBUG           (LWIP_DBG_ON)
#define IP_DEBUG            (LWIP_DBG_ON)
#define TCP_DEBUG           (LWIP_DBG_ON)
#define TCP_INPUT_DEBUG     (LWIP_DBG_ON)
#define TCP_OUTPUT_DEBUG    (LWIP_DBG_ON)
#define TCP_QLEN_DEBUG      (LWIP_DBG_ON)
#define TCP_CWND_DEBUG      (LWIP_DBG_ON)
#define UDP_DEBUG           (LWIP_DBG_ON)
#define TCPIP_DEBUG         (LWIP_DBG_ON)
#define LWIPERF_DEBUG       (LWIP_DBG_ON)
#endif

#ifndef LWIP_DBG_TYPES_ON
#define LWIP_DBG_TYPES_ON               (LWIP_DBG_OFF)
#endif

/*
   --------------------------------------------------
   ---------- Performance tracking options ----------
   --------------------------------------------------
*/

/*
   -----------------------------------
   ---------- Other options ----------
   -----------------------------------
*/

#ifndef LWIP_PROVIDE_ERRNO
#define LWIP_PROVIDE_ERRNO              (1)
#endif

#ifndef LWIP_RAND
#define LWIP_RAND()                     (mmhal_random_u32(0, UINT32_MAX))
#endif

#ifndef LWIP_PLATFORM_ASSERT
#define LWIP_PLATFORM_ASSERT(x)         MMOSAL_ASSERT(false)
#endif

#ifndef SO_REUSE
#define SO_REUSE              (1)
#endif
