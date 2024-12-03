/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "slip.h"

enum slip_special_chars
{
    SLIP_FRAME_END      = 0xc0,
    SLIP_FRAME_ESC      = 0xdb,
    SLIP_FRAME_ESC_END  = 0xdc,
    SLIP_FRAME_ESC_ESC  = 0xdd,
};

static enum slip_rx_status slip_rx_append(struct slip_rx_state *state, uint8_t c)
{
    if (state->length == state->buffer_length)
    {
        return SLIP_RX_BUFFER_LIMIT;
    }

    state->buffer[state->length] = c;
    state->length++;

    return SLIP_RX_IN_PROGRESS;
}

enum slip_rx_status slip_rx(struct slip_rx_state *state, uint8_t c)
{
    if (c == SLIP_FRAME_END)
    {
        if (state->escape)
        {
            state->escape = false;
            state->length = 0;
            return SLIP_RX_ERROR;
        }
        else if (state->length > 0)
        {
            return SLIP_RX_COMPLETE;
        }
        else
        {
            return SLIP_RX_IN_PROGRESS;
        }
    }
    else if (state->escape)
    {
        state->escape = false;
        if (c == SLIP_FRAME_ESC_END)
        {
            return slip_rx_append(state, SLIP_FRAME_END);
        }
        else if (c == SLIP_FRAME_ESC_ESC)
        {
            return slip_rx_append(state, SLIP_FRAME_ESC);
        }
        else
        {
            state->length = 0;
            return SLIP_RX_ERROR;
        }
    }
    else if (c == SLIP_FRAME_ESC)
    {
        state->escape = true;
        return SLIP_RX_IN_PROGRESS;
    }
    else
    {
        return slip_rx_append(state, c);
    }
}

int slip_tx(slip_transport_tx_fn transport_tx_fn, void *transport_tx_arg,
            const uint8_t *packet, size_t packet_len)
{
    int ret = 0;

    transport_tx_fn(SLIP_FRAME_END, transport_tx_arg);

    while (packet_len-- > 0)
    {
        uint8_t c = *packet++;
        switch (c)
        {
        case SLIP_FRAME_ESC:
            ret = transport_tx_fn(SLIP_FRAME_ESC, transport_tx_arg);
            if (ret != 0)
            {
                break;
            }
            ret = transport_tx_fn(SLIP_FRAME_ESC_ESC, transport_tx_arg);
            break;

        case SLIP_FRAME_END:
            ret = transport_tx_fn(SLIP_FRAME_ESC, transport_tx_arg);
            if (ret != 0)
            {
                break;
            }
            ret = transport_tx_fn(SLIP_FRAME_ESC_END, transport_tx_arg);
            break;

        default:
            ret = transport_tx_fn(c, transport_tx_arg);
            break;
        }
    }

    transport_tx_fn(SLIP_FRAME_END, transport_tx_arg);

    return ret;
}
