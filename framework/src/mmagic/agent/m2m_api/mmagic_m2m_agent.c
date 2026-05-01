/*
 * Copyright 2023-2026 Morse Micro
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

/** M2M stream data. */
struct mmagic_m2m_stream
{
    /** The agent. */
    struct mmagic_m2m_agent *agent;
    /** Subsystem ID */
    uint8_t subsystem_id;
    /** The stream ID */
    uint8_t sid;
    /** The queue to post this streams packets to */
    struct mmosal_queue *stream_queue;
    /** The application context for this stream */
    void *stream_context;
};

/** Stream request type. */
enum mmagic_m2m_stream_request_type
{
    /** Standard M2M request payload received from Controller. */
    MMAGIC_M2M_STREAM_REQUEST_WIRE,
    /** Local M2M command request (see @ref mmagic_m2m_agent_local_command). */
    MMAGIC_M2M_STREAM_REQUEST_LOCAL,
    /** Close stream and terminate stream task. */
    MMAGIC_M2M_STREAM_REQUEST_CLOSE,
};

/** M2M stream request descriptor. */
struct mmagic_m2m_stream_request
{
    /** Request kind that selects the active member in @ref mmagic_m2m_stream_request::data. */
    enum mmagic_m2m_stream_request_type type;

    union
    {
        struct
        {
            /** Received wire buffer to process for @ref MMAGIC_M2M_STREAM_REQUEST_WIRE. */
            struct mmbuf *rx_buffer;
        } wire; /**< Payload for @ref MMAGIC_M2M_STREAM_REQUEST_WIRE. */

        struct
        {
            /** Callback to execute for @ref MMAGIC_M2M_STREAM_REQUEST_LOCAL. */
            void (*cb)(struct mmagic_data *core, void *arg);
            /** User argument forwarded to @p cb. */
            void *cb_arg;
        } local; /**< Payload for @ref MMAGIC_M2M_STREAM_REQUEST_LOCAL. */
    } data;
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
    MMOSAL_DEV_ASSERT(stream->sid != CONTROL_STREAM);

    stream->agent->core.stream[stream->sid] = NULL;
    mmosal_queue_delete(stream->stream_queue);
    mmosal_free(stream);
}

static void mmagic_m2m_agent_stream_task(void *arg)
{
    struct mmagic_m2m_stream_request request;
    struct mmagic_m2m_stream *stream = (struct mmagic_m2m_stream *)arg;

    MMOSAL_ASSERT(stream);

    while (mmosal_queue_pop(stream->stream_queue, (void *)&request, UINT32_MAX))
    {
        switch (request.type)
        {
            case MMAGIC_M2M_STREAM_REQUEST_WIRE:
            {
                struct mmbuf *resp_buf = NULL;
                struct mmbuf *rx_buffer = request.data.wire.rx_buffer;
                MMOSAL_ASSERT(rx_buffer != NULL);

                struct mmagic_m2m_command_header *header =
                    (struct mmagic_m2m_command_header *)mmbuf_remove_from_start(rx_buffer,
                                                                                sizeof(*header));
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

                enum mmagic_status status = mmagic_llc_agent_tx(stream->agent->agent_llc,
                                                                MMAGIC_LLC_PTYPE_RESPONSE,
                                                                stream->sid,
                                                                resp_buf);
                MMOSAL_DEV_ASSERT(status == MMAGIC_STATUS_OK);
                mmbuf_release(rx_buffer);
                break;
            }

            case MMAGIC_M2M_STREAM_REQUEST_LOCAL:
                MMOSAL_ASSERT(request.data.local.cb != NULL);
                request.data.local.cb(&stream->agent->core, request.data.local.cb_arg);
                break;

            case MMAGIC_M2M_STREAM_REQUEST_CLOSE:
                MMOSAL_DEV_ASSERT(stream->sid != CONTROL_STREAM);
                if (stream->sid == CONTROL_STREAM)
                {
                    mmosal_printf("Ignoring request to close CONTROL stream\n", stream->sid);
                    /* We require the control stream to remain open, ignore closure. */
                }
                else
                {
                    goto cleanup;
                }
                break;

            default:
                MMOSAL_ASSERT(false);
                break;
        }
    }

cleanup:
    /* Free stream resources after stream task shuts down */
    mmagic_m2m_agent_free_stream(stream);
}

