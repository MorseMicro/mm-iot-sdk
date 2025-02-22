/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal_controller.h"
#include "mmbuf.h"
#include "mmutils.h"
#include "mmagic_controller.h"
#include "mmagic_datalink_controller.h"

/*
 * ---------------------------------------------------------------------------------------------
 *
 * Agent/Controller shared LLC definitions
 *
 * This must be kept in sync with Agent.
 */

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

/*
 * ---------------------------------------------------------------------------------------------
 *
 * Agent/Controller shared M2M API definitions
 *
 * This must be kept in sync with Agent.
 */

/**
 * M2M command header.
 *
 * This is prefixed to M2M command packets.
 */
struct MM_PACKED mmagic_m2m_command_header
{
    /** The subsystem this commend is directed at */
    uint8_t subsystem;
    /** The command to execute */
    uint8_t command;
    /** The subcommand or setting if applicable */
    uint8_t subcommand;
    /** Reserved */
    uint8_t reserved;
};

/**
 * M2M response header.
 *
 * This is prefixed to M2M response packets.
 */
struct MM_PACKED mmagic_m2m_response_header
{
    /** The subsystem this response is from */
    uint8_t subsystem;
    /** The command this response is for */
    uint8_t command;
    /** The subcommand or setting if applicable */
    uint8_t subcommand;
    /** The result code, the response packet is only valid if the result is a success */
    uint8_t result;
};

/**
 * M2M event header.
 *
 * This is prefixed to M2M event packets.
 */
struct MM_PACKED mmagic_m2m_event_header
{
    /** The subsystem this commend is directed at */
    uint8_t subsystem;
    /** The event identifier */
    uint8_t event;
    /** Reserved */
    uint8_t reserved1;
    /** Reserved */
    uint8_t reserved2;
};

/** Enumeration of event identifiers. These are unique within a given subsystem. */
enum mmagic_m2m_event_id
{
    MMAGIC_WLAN_EVT_BEACON_RX    = 1,
    MMAGIC_WLAN_EVT_STANDBY_EXIT = 2,
};

/*
 * ---------------------------------------------------------------------------------------------
 */

/** Maximum number of streams possible. */
#define MMAGIC_LLC_MAX_STREAMS  (32)

struct mmagic_controller
{
    struct
    {
        /** The HAL transport handle */
        struct mmagic_datalink_controller *controller_dl;
        /* The last sequence number we received, used to detect lost/repeat packets */
        uint8_t last_seen_seq;
        /* The sequence number we sent, we increment this by 1 for every new packet sent */
        uint8_t last_sent_seq;
    } controller_llc;

    struct
    {
        mmagic_wlan_beacon_rx_event_handler_t wlan_beacon_rx;
        void *wlan_beacon_rx_arg;
        mmagic_wlan_standby_exit_event_handler_t wlan_standby_exit;
        void *wlan_standby_exit_arg;
    } event_handlers;

    struct mmosal_queue *stream_queue[MMAGIC_LLC_MAX_STREAMS];
    /** The mutex to protect access to the streams @c mmbuf_list and TX path */
    struct mmosal_mutex *tx_mutex;
    /** Callback function to executed any time a event that the agent has started is
     * received. */
    mmagic_controller_agent_start_cb_t agent_start_cb;
    /** User argument that will be passed when the agent_start_cb is executed. */
    void *agent_start_arg;
};

static void mmagic_m2m_controller_rx_callback(struct mmagic_controller *controller, uint8_t sid,
                                              struct mmbuf *rx_buffer);

static void mmagic_m2m_controller_event_rx_callback(struct mmagic_controller *controller,
                                                    uint8_t sid, struct mmbuf *rx_buffer);

/* -------------------------------------------------------------------------------------------- */

void mmagic_controller_register_wlan_beacon_rx_handler(
    struct mmagic_controller *controller,
    mmagic_wlan_beacon_rx_event_handler_t handler,
    void *arg)
{
    controller->event_handlers.wlan_beacon_rx = handler;
    controller->event_handlers.wlan_beacon_rx_arg = arg;
}

void mmagic_controller_register_wlan_standby_exit_handler(
    struct mmagic_controller *controller,
    mmagic_wlan_standby_exit_event_handler_t handler,
    void *arg)
{
    controller->event_handlers.wlan_standby_exit = handler;
    controller->event_handlers.wlan_standby_exit_arg = arg;
}

/* -------------------------------------------------------------------------------------------- */

