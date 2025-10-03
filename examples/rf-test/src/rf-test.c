/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief RF test application, alpha release.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This application is not intended for general use and requires additional tools. Please
 * contact Morse support for more information.
 */

#include <endian.h>
#include <string.h>
#include "mmbuf.h"
#include "mmcrc.h"
#include "mmhal.h"
#include "mmhal_uart.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmwlan.h"
#include "mm_app_common.h"
#include "slip.h"

/** Length of the sequence number field appended to command/response packets. */
#define SEQ_NUM_LEN         (4)
/** Maximum possible length of a command that we may wish to receive. */
#define COMMAND_MAX_LEN     (252)
/** Maximum possible length of a response that we may wish to send. */
#define RESPONSE_MAX_LEN    (2048)
/** Length of a response header (excluding status field). */
#define RESPONSE_HDR_LEN    (12)

/**
 * TX callback handler for SLIP. This is invoked by SLIP to trigger transmission of a character.
 *
 * @param c     The character to transmit
 * @param arg   Opaque argument (ignored in this case).
 *
 * @returns 0 on success, otherwise an error code.
 */
static int slip_tx_handler(uint8_t c, void *arg)
{
    MM_UNUSED(arg);
    mmhal_uart_tx(&c, 1);
    return 0;
}

/**
 * Callback to handle reception of a command packet from the data-link HAL.
 *
 * @param cmd_buf       The received command.
*/
static void rf_test_handle_command(struct mmbuf *cmd_buf)
{
    uint8_t *seq_num;
    struct mmbuf *rsp_buf = NULL;
    uint32_t response_len;
    enum mmwlan_status status;
    int ret;
    uint16_t crc;

    seq_num = mmbuf_remove_from_end(cmd_buf, SEQ_NUM_LEN);
    if (seq_num == NULL)
    {
        printf("Received command packet too short\n");
        goto exit;
    }

    rsp_buf = mmbuf_alloc_on_heap(0, SEQ_NUM_LEN + RESPONSE_MAX_LEN);

    /*
     * `response_len` is initialized to the size of the response buffer before
     * `mmwlan_ate_execute_command()` is invoked and will be set to the actual length of the
     * response on success
     */
    response_len = RESPONSE_MAX_LEN;

    printf("Executing command...\n");
    status = mmwlan_ate_execute_command(
        mmbuf_get_data_start(cmd_buf), mmbuf_get_data_length(cmd_buf),
        mmbuf_append(rsp_buf, 0), &response_len);

    if (status == MMWLAN_SUCCESS)
    {
        (void)mmbuf_append(rsp_buf, response_len);
        printf("Command executed successfully. Sending response...\n");
    }
    else
    {
        uint8_t *hdr;
        uint8_t result_code[4] = {};

        printf("Failed to execute command. Status code %d\n", status);

        switch (status)
        {
        case MMWLAN_NO_MEM:
            result_code[0] = MM_ENOMEM;
            break;

        case MMWLAN_UNAVAILABLE:
            result_code[0] = MM_ENODEV;
            break;

        case MMWLAN_INVALID_ARGUMENT:
            result_code[0] = MM_EINVAL;
            break;

        case MMWLAN_TIMED_OUT:
            result_code[0] = MM_ETIMEDOUT;
            break;

        default:
            result_code[0] = MM_EFAULT;
            break;
        }

        /* Craft a response with an errno based error code in the response buffer. */
        hdr = mmbuf_append(rsp_buf, RESPONSE_HDR_LEN);
        memset(hdr, 0, RESPONSE_HDR_LEN);
        mmbuf_append_data(rsp_buf, result_code, sizeof(result_code));
    }

    mmbuf_append_data(rsp_buf, seq_num, SEQ_NUM_LEN);
    crc = htole16(mmcrc_16_xmodem(0, mmbuf_get_data_start(rsp_buf),
                                  mmbuf_get_data_length(rsp_buf)));
    mmbuf_append_data(rsp_buf, (uint8_t*)&crc, sizeof(crc));

    ret = slip_tx(slip_tx_handler, NULL,
                  mmbuf_get_data_start(rsp_buf), mmbuf_get_data_length(rsp_buf));
    if (ret != 0)
    {
        printf("Failed to send response (%d)\n", ret);
    }
    else
    {
        printf("Response sent\n");
    }
exit:
    mmbuf_release(cmd_buf);
    mmbuf_release(rsp_buf);
}

/**
 * Handler for UART HAL RX callback.
 *
 * @param data      The received data.
 * @param length    Length of the received data.
 * @param arg       Opaque argument -- the SLIP state in our case.
 */
static void uart_rx_handler(const uint8_t *data, size_t length, void *arg)
{
    size_t ii;
    enum slip_rx_status status;
    struct slip_rx_state *slip_state = (struct slip_rx_state *)arg;

    for (ii = 0; ii < length; ii++)
    {
        status = slip_rx(slip_state, data[ii]);
        if (status == SLIP_RX_COMPLETE)
        {
            uint16_t crc;

            if (slip_state->length < sizeof(uint16_t))
            {
                printf("Received command packet too short. Ignoring...\n");
                continue;
            }

            crc = mmcrc_16_xmodem(0, slip_state->buffer, slip_state->length - sizeof(uint16_t));
            if ((slip_state->buffer[slip_state->length - 2] == (crc & 0xFF)) &&
                (slip_state->buffer[slip_state->length - 1] == (crc >> 8)))
            {
                /* CRC matches, so allocate an mmbuf and pass to command handler function. */
                struct mmbuf* mmbuffer = mmbuf_alloc_on_heap(0, slip_state->length);
                if (mmbuffer == NULL)
                {
                    /* Insufficient memory to receive packet */
                    slip_state->length = 0;
                    printf("Error: memory allocation failure\n");
                    continue;
                }
                mmbuf_append_data(mmbuffer, slip_state->buffer,
                                  slip_state->length);
                mmbuf_remove_from_end(mmbuffer, sizeof(uint16_t));
                rf_test_handle_command(mmbuffer);
            }
            else
            {
                printf("CRC validation failure\n");
            }
            slip_state->length = 0;
        }
    }
}


/** Buffer for SLIP processing on receive path. */
static uint8_t slip_rx_buffer[SLIP_RX_BUFFER_SIZE];
/** State data for SLIP processing on   receive path. */
static struct slip_rx_state rx_slip_state =
    SLIP_RX_STATE_INIT(slip_rx_buffer, sizeof(slip_rx_buffer));

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    /* RF-test application does not need to worry about power saving. Take a deep sleep
     * veto and never release it so that we do not enter deep sleep. */
    mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_APP_MIN);

    printf("\n\nRF Test Application (Built "__DATE__ " " __TIME__ ")\n\n");

    app_wlan_init();

    /* Explicitly prevent the Morse Micro chip from entering it's power save state during rf-test
     * operations. */
    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);

    mmhal_uart_init(uart_rx_handler, &rx_slip_state);
}