enum mmagic_status mmagic_m2m_agent_open_stream(struct mmagic_data *core,
                                                void *stream_context,
                                                uint8_t subsystem_id,
                                                uint8_t *sid)
{
    MMOSAL_DEV_ASSERT(core);
    MMOSAL_DEV_ASSERT(sid);

    /* find a free stream */
    for (int ii = 0; ii < MMAGIC_MAX_STREAMS; ii++)
    {
        if (core->stream[ii] == NULL)
        {
            core->stream[ii] =
                (struct mmagic_m2m_stream *)mmosal_malloc(sizeof(struct mmagic_m2m_stream));
            if (core->stream[ii] == NULL)
            {
                return MMAGIC_STATUS_NO_MEM;
            }

            core->stream[ii]->stream_context = stream_context;
            core->stream[ii]->stream_queue =
                mmosal_queue_create(1, sizeof(struct mmagic_m2m_stream_request), NULL);
            if (core->stream[ii]->stream_queue == NULL)
            {
                mmosal_free(core->stream[ii]);
                return MMAGIC_STATUS_NO_MEM;
            }
            struct mmosal_task *stream_task = mmosal_task_create(mmagic_m2m_agent_stream_task,
                                                                 core->stream[ii],
                                                                 MMOSAL_TASK_PRI_NORM,
                                                                 1088,
                                                                 "mmagic_stream_task");
            if (stream_task == NULL)
            {
                mmosal_queue_delete(core->stream[ii]->stream_queue);
                mmosal_free(core->stream[ii]);
                return MMAGIC_STATUS_NO_MEM;
            }

            core->stream[ii]->agent = mmagic_m2m_agent_get();
            *sid = ii;
            core->stream[ii]->sid = *sid;
            core->stream[ii]->subsystem_id = subsystem_id;

            return MMAGIC_STATUS_OK;
        }
    }

    /* No free stream found */
    return MMAGIC_STATUS_UNAVAILABLE;
}

void *mmagic_m2m_agent_get_stream_context(struct mmagic_data *core, uint8_t stream_id)
{
    if (stream_id >= MMAGIC_MAX_STREAMS)
    {
        return NULL;
    }

    if (core->stream[stream_id])
    {
        return core->stream[stream_id]->stream_context;
    }
    return NULL;
}

uint8_t mmagic_m2m_agent_get_stream_subsystem_id(struct mmagic_data *core, uint8_t stream_id)
{
    if ((stream_id < MMAGIC_MAX_STREAMS) && (core->stream[stream_id] != NULL))
    {
        return core->stream[stream_id]->subsystem_id;
    }
    return 0;
}

enum mmagic_status mmagic_m2m_agent_close_stream(struct mmagic_data *core, uint8_t stream_id)
{
    static const struct mmagic_m2m_stream_request close_request = {
        .type = MMAGIC_M2M_STREAM_REQUEST_CLOSE
    };

    /* We cannot close the command stream */
    if (stream_id == CONTROL_STREAM || stream_id >= MMAGIC_MAX_STREAMS)
    {
        return MMAGIC_STATUS_INVALID_ARG;
    }

    if (core->stream[stream_id])
    {
        mmosal_queue_push(core->stream[stream_id]->stream_queue, &close_request, UINT32_MAX);
    }

    return MMAGIC_STATUS_OK;
}