struct mmbuf *mmagic_llc_controller_alloc_buffer_for_tx(struct mmagic_controller *controller,
                                                        uint8_t *payload, size_t payload_size)
{
    struct mmbuf *mmbuffer =
        mmagic_datalink_controller_alloc_buffer_for_tx(controller->controller_llc.controller_dl,
                                                       sizeof(struct mmagic_llc_header),
                                                       payload_size);
    if (mmbuffer && payload)
    {
        mmbuf_append_data(mmbuffer, payload, payload_size);
    }

    return mmbuffer;
}

static void mmagic_llc_controller_rx_callback(struct mmagic_datalink_controller *controller_dl,
                                              void *arg, struct mmbuf *rx_buffer)
{
    MM_UNUSED(controller_dl);

    struct mmagic_controller *controller = (struct mmagic_controller *)arg;

    if (rx_buffer == NULL)
    {
        /* Invalid packet received, ignore */
        printf("MMAGIC_LLC: Received NULL packet!\n");
        return;
    }

    /* Extract received header */
    struct mmagic_llc_header *rxheader = (struct mmagic_llc_header *)
        mmbuf_remove_from_start(rx_buffer, sizeof(*rxheader));
    if (rxheader == NULL)
    {
        /* Invalid packet received, ignore */
        printf("MMAGIC_LLC: Packet size was too small!\n");
        mmbuf_release(rx_buffer);
        return;
    }

    uint8_t sid = rxheader->sid;
    uint8_t seq = MMAGIC_LLC_GET_SEQ(rxheader->tseq);
    uint8_t ptype = MMAGIC_LLC_GET_PTYPE(rxheader->tseq);
    uint16_t length = rxheader->length;

    if (sid >= MMAGIC_LLC_MAX_STREAMS)
    {
        /* Invalid stream received, ignore */
        printf("MMAGIC_LLC: Invalid stream ID!\n");
        mmbuf_release(rx_buffer);
        return;
    }

    if (mmbuf_get_data_length(rx_buffer) < length)
    {
        printf("MMAGIC_LLC: Buffer smaller than length specified (%u)!\n", length);
        mmbuf_release(rx_buffer);
        return;
    }

    /* Only process if it is not a retransmission of a packet we have already seen
     * or it is an AGENT START packet as this could coincidentally be a repeat sequence */
    if ((seq != controller->controller_llc.last_seen_seq) ||
        (ptype == MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION))
    {
        switch (ptype)
        {
        case MMAGIC_LLC_PTYPE_RESPONSE:
            /* Response from agent, pass to appropriate stream queue */
            mmagic_m2m_controller_rx_callback(controller, sid, rx_buffer);

            /* mmagic_m2m_controller_rx_callback() takes ownership of rx_buffer, so we set the
             * reference to NULL here since we do not want it to be freed when this function
             * returns. */
            rx_buffer = NULL;
            break;

        case MMAGIC_LLC_PTYPE_EVENT:
            mmagic_m2m_controller_event_rx_callback(controller, sid, rx_buffer);

            /* mmagic_m2m_controller_event_rx_callback() takes ownership of rx_buffer, so we set
             * the reference to NULL here since we do not want it to be freed when this function
             * returns. */
            rx_buffer = NULL;
            break;

        case MMAGIC_LLC_PTYPE_ERROR:
            /* Log error and continue for now - we have to handle this explicitly or else we
             * could end up in an 'error loop' with both sides bouncing the error back and
             * forth. */
            printf("MMAGIC_LLC: Received error event from agent!\n");
            break;

        case MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION:
            printf("MMAGIC_LLC: Received agent START event!\n");
            if (controller->agent_start_cb)
            {
                controller->agent_start_cb(controller, controller->agent_start_arg);
            }
            break;

        case MMAGIC_LLC_PTYPE_INVALID_STREAM:
            printf("MMAGIC_LLC: Agent reports invalid stream!\n");
            break;

        case MMAGIC_LLC_PTYPE_PACKET_LOSS_DETECTED:
            printf("MMAGIC_LLC: Agent reports packet loss!\n");
            break;

        default:
            /* We have encountered an unexpected command or error */
            printf("MMAGIC_LLC: Received invalid packet from agent!\n");
            break;
        }

        /* Check if we missed a packet */
        if ((seq != MMAGIC_LLC_GET_NEXT_SEQ(controller->controller_llc.last_seen_seq)) &&
            (controller->controller_llc.last_seen_seq != MMAGIC_LLC_INVALID_SEQUENCE) &&
            (ptype != MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION))
        {
            /* We have encountered an out of order sequence */
            printf("MMAGIC_LLC: Packet loss detected!\n");
        }
    }
    else
    {
        printf("MMAGIC_LLC: Repeated packet dropped!\n");
    }

    /* Release RX buffer */
    mmbuf_release(rx_buffer);

    /* We always update the last seen sequence number even if we dropped the packet above */
    controller->controller_llc.last_seen_seq = seq;
}

