/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    App/hrs_app.c
  * @author  MCD Application Team
  * @brief   Heart Rate Service Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_common.h"

#include "ble.h"
#include "hrs_app.h"
#include "mmosal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  HRS_BodySensorLocation_t BodySensorLocationChar;
  HRS_MeasVal_t MeasurementvalueChar;
  uint8_t ResetEnergyExpended;
  uint8_t TimerMeasurement_Id;

} HRSAPP_Context_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
#define HRSAPP_MEASUREMENT_INTERVAL   (1000000/CFG_TS_TICK_VAL)  /**< 1s */
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/**
 * START of Section BLE_APP_CONTEXT
 */

PLACE_IN_SECTION("BLE_APP_CONTEXT") static HRSAPP_Context_t HRSAPP_Context;

/**
 * END of Section BLE_APP_CONTEXT
 */

/* USER CODE BEGIN PV */

/* USER CODE END PV */
static struct mmosal_task *hrs_task_handle;

/* Private functions prototypes-----------------------------------------------*/
static void HrMeas( void );
static void HRSAPP_Measurement(void);
static uint32_t HRSAPP_Read_RTC_SSR_SS ( void );
static void hrs_app_task(void *arg);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void HRS_Notification(HRS_App_Notification_evt_t *pNotification)
{
/* USER CODE BEGIN HRS_Notification_1 */

/* USER CODE END HRS_Notification_1 */
  switch(pNotification->HRS_Evt_Opcode)
  {
/* USER CODE BEGIN HRS_Notification_HRS_Evt_Opcode */

/* USER CODE END HRS_Notification_HRS_Evt_Opcode */
#if (BLE_CFG_HRS_ENERGY_EXPENDED_INFO_FLAG != 0)
    case HRS_RESET_ENERGY_EXPENDED_EVT:
/* USER CODE BEGIN HRS_RESET_ENERGY_EXPENDED_EVT */
    HRSAPP_Context.MeasurementvalueChar.EnergyExpended = 0;
    HRSAPP_Context.ResetEnergyExpended = 1;
/* USER CODE END HRS_RESET_ENERGY_EXPENDED_EVT */
      break;
#endif

    case HRS_NOTIFICATION_ENABLED:
/* USER CODE BEGIN HRS_NOTIFICATION_ENABLED */
      /**
       * It could be the enable notification is received twice without the disable notification in between
       */
      HW_TS_Stop(HRSAPP_Context.TimerMeasurement_Id);
      HW_TS_Start(HRSAPP_Context.TimerMeasurement_Id, HRSAPP_MEASUREMENT_INTERVAL);
/* USER CODE END HRS_NOTIFICATION_ENABLED */
      break;

    case HRS_NOTIFICATION_DISABLED:
/* USER CODE BEGIN HRS_NOTIFICATION_DISABLED */
    HW_TS_Stop(HRSAPP_Context.TimerMeasurement_Id);
/* USER CODE END HRS_NOTIFICATION_DISABLED */
      break;

#if (BLE_CFG_OTA_REBOOT_CHAR != 0)
    case HRS_STM_BOOT_REQUEST_EVT:
/* USER CODE BEGIN HRS_STM_BOOT_REQUEST_EVT */
    *(uint32_t*)SRAM1_BASE = *(uint32_t*)pNotification->DataTransfered.pPayload;
    NVIC_SystemReset();
/* USER CODE END HRS_STM_BOOT_REQUEST_EVT */
      break;
#endif

   default:
/* USER CODE BEGIN HRS_Notification_Default */

/* USER CODE END HRS_Notification_Default */
      break;
  }
/* USER CODE BEGIN HRS_Notification_2 */

/* USER CODE END HRS_Notification_2 */
  return;
}

