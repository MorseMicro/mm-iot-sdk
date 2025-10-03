/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>

#include "main.h"

#include "mmutils.h"
#include "mmhal.h"
#include "mmosal.h"
#include "mmagic_datalink_agent.h"

/** Enumeration of the different states of the SPI mmagic data link agent. */
enum mmagic_datalink_agent_state
{
    MMAGIC_DATALINK_AGENT_STATE_ERROR,
    MMAGIC_DATALINK_AGENT_STATE_IDLE,
    MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD,
    MMAGIC_DATALINK_AGENT_STATE_C2A_ACK,
    MMAGIC_DATALINK_AGENT_STATE_A2C_READ_LEN,
    MMAGIC_DATALINK_AGENT_STATE_A2C_READ_PAYLOAD,
    MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_LEN,
    MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_PAYLOAD,
};

/** There are some cases where we need to pad the allocated buffers for transfers. In the case of
 * the U575 SPI DMA transfers the buffer given to the peripheral needs to be the expected transfer
 * size +1. */
#define MMAGIC_DATALINK_AGENT_ALLOC_PADDING_BYTES 1

/** Maximum length in bytes we will transfer with before using DMA. */
#define IT_TRANSFER_MAX_LENGTH 16

/** ST HAL handle defined in main.c as part of the CubeMX generated code */
extern SPI_HandleTypeDef hspi2;

struct mmagic_datalink_agent
{
    /** Flag used to indicate if the data-link has been initialized successfully. */
    bool initialized;
    /** Handle for the spi interface. */
    SPI_HandleTypeDef *spi_handle;
    /** Callback to execute when an RX packet has been received. */
    mmagic_datalink_agent_rx_buffer_cb_t rx_buffer_callback;
    /** Argument to pass to the callback function. */
    void *rx_buffer_cb_arg;
    /** Size of the maximum expected buffer from the controller. */
    size_t max_rx_buffer_size;
    /** Flag to indicate that the background rx task should terminate. */
    bool shutdown;
    /** Flag to indicate when the rx_task has finished running. */
    bool rx_task_has_finished;
    /** Task handle for the background rx task. */
    struct mmosal_task *rx_task_handle;
    /** Buffer the read into when receiving data from the controller. */
    uint8_t *rx_packet_buf;
    /** Length of the data currently stored in the @c rx_packet_buf */
    uint16_t rx_packet_len;
    /** Current state of the mmagic data link agent. */
    enum mmagic_datalink_agent_state state;
    /** Array to store payload headers received from the controller. */
    uint8_t rx_payload_header[MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE];
    /** Reference to the active buffer to transmit. */
    struct mmbuf *tx_buf;
    /** Reference to the previously transmitted buffer to allow the Controller to reread erroneous
     *  transfers. */
    struct mmbuf *prev_tx_buf;
    /** Reference to a transmit buffer that is waiting to be released by a background task. */
    struct mmbuf *pending_release_tx_buf;
    /** Length of the active buffer data in big endian order. */
    uint8_t tx_payload_len[MMAGIC_DATALINK_PAYLOAD_LEN_SIZE];
    /** Length of the previously active tx buffer in big endian order */
    uint8_t prev_tx_payload_len[MMAGIC_DATALINK_PAYLOAD_LEN_SIZE];
    /** Binary semaphore to signal when the active buffer has been transmitted. */
    struct mmosal_semb *tx_buf_complete_semb;
    /** Current deep sleep mode of the data link. */
    enum mmagic_datalink_agent_deep_sleep_mode deep_sleep_mode;
    /** Binary semaphore to signal the receive task when there is data available. */
    struct mmosal_semb *rx_task_semb;
};

/** Ack payload for transmission on successful reception of @c MMAGIC_DATALINK_WRITE payload. */
static const uint8_t mmagic_datalink_ack_payload[MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE] =
{MMAGIC_DATALINK_ACK};

