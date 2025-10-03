/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @defgroup MMAGIC_LLC_AGENT Morse Micro Agent Logical Link Control API.
 *
 * This API defines the Morse Micro M2M LLC protocol for the agent.
 *
 * @{
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mmagic.h"
#include "mmagic_datalink_agent.h"
#include "core/autogen/mmagic_core_types.h"

/**
 * @defgroup MMAGIC_LLC_AGENT_COMMON Agent/Controller shared definitions
 *
 * This must be kept in sync with Controller.
 *
 * @{
 */

#define MMAGIC_LLC_PROTOCOL_VERSION    1U
MM_STATIC_ASSERT(MMAGIC_LLC_PROTOCOL_VERSION < UINT8_MAX, "Protocol version must be uint8");

/** The maximum size of packets we support */
#define MMAGIC_LLC_MAX_PACKET_SIZE    2048

/** Extracts the packet type from the TSEQ field */
#define MMAGIC_LLC_GET_PTYPE(x)       ((x) >> 4)

/** Extracts the sequence number from the TSEQ field */
#define MMAGIC_LLC_GET_SEQ(x)         ((x) & 0x0F)

/** Gets the next SEQ number */
#define MMAGIC_LLC_GET_NEXT_SEQ(x)    ((x + 1) & 0x0F)

/** Sets the packet type and seq in the the TSEQ field */
#define MMAGIC_LLC_SET_TSEQ(t, s)   (((t) << 4) | (s & 0x0F))

/* An invalid sequence ID that can never be encountered normally */
#define MMAGIC_LLC_INVALID_SEQUENCE   0xFF

/**
 * The LLC uses these packet types to sequence communications between the
 * controller and the agent. This is encoded into 4 bits of the PTYPE/SEQ byte and
 * so is limited to 16 packet type entries at most.
 */
enum mmagic_llc_packet_type
{
    /** This is a command from the controller to the agent */
    MMAGIC_LLC_PTYPE_COMMAND                  = 0,

    /** This is a response from the agent to the controller. */
    MMAGIC_LLC_PTYPE_RESPONSE                 = 1,

    /** This is an unsolicited event from Agent to Controller. */
    MMAGIC_LLC_PTYPE_EVENT                    = 2,

    /** An unspecified error condition has occured */
    MMAGIC_LLC_PTYPE_ERROR                    = 3,

    /** Instructs the agent to reset itself */
    MMAGIC_LLC_PTYPE_AGENT_RESET              = 4,

    /** Notifies the controller that the agent was just reset or started */
    MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION = 5,

    /** Notifies the other party that the referenced stream is invalid or not opened yet */
    MMAGIC_LLC_PTYPE_INVALID_STREAM           = 8,

    /** Notifies the other party that a packet was missed due to a gap in the sequence numbers */
    MMAGIC_LLC_PTYPE_PACKET_LOSS_DETECTED     = 9,

    /** Sent by the Controller to request the Agent to send a SYNC_RESP. Does not increment
     *  the sequence number counter. Sequence number counter is ignored by the Agent. */
    MMAGIC_LLC_PTYPE_SYNC_REQ                 = 10,

    /** Sent by the Agent in response to the Controller. Does not increment the sequence number
     *  counter. */
    MMAGIC_LLC_PTYPE_SYNC_RESP                = 11,
};

struct MM_PACKED mmagic_llc_header
{
    /** Packet type and sequence number, cmd is upper nibble, seq is lower nibble */
    uint8_t tseq;
    /** Stream ID */
    uint8_t sid;
    /** Length of the llc packet not including the header. */
    uint16_t length;
};

struct MM_PACKED mmagic_llc_sync_req
{
    /** Token to match respones to requests. */
    uint8_t token[4];
};

struct MM_PACKED mmagic_llc_sync_rsp
{
    /** Token to match respones to requests. */
    uint8_t token[4];
    /** The last seen sequence number for Controller to Agent transmissions. */
    uint8_t last_seen_seq;
    /** LLC protocol version. */
    uint8_t protocol_version;
};

MM_STATIC_ASSERT(MM_MEMBER_SIZE(struct mmagic_llc_sync_req, token) ==
                 MM_MEMBER_SIZE(struct mmagic_llc_sync_rsp, token),
                 "REQ and RESP tokens must match");

MM_STATIC_ASSERT(MM_MEMBER_SIZE(struct mmagic_llc_sync_req, token) == sizeof(uint32_t),
                 "Token must match generated token type");

/** Invalid token value in sync req/resp */
#define INVALID_TOKEN_U32 0U

/** @} */

/** Agent LLC struct used internally by the implementation. */
struct mmagic_llc_agent;

/**
 * Prototype for callback function invoked on LLC received data.
 *
 * This function is called whenever data is received from the controller. An @c mmbuf
 * is passed with the received data starting from the @c start_offset specified in the
 * @c mmbuf. It is the responsibility of the user to free the @c mmbuf when done processing
 * the data.
 *
 * @param  agent_llc The LLC handle.
 * @param  app_ctx   The application context passed in the @c args for @c mmagic_llc_agent_init().
 * @param  sid       The stream ID of this data.
 * @param  rx_buffer The @c mmbuf containing the received data. The callback takes ownership of
 *                   the buffer and is responsible for releasing it.
 *
 * @return           @c MMAGIC_STATUS_OK if packet was handled succesfully, error otherwise.
 */
typedef enum mmagic_status (*mmagic_llc_agent_rx_callback_t)(struct mmagic_llc_agent *agent_llc,
                                                             void *arg, uint8_t sid,
                                                             struct mmbuf *rx_buffer);

/**
 * Initialization structure for mmagic_llc.
 */
struct mmagic_llc_agent_int_args
{
    /** User argument to be passed when the rx callback is executed. */
    void *rx_arg;
    /** Callback to call when data is received. */
    mmagic_llc_agent_rx_callback_t rx_callback;
};

/**
 * Creates and initializes the LLC.
 *
 * @param  args Initialization arguments passed to the LLC.
 *
 * @return      A handle to the newly created LLC. NULL on error.
 */
struct mmagic_llc_agent *mmagic_llc_agent_init(struct mmagic_llc_agent_int_args *args);

/**
 * De-initialize the LLC layer and free any resources and buffers used.
 *
 * @param agent_llc The LLC handle.
 */
void mmagic_llc_agent_deinit(struct mmagic_llc_agent *agent_llc);

/**
 * This function will construct and transmit a packet to notify the controller that the agent has
 * started. The main purpose of this start notication packet is to allow the controller to detect
 * when the agent has been reset.
 *
 * @note This will block until the start notification packet has been transmitted.
 *
 * @return @c MMAGIC_STATUS_OK on success, else appropriate @ref mmagic_status error.
 */
enum mmagic_status mmagic_llc_send_start_notification(struct mmagic_llc_agent *agent_llc);

/**
 * This function allocates an @c mmbuf for use with @ref mmagic_llc_agent_tx(). If a payload is
 * provided it will copy it into the allocated buffer.
 *
 * @param  payload      A pointer to the payload, this will get copied into the @c mmbuf. Pass null
 *                      to only allocate and not copy.
 * @param  payload_size The size of the payload.
 *
 * @return              An @c mmbuf with the required space preallocated. @c NULL on error.
 */
struct mmbuf *mmagic_llc_agent_alloc_buffer_for_tx(uint8_t *payload, size_t payload_size);

/**
 * This function adds data to the TX queue of a specific stream id.
 *
 * The user must allocate an @c mmbuf using @ref mmagic_llc_agent_alloc_buffer_for_tx() and then
 * call this function to add it to the TX queue for the specified stream. The LLC takes ownership
 * of the @c mmbuf and will automatically free it once the data has been sent. This function can
 * be called as many times as required to add more data to the queue, but take care not to exhaust
 * the available memory.
 *
 * @param  agent_llc The LLC handle.
 * @param  ptype     The LLC packet type.
 * @param  sid       The stream ID to queue this data to.
 * @param  tx_buffer The @c mmbuf to add to the TX queue for the specified stream.
 *
 * @return           @c MMAGIC_STATUS_OK on success, else appropriate @ref mmagic_status error.
 */
enum mmagic_status mmagic_llc_agent_tx(struct mmagic_llc_agent *agent_llc,
                                       enum mmagic_llc_packet_type ptype,
                                       uint8_t sid, struct mmbuf *tx_buffer);

/**
 * Set the deep sleep mode. See @ref mmagic_deep_sleep_mode for possible deep sleep modes.
 *
 * @param agent_llc The LLC handle.
 * @param mode      The deep sleep mode to set.
 *
 * @returns @ true if the mode was set successfully; @c false on failure (e.g. unsupported mode).
 */
bool mmagic_llc_agent_set_deep_sleep_mode(struct mmagic_llc_agent *agent_llc,
                                          enum mmagic_deep_sleep_mode mode);

#ifdef __cplusplus
}
#endif

/** @} */