struct mmbuf *mmagic_m2m_create_response(uint8_t subsystem,
                                         uint8_t command,
                                         uint8_t subcommand,
                                         enum mmagic_status result,
                                         const void *data,
                                         size_t size)
{
    struct mmagic_m2m_response_header *txheader;

    struct mmbuf *txbuffer = mmagic_llc_agent_alloc_buffer_for_tx(NULL, sizeof(*txheader) + size);

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

enum mmagic_status mmagic_m2m_rx_callback(struct mmagic_llc_agent *agent_llc,
                                          void *app_ctx,
                                          uint8_t sid,
                                          struct mmbuf *rxbuffer)
{
    struct mmagic_m2m_agent *agent = (struct mmagic_m2m_agent *)app_ctx;
    MM_UNUSED(agent_llc);

    MMOSAL_ASSERT(agent);
    MMOSAL_ASSERT(sid < MMAGIC_MAX_STREAMS);

    if (agent->core.stream[sid] == NULL)
    {
        mmosal_printf("MMAGIC_M2M_AGENT: Invalid stream ID %u!\n", sid);
        mmbuf_release(rxbuffer);
        return MMAGIC_STATUS_INVALID_STREAM;
    }
    struct mmagic_m2m_stream_request request = {
        .type = MMAGIC_M2M_STREAM_REQUEST_WIRE,
        .data = { .wire = { .rx_buffer = rxbuffer } },
    };
    if (!mmosal_queue_push(agent->core.stream[sid]->stream_queue, (void *)&request, UINT32_MAX))
    {
        mmbuf_release(rxbuffer);
        return MMAGIC_STATUS_ERROR;
    }
    return MMAGIC_STATUS_OK;
}

enum mmagic_status mmagic_m2m_event_handler(void *arg,
                                            uint8_t subsystem_id,
                                            uint8_t notification_id,
                                            const uint8_t *payload,
                                            size_t payload_len)
{
    struct mmagic_m2m_agent *agent = (struct mmagic_m2m_agent *)arg;
    if (!agent->initialized)
    {
        /* Drop events until the M2M agent init sequence has completed. */
        return MMAGIC_STATUS_OK;
    }

    struct mmbuf *mmbuf = mmagic_m2m_create_response(subsystem_id,
                                                     notification_id,
                                                     0,
                                                     MMAGIC_STATUS_OK,
                                                     payload,
                                                     payload_len);
    if (mmbuf == NULL)
    {
        return MMAGIC_STATUS_NO_MEM;
    }

    return mmagic_llc_agent_tx(agent->agent_llc, MMAGIC_LLC_PTYPE_EVENT, 0, mmbuf);
}

/**
 * Set the deep sleep mode. See @ref mmagic_deep_sleep_mode for possible deep sleep modes.
 *
 * @param mode The deep sleep mode to set.
 * @param arg  This is an argument that is register with the callback. It is expected to be the LLC
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

    mmosal_safer_strcpy(agent->core.app_version,
                        args->app_version,
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

    MMOSAL_ASSERT(mmagic_m2m_agent_open_stream(&agent->core,
                                               (void *)agent,
                                               mmagic_subsystem_reserved,
                                               &command_sid) == MMAGIC_STATUS_OK);
    MMOSAL_ASSERT(command_sid == CONTROL_STREAM);

    MMOSAL_ASSERT(mmagic_llc_send_start_notification(agent->agent_llc) == MMAGIC_STATUS_OK);
    agent->initialized = true;

    return agent;
}

enum mmagic_status mmagic_m2m_agent_local_command(struct mmagic_m2m_agent *agent,
                                                  void (*cb)(struct mmagic_data *core, void *arg),
                                                  void *cb_arg,
                                                  uint32_t timeout)
{
    struct mmagic_m2m_stream_request request = {
        .type = MMAGIC_M2M_STREAM_REQUEST_LOCAL,
        .data = { .local = { .cb = cb, .cb_arg = cb_arg } },
    };

    if (!mmosal_queue_push(agent->core.stream[CONTROL_STREAM]->stream_queue,
                           (void *)&request,
                           timeout))
    {
        return MMAGIC_STATUS_ERROR;
    }
    return MMAGIC_STATUS_OK;
}
