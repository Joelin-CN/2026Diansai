/**
 * @file sensor_adapter.h
 * @brief 传感器适配器接口
 * @date 2026-07-29
 */

#ifndef SENSOR_ADAPTER_H
#define SENSOR_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface.h"

/**
 * @brief 获取传感器HAL接口
 * @return 传感器HAL接口指针
 */
sensor_hal_t *SensorAdapter_GetInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_ADAPTER_H */
