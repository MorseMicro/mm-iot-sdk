/*
 * Copyright 2021 Morse Micro
 */

#pragma once

#include "stm32f4xx_hal.h"

#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_exti.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_spi.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_rng.h"
#include "stm32f4xx_ll_rtc.h"

#include "mmosal.h"

/* Transiver Reset Lin */
#define RESET_N_GPIO_Port             GPIOA
#define RESET_N_Pin                   LL_GPIO_PIN_3

/* SPI1 */
#define SPI_PERIPH                  SPI1
#define SPI_CLK_GPIO_PORT           GPIOA
#define SPI_CLK_PIN                 LL_GPIO_PIN_5
#define SPI_MISO_GPIO_PORT          GPIOA
#define SPI_MISO_PIN                LL_GPIO_PIN_6
#define SPI_MOSI_GPIO_PORT          GPIOA
#define SPI_MOSI_PIN                LL_GPIO_PIN_7
#define SPI_CS_GPIO_Port            GPIOD
#define SPI_CS_Pin                  LL_GPIO_PIN_14

#define SPI_DMA_PERIPH              DMA2
#define SPI_DMA_CHANNEL             LL_DMA_CHANNEL_3
#define SPI_RX_DMA_STREAM           LL_DMA_STREAM_0
#define SPI_TX_DMA_STREAM           LL_DMA_STREAM_3

#define SPI_IRQ_GPIO_Port           GPIOF
#define SPI_IRQ_Pin                 LL_GPIO_PIN_5
#define SPI_IRQn                    EXTI9_5_IRQn
#define SPI_IRQ_LINE                LL_EXTI_LINE_5
#define SPI_IRQ_HANDLER             EXTI9_5_IRQHandler
/* These pins always remain high */
#define Data1_GPIO_Port             GPIOC
#define Data1_Pin                   LL_GPIO_PIN_9
#define Data2_GPIO_Port             GPIOC
#define Data2_Pin                   LL_GPIO_PIN_10

/* SWD */
#define TMS_GPIO_Port               GPIOA
#define TMS_Pin                     LL_GPIO_PIN_13
#define TCK_GPIO_Port               GPIOA
#define TCK_Pin                     LL_GPIO_PIN_14

/* UART3 */
#define DBG_BAUD                    115200
#define DBG_RX_GPIO_Port            GPIOD
#define DBG_RX_Pin                  LL_GPIO_PIN_8
#define DBG_TX_GPIO_Port            GPIOD
#define DBG_TX_Pin                  LL_GPIO_PIN_9

/* Power Save Pins */
#define WAKE_GPIO_Port              GPIOC
#define WAKE_Pin                    LL_GPIO_PIN_0
#define BUSY_GPIO_Port              GPIOC
#define BUSY_Pin                    LL_GPIO_PIN_3
#define BUSY_IRQn                   EXTI3_IRQn
#define BUSY_IRQ_LINE               LL_EXTI_LINE_3
#define BUSY_IRQ_HANDLER            EXTI3_IRQHandler

/* LED's */
#define GPIO_LED_RED_Pin            LL_GPIO_PIN_14
#define GPIO_LED_RED_GPIO_Port      GPIOB
#define GPIO_LED_GREEN_Pin          LL_GPIO_PIN_0
#define GPIO_LED_GREEN_GPIO_Port    GPIOB
#define GPIO_LED_BLUE_Pin           LL_GPIO_PIN_7
#define GPIO_LED_BLUE_GPIO_Port     GPIOB


void bsp_clock_init(void);

void bsp_peripherals_init(void);