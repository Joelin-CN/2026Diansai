/**
 * @file motor_hw_diagnostic.c
 * @brief Motor hardware diagnostic tool implementation
 * @date 2026-07-29
 */

#include "motor_hw_diagnostic.h"
#include "tim.h"
#include "gpio.h"
#include "main.h"
#include <stdio.h>

void MotorHW_Diagnostic(void) {
    printf("\n");
    printf("========================================\n");
    printf("  TB6612 Motor Driver Diagnostic\n");
    printf("========================================\n");
    printf("\n");

    // Check TIM1 configuration
    printf("[TIM1 Configuration]\n");
    printf("  Prescaler: %u\n", htim1.Init.Prescaler);
    printf("  Period (ARR): %u\n", htim1.Init.Period);
    printf("  Counter Mode: %u\n", htim1.Init.CounterMode);
    printf("  Clock Division: %u\n", htim1.Init.ClockDivision);

    // Calculate expected PWM frequency
    // F_pwm = F_timer / ((PSC + 1) * (ARR + 1))
    // F_timer = 168 MHz (APB2 timer clock for STM32F407)
    uint32_t timer_freq = 168000000;  // 168 MHz
    uint32_t pwm_freq = timer_freq / ((htim1.Init.Prescaler + 1) * (htim1.Init.Period + 1));
    printf("  Calculated PWM Frequency: %lu Hz\n", pwm_freq);
    printf("  Expected: ~20000 Hz (20 kHz)\n");
    printf("\n");

    // Check PWM channels
    printf("[TIM1 PWM Channels]\n");
    printf("  CH1 (Left Motor):\n");
    printf("    CCR1: %u (duty cycle: %u%%)\n",
           __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1),
           __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1) * 100 / (htim1.Init.Period + 1));
    printf("  CH2 (Right Motor):\n");
    printf("    CCR2: %u (duty cycle: %u%%)\n",
           __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_2),
           __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_2) * 100 / (htim1.Init.Period + 1));
    printf("\n");

    // Check GPIO pins
    printf("[TB6612 Control Pins]\n");

    // STBY pin (must be HIGH to enable)
    GPIO_PinState stby = HAL_GPIO_ReadPin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin);
    printf("  STBY: %s ", stby == GPIO_PIN_SET ? "HIGH" : "LOW");
    if (stby == GPIO_PIN_SET) {
        printf("(Enabled)\n");
    } else {
        printf("** ERROR: Should be HIGH to enable motor driver **\n");
    }

    // Left motor (Motor B)
    GPIO_PinState b_in1 = HAL_GPIO_ReadPin(MOTOR_B_IN1_GPIO_Port, MOTOR_B_IN1_Pin);
    GPIO_PinState b_in2 = HAL_GPIO_ReadPin(MOTOR_B_IN2_GPIO_Port, MOTOR_B_IN2_Pin);
    printf("  Left Motor (B):\n");
    printf("    IN1: %s\n", b_in1 == GPIO_PIN_SET ? "HIGH" : "LOW");
    printf("    IN2: %s\n", b_in2 == GPIO_PIN_SET ? "HIGH" : "LOW");
    if (b_in1 == GPIO_PIN_RESET && b_in2 == GPIO_PIN_RESET) {
        printf("    Direction: BRAKE (both LOW)\n");
    } else if (b_in1 == GPIO_PIN_SET && b_in2 == GPIO_PIN_RESET) {
        printf("    Direction: FORWARD (IN1=HIGH, IN2=LOW)\n");
    } else if (b_in1 == GPIO_PIN_RESET && b_in2 == GPIO_PIN_SET) {
        printf("    Direction: BACKWARD (IN1=LOW, IN2=HIGH)\n");
    } else {
        printf("    Direction: ** INVALID (both HIGH) **\n");
    }

    // Right motor (Motor C)
    GPIO_PinState c_in1 = HAL_GPIO_ReadPin(MOTOR_C_IN1_GPIO_Port, MOTOR_C_IN1_Pin);
    GPIO_PinState c_in2 = HAL_GPIO_ReadPin(MOTOR_C_IN2_GPIO_Port, MOTOR_C_IN2_Pin);
    printf("  Right Motor (C):\n");
    printf("    IN1: %s\n", c_in1 == GPIO_PIN_SET ? "HIGH" : "LOW");
    printf("    IN2: %s\n", c_in2 == GPIO_PIN_SET ? "HIGH" : "LOW");
    if (c_in1 == GPIO_PIN_RESET && c_in2 == GPIO_PIN_RESET) {
        printf("    Direction: BRAKE (both LOW)\n");
    } else if (c_in1 == GPIO_PIN_SET && c_in2 == GPIO_PIN_RESET) {
        printf("    Direction: FORWARD (IN1=HIGH, IN2=LOW)\n");
    } else if (c_in1 == GPIO_PIN_RESET && c_in2 == GPIO_PIN_SET) {
        printf("    Direction: BACKWARD (IN1=LOW, IN2=HIGH)\n");
    } else {
        printf("    Direction: ** INVALID (both HIGH) **\n");
    }
    printf("\n");

    // Hardware checklist
    printf("[Hardware Checklist]\n");
    printf("  [ ] VM (Motor Power) connected to 6-12V supply\n");
    printf("  [ ] VCC connected to 3.3V or 5V logic supply\n");
    printf("  [ ] GND connected to common ground\n");
    printf("  [ ] STBY pin pulled HIGH (currently: %s)\n", stby == GPIO_PIN_SET ? "YES" : "NO");
    printf("  [ ] Motor A/B wires connected to AO1/AO2 and BO1/BO2\n");
    printf("  [ ] PWM pins (PA8, PA9) connected to PWMA, PWMB\n");
    printf("  [ ] Direction pins connected to AIN1/AIN2, BIN1/BIN2\n");
    printf("\n");

    // Recommendations
    printf("[Diagnostic Results]\n");
    if (stby == GPIO_PIN_RESET) {
        printf("  ** CRITICAL: STBY is LOW - motor driver is DISABLED **\n");
        printf("  --> Check MOTOR_STBY pin definition in CubeMX\n");
        printf("  --> Verify Motor_Init() is called\n");
    } else if (__HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_1) == 0 &&
               __HAL_TIM_GET_COMPARE(&htim1, TIM_CHANNEL_2) == 0) {
        printf("  ** WARNING: Both PWM duty cycles are 0%% **\n");
        printf("  --> Motors are commanded to stop\n");
        printf("  --> This is normal if Motor_SetSpeed(0, 0) was called\n");
    } else if (pwm_freq < 10000 || pwm_freq > 30000) {
        printf("  ** WARNING: PWM frequency is %lu Hz (expected ~20 kHz) **\n", pwm_freq);
        printf("  --> Check TIM1 prescaler and period settings\n");
    } else {
        printf("  Configuration looks OK. Check:\n");
        printf("  1. Motor power supply (VM = 6-12V)\n");
        printf("  2. Motor wiring (A01/A02, B01/B02)\n");
        printf("  3. Measure PWM pins with oscilloscope\n");
        printf("  4. Measure direction pins with multimeter\n");
    }
    printf("\n");
    printf("========================================\n");
    printf("\n");
}