/**
 * Static allocation of the agent data-link structure. Currently it is expected that there will only
 * ever be one instance.
 */
static struct mmagic_datalink_agent agent_datalink = {};

/** Function to retrieve the global data-link address, used for interrupt handlers. */
static inline struct mmagic_datalink_agent *mmagic_datalink_agent_get(void)
{
    return &agent_datalink;
}

/**
 * Function to update the state of the agent. It will validate that it is a valid state transition
 * before updating. If invalid the function will assert.
 *
 * @param agent_dl  Reference to the data-link handle.
 * @param new_state The state for the agent to take.
 */
static void mmagic_datalink_agent_update_state(struct mmagic_datalink_agent *agent_dl,
                                               enum mmagic_datalink_agent_state new_state)
{
    switch (agent_dl->state)
    {
    case MMAGIC_DATALINK_AGENT_STATE_ERROR:
        if (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE)
        {
            MMOSAL_ASSERT(false);
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_IDLE:
        if ((new_state != MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_A2C_READ_LEN) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_LEN) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE))
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD:
        if ((new_state != MMAGIC_DATALINK_AGENT_STATE_C2A_ACK) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE))
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_C2A_ACK:
        if (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE)
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_READ_LEN:
        if ((new_state != MMAGIC_DATALINK_AGENT_STATE_A2C_READ_PAYLOAD) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE))
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_LEN:
        if ((new_state != MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_PAYLOAD) &&
            (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE))
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_READ_PAYLOAD:
    case MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_PAYLOAD:
        if (new_state != MMAGIC_DATALINK_AGENT_STATE_IDLE)
        {
            new_state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        }
        break;
    }

    MMOSAL_TASK_ENTER_CRITICAL();
    agent_dl->state = new_state;
    MMOSAL_TASK_EXIT_CRITICAL();
}

/** Function to assert the ready pin high. */
static void mmagic_datalink_agent_set_rdy_high(void)
{
    LL_GPIO_SetOutputPin(MMAGIC_DATALINK_IRQ_GPIO_Port, MMAGIC_DATALINK_IRQ_Pin);
}

/** Function to assert the ready pin low. */
static void mmagic_datalink_agent_set_rdy_low(void)
{
    LL_GPIO_ResetOutputPin(MMAGIC_DATALINK_IRQ_GPIO_Port, MMAGIC_DATALINK_IRQ_Pin);
}

/**
 * Non-blocking spi receive.
 *
 * @param agent_dl Reference to the data-link handle.
 * @param data     Reference to buffer where the data will be placed.
 * @param len      Length of the data to receive.
 */
static void mmagic_datalink_agent_spi_receive(struct mmagic_datalink_agent *agent_dl, uint8_t *data,
                                              uint16_t len)
{
    if (len > IT_TRANSFER_MAX_LENGTH)
    {
        HAL_SPI_Receive_DMA(agent_dl->spi_handle, data, len);
    }
    else
    {
        HAL_SPI_Receive_IT(agent_dl->spi_handle, data, len);
    }
}

/**
 * Non-blocking spi transmit.
 *
 * @param agent_dl Reference to the data-link handle.
 * @param data     Reference to buffer containing the data to be transmitted.
 * @param len      Length of the data to receive.
 */
static void mmagic_datalink_agent_spi_transmit(struct mmagic_datalink_agent *agent_dl,
                                               const uint8_t *data, uint16_t len)
{
    if (len > IT_TRANSFER_MAX_LENGTH)
    {
        HAL_SPI_Transmit_DMA(agent_dl->spi_handle, data, len);
    }
    else
    {
        HAL_SPI_Transmit_IT(agent_dl->spi_handle, data, len);
    }
}

/**
 * Function to configure the agent into its rx idle state. It will allocate a buffer large enough
 * receive the max packet size.
 *
 * @param agent_dl Reference to the data-link handle.
 */
