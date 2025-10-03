/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdatomic.h"
#include "mmhal.h"
#include "mmosal.h"
#include "mmagic_datalink_agent.h"
#include "mmbuf.h"
#include "mmutils.h"
#include "mmagic_llc_agent.h"
#include "m2m_api/mmagic_m2m_agent.h"

struct mmagic_llc_agent
{
    /** Callback to call when data is received. */
    mmagic_llc_agent_rx_callback_t rx_callback;
    /** User argument to be passed when the rx callback is executed. */
    void *rx_arg;
    /** The HAL transport handle */
    struct mmagic_datalink_agent *agent_dl;
    /** The mutex to protect access to the streams @c mmbuf_list and TX path */
    struct mmosal_mutex *datalink_mutex;
    /* The last sequence number we received, used to detect lost/repeat packets */
    uint8_t last_seen_seq;
    /* The sequence number we sent, we increment this by 1 for every new packet sent */
    uint8_t last_sent_seq;
};

/**
 * Static allocation of the Agent LLC structure. Currently it is expected that there will only ever
 * be one instance.
 */
static struct mmagic_llc_agent m2m_agent_llc;

/** Function to retrieve the global agent address. */
static struct mmagic_llc_agent *mmagic_llc_agent_get(void)
{
    return &m2m_agent_llc;
}

struct mmbuf *mmagic_llc_agent_alloc_buffer_for_tx(uint8_t *payload, size_t payload_size)
{
    struct mmbuf *mmbuffer =
        mmagic_datalink_agent_alloc_buffer_for_tx(sizeof(struct mmagic_llc_header),
                                                  payload_size);
    if (mmbuffer && payload)
    {
        mmbuf_append_data(mmbuffer, payload, payload_size);
    }

    return mmbuffer;
}

static enum mmagic_status mmagic_llc_respond_error(struct mmagic_llc_agent *agent_llc, uint8_t sid,
                                                   enum mmagic_llc_packet_type ptype)
{
    struct mmbuf *tx_buffer = mmagic_llc_agent_alloc_buffer_for_tx(NULL, 0);
    if (tx_buffer == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }
    return mmagic_llc_agent_tx(agent_llc, ptype, sid, tx_buffer);
}

static enum mmagic_status mmagic_llc_sync_resp(struct mmagic_llc_agent *agent_llc, uint8_t sid,
                                               struct mmagic_llc_sync_req *req)
{
    MMOSAL_DEV_ASSERT(req);
    struct mmagic_llc_sync_rsp rsp = {
        .last_seen_seq = agent_llc->last_seen_seq,
        .protocol_version = MMAGIC_LLC_PROTOCOL_VERSION
    };
    memcpy(rsp.token, req->token, sizeof(rsp.token));

    struct mmbuf *tx_buffer = mmagic_llc_agent_alloc_buffer_for_tx((uint8_t *)&rsp, sizeof(rsp));
    if (tx_buffer == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }

    return mmagic_llc_agent_tx(agent_llc, MMAGIC_LLC_PTYPE_SYNC_RESP, sid, tx_buffer);
}

static void mmagic_llc_agent_rx_buffer_callback(struct mmagic_datalink_agent *agent_dl, void *arg,
                                                struct mmbuf *rx_buffer)
{
    MM_UNUSED(agent_dl);
    if (rx_buffer == NULL)
    {
        /* Invalid packet received, ignore */
        mmosal_printf("MMAGIC_LLC: Received NULL packet!\n");
        return;
    }

    struct mmagic_llc_agent *agent_llc = (struct mmagic_llc_agent *)arg;
    enum mmagic_status tx_status = MMAGIC_STATUS_OK;
    uint8_t seq;

    /* Extract received header */
    struct mmagic_llc_header *rxheader =
        (struct mmagic_llc_header *)mmbuf_remove_from_start(rx_buffer, sizeof(*rxheader));

    if (rxheader == NULL)
    {
        /* Invalid packet received, ignore */
        mmosal_printf("MMAGIC_LLC: Packet size too small %lu!\n", mmbuf_get_data_length(rx_buffer));
        /* We haven't seen a new seq number as the packet was invalid. Use the last_seen_seq
         * so the update on exit is a no-op */
        seq = agent_llc->last_seen_seq;
        goto exit;
    }

    uint8_t sid = rxheader->sid;
    seq = MMAGIC_LLC_GET_SEQ(rxheader->tseq);
    enum mmagic_llc_packet_type ptype =
        (enum mmagic_llc_packet_type)MMAGIC_LLC_GET_PTYPE(rxheader->tseq);
    uint16_t length = rxheader->length;

    if (sid >= MMAGIC_MAX_STREAMS)
    {
        mmosal_printf("MMAGIC_LLC: Invalid stream ID %u!\n", sid);
        tx_status = mmagic_llc_respond_error(agent_llc, sid, MMAGIC_LLC_PTYPE_INVALID_STREAM);
        goto exit;
    }

