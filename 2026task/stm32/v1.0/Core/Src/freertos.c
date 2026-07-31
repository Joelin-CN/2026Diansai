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
#include "ir_calibration.h"
#include "ir_uart_sensor.h"
#include "motor.h"
#include "encoder.h"
#include "config.h"
#include "platform_time.h"
#include "motor_static_friction_test.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ============================================================================
 * 测试模式选择开关
 * ============================================================================
 *
 * 取消注释以下其中一个宏来选择运行模式：
 *
 * 1. TEST_MODE_IR_CALIBRATION        - IR传感器校准模式
 *    功能：白平衡校准 → 黑线阈值校准 → 实时监控验证
 *    适用场景：首次测试、光照变化后、更换传感器/赛道
 *    操作步骤：见下方详细说明
 *
 * 2. TEST_MODE_TRACK_CONTROL         - 完整循迹控制模式（Pure Pursuit）
 *    功能：EKF状态估计 + 轨迹规划 + Pure Pursuit循迹
 *    前提条件：IR传感器已校准、PID参数已调优
 *
 * 3. TEST_MODE_PLAYGROUND_TRACK      - 操场型循迹（第2/4题比赛模式）
 *    功能：分段自适应循迹，任务2（绕圈）或任务4（A→B直道）
 *    前提条件：IR传感器已校准
 *
 * 4. TEST_MODE_STATIC_FRICTION       - 静摩擦参数标定模式
 *    功能：通过串口手动发送PWM值，标定FF_K_STATIC参数
 *    适用场景：标定前馈控制的静摩擦补偿参数
 *
 * 注意：同时只能定义一个测试模式
 * ============================================================================ */

// #define TEST_MODE_IR_CALIBRATION        /* ← IR校准模式 */
// #define TEST_MODE_TRACK_CONTROL         /* ← Pure Pursuit循迹模式 */
// #define TEST_MODE_PLAYGROUND_TRACK      /* ← 操场型循迹（第2/4题） */
#define TEST_MODE_STATIC_FRICTION          /* ← 当前激活：静摩擦标定 */

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
  *
  * @note   测试模式说明：
  *         - TEST_MODE_IR_CALIBRATION: IR传感器校准（不驱动电机）
  *         - TEST_MODE_TRACK_CONTROL:  完整循迹控制
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

#ifdef TEST_MODE_IR_CALIBRATION
  /* ========================================================================
   * IR传感器一步校准模式
   * ======================================================================== */
  printf("\r\n=== IR One-Step Calibration v1.2.2 ===\r\n");
  osDelay(100);

  /* 步骤1: 初始化必要硬件（不启动电机） */
  printf("[STEP 1/4] Init hardware...\r\n");
  osDelay(50);
  Motor_Init();
  Motor_Stop();  /* 确保电机停止 */
  Encoder_Init();
  PlatformTime_Init();

  printf("[INFO] Init IR sensor...\r\n");
  osDelay(50);
  IrUartSensor_Init();
  IrUartSensor_RequestAnalogMode();

  printf("[INFO] Warm-up 2 sec...\r\n");
  osDelay(50);
  osDelay(2000);

  /* 加载默认配置 */
  sd_config_reset_defaults();

  printf("[OK] Hardware ready\r\n");
  osDelay(100);

  /* 步骤2: 一步校准 */
  printf("\r\n[STEP 2/4] Calibration\r\n");
  osDelay(50);
  printf("Place robot: Ch3&4 on BLACK, others on WHITE\r\n");
  osDelay(50);
  printf("Layout: [0][1][2][3][4][5][6][7]\r\n");
  osDelay(50);
  printf("        W  W  W  B  B  W  W  W\r\n");
  osDelay(50);

  printf("Starting in 5 sec...\r\n");
  osDelay(1000);
  printf("4...\r\n");
  osDelay(1000);
  printf("3...\r\n");
  osDelay(1000);
  printf("2...\r\n");
  osDelay(1000);
  printf("1...\r\n");
  osDelay(1000);

  IrCalibration_OneStep();

  printf("Wait 3 sec...\r\n");
  osDelay(3000);

  /* 步骤3: 显示校准结果 */
  printf("\r\n[STEP 3/4] Results\r\n");
  osDelay(100);

  IrCalibration_PrintConfig();

  printf("Wait 3 sec...\r\n");
  osDelay(3000);

  /* 步骤4: 实时监控验证 */
  printf("\r\n[STEP 4/4] Monitoring (60 sec)\r\n");
  osDelay(50);
  printf("Move robot left/right to check lateral_error sign\r\n");
  osDelay(50);
  printf("Right -> positive, Left -> negative\r\n");
  osDelay(50);

  IrCalibration_Monitor(60000, 1000);  /* 60秒监控，1秒间隔 */

  /* 完成 */
  printf("\r\n=== CALIBRATION COMPLETE ===\r\n");
  osDelay(100);
  printf("Next: Switch to TEST_MODE_TRACK_CONTROL\r\n");
  osDelay(100);

  printf("Restarting in 10 sec...\r\n");
  osDelay(10000);

  /* 无限循环：持续监控模式 */
  printf("Continuous monitoring...\r\n");
  osDelay(100);

  for (;;) {
      IrCalibration_Monitor(30000, 1000);  /* 每30秒一轮，1秒间隔 */
      osDelay(1000);
  }

