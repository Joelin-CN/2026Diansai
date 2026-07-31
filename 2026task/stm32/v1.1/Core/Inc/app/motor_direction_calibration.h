/**
 * @file motor_direction_calibration.h
 * @brief Motor direction calibration tool
 */

#ifndef MOTOR_DIRECTION_CALIBRATION_H
#define MOTOR_DIRECTION_CALIBRATION_H

#include <stdint.h>

/**
 * @brief Run motor direction calibration
 * Tests each motor individually at low speed (20% PWM)
 * to verify correct rotation direction
 */
void MotorDirectionCalibration_Run(void);

#endif // MOTOR_DIRECTION_CALIBRATION_H
