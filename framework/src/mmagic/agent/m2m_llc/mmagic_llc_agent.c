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

static void mmagic_llc_agent_rx_buffer_callback(struct mmagic_datalink_agent *agent_dl, void *arg,
                                                struct mmbuf *rx_buffer)
{
    uint8_t sid;
    uint8_t seq;
    uint8_t ptype;
    MM_UNUSED(agent_dl);

    struct mmagic_llc_agent *agent_llc = (struct mmagic_llc_agent *)arg;
    /* Extract received header */
    struct mmagic_llc_header *rxheader =
        (struct mmagic_llc_header *)mmbuf_remove_from_start(rx_buffer, sizeof(*rxheader));

    if (rxheader == NULL)
    {
        /* Invalid packet received, ignore */
        goto exit;
    }

    sid = rxheader->sid;
    seq = MMAGIC_LLC_GET_SEQ(rxheader->tseq);
    ptype = MMAGIC_LLC_GET_PTYPE(rxheader->tseq);

    /* Only process if it is not a retransmission of a packet we have already seen */
    if (seq != agent_llc->last_seen_seq)
    {
        switch (ptype)
        {
        case MMAGIC_LLC_PTYPE_COMMAND:
            /* WriteStream command, pass to rx_callback */
            if ((sid < MMAGIC_MAX_STREAMS) &&
                (agent_llc->rx_callback(agent_llc, agent_llc->rx_arg, sid, rx_buffer) ==
                 MMAGIC_STATUS_OK))
            {
                /* Do not release rx_buffer - this will be done by application */
                rx_buffer = NULL;
            }
            else
            {
                /* Upper layer could not process the packet or invalid stream ID */
                struct mmagic_llc_header txheader;

                /* We only take the mutex here to make this call thread safe */
                mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);
                txheader.sid = sid;
                agent_llc->last_sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
                txheader.tseq = MMAGIC_LLC_SET_TSEQ(MMAGIC_LLC_PTYPE_INVALID_STREAM,
                                                    agent_llc->last_sent_seq);
                txheader.length = 0;

                struct mmbuf *tx_buffer =
                    mmagic_llc_agent_alloc_buffer_for_tx((uint8_t *)&txheader, sizeof(txheader));
                if (tx_buffer != NULL)
                {
                    /* Send the buffer - tx_buffer will be freed by mmhal_datalink
                     * after sending */
                    mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer);
                }

                mmosal_mutex_release(agent_llc->datalink_mutex);
            }
            break;

        case MMAGIC_LLC_PTYPE_ERROR:
            /* Log error and continue for now - we have to handle this explicitly or else we
             * could end up in an 'error loop' with both sides bouncing the error back and
             * forth. */
            printf("MMAGIC_LLC: Received error notification from controller!\n");
            break;

        case MMAGIC_LLC_PTYPE_AGENT_RESET:
            printf("MMAGIC_LLC: Received AGENT_RESET packet, restarting!\n");
            mmhal_reset();
            break;

        default:
        {
            /* We have encountered an unexpected command or error */
            struct mmagic_llc_header txheader;

            /* We only take the mutex here to make this call thread safe */
            mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);
            txheader.sid = 0;
            agent_llc->last_sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
            txheader.tseq = MMAGIC_LLC_SET_TSEQ(MMAGIC_LLC_PTYPE_ERROR,
                                                agent_llc->last_sent_seq);
            txheader.length = 0;

            struct mmbuf *tx_buffer =
                mmagic_llc_agent_alloc_buffer_for_tx((uint8_t *)&txheader, sizeof(txheader));
            if (tx_buffer != NULL)
            {
                /* Send the buffer - tx_buffer will be freed by mmhal_datalink
                 * after sending */
                mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer);
            }

            mmosal_mutex_release(agent_llc->datalink_mutex);
            break;
        }
        }

        /* Check if we missed a packet */
        if ((seq != MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_seen_seq)) &&
            (agent_llc->last_seen_seq != MMAGIC_LLC_INVALID_SEQUENCE))
        {
            struct mmagic_llc_header *tx_header;

            struct mmbuf *tx_buffer =
                mmagic_llc_agent_alloc_buffer_for_tx(NULL, sizeof(*tx_header));

            if (tx_buffer == NULL)
            {
                goto exit;
            }

            tx_header = (struct mmagic_llc_header *)mmbuf_append(tx_buffer, sizeof(*tx_header));

            /* We only take the mutex here to make this call thread safe */
            mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);
            tx_header->sid = 0;
            agent_llc->last_sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
            tx_header->tseq = MMAGIC_LLC_SET_TSEQ(MMAGIC_LLC_PTYPE_PACKET_LOSS_DETECTED,
                                                  agent_llc->last_sent_seq);
            tx_header->length = 0;

            /* Send the buffer - tx_buffer will be freed by mmhal_datalink after sending */
            mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer);

            mmosal_mutex_release(agent_llc->datalink_mutex);
        }
    }
    else
    {
        printf("Ignoring packet due to seq type\n");
    }

    agent_llc->last_seen_seq = seq;

exit:
    mmbuf_release(rx_buffer);
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

bool mmagic_llc_send_start_notification(struct mmagic_llc_agent *agent_llc)
{
    int bytes_txed;
    struct mmagic_llc_header *tx_header;
    struct mmbuf *tx_buffer = NULL;

    tx_buffer = mmagic_llc_agent_alloc_buffer_for_tx(NULL, sizeof(*tx_header));
    if (tx_buffer == NULL)
    {
        goto error;
    }

    tx_header = (struct mmagic_llc_header *)mmbuf_append(tx_buffer, sizeof(*tx_header));
    if (tx_header == NULL)
    {
        goto error;
    }

    /* Notify controller that the agent has started */
    tx_header->sid = CONTROL_STREAM;
    agent_llc->last_sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
    tx_header->tseq = MMAGIC_LLC_SET_TSEQ(MMAGIC_LLC_PTYPE_AGENT_START_NOTIFICATION,
                                          agent_llc->last_sent_seq);
    tx_header->length = 0;

    bytes_txed = mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer);
    if (bytes_txed < 0)
    {
        return false;
    }

    return true;

error:
    mmbuf_release(tx_buffer);
    return false;
}

void mmagic_llc_agent_tx(struct mmagic_llc_agent *agent_llc, enum mmagic_llc_packet_type ptype,
                         uint8_t sid, struct mmbuf *tx_buffer)
{
    struct mmagic_llc_header txheader;
    MMOSAL_ASSERT(tx_buffer);

    /* We only take the mutex here to make this call thread safe */
    mmosal_mutex_get(agent_llc->datalink_mutex, UINT32_MAX);
    txheader.sid = sid;
    agent_llc->last_sent_seq = MMAGIC_LLC_GET_NEXT_SEQ(agent_llc->last_sent_seq);
    txheader.tseq = MMAGIC_LLC_SET_TSEQ(ptype, agent_llc->last_sent_seq);
    txheader.length = mmbuf_get_data_length(tx_buffer);

    if (mmbuf_available_space_at_start(tx_buffer) >= sizeof(txheader))
    {
        /* Send the buffer - tx_buffer will be freed by mmhal_datalink after sending */
        mmbuf_prepend_data(tx_buffer, (uint8_t *)&txheader, sizeof(txheader));
        mmagic_datalink_agent_tx_buffer(agent_llc->agent_dl, tx_buffer);
    }

    mmosal_mutex_release(agent_llc->datalink_mutex);
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
