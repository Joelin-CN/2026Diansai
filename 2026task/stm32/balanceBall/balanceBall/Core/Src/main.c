/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "balance_motor.h"
#include "emm_v5_uart.h"
#include "debug_cli.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EMM_V5_RESPONSE_TIMEOUT_MS 100U  /* Emm V5 typical response < 50ms at 115200 baud */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
EmmV5Uart g_emm_uart;
BalanceMotor g_balance_motor;
DebugCli g_debug_cli;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  extern DMA_HandleTypeDef hdma_usart3_rx;
  
  const BalanceMotorConfig motor_config = {
    .address = 0x01U,
    .pulses_per_position_unit = 1.0f,
    .max_consecutive_failures = 3U,
  };
  const BalanceMotorTransport motor_transport = {
    .send = emm_v5_uart_send,
    .context = &g_emm_uart,
  };

  emm_v5_uart_init(&g_emm_uart, &huart2, EMM_V5_RESPONSE_TIMEOUT_MS);
  balance_motor_init(&g_balance_motor, &motor_config, motor_transport);
  
  debug_cli_init(&g_debug_cli, &huart3);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);
  __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT); /* disable half-transfer, not needed */
  
  /* === 简单测试序列：使能 -> 移动1000脉冲 === */
  HAL_Delay(500); // 等待初始化稳定
  
  // 1. 使能电机
  uint8_t enable_frame[EMM_V5_MAX_FRAME_SIZE];
  EmmV5Frame enable = {enable_frame, sizeof(enable_frame), 0};
  emm_v5_encode_enable(motor_config.address, true, false, &enable);
  emm_v5_uart_send(&g_emm_uart, enable.data, enable.length, 0xF3U, 3U);
  HAL_Delay(200); // 等待使能完成
  
  // 2. 直接发送位置命令：1000脉冲，100rpm，加速度10
  EmmV5PositionCommand pos_cmd = {
    .direction = EMM_V5_DIRECTION_CW,
    .speed_rpm = 100,
    .acceleration = 10,
    .pulse_count = 1000,
    .absolute = true,
    .synchronized = false
  };
  uint8_t pos_frame[EMM_V5_MAX_FRAME_SIZE];
  EmmV5Frame pos = {pos_frame, sizeof(pos_frame), 0};
  emm_v5_encode_position(motor_config.address, &pos_cmd, &pos);
  emm_v5_uart_send(&g_emm_uart, pos.data, pos.length, 0xFDU, 3U);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    EmmV5UartResult uart_result;

    emm_v5_uart_poll(&g_emm_uart, HAL_GetTick());
    if (emm_v5_uart_take_result(&g_emm_uart, &uart_result))
    {
      if (uart_result.state == EMM_V5_UART_COMPLETE)
      {
        balance_motor_on_response(&g_balance_motor,
                                  uart_result.expected_function,
                                  uart_result.response,
                                  uart_result.response_length);
      }
      else
      {
        balance_motor_on_transport_error(&g_balance_motor);
      }
    }
    balance_motor_process(&g_balance_motor);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    emm_v5_uart_on_tx_complete(&g_emm_uart);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart->Instance == USART2)
  {
    emm_v5_uart_on_rx_event(&g_emm_uart, size);
  }
  else if (huart->Instance == USART3)
  {
    extern DMA_HandleTypeDef hdma_usart3_rx;
    debug_cli_process_line(&g_debug_cli, (const char *)g_debug_cli.rx_buf, size);
    /* Restart DMA reception for the next command */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);
    __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    emm_v5_uart_on_error(&g_emm_uart);
  }
  else if (huart->Instance == USART3)
  {
    extern DMA_HandleTypeDef hdma_usart3_rx;
    /* Restart DMA reception after any UART3 error */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);
    __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
