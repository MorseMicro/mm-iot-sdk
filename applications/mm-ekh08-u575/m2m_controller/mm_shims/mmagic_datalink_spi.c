/*
 * Copyright 2024 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "main.h"
#include "stm32u5xx_ll_spi.h"

#include "mmbuf.h"
#include "mmutils.h"
#include "mmhal.h"
#include "mmosal.h"
#include "mmagic_datalink_controller.h"

/* The SPI protocol uses a timer-based flow control approach. This means that there is no dedicated
 * pin for the slave to indicate RDY state. Instead a short delay is added in between transactions
 * to give the slave time to reconfigure. */

#define MMAGIC_DATALINK_SPI_RDY_DELAY_MS 100

#define MMAGIC_DATALINK_RETRY_ATTEMPTS 3

extern SPI_HandleTypeDef hspi2;

/**
 * Controller struct used internally by the data-link. This will be specific to each
 * type of interface. i.e SPI and UART may have different elements in the struct.
 */
struct mmagic_datalink_controller
{
    /** Flag used to indicate if the controller data-link has been initialized successfully. */
    bool initialized;
    /** Handle for the spi interface. */
    SPI_HandleTypeDef *spi_handle;
    /** Mutex to protect accessing the bus during transactions. */
    struct mmosal_mutex *spi_mutex;
    /** Callback to execute when an RX packet has been received. */
    mmagic_datalink_controller_rx_buffer_cb_t rx_buffer_callback;
    /** Argument to pass to the callback function. */
    void *rx_buffer_cb_arg;
    /** Flag to indicate that the background rx task should terminate. */
    bool shutdown;
    /** Flag to indicate when the rx_task has finished running. */
    bool rx_task_has_finished;
    /** Task handle for the background rx task. */
    struct mmosal_task *rx_task_handle;
};

/**
 * Static allocation of the controller data-link structure. Currently it is expected that there will
 * only ever be one instance.
 */
static struct mmagic_datalink_controller controller_datalink = {};

static inline struct mmagic_datalink_controller *mmagic_datalink_controller_get(void)
{
    return &controller_datalink;
}

/**
 * Function to wait until the ready line transitions high.
 *
 * @param  timeout_ms Time in ms to wait for the ready pin to go high.
 *
 * @return            @c true on success, else @c false
 */
static bool mmagic_datalink_wait_for_rdy_high(uint32_t timeout_ms)
{
    bool success = false;
    uint32_t timeout = timeout_ms + mmosal_get_time_ms();
    while (!mmosal_time_has_passed(timeout))
    {
        if (LL_GPIO_IsInputPinSet(MMAGIC_DATALINK_IRQ_GPIO_Port, MMAGIC_DATALINK_IRQ_Pin))
        {
            success = true;
            break;
        }
    }

    return success;
}

/**
 * Function to wait until the ready line transitions low.
 *
 * @param  timeout_ms Time in ms to wait for the ready pin to go low.
 *
 * @return            @c true on success, else @c false
 */
static bool mmagic_datalink_wait_for_rdy_low(uint32_t timeout_ms)
{
    bool success = false;
    uint32_t timeout = timeout_ms + mmosal_get_time_ms();
    while (!mmosal_time_has_passed(timeout))
    {
        if (!LL_GPIO_IsInputPinSet(MMAGIC_DATALINK_IRQ_GPIO_Port, MMAGIC_DATALINK_IRQ_Pin))
        {
            success = true;
            break;
        }
    }

    return success;
}

/**
 * Blocking spi receive.
 *
 * @param  controller_dl Reference to the data-link handle.
 * @param  data          Reference to buffer where the data will be placed.
 * @param  len           Length of the data to receive.
 *
 * @return               @c true if the data was successfully received else @c false.
 */
static bool mmagic_datalink_controller_spi_blocking_receive(
    struct mmagic_datalink_controller *controller_dl,
    uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;
    /* Drop the baud rate to below 38.5MHz as that's the fastest the slave can TX */
    LL_SPI_SetBaudRatePrescaler(controller_dl->spi_handle->Instance, LL_SPI_BAUDRATEPRESCALER_DIV8);
    status = HAL_SPI_Receive(controller_dl->spi_handle, data, len, UINT32_MAX);

    return (status == HAL_OK) ? true : false;
}

/**
 * Blocking spi transmit.
 *
 * @param  controller_dl Reference to the data-link handle.
 * @param  data          Reference to buffer containing the data to be transmitted.
 * @param  len           Length of the data to receive.
 *
 * @return               @c true if the data was successfully transmitted else @c false.
 */
static bool mmagic_datalink_controller_spi_blocking_transmit(
    struct mmagic_datalink_controller *controller_dl,
    const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;
    /* Increase the baud rate to the maximum value as the slave can RX at upto 100MHz */
    LL_SPI_SetBaudRatePrescaler(controller_dl->spi_handle->Instance, LL_SPI_BAUDRATEPRESCALER_DIV2);
    status = HAL_SPI_Transmit(controller_dl->spi_handle, data, len, UINT32_MAX);

    return (status == HAL_OK) ? true : false;
}

