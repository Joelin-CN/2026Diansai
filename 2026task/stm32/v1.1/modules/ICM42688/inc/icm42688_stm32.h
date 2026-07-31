/**
 * @file icm42688_stm32.h
 * @brief ICM42688 STM32平台适配器接口
 * @date 2026-07-29
 */

#ifndef ICM42688_STM32_H
#define ICM42688_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

#include "icm42688_hal.h"

/**
 * @brief 绑定STM32平台接口到ICM42688驱动
 * @param config ICM42688配置
 */
void icm42688_stm32_bind(const icm42688_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* ICM42688_STM32_H */
