/**
 * @file motion_kinematics.c
 * @brief 差速底盘运动学模块实现
 * @date 2026-07-14
 */

#include "motion_kinematics.h"
#include "motion_config.h"

/* ============================================================================
 * 私有变量
 * ============================================================================ */

static DiffKinematicsConfig_t g_kinConfig = {
    .wheel_base = WHEEL_BASE,
    .wheel_radius = WHEEL_RADIUS
};

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

void DiffKin_Init(float wheel_base, float wheel_radius) {
    g_kinConfig.wheel_base = wheel_base;
    g_kinConfig.wheel_radius = wheel_radius;
}

void DiffKin_Inverse(float v, float omega,
                     float *v_left, float *v_right) {
    // 差速底盘逆运动学 - 纯差速模式（禁止反转）
    // 标准公式:
    // v_left  = v - (b/2) * omega
    // v_right = v + (b/2) * omega

    float half_base = g_kinConfig.wheel_base * 0.5f;

    float v_l_raw = v - half_base * omega;
    float v_r_raw = v + half_base * omega;

    // 纯差速模式：确保两轮只能正转或停止，不允许反转
    // 如果omega过大导致一个轮子需要反转，则限制omega
    if (v_l_raw < 0.0f || v_r_raw < 0.0f) {
        // 计算允许的最大omega，使较慢的轮子刚好为0
        float omega_max_allowed;
        if (v_l_raw < 0.0f) {
            // 左轮要反转，限制omega使左轮速度为0
            omega_max_allowed = v / half_base;  // omega = v / (b/2)
        } else {
            // 右轮要反转，限制omega使右轮速度为0
            omega_max_allowed = -v / half_base;
        }

        // 使用限制后的omega重新计算
        v_l_raw = v - half_base * omega_max_allowed;
        v_r_raw = v + half_base * omega_max_allowed;

        // 确保结果非负
        if (v_l_raw < 0.0f) v_l_raw = 0.0f;
        if (v_r_raw < 0.0f) v_r_raw = 0.0f;
    }

    *v_left  = v_l_raw;
    *v_right = v_r_raw;

    // DEBUG: Uncomment to trace kinematics output
    // printf("[DiffKin] v=%.2f, ω=%.2f → vL=%.2f, vR=%.2f\n",
    //        v, omega, *v_left, *v_right);
}

void DiffKin_Forward(float v_left, float v_right,
                     float *v, float *omega) {
    // 差速底盘正运动学
    // v     = (v_left + v_right) / 2
    // omega = (v_right - v_left) / b
    
    *v = (v_left + v_right) * 0.5f;
    *omega = (v_right - v_left) / g_kinConfig.wheel_base;
}

float DiffKin_GetWheelBase(void) {
    return g_kinConfig.wheel_base;
}

float DiffKin_GetWheelRadius(void) {
    return g_kinConfig.wheel_radius;
}

void DiffKin_SetWheelBase(float wheel_base) {
    g_kinConfig.wheel_base = wheel_base;
}

void DiffKin_SetWheelRadius(float wheel_radius) {
    g_kinConfig.wheel_radius = wheel_radius;
}
