/**
 * @file motion_feedback.c
 * @brief 反馈控制模块实现
 * @date 2026-07-14
 */

#include "motion_feedback.h"
#include "motion_config.h"
#include <string.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 限幅函数
 */
static inline float clamp(float value, float min, float max) {
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

/* ============================================================================
 * PID控制器实现
 * ============================================================================ */

void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float out_min, float out_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    
    pid->output_min = out_min;
    pid->output_max = out_max;
    
    // 积分限幅默认为输出限幅的一半
    pid->integral_min = out_min * 0.5f;
    pid->integral_max = out_max * 0.5f;
}

float PID_Update(PID_t *pid, float setpoint, float measurement, float dt) {
    // 计算误差
    float error = setpoint - measurement;
    
    // 比例项
    float p_term = pid->kp * error;
    
    // 积分项（梯形积分 + 抗饱和）
    pid->integral += error * dt;
    
    // 积分限幅（防止积分饱和）
    pid->integral = clamp(pid->integral, pid->integral_min, pid->integral_max);
    
    float i_term = pid->ki * pid->integral;
    
    // 微分项
    float d_term = 0.0f;
    if (dt > 1e-6f) {  // 避免除零
        float derivative = (error - pid->prev_error) / dt;
        d_term = pid->kd * derivative;
    }
    pid->prev_error = error;
    
    // 输出合成
    float output = p_term + i_term + d_term;
    
    // 输出限幅
    output = clamp(output, pid->output_min, pid->output_max);
    
    return output;
}

void PID_Reset(PID_t *pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}

void PID_SetGains(PID_t *pid, float kp, float ki, float kd) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void PID_SetIntegralLimits(PID_t *pid, float min, float max) {
    pid->integral_min = min;
    pid->integral_max = max;
}

/* ============================================================================
 * 状态估计器实现
 * ============================================================================ */

void StateEst_Init(StateEstimator_t *est, 
                   EncoderInterface_t *encoder,
                   float wheel_radius,
                   float encoder_ppr,
                   float update_freq) {
    est->encoder = encoder;
    est->wheel_radius = wheel_radius;
    est->encoder_ppr = encoder_ppr;
    est->update_freq = update_freq;
    
    // 初始化历史数据
    memset(est->prev_count, 0, sizeof(est->prev_count));
    memset(est->wheel_speed, 0, sizeof(est->wheel_speed));
    memset(est->wheel_speed_filtered, 0, sizeof(est->wheel_speed_filtered));
    
    est->v_left = 0.0f;
    est->v_right = 0.0f;
}

void StateEst_Update(StateEstimator_t *est) {
    if (est->encoder == NULL) {
        return;
    }
    
    // 计算各轮速度
    for (int i = 0; i < ENCODER_COUNT; i++) {
        // 读取当前编码器计数
        int32_t current_count = est->encoder->getCount((EncoderId_t)i);
        
        // 计算脉冲增量
        int32_t delta_count = current_count - est->prev_count[i];
        est->prev_count[i] = current_count;
        
        // 脉冲 → 速度 (m/s)
        // 速度 = (脉冲增量 / PPR) * 轮周长 * 更新频率
        float wheel_circumference = 2.0f * 3.14159265f * est->wheel_radius;
        float speed = ((float)delta_count / est->encoder_ppr) * wheel_circumference * est->update_freq;
        
        est->wheel_speed[i] = speed;
        
        // 一阶低通滤波减少噪声
        // filtered = alpha * new + (1-alpha) * old
        float alpha = SPEED_FILTER_ALPHA;
        est->wheel_speed_filtered[i] = alpha * speed + (1.0f - alpha) * est->wheel_speed_filtered[i];
    }
    
    // 计算左右侧平均速度
    est->v_left = (est->wheel_speed_filtered[ENCODER_LEFT_FRONT] + 
                   est->wheel_speed_filtered[ENCODER_LEFT_REAR]) * 0.5f;
    
    est->v_right = (est->wheel_speed_filtered[ENCODER_RIGHT_FRONT] + 
                    est->wheel_speed_filtered[ENCODER_RIGHT_REAR]) * 0.5f;
}

float StateEst_GetLeftSpeed(StateEstimator_t *est) {
    return est->v_left;
}

float StateEst_GetRightSpeed(StateEstimator_t *est) {
    return est->v_right;
}

float StateEst_GetWheelSpeed(StateEstimator_t *est, EncoderId_t id) {
    if (id >= ENCODER_COUNT) {
        return 0.0f;
    }
    return est->wheel_speed_filtered[id];
}

void StateEst_Reset(StateEstimator_t *est) {
    memset(est->prev_count, 0, sizeof(est->prev_count));
    memset(est->wheel_speed, 0, sizeof(est->wheel_speed));
    memset(est->wheel_speed_filtered, 0, sizeof(est->wheel_speed_filtered));
    
    est->v_left = 0.0f;
    est->v_right = 0.0f;
    
    // 复位编码器计数
    if (est->encoder != NULL) {
        for (int i = 0; i < ENCODER_COUNT; i++) {
            est->encoder->resetCount((EncoderId_t)i);
        }
    }
}