static void mmagic_datalink_agent_configure_rx_idle(struct mmagic_datalink_agent *agent_dl)
{
    mmagic_datalink_agent_spi_receive(agent_dl, agent_dl->rx_payload_header,
                                      MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE);
    mmagic_datalink_agent_set_rdy_low();
    mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_IDLE);
}

/**
 * Function to update the agent state when a mmagic data link transmission is complete. In this case
 * "transmit" refers to the low level SPI payload transmission an not necessarily a packet
 * transmission from @ref mmagic_datalink_agent_tx_buffer.
 *
 * @note This is expected to be called from ISR context.
 *
 * @param agent_dl Reference to the data-link handle.
 */
static void mmagic_datalink_agent_handle_spi_tx_cplt(struct mmagic_datalink_agent *agent_dl)
{
    switch (agent_dl->state)
    {
    case MMAGIC_DATALINK_AGENT_STATE_C2A_ACK:
        /* Only notify rx thread once the ACK has been sent */
        mmosal_semb_give_from_isr(agent_dl->rx_task_semb);
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_READ_PAYLOAD:
        MMOSAL_ASSERT(agent_dl->pending_release_tx_buf == NULL);
        agent_dl->pending_release_tx_buf = agent_dl->prev_tx_buf;
        agent_dl->prev_tx_buf = agent_dl->tx_buf;
        agent_dl->prev_tx_payload_len[0] = agent_dl->tx_payload_len[0];
        agent_dl->prev_tx_payload_len[1] = agent_dl->tx_payload_len[1];
        agent_dl->tx_buf = NULL;
        memset(agent_dl->tx_payload_len, 0, MMAGIC_DATALINK_PAYLOAD_LEN_SIZE);
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        mmosal_semb_give_from_isr(agent_dl->tx_buf_complete_semb);
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_PAYLOAD:
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_READ_LEN:
        if (!agent_dl->tx_buf)
        {
            mmagic_datalink_agent_configure_rx_idle(agent_dl);
            return;
        }
        mmagic_datalink_agent_spi_transmit(agent_dl,
                                           mmbuf_get_data_start(agent_dl->tx_buf),
                                           mmbuf_get_data_length(agent_dl->tx_buf));
        mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_A2C_READ_PAYLOAD);
        mmagic_datalink_agent_set_rdy_high();
        break;

    case MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_LEN:
        if (!agent_dl->prev_tx_buf)
        {
            mmagic_datalink_agent_configure_rx_idle(agent_dl);
            return;
        }
        mmagic_datalink_agent_spi_transmit(agent_dl,
                                           mmbuf_get_data_start(agent_dl->prev_tx_buf),
                                           mmbuf_get_data_length(agent_dl->prev_tx_buf));
        mmagic_datalink_agent_update_state(agent_dl,
                                           MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_PAYLOAD);
        mmagic_datalink_agent_set_rdy_high();
        break;

    default:
        MMOSAL_ASSERT(false);
        break;
    }
}

/**
 * Function to process a mmagic data link payload header when we receive one.
 *
 * @note This is expected to be called from ISR context.
 *
 * @param agent_dl Reference to the data-link handle.
 */
static void mmagic_datalink_agent_handle_payload_header(struct mmagic_datalink_agent *agent_dl)
{
    const uint8_t *payload_header = agent_dl->rx_payload_header;

    switch (payload_header[0])
    {
    case MMAGIC_DATALINK_WRITE:
        agent_dl->rx_packet_len = (payload_header[1] << 8) | payload_header[2];
        mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD);
        mmagic_datalink_agent_spi_receive(agent_dl, agent_dl->rx_packet_buf,
                                          agent_dl->rx_packet_len);
        break;

    case MMAGIC_DATALINK_READ:
        mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_A2C_READ_LEN);
        mmagic_datalink_agent_spi_transmit(agent_dl, agent_dl->tx_payload_len,
                                           MMAGIC_DATALINK_PAYLOAD_LEN_SIZE);
        break;

    case MMAGIC_DATALINK_REREAD:
        mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_A2C_REREAD_LEN);
        mmagic_datalink_agent_spi_transmit(agent_dl, agent_dl->prev_tx_payload_len,
                                           MMAGIC_DATALINK_PAYLOAD_LEN_SIZE);
        break;

    default:
        /* Ignore unknown mmagic_datalink payload type. */
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        break;
    }
}

