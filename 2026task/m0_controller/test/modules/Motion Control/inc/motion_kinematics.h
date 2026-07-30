/**
 * @file motion_kinematics.h
 * @brief 差速底盘运动学模块
 * @date 2026-07-14
 * 
 * 提供差速底盘的正运动学和逆运动学转换
 */

#ifndef MOTION_KINEMATICS_H
#define MOTION_KINEMATICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 差速运动学配置结构
 */
typedef struct {
    float wheel_base;      /**< 轮距 (m) */
    float wheel_radius;    /**< 轮半径 (m) */
} DiffKinematicsConfig_t;

/* ============================================================================
 * 公共API
 * ============================================================================ */

/**
 * @brief 初始化差速运动学模块
 * @param wheel_base 轮距 (m)
 * @param wheel_radius 轮半径 (m)
 */
void DiffKin_Init(float wheel_base, float wheel_radius);

/**
 * @brief 逆运动学：底盘速度 → 左右轮速度
 * 
 * 公式:
 *   v_left  = v - (wheel_base/2) * omega
 *   v_right = v + (wheel_base/2) * omega
 * 
 * @param v 底盘线速度 (m/s)
 * @param omega 底盘角速度 (rad/s)
 * @param v_left 输出左轮速度 (m/s)
 * @param v_right 输出右轮速度 (m/s)
 */
void DiffKin_Inverse(float v, float omega, 
                     float *v_left, float *v_right);

/**
 * @brief 正运动学：左右轮速度 → 底盘速度
 * 
 * 公式:
 *   v     = (v_left + v_right) / 2
 *   omega = (v_right - v_left) / wheel_base
 * 
 * @param v_left 左轮速度 (m/s)
 * @param v_right 右轮速度 (m/s)
 * @param v 输出底盘线速度 (m/s)
 * @param omega 输出底盘角速度 (rad/s)
 */
void DiffKin_Forward(float v_left, float v_right,
                     float *v, float *omega);

/**
 * @brief 获取轮距
 * @return 轮距 (m)
 */
float DiffKin_GetWheelBase(void);

/**
 * @brief 获取轮半径
 * @return 轮半径 (m)
 */
float DiffKin_GetWheelRadius(void);

/**
 * @brief 设置轮距（用于标定后更新）
 * @param wheel_base 轮距 (m)
 */
void DiffKin_SetWheelBase(float wheel_base);

/**
 * @brief 设置轮半径（用于标定后更新）
 * @param wheel_radius 轮半径 (m)
 */
void DiffKin_SetWheelRadius(float wheel_radius);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_KINEMATICS_H */
