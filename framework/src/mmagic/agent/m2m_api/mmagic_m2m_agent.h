/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "mmagic.h"
#include "core/autogen/mmagic_core_types.h"
#include "core/autogen/mmagic_core_data.h"

/**
 * @defgroup MMAGIC_M2M_AGENT_COMMON Agent/Controller shared definitions
 *
 * This must be kept in sync with Controller.
 *
 * @{
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

/** @} */

/** Agent M2M struct used internally by the implementaton. */
struct mmagic_m2m_agent
{
    /** Core data for MMAGIC */
    struct mmagic_data core;
    /** Reference to agent LLC interface. */
    struct mmagic_llc_agent *agent_llc;
};

/**
 * Allocates the next available stream.
 *
 * @param  core        The MMAGIC context.
 * @param  stream_type The type of the stream, this is used for validating stream access later.
 * @param  sid         Returns the stream ID in this parameter.
 *
 * @return             @c MMAGIC_STATUS_OK on succes, error code on failure.
 */
enum mmagic_status mmagic_m2m_agent_open_stream(struct mmagic_data *core, void *stream_context,
                                                uint8_t *sid);

/**
 * Returns the requested stream context.
 *
 * @param  core        The MMAGIC context.
 * @param  stream_id   The requested stream ID.
 * @param  stream_type The type of the stream. This is used for checking that we are accessing the
 *                     correct stream.
 *
 * @return             The requested stream context, or NULL on error.
 */
void *mmagic_m2m_agent_get_stream_context(struct mmagic_data *core, uint8_t stream_id);

/**
 * Release the allocated stream.
 *
 * @param core        The MMAGIC context.
 * @param stream_id   The stream ID to release.
 * @param stream_type The type of the stream, this is checked before freeing the stream.
 */
void mmagic_m2m_agent_close_stream(struct mmagic_data *core, uint8_t stream_id);

struct mmbuf *mmagic_m2m_create_response(uint8_t subsystem, uint8_t command, uint8_t subcommand,
                                         enum mmagic_status result,
                                         const void *data, size_t size);
