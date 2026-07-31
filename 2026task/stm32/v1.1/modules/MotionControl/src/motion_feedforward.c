/**
 * @file motion_feedforward.c
 * @brief 前馈控制模块实现
 * @date 2026-07-14
 */

#include "motion_feedforward.h"
#include "motion_config.h"

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

void Feedforward_Init(Feedforward_t *ff, 
                      float k_accel, 
                      float k_friction,
                      float k_static) {
    ff->k_accel = k_accel;
    ff->k_friction = k_friction;
    ff->k_static = k_static;
    ff->prev_velocity = 0.0f;
}

float Feedforward_Update(Feedforward_t *ff, 
                        float target_velocity, 
                        float dt) {
    // 1. 加速度前馈（惯性补偿）
    float acceleration = 0.0f;
    if (dt > 1e-6f) {  // 避免除零
        acceleration = (target_velocity - ff->prev_velocity) / dt;
    }
    ff->prev_velocity = target_velocity;
    
    float ff_accel = ff->k_accel * acceleration;
    
    // 2. 黏性摩擦补偿（与速度成正比）
    float ff_friction = ff->k_friction * target_velocity;
    
    // 3. 静摩擦补偿（克服启动阻力）
    // 只有在速度不为零时施加，避免在停止时产生抖动
    float ff_static = 0.0f;
    const float VELOCITY_THRESHOLD = 0.01f;  // 1 cm/s
    
    if (target_velocity > VELOCITY_THRESHOLD) {
        ff_static = ff->k_static;
    } else if (target_velocity < -VELOCITY_THRESHOLD) {
        ff_static = -ff->k_static;
    }
    // 速度接近零时不施加静摩擦补偿
    
    // 4. 合成前馈输出
    float ff_output = ff_accel + ff_friction + ff_static;
    
    return ff_output;
}

void Feedforward_Reset(Feedforward_t *ff) {
    ff->prev_velocity = 0.0f;
}

void Feedforward_SetParams(Feedforward_t *ff,
                          float k_accel,
                          float k_friction,
                          float k_static) {
    ff->k_accel = k_accel;
    ff->k_friction = k_friction;
    ff->k_static = k_static;
}
