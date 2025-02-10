/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmosal.h"
#include "mmbuf.h"
#include "mmutils.h"
#include "mmagic_datalink_agent.h"
#include "m2m_llc/mmagic_llc_agent.h"
#include "m2m_api/mmagic_m2m_agent.h"
#include "m2m_api/autogen/mmagic_m2m_internal.h"
#include "m2m_api/autogen/mmagic_m2m.def"

/** M2M stream data. */
struct mmagic_m2m_stream
{
    /** The agent. */
    struct mmagic_m2m_agent *agent;
    /** The stream ID */
    uint8_t sid;
    /** The queue to post this streams packets to */
    struct mmosal_queue *stream_queue;
    /** The application context for this stream */
    void *stream_context;
};

/**
 * Static allocation of the agent structure. Currently it is expected that there will only ever
 * be one instance.
 */
static struct mmagic_m2m_agent m2m_agent;

/** Function to retrieve the global agent address. */
static struct mmagic_m2m_agent *mmagic_m2m_agent_get(void)
{
    return &m2m_agent;
}

static void mmagic_m2m_agent_free_stream(struct mmagic_m2m_stream *stream)
{
    /* Sanity check to ensure integrity of stream structures */
    MMOSAL_ASSERT(stream->agent->core.stream[stream->sid] == stream);

    stream->agent->core.stream[stream->sid] = NULL;
    mmosal_queue_delete(stream->stream_queue);
    mmosal_free(stream);
}

static void mmagic_m2m_agent_stream_task(void *arg)
{
    struct mmagic_m2m_stream *stream = (struct mmagic_m2m_stream *)arg;
    MMOSAL_ASSERT(stream);

    while (true)
    {
        struct mmbuf *rx_buffer = NULL;
        struct mmbuf *resp_buf = NULL;

        if (!mmosal_queue_pop(stream->stream_queue, (void *)&rx_buffer, UINT32_MAX))
        {
            break;
        }

        /* Posting a NULL to the queue will close the stream */
        if (rx_buffer == NULL)
        {
            break;
        }

        struct mmagic_m2m_command_header *header = (struct mmagic_m2m_command_header *)
            mmbuf_remove_from_start(rx_buffer, sizeof(*header));
        if (header)
        {
            resp_buf = mmagic_m2m_process(stream->agent, stream->sid, header, rx_buffer);
        }
        else
        {
            /* We could not parse the M2M header */
            resp_buf = mmagic_m2m_create_response(0, 0, 0, MMAGIC_STATUS_ERROR, NULL, 0);
        }

        /* Response buffer should not be NULL, assert here to catch problems early. */
        MMOSAL_ASSERT(resp_buf);

        mmagic_llc_agent_tx(stream->agent->agent_llc, MMAGIC_LLC_PTYPE_RESPONSE,
                            stream->sid, resp_buf);
        mmbuf_release(rx_buffer);
    }

    /* Free stream resources after stream task shuts down */
    mmagic_m2m_agent_free_stream(stream);
}

enum mmagic_status mmagic_m2m_agent_open_stream(struct mmagic_data *core,
                                                void *stream_context, uint8_t *sid)
{
    int ii;

    /* find a free stream */
    for (ii = 0; ii < MMAGIC_MAX_STREAMS; ii++)
    {
        if (core->stream[ii] == NULL)
        {
            core->stream[ii] = (struct mmagic_m2m_stream *)
                mmosal_malloc(sizeof(struct mmagic_m2m_stream));
            if (core->stream[ii] == NULL)
            {
                return MMAGIC_STATUS_UNAVAILABLE;
            }

            core->stream[ii]->stream_context = stream_context;
            core->stream[ii]->stream_queue = mmosal_queue_create(1, sizeof(struct mmbuf *), NULL);
            if (core->stream[ii]->stream_queue == NULL)
            {
                mmosal_free(core->stream[ii]);
                return MMAGIC_STATUS_UNAVAILABLE;
            }
            struct mmosal_task *stream_task =
                mmosal_task_create(mmagic_m2m_agent_stream_task, core->stream[ii],
                                   MMOSAL_TASK_PRI_NORM, 1024, "mmagic_stream_task");
            if (stream_task == NULL)
            {
                mmosal_queue_delete(core->stream[ii]->stream_queue);
                mmosal_free(core->stream[ii]);
                return MMAGIC_STATUS_OK;
            }

            core->stream[ii]->agent = mmagic_m2m_agent_get();
            *sid = ii;
            core->stream[ii]->sid = *sid;

            return MMAGIC_STATUS_OK;
        }
    }

    /* No free stream found */
    return MMAGIC_STATUS_UNAVAILABLE;
}

void *mmagic_m2m_agent_get_stream_context(struct mmagic_data *core, uint8_t stream_id)
{
    MMOSAL_ASSERT(stream_id < MMAGIC_MAX_STREAMS);

    if (core->stream[stream_id])
    {
        return core->stream[stream_id]->stream_context;
    }
    return NULL;
}

