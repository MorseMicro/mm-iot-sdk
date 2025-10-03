/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32wbxx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32wbxx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mmhal.h"
#include "mmosal.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
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
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
    MMOSAL_LOG_FAILURE_INFO(FAILURE_NMI);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  const uint32_t *fault_stack;
  if ((uintptr_t)(MMPORT_GET_LR()) & 0x04)
  {
    fault_stack = (const uint32_t *)__get_PSP();
  }
  else
  {
    fault_stack = (const uint32_t *)__get_MSP();
  }

  uint32_t fault_pc = fault_stack[6];
  (void)fault_pc; /* Protect against unused variable warnings */

  MMOSAL_LOG_FAILURE_INFO(FAILURE_HARDFAULT, SCB->HFSR, SCB->CFSR, fault_pc);
#ifdef HALT_ON_ASSERT
  while (1)
  {
    MMPORT_BREAKPOINT();
  }
#else
  mmhal_reset();
#endif
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
    MMOSAL_LOG_FAILURE_INFO(FAILURE_MEMMANAGE, SCB->CFSR, SCB->MMFAR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
    MMOSAL_LOG_FAILURE_INFO(FAILURE_BUSFAULT, SCB->CFSR, SCB->BFAR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
    MMOSAL_LOG_FAILURE_INFO(FAILURE_USAGEFAULT, SCB->CFSR);
#ifdef HALT_ON_ASSERT
    while (1)
    {
        MMPORT_BREAKPOINT();
    }
#else
    mmhal_reset();
#endif
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32WBxx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32wbxx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
