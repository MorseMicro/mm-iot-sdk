/**
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

#include "bsp.h"
#include "stm32f4xx_hal_tim.h"

TIM_HandleTypeDef        htim1;

/**
  * @brief  This function configures the TIM1 as a time base source.
  *         The time source is configured  to have 1ms time base with a dedicated
  *         Tick interrupt priority.
  * @note   This function is called  automatically at the beginning of program after
  *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
  * @param  TickPriority: Tick interrupt priority.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef    clkconfig;
    uint32_t              uwTimclock = 0;
    uint32_t              uwPrescalerValue = 0;
    uint32_t              pFLatency;
    /*Configure the TIM1 IRQ priority */
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn , TickPriority , 0);

    /* Enable the TIM1 global Interrupt */
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);

    /* Enable TIM1 clock */
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* Get clock configuration */
    HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

    /* Compute TIM1 clock */
    uwTimclock = 2*HAL_RCC_GetPCLK2Freq();
    /* Compute the prescaler value to have TIM1 counter clock equal to 1MHz */
    uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000U) - 1U);

    /* Initialize TIM1 */
    htim1.Instance = TIM1;

    /* Initialize TIMx peripheral as follow:
    + Period = [(TIM1CLK/1000) - 1]. to have a (1/1000) s time base.
    + Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
    + ClockDivision = 0
    + Counter direction = Up
    */
    htim1.Init.Period = (1000000U / 1000U) - 1U;
    htim1.Init.Prescaler = uwPrescalerValue;
    htim1.Init.ClockDivision = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&htim1) == HAL_OK)
    {
        /* Start the TIM time Base generation in interrupt mode */
        return HAL_TIM_Base_Start_IT(&htim1);
    }

    /* Return function status */
    return HAL_ERROR;
}

/**
  * @brief  Suspend Tick increment.
  * @note   Disable the tick increment by disabling TIM1 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_SuspendTick(void)
{
    /* Disable TIM1 update Interrupt */
    __HAL_TIM_DISABLE_IT(&htim1, TIM_IT_UPDATE);
}

