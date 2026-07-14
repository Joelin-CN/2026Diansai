/**
 * @file motion_control.c
 * @brief 运动控制层主控制器实现
 * @date 2026-07-14
 */

#include "motion_control.h"
#include "motion_config.h"
#include "motion_kinematics.h"
#include <string.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 限幅函数
 */
static inline float clamp_float(float value, float min, float max) {
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

static inline int16_t clamp_int16(int16_t value, int16_t min, int16_t max) {
    if (value > max) return max;
    if (value < min) return min;
    return value;
}

/**
 * @brief 轮速控制器初始化
 */
static void WheelController_Init(WheelController_t *wheel,
                                  float speed_kp, float speed_ki,
                                  float ff_k_accel, float ff_k_friction, float ff_k_static) {
    // 初始化速度PI控制器
    PID_Init(&wheel->pid, speed_kp, speed_ki, 0.0f, 
             SPEED_OUTPUT_MIN, SPEED_OUTPUT_MAX);
    
    // 初始化前馈控制器
    Feedforward_Init(&wheel->ff, ff_k_accel, ff_k_friction, ff_k_static);
    
    wheel->target_velocity = 0.0f;
    wheel->actual_velocity = 0.0f;
    wheel->pwm_output = 0;
}

/**
 * @brief 轮速控制器更新（前馈+反馈合成）
 */
static int16_t WheelController_Update(WheelController_t *wheel,
                                       float target_velocity,
                                       float actual_velocity,
                                       float dt) {
    wheel->target_velocity = target_velocity;
    wheel->actual_velocity = actual_velocity;
    
    // === 前馈控制（基于模型的开环补偿）===
    float pwm_ff = Feedforward_Update(&wheel->ff, target_velocity, dt);
    
    // === 反馈控制（基于误差的闭环修正）===
    float pwm_fb = PID_Update(&wheel->pid, target_velocity, actual_velocity, dt);
    
    // === 前馈+反馈合成 ===
    float pwm_total = pwm_ff + pwm_fb;
    
    // === PWM限幅 ===
    pwm_total = clamp_float(pwm_total, (float)PWM_MIN, (float)PWM_MAX);
    
    wheel->pwm_output = (int16_t)pwm_total;
    
    return wheel->pwm_output;
}

/**
 * @brief 复位轮速控制器
 */
static void WheelController_Reset(WheelController_t *wheel) {
    PID_Reset(&wheel->pid);
    Feedforward_Reset(&wheel->ff);
    wheel->target_velocity = 0.0f;
    wheel->actual_velocity = 0.0f;
    wheel->pwm_output = 0;
}

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

bool MotionControl_Init(MotionControl_t *ctrl,
                       PerceptionInterface_t *perception,
                       ActuatorInterface_t *actuator) {
    // 检查参数
    if (ctrl == NULL || perception == NULL || actuator == NULL) {
        return false;
    }
    
    // 保存接口
    ctrl->perception = perception;
    ctrl->actuator = actuator;
    
    // 初始化运动学模块
    DiffKin_Init(WHEEL_BASE, WHEEL_RADIUS);
    
    // 初始化状态估计器
    StateEst_Init(&ctrl->state_estimator,
                  perception->encoder,
                  WHEEL_RADIUS,
                  ENCODER_PPR,
                  CONTROL_FREQ_HZ);
    
    // 初始化外环：横向位置PID
    PID_Init(&ctrl->lateral_pid,
             LATERAL_KP, LATERAL_KI, LATERAL_KD,
             LATERAL_OUTPUT_MIN, LATERAL_OUTPUT_MAX);
    
    // 初始化中环：角速度PI
    PID_Init(&ctrl->omega_pid,
             OMEGA_KP, OMEGA_KI, 0.0f,
             OMEGA_OUTPUT_MIN, OMEGA_OUTPUT_MAX);
    
    // 初始化内环：左右轮速度控制器
    WheelController_Init(&ctrl->wheel_left,
                        SPEED_KP, SPEED_KI,
                        FF_K_ACCEL, FF_K_FRICTION, FF_K_STATIC);
    
    WheelController_Init(&ctrl->wheel_right,
                        SPEED_KP, SPEED_KI,
                        FF_K_ACCEL, FF_K_FRICTION, FF_K_STATIC);
    
    // 初始化状态变量
    ctrl->state = CONTROL_STATE_IDLE;
    ctrl->base_speed = BASE_SPEED_DEFAULT;
    ctrl->omega_ref = 0.0f;
    ctrl->lateral_error = 0;
    ctrl->gyro_z = 0.0f;
    ctrl->line_lost_count = 0;
    ctrl->last_error = 0;
    ctrl->loop_count = 0;
    ctrl->max_exec_time = 0;
    
    // 初始化IMU
    if (ctrl->perception->imu != NULL && ctrl->perception->imu->init != NULL) {
        ctrl->perception->imu->init();
    }
    
    return true;
}

void MotionControl_Update(MotionControl_t *ctrl) {
    if (ctrl->state != CONTROL_STATE_RUNNING) {
        return;
    }
    
    const float dt = CONTROL_PERIOD_S;
    
    // ============================================
    // 步骤1: 读取传感器数据
    // ============================================
    
    // 1.1 更新状态估计器（读取编码器）
    StateEst_Update(&ctrl->state_estimator);
    float v_left_actual = StateEst_GetLeftSpeed(&ctrl->state_estimator);
    float v_right_actual = StateEst_GetRightSpeed(&ctrl->state_estimator);
    
    // 1.2 读取循迹传感器
    uint16_t line_mask = ctrl->perception->lineSensor->readMask();
    bool on_line = ctrl->perception->lineSensor->getError(line_mask, &ctrl->lateral_error);
    
    // 1.3 读取IMU陀螺仪
    GyroData_t gyro;
    if (ctrl->perception->imu != NULL && ctrl->perception->imu->readGyro != NULL) {
        if (ctrl->perception->imu->readGyro(&gyro)) {
            ctrl->gyro_z = gyro.wz;  // Z轴角速度
        }
    }
    
    // ============================================
    // 步骤2: 外环 - 横向位置控制（100Hz，每5次执行1次）
    // ============================================
    
    static uint8_t outer_loop_divider = 0;
    if (++outer_loop_divider >= OUTER_LOOP_DIVIDER) {
        outer_loop_divider = 0;
        
        if (on_line) {
            // 在线上，执行PID控制
            // 横向误差归一化: -1100~+1100 → -1.0~+1.0
            float lateral_error_norm = (float)ctrl->lateral_error * LATERAL_ERROR_NORM;
            
            // PID输出期望角速度 (rad/s)
            ctrl->omega_ref = PID_Update(&ctrl->lateral_pid, 
                                         0.0f,  // 目标误差=0
                                         lateral_error_norm, 
                                         dt * OUTER_LOOP_DIVIDER);  // 外环周期
            
            // 清空丢线计数
            ctrl->line_lost_count = 0;
            ctrl->last_error = ctrl->lateral_error;
            
        } else {
            // 丢线处理：原地旋转搜索
            ctrl->line_lost_count++;
            
            if (ctrl->line_lost_count < LINE_LOST_TIMEOUT) {
                // 0.5秒内搜索
                // 根据上次误差方向旋转
                if (ctrl->last_error > 0) {
                    ctrl->omega_ref = LINE_SEARCH_OMEGA;  // 右转搜索
                } else {
                    ctrl->omega_ref = -LINE_SEARCH_OMEGA;  // 左转搜索
                }
                ctrl->base_speed = SEARCH_SPEED;  // 降低前进速度
                
            } else {
                // 超时未找到线，停止
                ctrl->state = CONTROL_STATE_LINE_LOST;
                MotionControl_Stop(ctrl);
                return;
            }
        }
    }
    
    // ============================================
    // 步骤3: 中环 - 角速度控制（500Hz）
    // ============================================
    
    // PI控制输出差速修正量
    float delta_v = PID_Update(&ctrl->omega_pid, 
                               ctrl->omega_ref, 
                               ctrl->gyro_z, 
                               dt);
    
    // 限制差速范围（防止单侧轮速过大）
    delta_v = clamp_float(delta_v, OMEGA_OUTPUT_MIN, OMEGA_OUTPUT_MAX);
    
    // ============================================
    // 步骤4: 内环 - 轮速控制 + 前馈（500Hz）
    // ============================================
    
    // 4.1 计算左右轮目标速度
    float v_left_target = ctrl->base_speed - delta_v;
    float v_right_target = ctrl->base_speed + delta_v;
    
    // 速度限幅
    v_left_target = clamp_float(v_left_target, -MAX_SPEED, MAX_SPEED);
    v_right_target = clamp_float(v_right_target, -MAX_SPEED, MAX_SPEED);
    
    // 4.2 左轮控制（前馈+反馈）
    int16_t pwm_left = WheelController_Update(&ctrl->wheel_left,
                                               v_left_target,
                                               v_left_actual,
                                               dt);
    
    // 4.3 右轮控制（前馈+反馈）
    int16_t pwm_right = WheelController_Update(&ctrl->wheel_right,
                                                v_right_target,
                                                v_right_actual,
                                                dt);
    
    // ============================================
    // 步骤5: 输出到执行器
    // ============================================
    
    // 差速控制：左右两侧各用相同PWM
    ctrl->actuator->motor->setDifferentialPWM(pwm_left, pwm_right);
    
    // ============================================
    // 步骤6: 性能监控
    // ============================================
    
    ctrl->loop_count++;
}

void MotionControl_SetBaseSpeed(MotionControl_t *ctrl, float speed) {
    ctrl->base_speed = clamp_float(speed, MIN_SPEED, MAX_SPEED);
}

void MotionControl_SetLateralPID(MotionControl_t *ctrl, 
                                 float kp, float ki, float kd) {
    PID_SetGains(&ctrl->lateral_pid, kp, ki, kd);
}

void MotionControl_SetOmegaPI(MotionControl_t *ctrl, 
                              float kp, float ki) {
    PID_SetGains(&ctrl->omega_pid, kp, ki, 0.0f);
}

void MotionControl_SetSpeedPI(MotionControl_t *ctrl, 
                              float kp, float ki) {
    PID_SetGains(&ctrl->wheel_left.pid, kp, ki, 0.0f);
    PID_SetGains(&ctrl->wheel_right.pid, kp, ki, 0.0f);
}

void MotionControl_SetFeedforward(MotionControl_t *ctrl,
                                  float k_accel, 
                                  float k_friction,
                                  float k_static) {
    Feedforward_SetParams(&ctrl->wheel_left.ff, k_accel, k_friction, k_static);
    Feedforward_SetParams(&ctrl->wheel_right.ff, k_accel, k_friction, k_static);
}

void MotionControl_Start(MotionControl_t *ctrl) {
    // 复位所有控制器
    PID_Reset(&ctrl->lateral_pid);
    PID_Reset(&ctrl->omega_pid);
    WheelController_Reset(&ctrl->wheel_left);
    WheelController_Reset(&ctrl->wheel_right);
    StateEst_Reset(&ctrl->state_estimator);
    
    // 复位状态变量
    ctrl->omega_ref = 0.0f;
    ctrl->lateral_error = 0;
    ctrl->gyro_z = 0.0f;
    ctrl->line_lost_count = 0;
    ctrl->last_error = 0;
    
    // 进入运行状态
    ctrl->state = CONTROL_STATE_RUNNING;
}

void MotionControl_Stop(MotionControl_t *ctrl) {
    // 停止电机
    if (ctrl->actuator != NULL && ctrl->actuator->motor != NULL) {
        ctrl->actuator->motor->stop();
    }
    
    // 复位控制器
    WheelController_Reset(&ctrl->wheel_left);
    WheelController_Reset(&ctrl->wheel_right);
    
    // 进入空闲状态
    ctrl->state = CONTROL_STATE_IDLE;
}

void MotionControl_EmergencyStop(MotionControl_t *ctrl) {
    // 立即停止电机
    if (ctrl->actuator != NULL && ctrl->actuator->motor != NULL) {
        ctrl->actuator->motor->stop();
    }
    
    // 进入紧急状态
    ctrl->state = CONTROL_STATE_EMERGENCY;
}

ControlState_t MotionControl_GetState(MotionControl_t *ctrl) {
    return ctrl->state;
}

int16_t MotionControl_GetLateralError(MotionControl_t *ctrl) {
    return ctrl->lateral_error;
}

void MotionControl_GetWheelSpeed(MotionControl_t *ctrl, 
                                 float *left, float *right) {
    if (left != NULL) {
        *left = ctrl->wheel_left.actual_velocity;
    }
    if (right != NULL) {
        *right = ctrl->wheel_right.actual_velocity;
    }
}

void MotionControl_GetPerformance(MotionControl_t *ctrl,
                                  uint32_t *loop_count,
                                  uint32_t *max_exec_time) {
    if (loop_count != NULL) {
        *loop_count = ctrl->loop_count;
    }
    if (max_exec_time != NULL) {
        *max_exec_time = ctrl->max_exec_time;
    }
}
