/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(ENABLE_MMAGIC_DATALINK_UART) && ENABLE_MMAGIC_DATALINK_UART

#include <endian.h>
#include <stdint.h>

#include "mmagic_datalink_agent.h"

#include "mmcrc.h"
#include "mmhal_uart.h"
#include "mmosal.h"
#include "mmutils.h"
#include "slip.h"

struct mmagic_datalink_agent
{
    /** Determines whether CRC should be appended to outgoing packets. */
    bool tx_crc_enabled;

    /** Slip receive state. */
    struct slip_rx_state slip_rx_state;
    /** The buffer that backs slip receive. */
    struct mmbuf *rxbuf;

    /** Callback to invoke on receive of a valid packet. */
    mmagic_datalink_agent_rx_buffer_cb_t rx_buffer_callback;
    /** Opaque argument for @c rx_buffer_callback. */
    void *rx_buffer_callback_arg;
    /** The maximum packet size we expect to receive. */
    size_t max_rx_packet_size;
};

#define UART_DATALINK_HDR_LENGTH    (1)
#define UART_DATALINK_CRC_LENGTH    (2)

#define UART_DATALINK_HDR_PKT_TYPE_MASK (0x0f)
#define UART_DATALINK_HDR_PKT_TYPE_DATA (0x00)

#define UART_DATALINK_FLAG_CRC_PRESENT  (0x80)

static void datalink_init_slip_rx_state(struct mmagic_datalink_agent *interface)
{
    MMOSAL_ASSERT(interface->rxbuf == NULL);
    if (interface->max_rx_packet_size > 0)
    {
        interface->rxbuf = mmbuf_alloc_on_heap(0, interface->max_rx_packet_size);
        if (interface->rxbuf != NULL)
        {
            slip_rx_state_reinit(&interface->slip_rx_state,
                                 mmbuf_get_data_start(interface->rxbuf),
                                 interface->max_rx_packet_size);
        }
    }
}

static void datalink_process_rxbuf(struct mmagic_datalink_agent *interface, struct mmbuf *rxbuf)
{
    uint8_t *hdr;

    hdr = mmbuf_remove_from_start(rxbuf, UART_DATALINK_HDR_LENGTH);
    if (hdr == NULL)
    {
        mmosal_printf("Received packet too short. Dropping...\n");
        goto exit;
    }

    if (*hdr & UART_DATALINK_FLAG_CRC_PRESENT)
    {
        uint16_t calc_crc;
        uint16_t rx_crc;
        uint8_t *rx_crc_buf;

        rx_crc_buf = mmbuf_remove_from_start(rxbuf, UART_DATALINK_CRC_LENGTH);
        if (rx_crc_buf == NULL)
        {
            mmosal_printf("Received packet too short. Dropping...\n");
            goto exit;
        }

        rx_crc = rx_crc_buf[0] | rx_crc_buf[1] << 8;
        calc_crc = mmcrc_16_xmodem(0, mmbuf_get_data_start(rxbuf), mmbuf_get_data_length(rxbuf));
        if (rx_crc != calc_crc)
        {
            mmosal_printf("CRC validation failure. Dropping...\n");
            goto exit;
        }
    }

    if ((*hdr & UART_DATALINK_HDR_PKT_TYPE_MASK) != UART_DATALINK_HDR_PKT_TYPE_DATA)
    {
        mmosal_printf("Unknown packet type %x\n", (*hdr & UART_DATALINK_HDR_PKT_TYPE_MASK));
        goto exit;
    }

    if (interface->rx_buffer_callback != NULL)
    {
        interface->rx_buffer_callback(interface, interface->rx_buffer_callback_arg, rxbuf);
        rxbuf = NULL;
    }

exit:
    mmbuf_release(rxbuf);
}

/**
 * Handler for UART HAL RX callback.
 *
 * @param data   The received data.
 * @param length Length of the received data.
 * @param arg    Opaque argument -- the SLIP state in our case.
 */
