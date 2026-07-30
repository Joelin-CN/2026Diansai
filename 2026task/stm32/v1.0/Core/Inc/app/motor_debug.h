/**
 * @file motor_debug.h
 * @brief Motor and encoder debug tool
 * @date 2026-07-29
 *
 * Hardware test sequence:
 * 1. PWM output test (no movement expected)
 * 2. Motor rotation test (left/right, forward/backward)
 * 3. Encoder feedback test (count verification)
 */

#ifndef MOTOR_DEBUG_H
#define MOTOR_DEBUG_H

#include <stdint.h>

/**
 * @brief Initialize motor debug system
 * @return 0 on success, -1 on failure
 */
int MotorDebug_Init(void);

/**
 * @brief Run motor debug sequence
 * Call this in a loop (e.g., from FreeRTOS task)
 */
void MotorDebug_Run(void);

#endif // MOTOR_DEBUG_H
