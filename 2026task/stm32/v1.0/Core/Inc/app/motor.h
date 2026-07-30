/**
 * @file motor.h
 * @brief 2轮差速电机驱动接口（TB6612）
 * @date 2026-07-29
 */

#ifndef MOTOR_H
#define MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化电机驱动（启用TB6612和PWM）
 */
void Motor_Init(void);

/**
 * @brief 设置左右轮速度
 * @param left 左轮速度百分比 [-100, +100]，正值=前进，负值=后退
 * @param right 右轮速度百分比 [-100, +100]，正值=前进，负值=后退
 */
void Motor_SetSpeed(int16_t left, int16_t right);

/**
 * @brief 停止所有电机
 */
void Motor_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_H */