/**
 * Function to update the agent state when a mmagic data link receive is complete. In this case
 * "receive" refers to the low level SPI payload receive and not necessarily a packet
 * reception.
 *
 * @note This is expected to be called from ISR context.
 *
 * @param agent_dl Reference to the data-link handle.
 */
static void mmagic_datalink_agent_handle_spi_rx_cplt(struct mmagic_datalink_agent *agent_dl)
{
    switch (agent_dl->state)
    {
    case MMAGIC_DATALINK_AGENT_STATE_IDLE:
        mmagic_datalink_agent_handle_payload_header(agent_dl);
        mmagic_datalink_agent_set_rdy_low();
        break;

    case MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD:
        mmosal_semb_give_from_isr(agent_dl->rx_task_semb);
        break;

    default:
        /* We shouldn't be receiving in any other state. */
        MMOSAL_ASSERT(false);
        break;
    }
}

/**
 * Function to handle any SPI bus errors that occur. It will attempt to recover and put the agent
 * back into a recoverable state. If this is not possible it will assert.
 *
 * @note This is expected to be called from ISR context.
 *
 * @param agent_dl Reference to the data-link handle.
 */
static void mmagic_datalink_agent_handle_spi_error(struct mmagic_datalink_agent *agent_dl)
{
    switch (agent_dl->state)
    {
    case MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD:
        agent_dl->rx_packet_len = 0;
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        break;

    default:
        agent_dl->state = MMAGIC_DATALINK_AGENT_STATE_ERROR;
        break;
    }
    mmagic_datalink_agent_set_rdy_low();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    struct mmagic_datalink_agent *agent_dl = mmagic_datalink_agent_get();
    if (hspi->Instance != agent_dl->spi_handle->Instance)
    {
        return;
    }
    mmagic_datalink_agent_handle_spi_tx_cplt(agent_dl);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    struct mmagic_datalink_agent *agent_dl = mmagic_datalink_agent_get();
    if (hspi->Instance != agent_dl->spi_handle->Instance)
    {
        return;
    }
    mmagic_datalink_agent_handle_spi_rx_cplt(agent_dl);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    struct mmagic_datalink_agent *agent_dl = mmagic_datalink_agent_get();
    if (hspi->Instance != agent_dl->spi_handle->Instance)
    {
        return;
    }
    mmagic_datalink_agent_handle_spi_error(agent_dl);
}

static struct mmbuf *mmagic_datalink_agent_handle_c2a_work(struct mmagic_datalink_agent *agent_dl)
{
    struct mmbuf *buf = mmbuf_alloc_on_heap(0, agent_dl->max_rx_buffer_size);
    if (!buf)
    {
        mmagic_datalink_agent_configure_rx_idle(agent_dl);
        return NULL;
    }
    mmbuf_append_data(buf, agent_dl->rx_packet_buf, agent_dl->rx_packet_len);
    mmagic_datalink_agent_update_state(agent_dl, MMAGIC_DATALINK_AGENT_STATE_C2A_ACK);
    mmagic_datalink_agent_spi_transmit(agent_dl, mmagic_datalink_ack_payload, 1);
    mmagic_datalink_agent_set_rdy_high();
    return buf;
}

/**
 * Task to handle processing of received packets. The callback registered on initialization will be
 * executed from this task context.
 */
static void mmagic_datalink_agent_rx_task(void *arg)
{
    struct mmagic_datalink_agent *agent_dl = (struct mmagic_datalink_agent *)arg;

    while (!agent_dl->shutdown)
    {
        struct mmbuf *rx_buf = NULL;
        if (agent_dl->state == MMAGIC_DATALINK_AGENT_STATE_C2A_PAYLOAD)
        {
            rx_buf = mmagic_datalink_agent_handle_c2a_work(agent_dl);
        }

        if (rx_buf)
        {
            agent_dl->rx_buffer_callback(agent_dl, agent_dl->rx_buffer_cb_arg, rx_buf);
        }

        mmosal_semb_wait(agent_dl->rx_task_semb, UINT32_MAX);
    }
    agent_dl->rx_task_has_finished = true;
}

void MMAGIC_DATALINK_AGENT_WAKE_HANDLER(void)
{
    struct mmagic_datalink_agent *agent_dl = mmagic_datalink_agent_get();

    if (LL_EXTI_IsActiveRisingFlag_0_31(MMAGIC_DATALINK_AGENT_WAKE_LINE) != RESET)
    {
        LL_EXTI_ClearRisingFlag_0_31(MMAGIC_DATALINK_AGENT_WAKE_LINE);
        mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_DATALINK);
        mmagic_datalink_agent_set_rdy_high();
    }

    if (LL_EXTI_IsActiveFallingFlag_0_31(MMAGIC_DATALINK_AGENT_WAKE_LINE) != RESET)
    {
        LL_EXTI_ClearFallingFlag_0_31(MMAGIC_DATALINK_AGENT_WAKE_LINE);
        if (agent_dl->state != MMAGIC_DATALINK_AGENT_STATE_IDLE)
        {
            mmagic_datalink_agent_configure_rx_idle(agent_dl);
        }

        if (agent_dl->tx_buf)
        {
            /* Indicate that there is pending data for the Controller to read and ensure the agent
             * stays awake */
            mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_DATALINK);
            mmagic_datalink_agent_set_rdy_high();
        }
        else if (agent_dl->deep_sleep_mode == MMAGIC_DATALINK_AGENT_DEEP_SLEEP_HARDWARE)
        {
            mmhal_clear_deep_sleep_veto(MMHAL_VETO_ID_DATALINK);
        }
    }
}

/**
 * Function to initialize the DMA peripherals for the SPI interface. This must be called after the
 * SPI interface has been initialized. */
static void mmagic_datalink_agent_dma_init(struct mmagic_datalink_agent *agent_dl);

struct mmagic_datalink_agent *mmagic_datalink_agent_init(
    const struct mmagic_datalink_agent_init_args *args)
{
    bool ok;
    struct mmagic_datalink_agent *agent_dl = mmagic_datalink_agent_get();
    if (agent_dl->initialized)
    {
        return NULL;
    }

    memset(agent_dl, 0, sizeof(*agent_dl));

    agent_dl->rx_buffer_callback = args->rx_callback;
    agent_dl->rx_buffer_cb_arg = args->rx_arg;
    agent_dl->max_rx_buffer_size = args->max_packet_size;
    if ((agent_dl->rx_buffer_callback == NULL) || (!agent_dl->max_rx_buffer_size))
    {
        /* These are required fields, do not proceed if not present. */
        return NULL;
    }

    agent_dl->rx_packet_buf = (uint8_t *)mmosal_malloc(agent_dl->max_rx_buffer_size +
                                                       MMAGIC_DATALINK_AGENT_ALLOC_PADDING_BYTES);
    if (!agent_dl->rx_packet_buf)
    {
        goto failure;
    }

    agent_dl->tx_buf_complete_semb = mmosal_semb_create("tx_complete");
    if (!agent_dl->tx_buf_complete_semb)
    {
        goto failure;
    }

    agent_dl->rx_task_semb = mmosal_semb_create("rx_task");
    if (!agent_dl->rx_task_semb)
    {
        goto failure;
    }

    agent_dl->rx_task_handle = mmosal_task_create(mmagic_datalink_agent_rx_task, agent_dl,
                                                  MMOSAL_TASK_PRI_HIGH, 512, "mmagic_datalink_rx");
    if (!agent_dl->rx_task_handle)
    {
        goto failure;
    }

    agent_dl->spi_handle = &hspi2;
    MX_SPI2_Init();
    mmagic_datalink_agent_dma_init(agent_dl);

    mmagic_datalink_agent_configure_rx_idle(agent_dl);

    agent_dl->initialized = true;
    ok = mmagic_datalink_agent_set_deep_sleep_mode(agent_dl,
                                                   MMAGIC_DATALINK_AGENT_DEEP_SLEEP_HARDWARE);
    if (!ok)
    {
        MMOSAL_ASSERT(false);
    }

    return agent_dl;

failure:
    mmosal_free(agent_dl->rx_packet_buf);
    if (agent_dl->rx_task_semb != NULL)
    {
        mmosal_semb_delete(agent_dl->rx_task_semb);
    }
    if (agent_dl->tx_buf_complete_semb != NULL)
    {
        mmosal_semb_delete(agent_dl->tx_buf_complete_semb);
    }
    return NULL;
}

