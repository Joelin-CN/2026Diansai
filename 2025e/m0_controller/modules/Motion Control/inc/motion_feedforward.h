/**
 * @file motion_feedforward.h
 * @brief 前馈控制模块：转矩前馈（准电流环效果）
 * @date 2026-07-14
 * 
 * 基于电机模型的前馈控制，补偿惯性、摩擦等非线性因素
 */

#ifndef MOTION_FEEDFORWARD_H
#define MOTION_FEEDFORWARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 前馈控制器结构
 */
typedef struct {
    float k_accel;      /**< 加速度系数 (PWM/(m/s²)) - 惯性补偿 */
    float k_friction;   /**< 摩擦系数 (PWM/(m/s)) - 黏性摩擦补偿 */
    float k_static;     /**< 静摩擦补偿 (PWM) - 克服启动阻力 */
    
    float prev_velocity; /**< 上次速度 (用于计算加速度) */
} Feedforward_t;

/* ============================================================================
 * 公共API
 * ============================================================================ */

/**
 * @brief 初始化前馈控制器
 * @param ff 前馈控制器实例
 * @param k_accel 加速度系数 (PWM/(m/s²))
 * @param k_friction 摩擦系数 (PWM/(m/s))
 * @param k_static 静摩擦补偿 (PWM)
 */
void Feedforward_Init(Feedforward_t *ff, 
                      float k_accel, 
                      float k_friction,
                      float k_static);

/**
 * @brief 前馈控制更新
 * 
 * 前馈输出 = 惯性补偿 + 摩擦补偿 + 静摩擦补偿
 * 
 * 1. 惯性补偿: PWM = k_accel * acceleration
 *    - 补偿电机转动惯量和负载惯量
 * 
 * 2. 摩擦补偿: PWM = k_friction * velocity
 *    - 补偿与速度成正比的黏性摩擦
 * 
 * 3. 静摩擦补偿: PWM = k_static * sign(velocity)
 *    - 补偿启动时的静摩擦力
 * 
 * @param ff 前馈控制器实例
 * @param target_velocity 目标速度 (m/s)
 * @param dt 时间步长 (秒)
 * @return 前馈PWM输出
 */
float Feedforward_Update(Feedforward_t *ff, 
                        float target_velocity, 
                        float dt);

/**
 * @brief 复位前馈控制器
 * @param ff 前馈控制器实例
 */
void Feedforward_Reset(Feedforward_t *ff);

/**
 * @brief 设置前馈参数
 * @param ff 前馈控制器实例
 * @param k_accel 加速度系数
 * @param k_friction 摩擦系数
 * @param k_static 静摩擦补偿
 */
void Feedforward_SetParams(Feedforward_t *ff,
                          float k_accel,
                          float k_friction,
                          float k_static);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_FEEDFORWARD_H */
