/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   ADC configuration for the TB6612 carrier board ADC output.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "adc.h"

ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adcHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (adcHandle->Instance == ADC1)
  {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /**
      * ADC1 GPIO Configuration
      * PA1 ------> ADC1_IN1 (TB6612 carrier ADC)
      */
    GPIO_InitStruct.Pin = TB6612_ADC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TB6612_ADC_GPIO_Port, &GPIO_InitStruct);
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adcHandle)
{
  if (adcHandle->Instance == ADC1)
  {
    __HAL_RCC_ADC1_CLK_DISABLE();
    HAL_GPIO_DeInit(TB6612_ADC_GPIO_Port, TB6612_ADC_Pin);
  }
}

/* USER CODE BEGIN 1 */

HAL_StatusTypeDef TB6612_ADC_ReadRaw(uint16_t *raw)
{
  HAL_StatusTypeDef status;

  if (raw == NULL)
  {
    return HAL_ERROR;
  }

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK)
  {
    return status;
  }

  status = HAL_ADC_PollForConversion(&hadc1, 10U);
  if (status == HAL_OK)
  {
    *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  (void)HAL_ADC_Stop(&hadc1);
  return status;
}

uint32_t TB6612_ADC_RawToPinMillivolts(uint16_t raw, uint32_t vdda_mv)
{
  if (raw > 4095U)
  {
    raw = 4095U;
  }

  return (((uint32_t)raw * vdda_mv) + 2047U) / 4095U;
}

/* USER CODE END 1 */
