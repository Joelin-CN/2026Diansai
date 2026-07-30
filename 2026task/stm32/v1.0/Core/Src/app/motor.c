/**
 * @file motor.c
 * @brief 2轮差速电机驱动实现（TB6612，TIM1 PWM）
 * @date 2026-07-29
 */

#include "motor.h"
#include "tim.h"
#include "gpio.h"
#include "main.h"
#include <stdlib.h>

#define MOTOR_PWM_ARR  8399U   // TIM1 ARR，对应20kHz、100%占空比
#define MOTOR_SPEED_MAX 100    // 输入范围：-100到+100（百分比）

static void _set_wheel(uint32_t ch,
                       GPIO_TypeDef *in1p, uint16_t in1,
                       GPIO_TypeDef *in2p, uint16_t in2,
                       int16_t speed) {
    // Clamp speed to valid range [-100, +100]
    if (speed >  MOTOR_SPEED_MAX) speed =  MOTOR_SPEED_MAX;
    if (speed < -MOTOR_SPEED_MAX) speed = -MOTOR_SPEED_MAX;

    // Calculate PWM pulse width: speed in percent, ARR is max count
    uint32_t pulse = (uint32_t)(abs(speed) * (int32_t)MOTOR_PWM_ARR / MOTOR_SPEED_MAX);

    // Set motor direction (IN1/IN2 logic reversed to correct motor direction)
    if (speed > 0) {
        // Positive speed = FORWARD
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_SET);
    } else if (speed < 0) {
        // Negative speed = BACKWARD
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_RESET);
    } else {
        // Stop: short brake (IN1=H, IN2=H → TB6612 shorts motor terminals to GND)
        // NOTE: IN1=L, IN2=L is Coast (Hi-Z), NOT brake — see TB6612 truth table
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_SET);
    }
    __HAL_TIM_SET_COMPARE(&htim1, ch, pulse);
}

void Motor_Init(void) {
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_SET); // 使能TB6612

    // TIM1是高级定时器，必须使能主输出（MOE）才能输出PWM
    __HAL_TIM_MOE_ENABLE(&htim1);

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    Motor_Stop();
}

void Motor_SetSpeed(int16_t left, int16_t right) {
    // Note: MOTOR_B -> Left wheel, MOTOR_C -> Right wheel
    _set_wheel(TIM_CHANNEL_1,
               MOTOR_B_IN1_GPIO_Port, MOTOR_B_IN1_Pin,
               MOTOR_B_IN2_GPIO_Port, MOTOR_B_IN2_Pin, left);
    _set_wheel(TIM_CHANNEL_2,
               MOTOR_C_IN1_GPIO_Port, MOTOR_C_IN1_Pin,
               MOTOR_C_IN2_GPIO_Port, MOTOR_C_IN2_Pin, right);
}

void Motor_Stop(void) {
    Motor_SetSpeed(0, 0);
}
