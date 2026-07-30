/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "track_control_app.h"
#include "usart.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TARGET_LAPS  3U   /* 比赛圈数，可按需修改 (1-5) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 768 * 4,  /* Increased from 512*4 (2048) to 768*4 (3072) bytes for EKF safety */
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  /* 使用操场路径 (Track Path) 而不是方形路径 (Square Path) */
  if (!TrackControlApp_Init(TARGET_LAPS)) {
      printf("[FATAL] TrackControlApp_Init failed — motors halted\r\n");
      for (;;) { osDelay(1000); }
  }

  /* Defensive: TrackControlApp_Init issues multiple printf calls after IR init;
   * HAL_UART_Transmit (UART5/printf) can momentarily clear RXNEIE on USART2.
   * Re-arm the interrupt now that all init is complete. */
  SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

  printf("[TrackControlApp] Running — target laps: %u\r\n", (unsigned)TARGET_LAPS);

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
  /* Report initial stack watermark for monitoring */
  UBaseType_t stack_watermark = uxTaskGetStackHighWaterMark(NULL);
  printf("[RTOS] defaultTask stack size: 3072 bytes, high-water mark: %u words (%u bytes free)\r\n",
         (unsigned)stack_watermark, (unsigned)(stack_watermark * 4));
#endif

  /* 500 Hz control loop (2 ms tick, assuming 1 kHz FreeRTOS tick rate) */
  uint32_t loop_counter = 0;
  for (;;)
  {
      TrackControlApp_RunFastCycle();
      osDelay(2);

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
      /* Report stack usage every 10 seconds (5000 loops at 500Hz) */
      if (++loop_counter >= 5000U) {
          loop_counter = 0;
          stack_watermark = uxTaskGetStackHighWaterMark(NULL);
          printf("[RTOS] Stack high-water mark: %u words (%u bytes free, %u%% used)\r\n",
                 (unsigned)stack_watermark,
                 (unsigned)(stack_watermark * 4),
                 (unsigned)(100 - (stack_watermark * 400 / 3072)));
      }
#endif
  }

  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
