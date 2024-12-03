/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @ingroup MMAGIC_DATALINK
 * @defgroup MMAGIC_DATALINK_AGENT Morse Micro Agent Data-link API
 *
 * @{
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "mmbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DATALINK_AGENT_SPI Definitions for SPI data link
 *
 * This must be kept in sync with Controller.
 *
 * @{
 */

#define MMAGIC_DATALINK_PAYLOAD_TYPE_SIZE (1)
#define MMAGIC_DATALINK_PAYLOAD_LEN_SIZE (2)
#define MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE (MMAGIC_DATALINK_PAYLOAD_TYPE_SIZE + \
                                             MMAGIC_DATALINK_PAYLOAD_LEN_SIZE)

enum mmagic_datalink_payload_type
{
    MMAGIC_DATALINK_NACK,
    MMAGIC_DATALINK_ACK,
    MMAGIC_DATALINK_WRITE,
    MMAGIC_DATALINK_READ,
    MMAGIC_DATALINK_REREAD,
};

/** @} */

/** Enumeration of deep sleep modes for the data-link subsystem. */
enum mmagic_datalink_agent_deep_sleep_mode
{
    /** Deep sleep mode is disabled. */
    MMAGIC_DATALINK_AGENT_DEEP_SLEEP_DISABLED,
    /** Enable deep sleep until activity occurs. To re-enter deep sleep the mode will need to be set
     * again. */
    MMAGIC_DATALINK_AGENT_DEEP_SLEEP_ONE_SHOT,
    /** There the mmagic_datalink_agent hardware controls the deep sleep veto. */
    MMAGIC_DATALINK_AGENT_DEEP_SLEEP_HARDWARE,
};

/**
 * Agent datalink struct used internally by the implementaton. This will be specific to each
 * type of interface. i.e SPI and UART may have different elements in the struct.
 */
struct mmagic_datalink_agent;

/**
 * Prototype for callback function invoked when the mmagic_datalink_agent receives a data payload.
 *
 * @note The callback function will executed in a thread context.
 *
 * @param agent_dl The datalink handle.
 * @param arg      User argument that was given when the callback was registered.
 * @param buf      mmbuf containing the received data. The callback takes ownership of the buffer
 *                  and is responsible for releasing it.
 */
typedef void (*mmagic_datalink_agent_rx_buffer_cb_t)(struct mmagic_datalink_agent *agent_dl,
                                                     void *arg,
                                                     struct mmbuf *buf);

/**
 * Initialization structure for mmagic_datalink_agent.
 */
struct mmagic_datalink_agent_init_args
{
    /** Callback function to execute when a packet has been received. */
    mmagic_datalink_agent_rx_buffer_cb_t rx_callback;
    /** User argument that will be passed when the rx callback is executed. */
    void *rx_arg;
    /** The maximum packet size expected to be received. */
    size_t max_packet_size;
};

/**
 * Initializer for @ref mmagic_datalink_agent_init_args.
 */
#define MMAGIC_DATALINK_AGENT_ARGS_INIT { 0 }

/**
 * Initialize the mmagic_datalink_agent.
 *
 * @param  args Reference to the initialization arguments for the mmagic_datalink_agent.
 *
 * @return      Reference to the agent datalink handle on success. @c Null on error.
 */
struct mmagic_datalink_agent *mmagic_datalink_agent_init(
    const struct mmagic_datalink_agent_init_args *args);

/**
 * De-initialize the data-link layer and free any resources and buffers used.
 *
 * @param agent_dl The datalink handle.
 */
void mmagic_datalink_agent_deinit(struct mmagic_datalink_agent *agent_dl);

/**
 * Allocates a buffer that can subsequently used with @ref mmagic_datalink_agent_tx_buffer().
 *
 * The allocated buffer may also include additional space at the end of the payload to allow for
 * things like CRC bits.
 *
 * @param  header_size  The size of any headers that may be required.
 * @param  payload_size The size of the payload.
 *
 * @return              An @c mmbuf with the required space pre allocated. NULL on error.
 */
struct mmbuf *mmagic_datalink_agent_alloc_buffer_for_tx(size_t header_size, size_t payload_size);

/**
 * Transmits the given buffer as a packet to the controller. This function will block until the
 * packet has been transmitted. @ref mmagic_datalink_controller_alloc_buffer_for_tx() should be used
 * to
 * allocated the buffer.
 *
 * @param  agent_dl Reference to the datalink handle.
 * @param  buf      The buffer containing the data to be framed and sent to the controller. This
 *                  function takes ownership of the buffer and is responsible for releasing it.
 *
 * @return          The number of bytes transmitted on success. Negative value on error.
 */
int mmagic_datalink_agent_tx_buffer(struct mmagic_datalink_agent *agent_dl, struct mmbuf *buf);

/**
 * Set the data-link deep sleep mode. See @ref mmagic_datalink_agent_deep_sleep_mode for possible
 * deep
 * sleep modes. Note that a given platform may not support all modes.
 *
 * @param agent_dl The datalink handle.
 * @param mode     The deep sleep mode to set.
 *
 * @returns @ true if the mode was set successfully; @c false on failure (e.g. unsupported mode).
 */
bool mmagic_datalink_agent_set_deep_sleep_mode(struct mmagic_datalink_agent *agent_dl,
                                               enum mmagic_datalink_agent_deep_sleep_mode mode);

#ifdef __cplusplus
}
#endif

/** @} */