static void datalink_uart_rx_handler(const uint8_t *data, size_t length, void *arg)
{
    size_t ii;
    enum slip_rx_status status;
    struct mmagic_datalink_agent *interface = (struct mmagic_datalink_agent *)arg;

    /* If rxbuf is NULL then memory allocation may have previously failed. Try it again before
     * processing the received character. */
    if (interface->rxbuf == NULL)
    {
        datalink_init_slip_rx_state(interface);
    }

    for (ii = 0; ii < length; ii++)
    {
        status = slip_rx(&interface->slip_rx_state, data[ii]);
        if (status == SLIP_RX_COMPLETE)
        {
            struct mmbuf *rxbuf = interface->rxbuf;
            interface->rxbuf = NULL;
            if (rxbuf != NULL)
            {
                /* Update the data length of the mmbuf based on the length of data counted by
                 * slip_rx_state. */
                mmbuf_append(rxbuf, interface->slip_rx_state.length);
                datalink_process_rxbuf(interface, rxbuf);
            }
            datalink_init_slip_rx_state(interface);
        }
    }
}

struct mmagic_datalink_agent *mmagic_datalink_agent_init(
    const struct mmagic_datalink_agent_init_args *args)
{
    static struct mmagic_datalink_agent ginterface;
    struct mmagic_datalink_agent *interface = &ginterface;

    memset(interface, 0, sizeof(*interface));

    interface->rx_buffer_callback = args->rx_callback;
    interface->rx_buffer_callback_arg = args->rx_arg;
    interface->max_rx_packet_size = args->max_packet_size;
    if ((interface->rx_buffer_callback == NULL) || (!interface->max_rx_packet_size))
    {
        /* These are required fields, do not proceed if not present. */
        return NULL;
    }

    /* Disable deep sleep on startup to ensure we stay awake to receive data */
    (void)mmagic_datalink_agent_set_deep_sleep_mode(interface,
                                                    MMAGIC_DATALINK_AGENT_DEEP_SLEEP_DISABLED);

    datalink_init_slip_rx_state(interface);
    mmhal_uart_init(datalink_uart_rx_handler, interface);

    return interface;
}

void mmagic_datalink_agent_deinit(
    struct mmagic_datalink_agent *interface)
{
    mmbuf_release(interface->rxbuf);
    interface->rxbuf = NULL;
    interface->slip_rx_state.buffer = NULL;
    interface->slip_rx_state.buffer_length = 0;
}

struct mmbuf *mmagic_datalink_agent_alloc_buffer_for_tx(
    size_t header_size, size_t payload_size)
{
    return mmbuf_alloc_on_heap(header_size + UART_DATALINK_HDR_LENGTH,
                               payload_size + UART_DATALINK_CRC_LENGTH);
}

/**
 * Handler for the CLI transmit callback.
 *
 * @param data   Data to transmit.
 * @param length Length of data to transmit.
 * @param arg    Opaque argument (unused).
 */
int datalink_slip_tx_handler(uint8_t c, void *arg)
{
    MM_UNUSED(arg);
    mmhal_uart_tx(&c, 1);
    return 0;
}

int mmagic_datalink_agent_tx_buffer(struct mmagic_datalink_agent *interface, struct mmbuf *buf)
{
    int ret;
    uint8_t hdr = 0;

    if (interface->tx_crc_enabled)
    {
        uint16_t crc = htole16(mmcrc_16_xmodem(0, mmbuf_get_data_start(buf),
                                               mmbuf_get_data_length(buf)));
        mmbuf_append_data(buf, (uint8_t *)&crc, sizeof(crc));
        hdr |= UART_DATALINK_FLAG_CRC_PRESENT;
    }

    mmbuf_prepend_data(buf, &hdr, sizeof(hdr));

    ret = slip_tx(datalink_slip_tx_handler, interface,
                  mmbuf_get_data_start(buf), mmbuf_get_data_length(buf));

    mmbuf_release(buf);
    return ret;
}

bool mmagic_datalink_agent_set_deep_sleep_mode(
    struct mmagic_datalink_agent *interface,
    enum mmagic_datalink_agent_deep_sleep_mode mode)
{
    enum mmhal_uart_deep_sleep_mode mode_uart;

    MM_UNUSED(interface);

    switch (mode)
    {
    case MMAGIC_DATALINK_AGENT_DEEP_SLEEP_DISABLED:
        mode_uart = MMHAL_UART_DEEP_SLEEP_DISABLED;
        break;

    case MMAGIC_DATALINK_AGENT_DEEP_SLEEP_ONE_SHOT:
        mode_uart = MMHAL_UART_DEEP_SLEEP_ONE_SHOT;
        break;

    default:
        return false;
    }

    return mmhal_uart_set_deep_sleep_mode(mode_uart);
}

#endif
