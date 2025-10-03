/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_ll_dma.h"
#include "stm32u5xx_ll_lptim.h"
#include "stm32u5xx_ll_lpuart.h"
#include "stm32u5xx_ll_rcc.h"
#include "stm32u5xx_ll_rtc.h"
#include "stm32u5xx_ll_spi.h"
#include "stm32u5xx_ll_system.h"
#include "stm32u5xx_ll_gpio.h"
#include "stm32u5xx_ll_exti.h"
#include "stm32u5xx_ll_lpgpio.h"
#include "stm32u5xx_ll_bus.h"
#include "stm32u5xx_ll_cortex.h"
#include "stm32u5xx_ll_utils.h"
#include "stm32u5xx_ll_pwr.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define SPI_PERIPH (SPI2)
#define SPI_DMA_PERIPH (GPDMA1)
#define SPI_RX_DMA_CHANNEL  (LL_DMA_CHANNEL_14)
#define SPI_TX_DMA_CHANNEL  (LL_DMA_CHANNEL_15)

#define SPI_IRQn            (EXTI15_IRQn)
#define SPI_IRQ_LINE        (LL_EXTI_LINE_15)
#define SPI_IRQ_HANDLER     EXTI15_IRQHandler
#define BUSY_IRQn           (EXTI5_IRQn)
#define BUSY_IRQ_LINE       (LL_EXTI_LINE_5)
#define BUSY_IRQ_HANDLER    EXTI5_IRQHandler

#define LOG_USART               (LPUART1)
#define LOG_USART_IRQ           (LPUART1_IRQn)
#define LOG_USART_IRQ_HANDLER   LPUART1_IRQHandler

/* ONE_SHOT is USART RX GPIO */
#define ONE_SHOT_IRQn           (EXTI0_IRQn)
#define ONE_SHOT_IRQ_LINE       (LL_EXTI_LINE_0)
#define ONE_SHOT_IRQ_HANDLER    EXTI0_IRQHandler
#define ONE_SHOT_EXTI_Port      (LL_EXTI_EXTI_PORTC)
#define ONE_SHOT_EXTI_Line      (LL_EXTI_EXTI_LINE0)
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CAM_PWDN_Pin LL_GPIO_PIN_3
#define CAM_PWDN_GPIO_Port GPIOE
#define LOG_USART_RX_Pin LL_GPIO_PIN_0
#define LOG_USART_RX_GPIO_Port GPIOC
#define LOG_USART_TX_Pin LL_GPIO_PIN_1
#define LOG_USART_TX_GPIO_Port GPIOC
#define QSPI_FLASH_CS_Pin LL_GPIO_PIN_2
#define QSPI_FLASH_CS_GPIO_Port GPIOA
#define GPIO_LED_GREEN_Pin LL_GPIO_PIN_7
#define GPIO_LED_GREEN_GPIO_Port GPIOE
#define GPIO_LED_BLUE_Pin LL_GPIO_PIN_8
#define GPIO_LED_BLUE_GPIO_Port GPIOE
#define GPIO_LED_RED_Pin LL_GPIO_PIN_11
#define GPIO_LED_RED_GPIO_Port GPIOE
#define SPI_IRQ_Pin LL_GPIO_PIN_15
#define SPI_IRQ_GPIO_Port GPIOB
#define SPI_IRQ_EXTI_IRQn EXTI15_IRQn
#define MM_DEBUG_1_Pin LL_GPIO_PIN_9
#define MM_DEBUG_1_GPIO_Port GPIOA
#define MM_DEBUG_0_Pin LL_GPIO_PIN_10
#define MM_DEBUG_0_GPIO_Port GPIOA
#define WAKE_Pin LL_GPIO_PIN_0
#define WAKE_GPIO_Port GPIOD
#define SPI_SCK_Pin LL_GPIO_PIN_1
#define SPI_SCK_GPIO_Port GPIOD
#define BLE_RESET_N_Pin LL_GPIO_PIN_2
#define BLE_RESET_N_GPIO_Port GPIOD
#define SPI_MISO_Pin LL_GPIO_PIN_3
#define SPI_MISO_GPIO_Port GPIOD
#define SPI_MOSI_Pin LL_GPIO_PIN_4
#define SPI_MOSI_GPIO_Port GPIOD
#define SPI_CS_Pin LL_GPIO_PIN_4
#define SPI_CS_GPIO_Port GPIOB
#define BUSY_Pin LL_GPIO_PIN_5
#define BUSY_GPIO_Port GPIOB
#define BUSY_EXTI_IRQn EXTI5_IRQn
#define RESET_N_Pin LL_GPIO_PIN_0
#define RESET_N_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