#elif defined(TEST_MODE_TRACK_CONTROL)
  /* ========================================================================
   * 完整循迹控制模式
   * ======================================================================== */
  printf("\r\n");
  printf("╔════════════════════════════════════════════════════════════════╗\r\n");
  printf("║       Track Control Mode - STM32 Track Robot v1.2.1           ║\r\n");
  printf("╚════════════════════════════════════════════════════════════════╝\r\n");
  printf("\r\n");

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

#elif defined(TEST_MODE_PLAYGROUND_TRACK)
  /* ========================================================================
   * 操场型循迹模式（第2/4题比赛）
   * ======================================================================== */
  #include "playground_track.h"

  printf("\r\n");
  printf("╔════════════════════════════════════════════════════════════════╗\r\n");
  printf("║     Playground Track Mode - STM32 Track Robot v1.4.0          ║\r\n");
  printf("╚════════════════════════════════════════════════════════════════╝\r\n");
  printf("\r\n");

  /* Initialize playground track control
   * Change PLAYGROUND_TASK_LAP to PLAYGROUND_TASK_AB_STRAIGHT for Task 4 */
  if (!PlaygroundTrack_Init(PLAYGROUND_TASK_LAP)) {
      printf("[FATAL] PlaygroundTrack_Init failed — motors halted\r\n");
      for (;;) { osDelay(1000); }
  }

  /* Re-arm USART2 RX interrupt after all printf calls */
  SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

  printf("[PlaygroundTrack] Running Task 2 (Full Lap) mode\r\n");
  printf("[PlaygroundTrack] To switch to Task 4: change to PLAYGROUND_TASK_AB_STRAIGHT\r\n");

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
  UBaseType_t stack_watermark = uxTaskGetStackHighWaterMark(NULL);
  printf("[RTOS] Stack size: 3072 bytes, watermark: %u words (%u bytes free)\r\n",
         (unsigned)stack_watermark, (unsigned)(stack_watermark * 4));
#endif

  /* 500 Hz control loop (2 ms tick) */
  uint32_t loop_counter = 0;
  for (;;)
  {
      PlaygroundTrack_RunFastCycle();
      osDelay(2);

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
      /* Report stack usage every 10 seconds (5000 loops at 500Hz) */
      if (++loop_counter >= 5000U) {
          loop_counter = 0;
          stack_watermark = uxTaskGetStackHighWaterMark(NULL);
          printf("[RTOS] Stack watermark: %u words (%u bytes free, %u%% used)\r\n",
                 (unsigned)stack_watermark,
                 (unsigned)(stack_watermark * 4),
                 (unsigned)(100 - (stack_watermark * 400 / 3072)));
      }
#endif

      /* Check for task completion */
      if (PlaygroundTrack_IsComplete()) {
          printf("[PlaygroundTrack] Task complete! Distance: %.3f m\r\n",
                 PlaygroundTrack_GetDistance());
          for (;;) { osDelay(1000); }
      }
  }

#elif defined(TEST_MODE_STATIC_FRICTION)
  /* ========================================================================
   * 静摩擦补偿参数标定模式
   * ======================================================================== */
  printf("\r\n");
  printf("╔════════════════════════════════════════════════════════════════╗\r\n");
  printf("║   Static Friction Calibration - FF_K_STATIC Test Mode         ║\r\n");
  printf("╚════════════════════════════════════════════════════════════════╝\r\n");
  printf("\r\n");

  /* 初始化硬件 */
  printf("[Init] Initializing hardware...\r\n");
  Motor_Init();
  Encoder_Init();
  printf("[OK] Hardware ready\r\n");
  printf("\r\n");

  /* 进入交互式测试模式（永不返回） */
  Motor_StaticFriction_InteractiveTest();

#else
  /* ========================================================================
   * 错误：未定义测试模式
   * ======================================================================== */
  #error "Please define one of: TEST_MODE_IR_CALIBRATION, TEST_MODE_TRACK_CONTROL, TEST_MODE_PLAYGROUND_TRACK, or TEST_MODE_STATIC_FRICTION in freertos.c"
#endif

  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
