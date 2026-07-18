/**
 * @file motion_control.h
 * @brief 运动控制层主控制器 - 轨迹跟踪模式
 * @date 2026-07-14
 * 
 * 本模块作为执行层，接收上层（Sens-Decision）的速度指令 (v, ω)，
 * 通过差速运动学解算 + 前馈/反馈轮速控制，输出PWM到电机。
 *
 * 架构:
 *   上层决策              本模块                    底层硬件
 *   ─────────            ────────────              ────────
 *   (v_cmd, ω_cmd)  →  逆运动学      →  vL_target │
 *                            ↓                   │ 前馈 + PI  → PWM
 *                       状态估计      ←  编码器    │
 *
 * 控制流程 (500Hz):
 *   1. 状态估计: 编码器 → 实际轮速
 *   2. 指令平滑: 对 (v_cmd, ω_cmd) 施加一阶低通滤波
 *   3. 加速度限幅: 限制速度变化率
 *   4. 逆运动学: (v, ω) → (vL_target, vR_target)
 *   5. 轮速控制: 前馈(惯性+摩擦) + PI反馈 → PWM
 *   6. 输出到电机
 */

#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "motion_feedback.h"
#include "motion_feedforward.h"

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief 控制状态枚举
 */
typedef enum {
    CONTROL_STATE_INIT,         /**< 初始化状态 */
    CONTROL_STATE_IDLE,         /**< 空闲状态 */
    CONTROL_STATE_RUNNING,      /**< 运行状态 */
    CONTROL_STATE_EMERGENCY,    /**< 紧急停止状态 */
    CONTROL_STATE_ERROR         /**< 故障状态 */
} ControlState_t;

/**
 * @brief 电机接口（虚表）
 */
typedef struct {
    void (*setDifferentialPWM)(int16_t left, int16_t right);
    void (*stop)(void);
    void (*init)(void);
} MotorInterface_t;

/**
 * @brief 速度指令（由上层决策模块提供）
 */
typedef struct {
    float v_linear;    /**< 线速度指令 (m/s) */
    float omega;       /**< 角速度指令 (rad/s) */
} VelocityCommand_t;

/**
 * @brief 轮速控制器（前馈+反馈组合）
 * 
 * 每个轮子独立一个实例，控制逻辑:
 *   PWM = 前馈(惯性+摩擦+静摩擦) + PI反馈(速度误差)
 */
typedef struct {
    PID_t pid;              /**< 速度PI控制器 */
    Feedforward_t ff;       /**< 前馈控制器 */
    
    float target_velocity;  /**< 目标速度 (m/s) */
    float actual_velocity;  /**< 实际速度 (m/s) */
    
    int16_t pwm_output;     /**< 输出PWM */
} WheelController_t;

/**
 * @brief 运动控制器主结构
 */
typedef struct {
    /* ---- 硬件接口 ---- */
    EncoderInterface_t *encoder;   /**< 编码器接口（状态估计用） */
    MotorInterface_t *motor;       /**< 电机PWM输出接口（直接持有，无执行器层） */
    
    /* ---- 状态估计器 ---- */
    StateEstimator_t state_estimator;
    
    /* ---- 控制状态 ---- */
    ControlState_t state;
    
    /* ---- 速度指令 ---- */
    VelocityCommand_t cmd;          /**< 当前速度指令 */
    
    /* ---- 指令平滑（一阶低通滤波） ---- */
    float smoothed_v;       /**< 平滑后的线速度 (m/s) */
    float smoothed_omega;   /**< 平滑后的角速度 (rad/s) */
    
    /* ---- 内环：左右轮速度控制器（前馈+反馈） ---- */
    WheelController_t wheel_left;
    WheelController_t wheel_right;
    
    /* ---- 性能监控 ---- */
    uint32_t loop_count;       /**< 循环计数 */
    uint32_t max_exec_time;    /**< 最大执行时间(us) */
    uint32_t last_stamp;       /**< 上次时间戳(us)，用于性能统计 */
    
} MotionControl_t;

/**
 * @brief 运动控制器配置（初始化时传入）
 */
typedef struct {
    float kp_speed;         /**< 轮速PI比例增益 */
    float ki_speed;         /**< 轮速PI积分增益 */
    float k_ff_accel;       /**< 前馈加速度系数 */
    float k_ff_friction;    /**< 前馈摩擦系数 */
    float k_ff_static;      /**< 前馈静摩擦补偿 */
} MotionControlConfig_t;

/* ============================================================================
 * 公共API
 * ============================================================================ */

/**
 * @brief 初始化运动控制层
 * 
 * @param ctrl           控制器实例
 * @param encoder        编码器接口（不能为NULL）
 * @param motor          电机PWM输出接口（不能为NULL）
 * @return true=成功, false=失败（参数为NULL）
 */
bool MotionControl_Init(MotionControl_t *ctrl,
                       EncoderInterface_t *encoder,
                       MotorInterface_t *motor);

/**
 * @brief 主控制循环（500Hz定时器中断中调用）
 * 
 * 控制流程:
 *   1. 状态估计: 读取编码器计算实际轮速
 *   2. 指令平滑: 一阶低通滤波 (v_cmd, ω_cmd)
 *   3. 加速度限幅: 限制速度变化率
 *   4. 逆运动学: (v, ω) → (vL_target, vR_target)
 *   5. 轮速控制: 前馈(惯性+摩擦+静摩擦) + PI反馈
 *   6. 输出PWM到电机
 * 
 * @param ctrl 控制器实例
 */
void MotionControl_Update(MotionControl_t *ctrl);

/**
 * @brief 设置速度指令（由上层决策模块周期性调用）
 * 
 * 该函数只记录指令，实际执行在 MotionControl_Update() 中，
 * 会经过平滑和限幅处理后再输出。
 * 
 * @param ctrl     控制器实例
 * @param v_linear 线速度指令 (m/s)
 * @param omega    角速度指令 (rad/s)
 */
void MotionControl_SetVelocityCommand(MotionControl_t *ctrl,
                                      float v_linear,
                                      float omega);

/**
 * @brief 设置速度PI参数
 * @param ctrl 控制器实例
 * @param kp   比例增益
 * @param ki   积分增益
 */
void MotionControl_SetSpeedPI(MotionControl_t *ctrl,
                              float kp, float ki);

/**
 * @brief 设置前馈参数
 * @param ctrl       控制器实例
 * @param k_accel    加速度系数
 * @param k_friction 摩擦系数
 * @param k_static   静摩擦补偿
 */
void MotionControl_SetFeedforward(MotionControl_t *ctrl,
                                  float k_accel,
                                  float k_friction,
                                  float k_static);

/**
 * @brief 启动控制
 * 
 * 复位所有控制器状态并进入RUNNING状态。
 * 调用前需先通过 SetVelocityCommand 设置初始速度指令。
 * 
 * @param ctrl 控制器实例
 */
void MotionControl_Start(MotionControl_t *ctrl);

/**
 * @brief 停止控制
 * 
 * 停止电机并进入IDLE状态。
 * 
 * @param ctrl 控制器实例
 */
void MotionControl_Stop(MotionControl_t *ctrl);

/**
 * @brief 紧急停止
 * 
 * 立即停止电机并进入EMERGENCY状态。
 * 需要显式调用 MotionControl_Start() 才能恢复。
 * 
 * @param ctrl 控制器实例
 */
void MotionControl_EmergencyStop(MotionControl_t *ctrl);

/**
 * @brief 获取当前状态
 * @param ctrl 控制器实例
 * @return 控制状态
 */
ControlState_t MotionControl_GetState(MotionControl_t *ctrl);

/**
 * @brief 获取左右轮实际速度
 * @param ctrl  控制器实例
 * @param left  输出左轮速度 (m/s)
 * @param right 输出右轮速度 (m/s)
 */
void MotionControl_GetWheelSpeed(MotionControl_t *ctrl,
                                 float *left, float *right);

/**
 * @brief 获取左右轮目标速度（运动学解算后）
 * @param ctrl  控制器实例
 * @param left  输出左轮目标速度 (m/s)
 * @param right 输出右轮目标速度 (m/s)
 */
void MotionControl_GetTargetWheelSpeed(MotionControl_t *ctrl,
                                       float *left, float *right);

/**
 * @brief 获取性能统计
 * @param ctrl          控制器实例
 * @param loop_count    输出循环计数
 * @param max_exec_time 输出最大执行时间(us)
 */
void MotionControl_GetPerformance(MotionControl_t *ctrl,
                                  uint32_t *loop_count,
                                  uint32_t *max_exec_time);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_CONTROL_H */