enum mmagic_status mmagic_llc_controller_tx(struct mmagic_controller *controller, uint8_t sid,
                                            struct mmbuf *tx_buffer)
{
    struct mmagic_llc_header txheader;
    int tx_ret;

    if (tx_buffer == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    txheader.sid = sid;
    controller->controller_llc.last_sent_seq =
        MMAGIC_LLC_GET_NEXT_SEQ(controller->controller_llc.last_sent_seq);
    txheader.tseq = MMAGIC_LLC_SET_TSEQ(MMAGIC_LLC_PTYPE_COMMAND,
                                        controller->controller_llc.last_sent_seq);
    txheader.length = mmbuf_get_data_length(tx_buffer);

    if (mmbuf_available_space_at_start(tx_buffer) < sizeof(txheader))
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    /* Send the buffer - tx_buffer will be freed by mmhal_datalink after sending */
    mmbuf_prepend_data(tx_buffer, (uint8_t *)&txheader, sizeof(txheader));
    tx_ret = mmagic_datalink_controller_tx_buffer(controller->controller_llc.controller_dl,
                                                  tx_buffer);

    return (tx_ret > 0) ? MMAGIC_STATUS_OK : MMAGIC_STATUS_TX_ERROR;
}

/* -------------------------------------------------------------------------------------------- */

/**
 * Safely convert from an unsigned 8-bit integer value to @ref mmagic_status code.
 *
 * @note While this may look inefficient, the compiler should be smart in optimizing it.
 *
 * @param status The input status code (integer type).
 *
 * @returns The status code (as @c mmagic_status). Unrecognized input values will result in a
 *          return value of @c MMAGIC_STATUS_ERROR.
 */
static enum mmagic_status mmagic_status_from_u8(uint8_t status)
{
    switch (status)
    {
    case MMAGIC_STATUS_OK:
        return MMAGIC_STATUS_OK;

    case MMAGIC_STATUS_ERROR:
        return MMAGIC_STATUS_ERROR;

    case MMAGIC_STATUS_INVALID_ARG:
        return MMAGIC_STATUS_INVALID_ARG;

    case MMAGIC_STATUS_UNAVAILABLE:
        return MMAGIC_STATUS_UNAVAILABLE;

    case MMAGIC_STATUS_TIMEOUT:
        return MMAGIC_STATUS_TIMEOUT;

    case MMAGIC_STATUS_INVALID_STREAM:
        return MMAGIC_STATUS_INVALID_STREAM;

    case MMAGIC_STATUS_NOT_FOUND:
        return MMAGIC_STATUS_NOT_FOUND;

    case MMAGIC_STATUS_NOT_SUPPORTED:
        return MMAGIC_STATUS_NOT_SUPPORTED;

    case MMAGIC_STATUS_TX_ERROR:
        return MMAGIC_STATUS_TX_ERROR;

    case MMAGIC_STATUS_NO_MEM:
        return MMAGIC_STATUS_NO_MEM;

    case MMAGIC_STATUS_CLOSED:
        return MMAGIC_STATUS_CLOSED;

    case MMAGIC_STATUS_CHANNEL_LIST_NOT_SET:
        return MMAGIC_STATUS_CHANNEL_LIST_NOT_SET;

    case MMAGIC_STATUS_SHUTDOWN_BLOCKED:
        return MMAGIC_STATUS_SHUTDOWN_BLOCKED;

    case MMAGIC_STATUS_CHANNEL_INVALID:
        return MMAGIC_STATUS_CHANNEL_INVALID;

    case MMAGIC_STATUS_NOT_RUNNING:
        return MMAGIC_STATUS_NOT_RUNNING;

    case MMAGIC_STATUS_NO_LINK:
        return MMAGIC_STATUS_NO_LINK;

    case MMAGIC_STATUS_UNKNOWN_HOST:
        return MMAGIC_STATUS_UNKNOWN_HOST;

    case MMAGIC_STATUS_SOCKET_FAILED:
        return MMAGIC_STATUS_SOCKET_FAILED;

    case MMAGIC_STATUS_SOCKET_CONNECT_FAILED:
        return MMAGIC_STATUS_SOCKET_CONNECT_FAILED;

    case MMAGIC_STATUS_SOCKET_BIND_FAILED:
        return MMAGIC_STATUS_SOCKET_BIND_FAILED;

    case MMAGIC_STATUS_SOCKET_LISTEN_FAILED:
        return MMAGIC_STATUS_SOCKET_LISTEN_FAILED;

    default:
        return MMAGIC_STATUS_ERROR;
    }
}

enum mmagic_status mmagic_controller_rx(struct mmagic_controller *controller, uint8_t stream_id,
                                        uint8_t submodule_id, uint8_t command_id,
                                        uint8_t subcommand_id, uint8_t *buffer,
                                        size_t buffer_length)
{
    struct mmbuf *rx_buffer = NULL;
    struct mmagic_m2m_response_header *rx_header;
    if (!mmosal_queue_pop(controller->stream_queue[stream_id], &rx_buffer, UINT32_MAX))
    {
        return MMAGIC_STATUS_ERROR;
    }

    if (rx_buffer == NULL)
    {
        return MMAGIC_STATUS_ERROR;
    }

    if (stream_id >= MMAGIC_LLC_MAX_STREAMS)
    {
        mmbuf_release(rx_buffer);
        return MMAGIC_STATUS_INVALID_STREAM;
    }

    rx_header = (struct mmagic_m2m_response_header *)mmbuf_remove_from_start(rx_buffer,
                                                                             sizeof(*rx_header));
    if (rx_header == NULL)
    {
        /* Packet too small */
        mmbuf_release(rx_buffer);
        return MMAGIC_STATUS_ERROR;
    }

    size_t payload_len = mmbuf_get_data_length(rx_buffer);
    if (payload_len > buffer_length)
    {
        /* Packet too big */
        mmbuf_release(rx_buffer);
        return MMAGIC_STATUS_ERROR;
    }

    if (rx_header->result != 0)
    {
        /* Error condition indicated by the agent. */
        mmbuf_release(rx_buffer);
        return mmagic_status_from_u8(rx_header->result);
    }

    if ((rx_header->command == command_id) && (rx_header->subsystem == submodule_id) &&
        (rx_header->subcommand == subcommand_id))
    {
        memcpy(buffer, mmbuf_get_data_start(rx_buffer), payload_len);
        mmbuf_release(rx_buffer);
        return MMAGIC_STATUS_OK;
    }

    /* Packet header did not match */
    mmbuf_release(rx_buffer);
    return MMAGIC_STATUS_NOT_FOUND;
}

static void mmagic_m2m_controller_rx_callback(struct mmagic_controller *controller,
                                              uint8_t sid, struct mmbuf *rx_buffer)
{
    if (rx_buffer == NULL)
    {
        return;
    }

    if (sid >= MMAGIC_LLC_MAX_STREAMS)
    {
        /* Invalid stream */
        mmbuf_release(rx_buffer);
        return;
    }

    mmosal_queue_push(controller->stream_queue[sid], &rx_buffer, UINT32_MAX);
}

static void mmagic_m2m_controller_event_rx_callback(struct mmagic_controller *controller,
                                                    uint8_t sid, struct mmbuf *rx_buffer)
{
    struct mmagic_m2m_event_header *rx_header;

    if (rx_buffer == NULL)
    {
        return;
    }

    if (sid != CONTROL_STREAM)
    {
        /* Invalid stream */
        goto cleanup;
    }

    rx_header = (struct mmagic_m2m_event_header *)mmbuf_remove_from_start(
        rx_buffer, sizeof(*rx_header));
    if (rx_header == NULL)
    {
        /* Packet too small */
        goto cleanup;
    }

    switch (rx_header->subsystem)
    {
    case MMAGIC_WLAN:
        switch (rx_header->event)
        {
        case MMAGIC_WLAN_EVT_BEACON_RX:
            if (controller->event_handlers.wlan_beacon_rx != NULL)
            {
                /** Event arguments structure for wlan_beacon_rx */
                struct mmagic_wlan_beacon_rx_event_args *args =
                    (struct mmagic_wlan_beacon_rx_event_args *)
                    mmbuf_remove_from_start(rx_buffer, sizeof(*args));
                if (args == NULL)
                {
                    goto cleanup;
                }

                controller->event_handlers.wlan_beacon_rx(
                    args, controller->event_handlers.wlan_beacon_rx_arg);
            }
            break;

        case MMAGIC_WLAN_EVT_STANDBY_EXIT:
            if (controller->event_handlers.wlan_standby_exit != NULL)
            {
                /** Event arguments structure for wlan_standby_exit */
                struct mmagic_wlan_standby_exit_event_args *args =
                    (struct mmagic_wlan_standby_exit_event_args *)
                    mmbuf_remove_from_start(rx_buffer, sizeof(*args));
                if (args == NULL)
                {
                    goto cleanup;
                }

                controller->event_handlers.wlan_standby_exit(
                    args, controller->event_handlers.wlan_standby_exit_arg);
            }
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

cleanup:
    mmbuf_release(rx_buffer);
}

enum mmagic_status mmagic_controller_tx(struct mmagic_controller *controller, uint8_t stream_id,
                                        uint8_t submodule_id, uint8_t command_id,
                                        uint8_t subcommand_id,
                                        const uint8_t *buffer, size_t buffer_length)
{
    struct mmagic_m2m_command_header tx_header;
    enum mmagic_status status;
    struct mmbuf *tx_buffer =
        mmagic_llc_controller_alloc_buffer_for_tx(controller, NULL,
                                                  sizeof(struct mmagic_m2m_command_header) +
                                                  buffer_length);
    if (!tx_buffer)
    {
        status = MMAGIC_STATUS_NO_MEM;
        goto exit;
    }

    tx_header.subsystem = submodule_id;
    tx_header.command = command_id;
    tx_header.subcommand = subcommand_id;
    tx_header.reserved = 0;

    mmbuf_append_data(tx_buffer, (uint8_t *)&tx_header, sizeof(tx_header));
    mmbuf_append_data(tx_buffer, buffer, buffer_length);

    /* We only take the mutex here to make this call thread safe */
    mmosal_mutex_get(controller->tx_mutex, UINT32_MAX);
    status = mmagic_llc_controller_tx(controller, stream_id, tx_buffer);
    mmosal_mutex_release(controller->tx_mutex);

exit:
    return status;
}

static struct mmagic_controller m2m_controller;

static struct mmagic_controller *mmagic_controller_get(void)
{
    return &m2m_controller;
}

struct mmagic_controller *mmagic_controller_init(const struct mmagic_controller_init_args *args)
{
    int ii;
    struct mmagic_controller *controller = mmagic_controller_get();
    struct mmagic_datalink_controller_init_args init_args = MMAGIC_DATALINK_CONTROLLER_ARGS_INIT;

    memset(controller, 0, sizeof(*controller));

    controller->agent_start_cb = args->agent_start_cb;
    controller->agent_start_arg = args->agent_start_arg;
    controller->tx_mutex = mmosal_mutex_create("mmagic_llc_agent_datalink");
    if (controller->tx_mutex == NULL)
    {
        return NULL;
    }

    /* Create queues */
    for (ii = 0; ii < MMAGIC_LLC_MAX_STREAMS; ii++)
    {
        controller->stream_queue[ii] = mmosal_queue_create(1, sizeof(struct mmbuf *), NULL);
        if (controller->stream_queue[ii] == NULL)
        {
            goto error;
        }
    }

    /* Set this to an invalid number so we know we never saw a packet before */
    controller->controller_llc.last_seen_seq = MMAGIC_LLC_INVALID_SEQUENCE;
    /* Set this to an invalid number so we know we never sent a packet before */
    controller->controller_llc.last_sent_seq = MMAGIC_LLC_INVALID_SEQUENCE;

    init_args.rx_callback = mmagic_llc_controller_rx_callback;
    init_args.rx_arg = (void *)controller;
    controller->controller_llc.controller_dl = mmagic_datalink_controller_init(&init_args);
    if (!controller->controller_llc.controller_dl)
    {
        goto error;
    }

    return controller;

error:
    for (ii = 0; ii < MMAGIC_LLC_MAX_STREAMS; ii++)
    {
        mmosal_queue_delete(controller->stream_queue[ii]);
    }
    return NULL;
}

void mmagic_controller_deinit(struct mmagic_controller *controller)
{
    int ii;

    /* Delete queues */
    for (ii = 0; ii < MMAGIC_LLC_MAX_STREAMS; ii++)
    {
        mmosal_queue_delete(controller->stream_queue[ii]);
    }

    mmagic_datalink_controller_deinit(controller->controller_llc.controller_dl);
}
