/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for adc.c.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */
/**
 * Read the TB6612 carrier board ADC output connected to PA1/ADC1_IN1.
 *
 * This function performs one blocking conversion. Call it from task context,
 * not from an interrupt. The returned value is 0..4095.
 */
HAL_StatusTypeDef TB6612_ADC_ReadRaw(uint16_t *raw);

/**
 * Convert a 12-bit raw value to the voltage present at the STM32 PA1 pin.
 * vdda_mv should be the measured/calibrated VDDA value in millivolts.
 *
 * This does not apply the unknown divider ratio on the TB6612 carrier board.
 */
uint32_t TB6612_ADC_RawToPinMillivolts(uint16_t raw, uint32_t vdda_mv);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
