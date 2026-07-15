/**
 * @file motion_control.h
 * @brief 运动控制层主控制器
 * @date 2026-07-14
 * 
 * 三环串级控制系统：横向位置环 + 角速度环 + 轮速环
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
    CONTROL_STATE_LINE_LOST,    /**< 丢线搜索状态 */
    CONTROL_STATE_EMERGENCY,    /**< 紧急停止状态 */
    CONTROL_STATE_ERROR         /**< 故障状态 */
} ControlState_t;

/**
 * @brief 循迹传感器接口（虚表）
 */
typedef struct {
    uint16_t (*readMask)(void);
    bool (*getError)(uint16_t mask, int16_t *error);
} LineSensorInterface_t;

/**
 * @brief IMU接口（虚表）
 */
typedef struct {
    float wx;
    float wy;
    float wz;
} GyroData_t;

typedef struct {
    bool (*readGyro)(GyroData_t *data);
    bool (*init)(void);
} ImuInterface_t;

/**
 * @brief 感知层接口（虚表）
 */
typedef struct {
    LineSensorInterface_t *lineSensor;
    EncoderInterface_t *encoder;
    ImuInterface_t *imu;
} PerceptionInterface_t;

/**
 * @brief 电机接口（虚表）
 */
typedef enum {
    MOTOR_LEFT_FRONT = 0,
    MOTOR_LEFT_REAR = 1,
    MOTOR_RIGHT_FRONT = 2,
    MOTOR_RIGHT_REAR = 3,
    MOTOR_COUNT
} MotorId_t;

typedef struct {
    void (*setDifferentialPWM)(int16_t left, int16_t right);
    void (*stop)(void);
    void (*init)(void);
} MotorInterface_t;

/**
 * @brief 执行器层接口（虚表）
 */
typedef struct {
    MotorInterface_t *motor;
} ActuatorInterface_t;

/**
 * @brief 轮速控制器（前馈+反馈组合）
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
    // 接口表
    PerceptionInterface_t *perception;
    ActuatorInterface_t *actuator;
    
    // 状态估计器
    StateEstimator_t state_estimator;
    
    // 控制状态
    ControlState_t state;
    
    // 三环控制器
    PID_t lateral_pid;      /**< 外环：横向位置PID */
    PID_t omega_pid;        /**< 中环：角速度PI */
    WheelController_t wheel_left;   /**< 内环：左轮速度控制 */
    WheelController_t wheel_right;  /**< 内环：右轮速度控制 */
    
    // 控制变量
    float base_speed;       /**< 基准速度 (m/s) */
    float omega_ref;        /**< 期望角速度 (rad/s) */
    
    // 状态变量
    int16_t lateral_error;  /**< 横向误差 */
    float gyro_z;           /**< Z轴角速度 */
    
    // 丢线处理
    uint32_t line_lost_count;  /**< 丢线计数 */
    int16_t last_error;        /**< 丢线前的误差方向 */
    
    // 性能监控
    uint32_t loop_count;       /**< 循环计数 */
    uint32_t max_exec_time;    /**< 最大执行时间(us) */
    
} MotionControl_t;

/* ============================================================================
 * 公共API
 * ============================================================================ */

/**
 * @brief 初始化运动控制层
 * @param ctrl 控制器实例
 * @param perception 感知层接口
 * @param actuator 执行器层接口
 * @return true=成功, false=失败
 */
bool MotionControl_Init(MotionControl_t *ctrl,
                       PerceptionInterface_t *perception,
                       ActuatorInterface_t *actuator);

/**
 * @brief 主控制循环（500Hz调用）
 * 
 * 控制流程:
 * 1. 读取传感器数据
 * 2. 外环: 横向位置控制 (100Hz)
 * 3. 中环: 角速度控制 (500Hz)
 * 4. 内环: 轮速控制 + 前馈 (500Hz)
 * 5. 输出到执行器
 * 
 * @param ctrl 控制器实例
 */
void MotionControl_Update(MotionControl_t *ctrl);

/**
 * @brief 设置基准速度
 * @param ctrl 控制器实例
 * @param speed 速度 (m/s)
 */
void MotionControl_SetBaseSpeed(MotionControl_t *ctrl, float speed);

/**
 * @brief 设置横向PID参数
 * @param ctrl 控制器实例
 * @param kp 比例增益
 * @param ki 积分增益
 * @param kd 微分增益
 */
void MotionControl_SetLateralPID(MotionControl_t *ctrl, 
                                 float kp, float ki, float kd);

/**
 * @brief 设置角速度PI参数
 * @param ctrl 控制器实例
 * @param kp 比例增益
 * @param ki 积分增益
 */
void MotionControl_SetOmegaPI(MotionControl_t *ctrl, 
                              float kp, float ki);

/**
 * @brief 设置轮速PI参数
 * @param ctrl 控制器实例
 * @param kp 比例增益
 * @param ki 积分增益
 */
void MotionControl_SetSpeedPI(MotionControl_t *ctrl, 
                              float kp, float ki);

/**
 * @brief 设置前馈参数
 * @param ctrl 控制器实例
 * @param k_accel 加速度系数
 * @param k_friction 摩擦系数
 * @param k_static 静摩擦补偿
 */
void MotionControl_SetFeedforward(MotionControl_t *ctrl,
                                  float k_accel, 
                                  float k_friction,
                                  float k_static);

/**
 * @brief 启动控制
 * @param ctrl 控制器实例
 */
void MotionControl_Start(MotionControl_t *ctrl);

/**
 * @brief 停止控制
 * @param ctrl 控制器实例
 */
void MotionControl_Stop(MotionControl_t *ctrl);

/**
 * @brief 紧急停止
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
 * @brief 获取横向误差
 * @param ctrl 控制器实例
 * @return 误差值
 */
int16_t MotionControl_GetLateralError(MotionControl_t *ctrl);

/**
 * @brief 获取左右轮速度
 * @param ctrl 控制器实例
 * @param left 输出左轮速度
 * @param right 输出右轮速度
 */
void MotionControl_GetWheelSpeed(MotionControl_t *ctrl, 
                                 float *left, float *right);

/**
 * @brief 获取性能统计
 * @param ctrl 控制器实例
 * @param loop_count 输出循环计数
 * @param max_exec_time 输出最大执行时间(us)
 */
void MotionControl_GetPerformance(MotionControl_t *ctrl,
                                  uint32_t *loop_count,
                                  uint32_t *max_exec_time);

#ifdef __cplusplus
}
#endif

#endif /* MOTION_CONTROL_H */
