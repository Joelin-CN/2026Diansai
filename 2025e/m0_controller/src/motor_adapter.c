/**
 * @file motor_adapter.c
 * @brief Motor adapter for Motion Control - implementation
 * @date 2026-07-18
 */

#include "motor_adapter.h"
#include "motor.h"

/**
 * @brief Motor mapping:
 * - Left side: M1, M2
 * - Right side: M3, M4
 * 
 * Motor_SetSpeed(left, right) already handles this fan-out internally,
 * so we can directly forward the differential PWM commands.
 */

/* Static interface instance */
static MotorInterface_t g_motor_interface = {
    .setDifferentialPWM = Motor_SetSpeed,
    .stop = Motor_Stop,
    .init = Motor_Init
};

MotorInterface_t *MotorAdapter_GetInterface(void)
{
    return &g_motor_interface;
}
