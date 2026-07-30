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
    // 差速底盘逆运动学
    // v_left  = v - (b/2) * omega
    // v_right = v + (b/2) * omega
    
    float half_base = g_kinConfig.wheel_base * 0.5f;
    
    *v_left  = v - half_base * omega;
    *v_right = v + half_base * omega;
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
