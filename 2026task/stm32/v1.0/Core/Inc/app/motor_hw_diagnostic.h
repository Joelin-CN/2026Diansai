/**
 * @file motor_hw_diagnostic.h
 * @brief Motor hardware diagnostic tool
 * @date 2026-07-29
 *
 * Diagnoses TB6612 motor driver issues:
 * - PWM signal generation
 * - Direction control pins
 * - STBY enable pin
 * - TIM1 configuration
 */

#ifndef MOTOR_HW_DIAGNOSTIC_H
#define MOTOR_HW_DIAGNOSTIC_H

#include <stdint.h>

/**
 * @brief Run motor hardware diagnostic
 * Prints detailed hardware status to UART
 */
void MotorHW_Diagnostic(void);

#endif // MOTOR_HW_DIAGNOSTIC_H
