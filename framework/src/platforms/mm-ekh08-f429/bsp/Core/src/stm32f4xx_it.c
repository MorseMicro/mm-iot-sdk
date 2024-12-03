/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics
  *                    Copyright 2022 Morse Micro.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "stm32f4xx_it.h"
#include "mmhal.h"
#include "mmosal.h"

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim1;


/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/

/** Enumeration of platform-specific failure codes. We put this in the first word
 *  of the platform-specific failure data. */
enum platform_failure_codes
{
    FAILURE_NMI = 1,
    FAILURE_HARDFAULT,
    FAILURE_MEMMANAGE,
    FAILURE_BUSFAULT,
    FAILURE_USAGEFAULT,
};

/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
    MMOSAL_LOG_FAILURE_INFO(FAILURE_NMI);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
    MMOSAL_LOG_FAILURE_INFO(FAILURE_HARDFAULT, SCB->HFSR, SCB->CFSR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
    MMOSAL_LOG_FAILURE_INFO(FAILURE_MEMMANAGE, SCB->CFSR, SCB->MMFAR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
    MMOSAL_LOG_FAILURE_INFO(FAILURE_BUSFAULT, SCB->CFSR, SCB->BFAR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
    MMOSAL_LOG_FAILURE_INFO(FAILURE_USAGEFAULT, SCB->CFSR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM1 update interrupt and TIM10 global interrupt.
  */
void TIM1_UP_TIM10_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}

/**
  * @brief This function handles DMA2 stream3 global interrupt.
  */
void DMA2_Stream3_IRQHandler(void)
{
    LL_DMA_ClearFlag_TC3(DMA2);
}