void HRSAPP_Init(void)
{
  hrs_task_handle = mmosal_task_create(hrs_app_task, 0, MMOSAL_TASK_PRI_LOW,
                                       CFG_HRS_PROCESS_STACK_SIZE,
                                       CFG_HRS_PROCESS_NAME);
  MMOSAL_ASSERT(hrs_task_handle != NULL);
/* USER CODE BEGIN HRSAPP_Init */
/**
   * Set Body Sensor Location
   */
  HRSAPP_Context.ResetEnergyExpended = 0;
  HRSAPP_Context.BodySensorLocationChar = HRS_BODY_SENSOR_LOCATION_HAND;
  HRS_UpdateChar(SENSOR_LOCATION_UUID, (uint8_t *)&HRSAPP_Context.BodySensorLocationChar);


  /**
   * Set Flags for measurement value
   */

  HRSAPP_Context.MeasurementvalueChar.Flags = ( HRS_HRM_VALUE_FORMAT_UINT16      |
                                                  HRS_HRM_SENSOR_CONTACTS_PRESENT   |
                                                  HRS_HRM_SENSOR_CONTACTS_SUPPORTED |
                                                  HRS_HRM_ENERGY_EXPENDED_PRESENT  |
                                                  HRS_HRM_RR_INTERVAL_PRESENT );

#if (BLE_CFG_HRS_ENERGY_EXPENDED_INFO_FLAG != 0)
  if(HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_ENERGY_EXPENDED_PRESENT)
    HRSAPP_Context.MeasurementvalueChar.EnergyExpended = 10;
#endif

#if (BLE_CFG_HRS_ENERGY_RR_INTERVAL_FLAG != 0)
  if(HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_RR_INTERVAL_PRESENT)
  {
    uint8_t i;

    HRSAPP_Context.MeasurementvalueChar.NbreOfValidRRIntervalValues = BLE_CFG_HRS_ENERGY_RR_INTERVAL_FLAG;
    for(i = 0; i < BLE_CFG_HRS_ENERGY_RR_INTERVAL_FLAG; i++)
      HRSAPP_Context.MeasurementvalueChar.aRRIntervalValues[i] = 1024;
  }
#endif

  /**
   * Create timer for Heart Rate Measurement
   */
  HW_TS_Create(CFG_TIM_PROC_ID_ISR, &(HRSAPP_Context.TimerMeasurement_Id), hw_ts_Repeated, HrMeas);

/* USER CODE END HRSAPP_Init */
  return;
}

static void hrs_app_task(void *arg)
{
    UNUSED(arg);

    for (;;)
    {
        mmosal_task_wait_for_notification(UINT32_MAX);
        HRSAPP_Measurement( );
    }
}

static void HRSAPP_Measurement(void)
{
/* USER CODE BEGIN HRSAPP_Measurement */
uint32_t measurement;

  measurement = ((HRSAPP_Read_RTC_SSR_SS()) & 0x07) + 65;

  HRSAPP_Context.MeasurementvalueChar.MeasurementValue = measurement;
#if (BLE_CFG_HRS_ENERGY_EXPENDED_INFO_FLAG != 0)
  if((HRSAPP_Context.MeasurementvalueChar.Flags & HRS_HRM_ENERGY_EXPENDED_PRESENT) &&
     (HRSAPP_Context.ResetEnergyExpended == 0))
    HRSAPP_Context.MeasurementvalueChar.EnergyExpended += 5;
  else if(HRSAPP_Context.ResetEnergyExpended == 1)
    HRSAPP_Context.ResetEnergyExpended = 0;
#endif

  HRS_UpdateChar(HEART_RATE_MEASURMENT_UUID, (uint8_t *)&HRSAPP_Context.MeasurementvalueChar);
/* USER CODE END HRSAPP_Measurement */
  return;
}

static void HrMeas( void )
{
  /**
   * The code shall be executed in the background as aci command may be sent
   * The background is the only place where the application can make sure a new aci command
   * is not sent if there is a pending one
   */
  mmosal_task_notify_from_isr(hrs_task_handle);
/* USER CODE BEGIN HrMeas */

/* USER CODE END HrMeas */

  return;
}

static uint32_t HRSAPP_Read_RTC_SSR_SS ( void )
{
  return ((uint32_t)(READ_BIT(RTC->SSR, RTC_SSR_SS)));
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */
