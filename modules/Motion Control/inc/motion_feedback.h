/**
 * @file motion_feedback.h
 * @brief 反馈控制模块：PID控制器 + 状态估计器
 * @date 2026-07-14
 * 
 * 提供PID控制器和基于编码器的状态估计功能
 */

#ifndef MOTION_FEEDBACK_H
#define MOTION_FEEDBACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief PID控制器结构
 */
typedef struct {
    float kp;              /**< 比例增益 */
    float ki;              /**< 积分增益 */
    float kd;              /**< 微分增益 */
    
    float integral;        /**< 积分累计 */
    float prev_error;      /**< 上次误差 */
    
    float output_min;      /**< 输出下限 */
    float output_max;      /**< 输出上限 */
    
    float integral_min;    /**< 积分限幅下限 */
    float integral_max;    /**< 积分限幅上限 */
} PID_t;

/**
 * @brief 编码器接口（虚表）
 */
typedef enum {
    ENCODER_LEFT_FRONT = 0,
    ENCODER_LEFT_REAR = 1,
    ENCODER_RIGHT_FRONT = 2,
    ENCODER_RIGHT_REAR = 3,
    ENCODER_COUNT
} EncoderId_t;

typedef struct {
    int32_t (*getCount)(EncoderId_t id);
    void (*resetCount)(EncoderId_t id);
} EncoderInterface_t;

/**
 * @brief 状态估计器结构
 */
typedef struct {
    EncoderInterface_t *encoder;  /**< 编码器接口 */
    
    // 编码器历史数据
    int32_t prev_count[ENCODER_COUNT];
    
    // 速度估计
    float wheel_speed[ENCODER_COUNT];  /**< 各轮速度 (m/s) */
    float wheel_speed_filtered[ENCODER_COUNT];  /**< 滤波后速度 */
    
    // 底盘速度
    float v_left;      /**< 左侧速度 (m/s) */
    float v_right;     /**< 右侧速度 (m/s) */
    
    // 物理参数
    float wheel_radius;
    float encoder_ppr;
    float update_freq;
} StateEstimator_t;

/* ============================================================================
 * PID控制器API
 * ============================================================================ */

/**
 * @brief 初始化PID控制器
 * @param pid PID实例
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 * @param out_min 输出下限
 * @param out_max 输出上限
 */
void PID_Init(PID_t *pid, float kp, float ki, float kd,
              float out_min, float out_max);

/**
 * @brief PID控制器更新
 * @param pid PID实例
 * @param setpoint 目标值
 * @param measurement 测量值
 * @param dt 时间步长 (秒)
 * @return 控制输出
 */
float PID_Update(PID_t *pid, float setpoint, float measurement, float dt);

/**
 * @brief 复位PID控制器
 * @param pid PID实例
 */
void PID_Reset(PID_t *pid);

/**
 * @brief 设置PID参数
 * @param pid PID实例
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 */
void PID_SetGains(PID_t *pid, float kp, float ki, float kd);

/**
 * @brief 设置积分限幅
 * @param pid PID实例
 * @param min 积分下限
 * @param max 积分上限
 */
void PID_SetIntegralLimits(PID_t *pid, float min, float max);

/* ============================================================================
 * 状态估计器API
 * ============================================================================ */

/**
 * @brief 初始化状态估计器
 * @param est 估计器实例
 * @param encoder 编码器接口
 * @param wheel_radius 轮半径 (m)
 * @param encoder_ppr 编码器每转脉冲数
 * @param update_freq 更新频率 (Hz)
 */
void StateEst_Init(StateEstimator_t *est, 
                   EncoderInterface_t *encoder,
                   float wheel_radius,
                   float encoder_ppr,
                   float update_freq);

/**
 * @brief 更新状态估计（读取编码器并计算速度）
 * @param est 估计器实例
 */
void StateEst_Update(StateEstimator_t *est);

/**
 * @brief 获取左侧速度
 * @param est 估计器实例
 * @return 左侧速度 (m/s)
 */
float StateEst_GetLeftSpeed(StateEstimator_t *est);

/**
 * @brief 获取右侧速度
 * @param est 估计器实例
 * @return 右侧速度 (m/s)
 */
float StateEst_GetRightSpeed(StateEstimator_t *est);

/**
 * @brief 获取单个轮子的速度
 * @param est 估计器实例
 * @param id 编码器ID
 * @return 轮速 (m/s)
 */
float StateEst_GetWheelSpeed(StateEstimator_t *est, EncoderId_t id);

/**
 * @brief 复位状态估计器
 * @param est 估计器实例
 */
void StateEst_Reset(StateEstimator_t *est);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_FEEDBACK_H */
