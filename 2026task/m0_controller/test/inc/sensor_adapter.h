/**
 * @file sensor_adapter.h
 * @brief Sensor HAL adapter for Sens-Decision module
 * @date 2026-07-18
 * 
 * Provides the sensor_hal_t interface that bridges Sens-Decision to the
 * low-level hardware drivers (encoders, ICM42688, MCP23017).
 */

#ifndef SENSOR_ADAPTER_H
#define SENSOR_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../modules/Sens-Decision/inc/interface.h"

/**
 * @brief Get the sensor HAL interface for Sens-Decision
 * @return Pointer to static sensor_hal_t instance
 */
const sensor_hal_t *SensorAdapter_GetHal(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_ADAPTER_H */