void mmagic_m2m_agent_close_stream(struct mmagic_data *core, uint8_t stream_id)
{
    void *nullpointer = NULL;

    /* We cannot close the command stream */
    MMOSAL_ASSERT(stream_id != 0);

    MMOSAL_ASSERT(stream_id < MMAGIC_MAX_STREAMS);

    if (core->stream[stream_id])
    {
        /* Push a null pointer to the queue to signal the task to close */
        mmosal_queue_push(core->stream[stream_id]->stream_queue, &nullpointer, UINT32_MAX);
    }
}

struct mmbuf *mmagic_m2m_create_response(uint8_t subsystem, uint8_t command, uint8_t subcommand,
                                         enum mmagic_status result,
                                         const void *data, size_t size)
{
    struct mmagic_m2m_response_header *txheader;

    struct mmbuf *txbuffer = mmagic_llc_agent_alloc_buffer_for_tx(NULL, sizeof(txheader) + size);

    if (txbuffer)
    {
        txheader = (struct mmagic_m2m_response_header *)mmbuf_append(txbuffer, sizeof(*txheader));

        txheader->subsystem = subsystem;
        txheader->command = command;
        txheader->subcommand = subcommand;
        txheader->result = result;

        if (data && size)
        {
            mmbuf_append_data(txbuffer, (const uint8_t *)data, size);
        }
    }
    return txbuffer;
}

enum mmagic_status mmagic_m2m_rx_callback(struct mmagic_llc_agent *agent_llc, void *app_ctx,
                                          uint8_t sid, struct mmbuf *rxbuffer)
{
    struct mmagic_m2m_agent *agent = (struct mmagic_m2m_agent *)app_ctx;
    MM_UNUSED(agent_llc);

    MMOSAL_ASSERT(agent);
    MMOSAL_ASSERT(sid < MMAGIC_MAX_STREAMS);

    if (agent->core.stream[sid] == NULL)
    {
        printf("MMAGIC_M2M_AGENT: Invalid stream ID %u!\n", sid);
        mmbuf_release(rxbuffer);
        return MMAGIC_STATUS_INVALID_STREAM;
    }
    else
    {
        mmosal_queue_push(agent->core.stream[sid]->stream_queue, (void *)&rxbuffer, UINT32_MAX);
        return MMAGIC_STATUS_OK;
    }
}

enum mmagic_status mmagic_m2m_event_handler(void *arg, uint8_t subsystem_id,
                                            uint8_t notification_id, const uint8_t *payload,
                                            size_t payload_len)
{
    struct mmagic_m2m_agent *agent = (struct mmagic_m2m_agent *)arg;

    struct mmbuf *mmbuf = mmagic_m2m_create_response(subsystem_id, notification_id, 0,
                                                     MMAGIC_STATUS_OK, payload, payload_len);
    if (mmbuf == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }

    mmagic_llc_agent_tx(agent->agent_llc, MMAGIC_LLC_PTYPE_EVENT, 0, mmbuf);
    return MMAGIC_STATUS_OK;
}

/**
 * Set the deep sleep mode. See @ref mmagic_deep_sleep_mode for possible deep sleep modes.
 *
 * @param mode The deep sleep mode to set.
 * @param arg  This is an argurment that is register with the callback. It is expected to be the LLC
 *             handle.
 *
 * @returns @ true if the mode was set successfully; @c false on failure (e.g. unsupported mode).
 */
static bool mmagic_m2m_agent_deep_sleep_mode_handler(enum mmagic_deep_sleep_mode mode, void *arg)
{
    struct mmagic_llc_agent *agent_llc = (struct mmagic_llc_agent *)arg;

    return mmagic_llc_agent_set_deep_sleep_mode(agent_llc, mode);
}

struct mmagic_m2m_agent *mmagic_m2m_agent_init(const struct mmagic_m2m_agent_init_args *args)
{
    struct mmagic_m2m_agent *agent = mmagic_m2m_agent_get();
    struct mmagic_llc_agent_int_args init_args = { 0 };
    uint8_t command_sid;

    memset(agent, 0, sizeof(*agent));

    mmosal_safer_strcpy(agent->core.app_version, args->app_version,
                        sizeof(agent->core.app_version));
    agent->core.reg_db = args->reg_db;
    agent->core.event_fn = mmagic_m2m_event_handler;
    agent->core.event_fn_arg = agent;

    mmagic_core_init_modules(&agent->core);

    init_args.rx_arg = (void *)agent;
    init_args.rx_callback = mmagic_m2m_rx_callback;
    agent->agent_llc = mmagic_llc_agent_init(&init_args);
    MMOSAL_ASSERT(agent->agent_llc);
    agent->core.set_deep_sleep_mode_cb = mmagic_m2m_agent_deep_sleep_mode_handler;
    agent->core.set_deep_sleep_mode_cb_arg = (void *)agent->agent_llc;

    MMOSAL_ASSERT(mmagic_m2m_agent_open_stream(&agent->core, (void *)agent, &command_sid) ==
                  MMAGIC_STATUS_OK);
    MMOSAL_ASSERT(command_sid == CONTROL_STREAM);

    MMOSAL_ASSERT(mmagic_llc_send_start_notification(agent->agent_llc));

    return agent;
}