static struct mmbuf *controller_rx_buffer(struct mmagic_datalink_controller *controller_dl,
                                          enum mmagic_datalink_payload_type read_type)
{
    if ((read_type != MMAGIC_DATALINK_READ) && (read_type != MMAGIC_DATALINK_REREAD))
    {
        return NULL;
    }

    bool ok;
    uint8_t *data = NULL;
    struct mmbuf *rx_buf = NULL;
    uint16_t payload_len = 0;
    uint8_t payload_header[MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE] = {read_type};

    MMOSAL_MUTEX_GET_INF(controller_dl->spi_mutex);
    NVIC_DisableIRQ(MMAGIC_DATALINK_IRQ_EXTI_IRQn);
    LL_GPIO_SetOutputPin(MMAGIC_DATALINK_WAKE_GPIO_Port, MMAGIC_DATALINK_WAKE_Pin);

    ok = mmagic_datalink_wait_for_rdy_high(0xFFFF);
    if (!ok)
    {
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_transmit(controller_dl, payload_header,
                                                          MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE);
    if (!ok)
    {
        goto exit;
    }

    ok = mmagic_datalink_wait_for_rdy_low(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);
    if (!ok)
    {
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_receive(controller_dl, &payload_header[1],
                                                         MMAGIC_DATALINK_PAYLOAD_LEN_SIZE);
    if (!ok)
    {
        goto exit;
    }

    payload_len = (payload_header[1] << 8) | payload_header[2];
    if (!payload_len)
    {
        goto exit;
    }

    rx_buf = mmbuf_alloc_on_heap(0, payload_len);
    if (!rx_buf)
    {
        goto exit;
    }
    data = mmbuf_append(rx_buf, payload_len);
    ok = mmagic_datalink_wait_for_rdy_high(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);
    if (!ok)
    {
        mmbuf_release(rx_buf);
        rx_buf = NULL;
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_receive(controller_dl, data, payload_len);
    if (!ok)
    {
        mmbuf_release(rx_buf);
        rx_buf = NULL;
        goto exit;
    }

    /* We don't bother checking the result here as regardless we're going to the exit condition. */
    (void)mmagic_datalink_wait_for_rdy_low(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);
exit:
    LL_EXTI_ClearRisingFlag_0_31(MMAGIC_DATALINK_CONTROLLER_IRQ_LINE);
    NVIC_EnableIRQ(MMAGIC_DATALINK_IRQ_EXTI_IRQn);
    LL_GPIO_ResetOutputPin(MMAGIC_DATALINK_WAKE_GPIO_Port, MMAGIC_DATALINK_WAKE_Pin);
    MMOSAL_MUTEX_RELEASE(controller_dl->spi_mutex);
    return rx_buf;
}

static struct mmbuf *mmagic_datalink_controller_rx_buffer(
    struct mmagic_datalink_controller *controller_dl)
{
    uint8_t attempts = 1;
    struct mmbuf *rx_buf = NULL;
    rx_buf = controller_rx_buffer(controller_dl, MMAGIC_DATALINK_READ);

    while ((attempts < MMAGIC_DATALINK_RETRY_ATTEMPTS) && (!rx_buf))
    {
        rx_buf = controller_rx_buffer(controller_dl, MMAGIC_DATALINK_REREAD);
        attempts++;
    }

    return rx_buf;
}

void MMAGIC_DATALINK_CONTROLLER_IRQ_HANLDER(void)
{
    if (LL_EXTI_IsActiveRisingFlag_0_31(MMAGIC_DATALINK_CONTROLLER_IRQ_LINE) != RESET)
    {
        LL_EXTI_ClearRisingFlag_0_31(MMAGIC_DATALINK_CONTROLLER_IRQ_LINE);
        struct mmagic_datalink_controller *controller_dl = mmagic_datalink_controller_get();
        mmosal_task_notify_from_isr(controller_dl->rx_task_handle);
    }
}

/**
 * Task to handle processing of received packets. The callback registered on initialization will be
 * executed from this task context.
 */
static void mmagic_datalink_controller_rx_task(void *arg)
{
    struct mmagic_datalink_controller *controller_dl = (struct mmagic_datalink_controller *)arg;

    while (!controller_dl->shutdown)
    {
        mmosal_task_wait_for_notification(UINT32_MAX);
        struct mmbuf *rx_buf = mmagic_datalink_controller_rx_buffer(controller_dl);
        if (rx_buf != NULL)
        {
            controller_dl->rx_buffer_callback(controller_dl, controller_dl->rx_buffer_cb_arg,
                                              rx_buf);
        }
        else
        {
            printf("Error with controller rx buffer\n");
        }
    }
    controller_dl->rx_task_has_finished = true;
}

struct mmagic_datalink_controller *mmagic_datalink_controller_init(
    const struct mmagic_datalink_controller_init_args *args)
{
    struct mmagic_datalink_controller *controller_dl = mmagic_datalink_controller_get();
    if (controller_dl->initialized)
    {
        return NULL;
    }

    memset(controller_dl, 0, sizeof(*controller_dl));
    controller_dl->rx_buffer_callback = args->rx_callback;
    controller_dl->rx_buffer_cb_arg = args->rx_arg;
    if (controller_dl->rx_buffer_callback == NULL)
    {
        /* These are required fields, do not proceed if not present. */
        return NULL;
    }

    controller_dl->spi_mutex = mmosal_mutex_create("mmagic_datalink_spi");
    if (!controller_dl->spi_mutex)
    {
        return NULL;
    }

    controller_dl->rx_task_handle = mmosal_task_create(mmagic_datalink_controller_rx_task,
                                                       controller_dl,
                                                       MMOSAL_TASK_PRI_LOW, 512,
                                                       "mmagic_datalink_rx");
    if (!controller_dl->rx_task_handle)
    {
        mmosal_mutex_delete(controller_dl->spi_mutex);
        return NULL;
    }

    controller_dl->spi_handle = &hspi2;
    MX_SPI2_Init();

    NVIC_EnableIRQ(MMAGIC_DATALINK_IRQ_EXTI_IRQn);
    controller_dl->initialized = true;
    return controller_dl;
}

/**
 * Deinitialize the mmagic data link controller. Any resources used will be freed.
 *
 * @param controller_dl Reference to the data-link handle.
 */
void mmagic_datalink_controller_deinit(struct mmagic_datalink_controller *controller_dl);

struct mmbuf *mmagic_datalink_controller_alloc_buffer_for_tx(
    struct mmagic_datalink_controller *controller_dl,
    size_t header_size,
    size_t payload_size)
{
    MM_UNUSED(controller_dl);
    return mmbuf_alloc_on_heap(header_size + 3, payload_size);
}

static int controller_tx_buffer(struct mmagic_datalink_controller *controller_dl,
                                struct mmbuf *buf)
{
    bool ok;
    uint8_t payload_header[MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE] = {MMAGIC_DATALINK_WRITE};
    uint16_t payload_len = (uint16_t)mmbuf_get_data_length(buf);
    payload_header[1] = (uint8_t)(payload_len >> 8);
    payload_header[2] = (uint8_t)payload_len;
    uint8_t ack = MMAGIC_DATALINK_NACK;

    MMOSAL_MUTEX_GET_INF(controller_dl->spi_mutex);
    NVIC_DisableIRQ(MMAGIC_DATALINK_IRQ_EXTI_IRQn);
    LL_GPIO_SetOutputPin(MMAGIC_DATALINK_WAKE_GPIO_Port, MMAGIC_DATALINK_WAKE_Pin);

    ok = mmagic_datalink_wait_for_rdy_high(0xFFFF);
    if (!ok)
    {
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_transmit(controller_dl, payload_header,
                                                          MMAGIC_DATALINK_PAYLOAD_HEADER_SIZE);
    if (!ok)
    {
        goto exit;
    }

    ok = mmagic_datalink_wait_for_rdy_low(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);
    if (!ok)
    {
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_transmit(controller_dl, mmbuf_get_data_start(buf),
                                                          payload_len);
    if (!ok)
    {
        goto exit;
    }

    ok = mmagic_datalink_wait_for_rdy_high(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);
    if (!ok)
    {
        goto exit;
    }
    ok = mmagic_datalink_controller_spi_blocking_receive(controller_dl, &ack, 1);
    if (!ok)
    {
        ack = MMAGIC_DATALINK_NACK;
        goto exit;
    }

    (void)mmagic_datalink_wait_for_rdy_low(MMAGIC_DATALINK_SPI_RDY_DELAY_MS);

exit:
    LL_EXTI_ClearRisingFlag_0_31(MMAGIC_DATALINK_CONTROLLER_IRQ_LINE);
    NVIC_EnableIRQ(MMAGIC_DATALINK_IRQ_EXTI_IRQn);
    LL_GPIO_ResetOutputPin(MMAGIC_DATALINK_WAKE_GPIO_Port, MMAGIC_DATALINK_WAKE_Pin);
    MMOSAL_MUTEX_RELEASE(controller_dl->spi_mutex);

    return ack == MMAGIC_DATALINK_ACK ? payload_len : -1;
}

int mmagic_datalink_controller_tx_buffer(struct mmagic_datalink_controller *controller_dl,
                                         struct mmbuf *buf)
{
    uint8_t attempts = 0;
    uint16_t payload_len = (uint16_t)mmbuf_get_data_length(buf);
    int result = 0;
    while ((attempts < MMAGIC_DATALINK_RETRY_ATTEMPTS) && (result != payload_len))
    {
        /* We can re-transmit the same packet because the upper layers handle duplicates. */
        result = controller_tx_buffer(controller_dl, buf);
        attempts++;
    }
    mmbuf_release(buf);
    return result;
}