void mmagic_datalink_agent_deinit(struct mmagic_datalink_agent *agent_dl)
{
    if (!agent_dl->initialized)
    {
        return;
    }

    agent_dl->shutdown = true;
    mmosal_semb_give(agent_dl->rx_task_semb);
    while (!agent_dl->rx_task_has_finished)
    {
        mmosal_task_sleep(2);
    }
    mmosal_free(agent_dl->rx_packet_buf);
    if (agent_dl->rx_task_semb != NULL)
    {
        mmosal_semb_delete(agent_dl->rx_task_semb);
    }
    if (agent_dl->tx_buf_complete_semb != NULL)
    {
        mmosal_semb_delete(agent_dl->tx_buf_complete_semb);
    }
    agent_dl->initialized = false;
}

struct mmbuf *mmagic_datalink_agent_alloc_buffer_for_tx(size_t header_size, size_t payload_size)
{
    return mmbuf_alloc_on_heap(header_size,
                               payload_size + MMAGIC_DATALINK_AGENT_ALLOC_PADDING_BYTES);
}

int mmagic_datalink_agent_tx_buffer(struct mmagic_datalink_agent *agent_dl, struct mmbuf *buf)
{
    const uint32_t buf_len = mmbuf_get_data_length(buf);
    if ((!buf) || (agent_dl->tx_buf) || (!agent_dl->initialized) || (buf_len == 0) ||
        (buf_len > UINT16_MAX))
    {
        MMOSAL_DEV_ASSERT(buf);
        MMOSAL_DEV_ASSERT(agent_dl->tx_buf == NULL);
        MMOSAL_DEV_ASSERT(agent_dl->initialized);
        MMOSAL_DEV_ASSERT(buf_len);
        MMOSAL_DEV_ASSERT(buf_len <= UINT16_MAX);
        mmbuf_release(buf);
        return -1;
    }

    MMOSAL_TASK_ENTER_CRITICAL();
    mmbuf_release(agent_dl->pending_release_tx_buf);
    agent_dl->pending_release_tx_buf = NULL;
    agent_dl->tx_buf = buf;
    agent_dl->tx_payload_len[0] = (uint8_t)(buf_len >> 8);
    agent_dl->tx_payload_len[1] = (uint8_t)(buf_len);
    if (agent_dl->state == MMAGIC_DATALINK_AGENT_STATE_IDLE)
    {
        /* Only raise the ready line if no transaction is in progress. If there is a transaction in
         * progress the ready line will be set when it concludes. */
        mmagic_datalink_agent_set_rdy_high();
    }
    MMOSAL_TASK_EXIT_CRITICAL();

    mmosal_semb_wait(agent_dl->tx_buf_complete_semb, UINT32_MAX);

    return buf_len;
}