    if (mmbuf_get_data_length(rx_buffer) < length)
    {
        mmosal_printf("MMAGIC_LLC: Buffer smaller than length specified (%u < %u)!\n",
                      mmbuf_get_data_length(rx_buffer), length);
        tx_status = mmagic_llc_respond_error(agent_llc, sid, MMAGIC_LLC_PTYPE_ERROR);
        goto exit;
    }

    /* Ignore if packet is a retransmission of a packet we have already seen, unless it is a
     * recovery packet. */
    if ((seq == agent_llc->last_seen_seq) && (ptype != MMAGIC_LLC_PTYPE_AGENT_RESET) &&
        (ptype != MMAGIC_LLC_PTYPE_SYNC_REQ))
    {
        mmosal_printf("MMAGIC_LLC: Repeated packet dropped!\n");
        goto exit;
    }

    switch (ptype)
    {
    case MMAGIC_LLC_PTYPE_COMMAND:
        /* WriteStream command, pass to rx_callback */
        tx_status = agent_llc->rx_callback(agent_llc, agent_llc->rx_arg, sid,
                                           rx_buffer);
        if (tx_status == MMAGIC_STATUS_OK)
        {
            /* Do not release rx_buffer - this will be done by application */
            rx_buffer = NULL;
            break;
        }
        /* Upper layer could not process the packet or invalid stream ID */
        tx_status = mmagic_llc_respond_error(agent_llc, sid,
                                             (tx_status == MMAGIC_STATUS_INVALID_STREAM) ?
                                             MMAGIC_LLC_PTYPE_INVALID_STREAM :
                                             MMAGIC_LLC_PTYPE_ERROR);
        break;

    case MMAGIC_LLC_PTYPE_ERROR:
        /* Log error and continue for now - we have to handle this explicitly or else we
         * could end up in an 'error loop' with both sides bouncing the error back and
         * forth. */
        mmosal_printf("MMAGIC_LLC: Received error notification from controller!\n");
        break;

    case MMAGIC_LLC_PTYPE_AGENT_RESET:
        mmosal_printf("MMAGIC_LLC: Received AGENT_RESET packet, restarting!\n");
        mmhal_reset();
        break;

    case MMAGIC_LLC_PTYPE_SYNC_REQ:
        mmosal_printf("MMAGIC_LLC: Sync request with seq: %u (last seen: %u, last sent: %u)\n",
                      seq, agent_llc->last_seen_seq, agent_llc->last_sent_seq);

        struct mmagic_llc_sync_req *req;
        if (length != sizeof(*req))
        {
            mmosal_printf("MMAGIC_LLC: Sync bad data length!\n");
            mmagic_llc_respond_error(agent_llc, sid, MMAGIC_LLC_PTYPE_ERROR);
            break;
        }

        req = (struct mmagic_llc_sync_req *)mmbuf_get_data_start(rx_buffer);
        tx_status = mmagic_llc_sync_resp(agent_llc, sid, req);
        break;

    case MMAGIC_LLC_PTYPE_RESPONSE:
    case MMAGIC_LLC_PTYPE_EVENT:
    case MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION:
    case MMAGIC_LLC_PTYPE_INVALID_STREAM:
    case MMAGIC_LLC_PTYPE_PACKET_LOSS_DETECTED:
    case MMAGIC_LLC_PTYPE_SYNC_RESP:
    default:
        /* We have encountered an unexpected command or error. */
        mmosal_printf("MMAGIC_LLC: Received invalid packet of ptype: %u\n", ptype);
        tx_status = mmagic_llc_respond_error(agent_llc, sid, MMAGIC_LLC_PTYPE_ERROR);
        break;
    }

    /* Check if we missed a packet. Ignore if sync request */
    if ((seq != MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_seen_seq)) &&
        (agent_llc->last_seen_seq != MMAGIC_LLC_INVALID_SEQUENCE) &&
        (ptype != MMAGIC_LLC_PTYPE_SYNC_REQ))
    {
        /* We have encountered an out of order sequence */
        mmosal_printf("MMAGIC_LLC: Observed packet loss - seq num observed: %u expected: %u\n",
                      seq, MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_seen_seq));
        tx_status = mmagic_llc_respond_error(agent_llc, sid,
                                             MMAGIC_LLC_PTYPE_PACKET_LOSS_DETECTED);
    }

exit:
    if (tx_status != MMAGIC_STATUS_OK)
    {
        mmosal_printf("Failed to TX to controller with status %lu\n", tx_status);
    }

    mmbuf_release(rx_buffer);

    /* We always update the last seen sequence number even if we dropped the packet above. */
    agent_llc->last_seen_seq = seq;
}

