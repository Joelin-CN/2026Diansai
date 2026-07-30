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

/**
 * @brief TIM1 ARR值 - PWM周期配置
 *
 * @category D: 硬件约束参数（固定）
 *
 * @value 8399
 *
 * @origin 硬件计算
 *   目标: 生成20 kHz PWM频率
 *
 *   计算依据:
 *   - 系统时钟: 168 MHz (STM32F407)
 *   - TIM1时钟: 168 MHz (APB2时钟，预分频器PSC=0)
 *   - 目标PWM频率: 20 kHz
 *   - 计算公式: ARR = (TIM_CLK / PWM_FREQ) - 1
 *   - ARR = 168,000,000 / 20,000 - 1 = 8399
 *
 * @validation
 *   - CubeMX配置文件: v1.0_freeRTOS.ioc
 *   - 实测PWM频率应为 20.0 kHz ± 0.1 kHz
 *   - 使用示波器验证
 *
 * @tuning_guide
 *   PWM频率选择原则:
 *   - 太低 (< 10 kHz): 电机噪音大，转矩脉动明显
 *   - 适中 (15-25 kHz): 平衡性能和效率（推荐）
 *   - 太高 (> 30 kHz): 开关损耗增加，效率降低
 *
 *   修改PWM频率:
 *   1. 确定新的目标频率 f (Hz)
 *   2. 计算新的ARR = 168,000,000 / f - 1
 *   3. 同步修改CubeMX配置
 *   4. 重新生成代码
 *
 * @warnings
 *   - 不可随意修改，会改变PWM频率
 *   - 修改后必须更新CubeMX配置
 *   - TB6612驱动器支持的PWM频率范围: 1kHz ~ 100kHz
 */
#define MOTOR_PWM_ARR  8399U

/**
 * @brief 电机速度输入范围
 *
 * @category D: 硬件约束参数（设计值）
 *
 * @value 100 (对应百分比模式)
 *
 * @origin 设计选择
 *   - 使用百分比模式: -100% ~ +100%
 *   - 便于理解和使用
 *   - 内部转换为PWM脉宽: pulse = |speed| × ARR / 100
 *
 * @warnings
 *   - 输入范围: -100 到 +100
 *   - 超出范围会被自动限幅
 */
#define MOTOR_SPEED_MAX 100

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
