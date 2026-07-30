/**
 * @file motor_adapter.h
 * @brief Motor adapter for Motion Control module
 * @date 2026-07-18
 * 
 * Provides the MotorInterface_t vtable that bridges Motion Control's
 * differential PWM commands to the hardware motor driver.
 */

#ifndef MOTOR_ADAPTER_H
#define MOTOR_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../modules/Motion Control/inc/motion_control.h"

/**
 * @brief Get the motor interface for Motion Control
 * @return Pointer to static MotorInterface_t instance
 */
MotorInterface_t *MotorAdapter_GetInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_ADAPTER_H */