bool mmagic_datalink_agent_set_deep_sleep_mode(struct mmagic_datalink_agent *agent_dl,
                                               enum mmagic_datalink_agent_deep_sleep_mode mode)
{
    bool success = false;

    if (mode == agent_dl->deep_sleep_mode)
    {
        return true;
    }

    switch (mode)
    {
    case MMAGIC_DATALINK_AGENT_DEEP_SLEEP_DISABLED:
        NVIC_DisableIRQ(MMAGIC_DATALINK_WAKE_EXTI_IRQn);
        agent_dl->deep_sleep_mode = mode;
        mmhal_set_deep_sleep_veto(MMHAL_VETO_ID_DATALINK);
        success = true;
        break;

    case MMAGIC_DATALINK_AGENT_DEEP_SLEEP_ONE_SHOT:
    case MMAGIC_DATALINK_AGENT_DEEP_SLEEP_HARDWARE:
        agent_dl->deep_sleep_mode = mode;
        mmhal_clear_deep_sleep_veto(MMHAL_VETO_ID_DATALINK);
        NVIC_EnableIRQ(MMAGIC_DATALINK_WAKE_EXTI_IRQn);
        success = true;
        break;

    default:
        success = false;
        break;
    }

    return success;
}

/*
 * DMA initialization code provided by ST.
 *
 * Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 */

DMA_HandleTypeDef handle_GPDMA1_Channel7;
DMA_HandleTypeDef handle_GPDMA1_Channel6;
DMA_NodeTypeDef Node_tx;
DMA_QListTypeDef Queue_tx;
DMA_NodeTypeDef Node_rx;
DMA_QListTypeDef Queue_rx;

/**
 * DMA Linked-list Queue_tx configuration
 */
HAL_StatusTypeDef MX_Queue_tx_Config(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    /* DMA node configuration declaration */
    DMA_NodeConfTypeDef pNodeConfig;

    /* Set node configuration ################################################*/
    pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    pNodeConfig.Init.Request = GPDMA1_REQUEST_SPI2_TX;
    pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    pNodeConfig.Init.Direction = DMA_MEMORY_TO_PERIPH;
    pNodeConfig.Init.SrcInc = DMA_SINC_INCREMENTED;
    pNodeConfig.Init.DestInc = DMA_DINC_FIXED;
    pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    pNodeConfig.Init.SrcBurstLength = 1;
    pNodeConfig.Init.DestBurstLength = 1;
    pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT1 | DMA_DEST_ALLOCATED_PORT0;
    pNodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
    pNodeConfig.SrcAddress = 0;
    pNodeConfig.DstAddress = 0;
    pNodeConfig.DataSize = 0;

    /* Build Node_tx Node */
    ret = HAL_DMAEx_List_BuildNode(&pNodeConfig, &Node_tx);

    /* Insert Node_tx to Queue */
    ret = HAL_DMAEx_List_InsertNode_Tail(&Queue_tx, &Node_tx);

    return ret;
}

/**
 * DMA Linked-list Queue_rx configuration
 */
HAL_StatusTypeDef MX_Queue_rx_Config(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    /* DMA node configuration declaration */
    DMA_NodeConfTypeDef pNodeConfig;

    /* Set node configuration ################################################*/
    pNodeConfig.NodeType = DMA_GPDMA_LINEAR_NODE;
    pNodeConfig.Init.Request = GPDMA1_REQUEST_SPI2_RX;
    pNodeConfig.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    pNodeConfig.Init.Direction = DMA_PERIPH_TO_MEMORY;
    pNodeConfig.Init.SrcInc = DMA_SINC_FIXED;
    pNodeConfig.Init.DestInc = DMA_DINC_INCREMENTED;
    pNodeConfig.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    pNodeConfig.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    pNodeConfig.Init.SrcBurstLength = 1;
    pNodeConfig.Init.DestBurstLength = 1;
    pNodeConfig.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
    pNodeConfig.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    pNodeConfig.TriggerConfig.TriggerPolarity = DMA_TRIG_POLARITY_MASKED;
    pNodeConfig.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
    pNodeConfig.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
    pNodeConfig.SrcAddress = 0;
    pNodeConfig.DstAddress = 0;
    pNodeConfig.DataSize = 0;

    /* Build Node_rx Node */
    ret = HAL_DMAEx_List_BuildNode(&pNodeConfig, &Node_rx);

    /* Insert Node_rx to Queue */
    ret = HAL_DMAEx_List_InsertNode_Tail(&Queue_rx, &Node_rx);

    return ret;
}