struct mmagic_llc_agent *mmagic_llc_agent_init(struct mmagic_llc_agent_int_args *args)
{
    struct mmagic_datalink_agent_init_args init_args = MMAGIC_DATALINK_AGENT_ARGS_INIT;
    struct mmagic_llc_agent *agent_llc = mmagic_llc_agent_get();

    agent_llc->datalink_mutex = mmosal_mutex_create("mmagic_llc_agent_datalink");
    if (agent_llc->datalink_mutex == NULL)
    {
        goto free_and_exit;
    }

    agent_llc->rx_arg = args->rx_arg;
    agent_llc->rx_callback = args->rx_callback;

    /* Set this to an invalid number so we know we never saw a packet before */
    agent_llc->last_seen_seq = MMAGIC_LLC_INVALID_SEQUENCE;
    /* Set this to an invalid number so we know we never sent a packet before */
    agent_llc->last_sent_seq = MMAGIC_LLC_INVALID_SEQUENCE;

    init_args.rx_callback = mmagic_llc_agent_rx_buffer_callback;
    init_args.rx_arg = (void *)agent_llc;
    init_args.max_packet_size = MMAGIC_LLC_MAX_PACKET_SIZE;
    agent_llc->agent_dl = mmagic_datalink_agent_init(&init_args);
    if (!agent_llc->agent_dl)
    {
        goto free_delete_and_exit;
    }

    return agent_llc;

free_delete_and_exit:
    mmosal_mutex_delete(agent_llc->datalink_mutex);

free_and_exit:
    return NULL;
}

void mmagic_llc_agent_deinit(struct mmagic_llc_agent *agent_llc)
{
    mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);

    /* Free any buffers in the TX queue if required */
    mmosal_mutex_release(agent_llc->datalink_mutex);
    mmosal_mutex_delete(agent_llc->datalink_mutex);
    mmagic_datalink_agent_deinit(agent_llc->agent_dl);

    mmosal_free(agent_llc);
}

enum mmagic_status mmagic_llc_send_start_notification(struct mmagic_llc_agent *agent_llc)
{
    struct mmbuf *tx_buffer = mmagic_llc_agent_alloc_buffer_for_tx(NULL, 0);
    if (tx_buffer == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }

    /* Notify controller that the agent has started */
    return mmagic_llc_agent_tx(agent_llc, MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION,
                               CONTROL_STREAM, tx_buffer);
}

enum mmagic_status mmagic_llc_agent_tx(struct mmagic_llc_agent *agent_llc,
                                       enum mmagic_llc_packet_type ptype,
                                       uint8_t sid, struct mmbuf *tx_buffer)
{
    struct mmagic_llc_header *txheader;
    if (tx_buffer == NULL)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    uint32_t payload_len = mmbuf_get_data_length(tx_buffer);
    if (payload_len > UINT16_MAX)
    {
        mmbuf_release(tx_buffer);
        return MMAGIC_STATUS_INVALID_ARG;
    }

    if (mmbuf_available_space_at_start(tx_buffer) < sizeof(*txheader))
    {
        mmbuf_release(tx_buffer);
        return MMAGIC_STATUS_NO_MEM;
    }

    txheader = (struct mmagic_llc_header *)mmbuf_prepend(tx_buffer, sizeof(*txheader));
    txheader->sid = sid;
    txheader->length = payload_len;

    /* We take the mutex here to make tx_seq transmission thread safe */
    mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);
    uint8_t sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
    txheader->tseq = MMAGIC_LLC_SET_TSEQ(ptype, sent_seq);

    /* Send the buffer - tx_buffer will be freed by mmhal_datalink */
    enum mmagic_status status = MMAGIC_STATUS_TX_ERROR;
    if (mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer) > 0)
    {
        /* Update last_sent_seq if datalink indicates data sent */
        status = MMAGIC_STATUS_OK;
        agent_llc->last_sent_seq = sent_seq;
    }
    mmosal_mutex_release(agent_llc->datalink_mutex);
    return status;
}

bool mmagic_llc_agent_set_deep_sleep_mode(struct mmagic_llc_agent *agent_llc,
                                          enum mmagic_deep_sleep_mode mode)
{
    enum mmagic_datalink_agent_deep_sleep_mode datalink_mode;

    switch (mode)
    {
    case MMAGIC_DEEP_SLEEP_MODE_DISABLED:
        datalink_mode = MMAGIC_DATALINK_AGENT_DEEP_SLEEP_DISABLED;
        break;

    case MMAGIC_DEEP_SLEEP_MODE_ONE_SHOT:
        datalink_mode = MMAGIC_DATALINK_AGENT_DEEP_SLEEP_ONE_SHOT;
        break;

    case MMAGIC_DEEP_SLEEP_MODE_HARDWARE:
        datalink_mode = MMAGIC_DATALINK_AGENT_DEEP_SLEEP_HARDWARE;
        break;

    default:
        return false;
    }

    return mmagic_datalink_agent_set_deep_sleep_mode(agent_llc->agent_dl, datalink_mode);
}