/**
  * @brief  Resume Tick increment.
  * @note   Enable the tick increment by Enabling TIM1 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_ResumeTick(void)
{
    /* Enable TIM1 Update interrupt */
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_UPDATE);
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void bsp_clock_init(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);
    while (LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_5)
    {}
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_PWR_EnableOverDriveMode();
    LL_RCC_HSE_EnableBypass();
    LL_RCC_HSE_Enable();

    /* Wait till HSE is ready */
    while (LL_RCC_HSE_IsReady() != 1)
    {}
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_4, 180, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_4, 180, LL_RCC_PLLQ_DIV_8);
    LL_RCC_PLL_Enable();

    /* Wait till PLL is ready */
    while (LL_RCC_PLL_IsReady() != 1)
    {}
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

    /* Wait till System clock is ready */
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
    {}
    LL_SetSystemCoreClock(180000000);

    /* Enable LSI */
    LL_PWR_EnableBkUpAccess();
    LL_RCC_LSI_Enable();

    /* Wait till LSI is ready */
    while(LL_RCC_LSI_IsReady() != 1)
    {

    }

    /* Update the time base */
    if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
    {
        MMOSAL_ASSERT(false);
    }
    LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        HAL_IncTick();
    }
}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{
    /* Peripheral clock enable */
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_RNG);

    LL_RNG_Enable(RNG);
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
    /**USART3 GPIO Configuration
     PD8   ------> USART3_TX
     PD9   ------> USART3_RX
    */
    GPIO_InitStruct.Pin = DBG_RX_Pin|DBG_TX_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
    LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    USART_InitStruct.BaudRate = DBG_BAUD;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART3, &USART_InitStruct);
    LL_USART_ConfigAsyncMode(USART3);
    LL_USART_Enable(USART3);
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{
    LL_SPI_InitTypeDef SPI_InitStruct = {0};

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    /**SPI1 GPIO Configuration
     PA5   ------> SPI1_SCK
     PA6   ------> SPI1_MISO
     PA7   ------> SPI1_MOSI
    */
    GPIO_InitStruct.Pin = SPI_CLK_PIN|SPI_MISO_PIN|SPI_MOSI_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* SPI1 DMA Init */

    /* SPI1_RX Init */
    LL_DMA_SetChannelSelection(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, SPI_DMA_CHANNEL);
    LL_DMA_SetDataTransferDirection(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM,
                                    LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetStreamPriorityLevel(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);

    /* SPI1_TX Init */
    LL_DMA_SetChannelSelection(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, SPI_DMA_CHANNEL);
    LL_DMA_SetDataTransferDirection(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM,
                                    LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetStreamPriorityLevel(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_PRIORITY_LOW);
    LL_DMA_SetMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_PERIPH_NOINCREMENT);
    LL_DMA_SetMemoryIncMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_MEMORY_INCREMENT);
    LL_DMA_SetPeriphSize(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_PDATAALIGN_BYTE);
    LL_DMA_SetMemorySize(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM, LL_DMA_MDATAALIGN_BYTE);
    LL_DMA_DisableFifoMode(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);

    /* SPI1 parameter configuration*/
    SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
    SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
    SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
    SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
    SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
    SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
    SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV2;
    SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
    SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
    SPI_InitStruct.CRCPoly = 10;
    LL_SPI_Init(SPI_PERIPH, &SPI_InitStruct);
    LL_SPI_SetStandard(SPI_PERIPH, LL_SPI_PROTOCOL_MOTOROLA);
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
    /* Init with LL driver */
    /* DMA controller clock enable */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);

    /* DMA interrupt init */
    LL_DMA_EnableIT_TC(SPI_DMA_PERIPH, SPI_RX_DMA_STREAM);
    LL_DMA_EnableIT_TC(SPI_DMA_PERIPH, SPI_TX_DMA_STREAM);
    /* DMA2_Stream0_IRQn interrupt configuration */
    NVIC_SetPriority(DMA2_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    /* DMA2_Stream3_IRQn interrupt configuration */
    NVIC_SetPriority(DMA2_Stream3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
    NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);

    /* Configure GPIO initial state */
    LL_GPIO_ResetOutputPin(WAKE_GPIO_Port, WAKE_Pin);
    LL_GPIO_SetOutputPin(RESET_N_GPIO_Port, RESET_N_Pin);
    LL_GPIO_SetOutputPin(SPI_CS_GPIO_Port, SPI_CS_Pin);
    LL_GPIO_SetOutputPin(GPIOC, Data1_Pin|Data2_Pin);
    LL_GPIO_ResetOutputPin(GPIO_LED_RED_GPIO_Port, GPIO_LED_RED_Pin);
    LL_GPIO_ResetOutputPin(GPIO_LED_GREEN_GPIO_Port, GPIO_LED_GREEN_Pin);
    LL_GPIO_ResetOutputPin(GPIO_LED_BLUE_GPIO_Port, GPIO_LED_BLUE_Pin);

    /**/
    GPIO_InitStruct.Pin = RESET_N_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(RESET_N_GPIO_Port, &GPIO_InitStruct);

    /**/
    GPIO_InitStruct.Pin = SPI_CS_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(SPI_CS_GPIO_Port, &GPIO_InitStruct);

    /**/
    GPIO_InitStruct.Pin = WAKE_Pin|Data1_Pin|Data2_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* LED1 */
    GPIO_InitStruct.Pin = GPIO_LED_RED_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIO_LED_RED_GPIO_Port, &GPIO_InitStruct);

    /* LED2 */
    GPIO_InitStruct.Pin = GPIO_LED_GREEN_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIO_LED_GREEN_GPIO_Port, &GPIO_InitStruct);

    /* LED3 */
    GPIO_InitStruct.Pin = GPIO_LED_BLUE_Pin;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIO_LED_BLUE_GPIO_Port, &GPIO_InitStruct);

    /* busy IRQ */

    /* Set Interrupt Source */
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTC, LL_SYSCFG_EXTI_LINE3);
    /* Initialise External Interrupt */
    EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_3;
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_RISING;
    LL_EXTI_Init(&EXTI_InitStruct);
    /* Configure GPIO, must be pull-down */
    LL_GPIO_SetPinPull(BUSY_GPIO_Port, BUSY_Pin, LL_GPIO_PULL_DOWN);
    LL_GPIO_SetPinMode(BUSY_GPIO_Port, BUSY_Pin, LL_GPIO_MODE_INPUT);
    /* Configure NVIC */
    NVIC_SetPriority(BUSY_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_DisableIRQ(BUSY_IRQn);

    /* SPI IRQ Config */

    /* Set Interrupt Source */
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTF, LL_SYSCFG_EXTI_LINE5);
    /* Initialise External Interrupt */
    EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_5;
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;
    LL_EXTI_Init(&EXTI_InitStruct);
    /* Configure GPIO */
    LL_GPIO_SetPinPull(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(SPI_IRQ_GPIO_Port, SPI_IRQ_Pin, LL_GPIO_MODE_INPUT);
    /* Configure NVIC */
    NVIC_SetPriority(SPI_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    NVIC_DisableIRQ(SPI_IRQn);
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
    LL_RTC_InitTypeDef RTC_InitStruct = {0};
    LL_RTC_TimeTypeDef RTC_TimeStruct = {0};
    LL_RTC_DateTypeDef RTC_DateStruct = {0};

    if(LL_RCC_GetRTCClockSource() != LL_RCC_RTC_CLKSOURCE_LSE)
    {
        FlagStatus pwrclkchanged = RESET;
        /* Update LSE configuration in Backup Domain control register */
        /* Requires to enable write access to Backup Domain if necessary */
        if (LL_APB1_GRP1_IsEnabledClock (LL_APB1_GRP1_PERIPH_PWR) != 1U)
        {
            /* Enables the PWR Clock and Enables access to the backup domain */
            LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
            pwrclkchanged = SET;
        }
        if (LL_PWR_IsEnabledBkUpAccess () != 1U)
        {
            /* Enable write access to Backup domain */
            LL_PWR_EnableBkUpAccess();
            while (LL_PWR_IsEnabledBkUpAccess () == 0U)
            {
            }
        }
        LL_RCC_ForceBackupDomainReset();
        LL_RCC_ReleaseBackupDomainReset();
        LL_RCC_LSE_Enable();

        /* Wait till LSE is ready */
        while(LL_RCC_LSE_IsReady() != 1)
        {
        }

        LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
        /* Restore clock configuration if changed */
        if (pwrclkchanged == SET)
        {
            LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_PWR);
        }
    }

    /* Peripheral clock enable */
    LL_RCC_EnableRTC();

    /* Initialize RTC */
    RTC_InitStruct.HourFormat = LL_RTC_HOURFORMAT_24HOUR;
    RTC_InitStruct.AsynchPrescaler = 127;
    RTC_InitStruct.SynchPrescaler = 255;
    LL_RTC_Init(RTC, &RTC_InitStruct);
    LL_RTC_SetAsynchPrescaler(RTC, 127);
    LL_RTC_SetSynchPrescaler(RTC, 255);

    /* Reset the Time and Date if battery failed */
    if(LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0) != 0x32F2)
    {
        RTC_TimeStruct.Hours = 0;
        RTC_TimeStruct.Minutes = 0;
        RTC_TimeStruct.Seconds = 0;
        LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BCD, &RTC_TimeStruct);

        RTC_DateStruct.WeekDay = 5;
        RTC_DateStruct.Month = 0;
        RTC_DateStruct.Day = 0x1;
        RTC_DateStruct.Year = 0;
        LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BCD, &RTC_DateStruct);

        /* Time has been set */
        LL_RTC_BAK_SetRegister(RTC,LL_RTC_BKP_DR0,0x32F2);
    }
}

void bsp_peripherals_init(void)
{
    MX_USART3_UART_Init();
    MX_DMA_Init();
    MX_SPI1_Init();
    MX_GPIO_Init();
    MX_RNG_Init();
    MX_RTC_Init();
}

/** Appliation entry point function. */
void app_init(void);

/**
 * Main entry point. This simply initialises the operating system abstraction layer (which
 * implicitly initalises the hardware abstaction layer.
 *
 * @returns 0 on success else an error code.
 */
int main(void)
{
    return mmosal_main(app_init);
}