/**
 * General Purpose DMA 1 Initialization Function
 */
static void MX_GPDMA1_Init(void)
{
    /* USER CODE BEGIN GPDMA1_Init 0 */

    /* USER CODE END GPDMA1_Init 0 */

    /* Peripheral clock enable */
    __HAL_RCC_GPDMA1_CLK_ENABLE();

    /* GPDMA1 interrupt Init */
    HAL_NVIC_SetPriority(GPDMA1_Channel6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(GPDMA1_Channel7_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel7_IRQn);

    /* USER CODE BEGIN GPDMA1_Init 1 */

    /* USER CODE END GPDMA1_Init 1 */
    handle_GPDMA1_Channel7.Instance = GPDMA1_Channel7;
    handle_GPDMA1_Channel7.InitLinkedList.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
    handle_GPDMA1_Channel7.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    handle_GPDMA1_Channel7.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
    handle_GPDMA1_Channel7.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
    handle_GPDMA1_Channel7.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_NORMAL;
    if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel7) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel7, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
        Error_Handler();
    }
    handle_GPDMA1_Channel6.Instance = GPDMA1_Channel6;
    handle_GPDMA1_Channel6.InitLinkedList.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
    handle_GPDMA1_Channel6.InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
    handle_GPDMA1_Channel6.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT1;
    handle_GPDMA1_Channel6.InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
    handle_GPDMA1_Channel6.InitLinkedList.LinkedListMode = DMA_LINKEDLIST_NORMAL;
    if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel6) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel6, DMA_CHANNEL_NPRIV) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN GPDMA1_Init 2 */

    /* USER CODE END GPDMA1_Init 2 */
}

/**
 * This function handles General Purpose DMA 1 Channel 6 global interrupt.
 */
void GPDMA1_Channel6_IRQHandler(void)
{
    /* USER CODE BEGIN GPDMA1_Channel6_IRQn 0 */

    /* USER CODE END GPDMA1_Channel6_IRQn 0 */
    HAL_DMA_IRQHandler(&handle_GPDMA1_Channel6);
    /* USER CODE BEGIN GPDMA1_Channel6_IRQn 1 */

    /* USER CODE END GPDMA1_Channel6_IRQn 1 */
}

/**
 * This function handles General Purpose DMA 1 Channel 7 global interrupt.
 */
void GPDMA1_Channel7_IRQHandler(void)
{
    /* USER CODE BEGIN GPDMA1_Channel7_IRQn 0 */

    /* USER CODE END GPDMA1_Channel7_IRQn 0 */
    HAL_DMA_IRQHandler(&handle_GPDMA1_Channel7);
    /* USER CODE BEGIN GPDMA1_Channel7_IRQn 1 */

    /* USER CODE END GPDMA1_Channel7_IRQn 1 */
}

static void mmagic_datalink_agent_dma_init(struct mmagic_datalink_agent *agent_dl)
{
    MX_GPDMA1_Init();

    MX_Queue_tx_Config();
    HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel7, &Queue_tx);
    __HAL_LINKDMA(agent_dl->spi_handle, hdmatx, handle_GPDMA1_Channel7);
    MX_Queue_rx_Config();
    HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel6, &Queue_rx);
    __HAL_LINKDMA(agent_dl->spi_handle, hdmarx, handle_GPDMA1_Channel6);
}
