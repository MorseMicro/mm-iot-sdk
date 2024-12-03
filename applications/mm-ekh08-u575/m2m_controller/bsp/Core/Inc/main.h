/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "stm32u5xx_ll_rtc.h"
#include "stm32u5xx_ll_usart.h"
#include "stm32u5xx_ll_rcc.h"
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
#define LOG_USART               (USART1)
#define LOG_USART_IRQ           (USART1_IRQn)
#define LOG_USART_IRQ_HANDLER   USART1_IRQHandler

#define MMAGIC_DATALINK_CONTROLLER_IRQ_LINE (LL_EXTI_LINE_4)
#define MMAGIC_DATALINK_CONTROLLER_IRQ_HANLDER EXTI4_IRQHandler
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_SPI2_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MMAGIC_DATALINK_IRQ_Pin LL_GPIO_PIN_4
#define MMAGIC_DATALINK_IRQ_GPIO_Port GPIOE
#define MMAGIC_DATALINK_IRQ_EXTI_IRQn EXTI4_IRQn
#define MMAGIC_DATALINK_WAKE_Pin LL_GPIO_PIN_5
#define MMAGIC_DATALINK_WAKE_GPIO_Port GPIOE
#define MMAGIC_DATALINK_SPI_SCK_Pin LL_GPIO_PIN_13
#define MMAGIC_DATALINK_SPI_SCK_GPIO_Port GPIOB
#define GPIO_LED_RED_Pin LL_GPIO_PIN_2
#define GPIO_LED_RED_GPIO_Port GPIOG
#define GPIO_LED_GREEN_Pin LL_GPIO_PIN_7
#define GPIO_LED_GREEN_GPIO_Port GPIOC
#define LOG_USART_TX_Pin LL_GPIO_PIN_9
#define LOG_USART_TX_GPIO_Port GPIOA
#define LOG_USART_RX_Pin LL_GPIO_PIN_10
#define LOG_USART_RX_GPIO_Port GPIOA
#define MM_DEBUG_0_Pin LL_GPIO_PIN_0
#define MM_DEBUG_0_GPIO_Port GPIOD
#define MM_DEBUG_1_Pin LL_GPIO_PIN_1
#define MM_DEBUG_1_GPIO_Port GPIOD
#define MMAGIC_DATALINK_SPI_MISO_Pin LL_GPIO_PIN_3
#define MMAGIC_DATALINK_SPI_MISO_GPIO_Port GPIOD
#define MMAGIC_DATALINK_SPI_MOSI_Pin LL_GPIO_PIN_4
#define MMAGIC_DATALINK_SPI_MOSI_GPIO_Port GPIOD
#define GPIO_LED_BLUE_Pin LL_GPIO_PIN_7
#define GPIO_LED_BLUE_GPIO_Port GPIOB
#define MMAGIC_DATALINK_SPI_CS_Pin LL_GPIO_PIN_9
#define MMAGIC_DATALINK_SPI_CS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
