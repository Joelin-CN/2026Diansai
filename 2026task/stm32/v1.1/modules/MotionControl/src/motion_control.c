/**
 * @file motion_control.c
 * @brief 运动控制层主控制器实现 - 轨迹跟踪模式
 * @date 2026-07-14
 */

#include "motion_control.h"
#include "motion_config.h"
#include "motion_kinematics.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

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

static inline float sign_float(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

/**
 * @brief Mix common propulsion and differential steering without losing turn.
 *
 * A conventional independent clamp turns (35, 15) into (25, 15), and two
 * saturated wheel loops can even become (25, 25).  Here the same amount is
 * shifted from both wheels when either side exceeds the limit.  The requested
 * left/right difference is therefore retained for as long as physically
 * possible.
 */
static void MixSteeringPriority(float common_pwm, float turn_pwm,
                                bool forward_only,
                                int16_t *left_pwm, int16_t *right_pwm) {
    const float limit = (float)PWM_MAX;
    const float turn_limit = limit * STEERING_PWM_LIMIT_RATIO;
    float left;
    float right;
    float excess;

    turn_pwm = clamp_float(turn_pwm, -turn_limit, turn_limit);
    left = common_pwm - turn_pwm;
    right = common_pwm + turn_pwm;

    if (left > limit || right > limit) {
        float high = left > right ? left : right;
        excess = high - limit;
        left -= excess;
        right -= excess;
    }
    if (left < -limit || right < -limit) {
        float low = left < right ? left : right;
        excess = -limit - low;
        left += excess;
        right += excess;
    }

    /*
     * The competition course is one continuous clockwise ring.  While the
     * vehicle has a non-negative forward command, steering may slow the inner
     * wheel to zero but must never reverse it into a pivot turn.
     */
    if (forward_only) {
        left = clamp_float(left, 0.0f, limit);
        right = clamp_float(right, 0.0f, limit);
    } else {
        left = clamp_float(left, -limit, limit);
        right = clamp_float(right, -limit, limit);
    }
    *left_pwm = (int16_t)lroundf(left);
    *right_pwm = (int16_t)lroundf(right);
}

/**
 * @brief 轮速控制器初始化
 */
static void WheelController_Init(WheelController_t *wheel,
                                  float speed_kp, float speed_ki,
                                  float ff_k_accel, float ff_k_friction,
                                  float ff_k_static) {
    PID_Init(&wheel->pid, speed_kp, speed_ki, 0.0f,
             SPEED_OUTPUT_MIN, SPEED_OUTPUT_MAX);
    Feedforward_Init(&wheel->ff, ff_k_accel, ff_k_friction, ff_k_static);
    wheel->target_velocity = 0.0f;
    wheel->actual_velocity = 0.0f;
    wheel->pwm_output = 0;
}

/**
 * @brief 轮速控制器更新（前馈+反馈合成）
 * 
 * PWM = 前馈(加速度+摩擦+静摩擦) + PI反馈(速度误差)
 */
static int16_t WheelController_Update(WheelController_t *wheel,
                                       float target_velocity,
                                       float actual_velocity,
                                       float dt,
                                       float *feedback_pwm) {
    wheel->target_velocity = target_velocity;
    wheel->actual_velocity = actual_velocity;

    /* 前馈控制（基于模型的开环补偿） */
    float pwm_ff = Feedforward_Update(&wheel->ff, target_velocity, dt);

    /* 反馈控制（基于误差的闭环修正） */
    float pwm_fb = PID_Update(&wheel->pid, target_velocity, actual_velocity, dt);
    if (feedback_pwm != NULL) {
        *feedback_pwm = pwm_fb;
    }

    /* 前馈+反馈合成 */
    float pwm_total = pwm_ff + pwm_fb;
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
 * 指令平滑与限幅
 * ============================================================================ */

/**
 * @brief 速度指令平滑（一阶低通滤波）
 *
 * 避免指令跳变导致车轮打滑或机械冲击。
 * smoothed = alpha * new + (1-alpha) * old
 * alpha 由 CMD_SMOOTH_TAU 控制
 */
static void SmoothCommand(MotionControl_t *ctrl) {
    const float alpha = CMD_SMOOTH_ALPHA;
    ctrl->smoothed_v     = alpha * ctrl->cmd.v_linear + (1.0f - alpha) * ctrl->smoothed_v;
#if FAST_STEERING_TEST_PROFILE
    /*
     * Keep linear-speed smoothing, but pass the yaw command directly.  The
     * old extra 50 ms omega filter came after the application-level slew
     * limiter and delayed steering reversal after the line error crossed zero.
     */
    ctrl->smoothed_omega = ctrl->cmd.omega;
#else
    ctrl->smoothed_omega = alpha * ctrl->cmd.omega    + (1.0f - alpha) * ctrl->smoothed_omega;
#endif
}

/**
 * @brief 加速度限幅
 *
 * 限制线速度变化率不超过 MAX_ACCELERATION / MAX_DECELERATION。
 * 这对轨迹跟踪至关重要：过大的加速度指令会导致前馈输出饱和、积分饱和。
 */
static float ClampAcceleration(float target, float previous, float dt) {
    float delta = target - previous;
    float max_delta_accel  = MAX_ACCELERATION  * dt;
    float max_delta_decel  = MAX_DECELERATION  * dt;

    if (delta > max_delta_accel) {
        return previous + max_delta_accel;
    } else if (delta < -max_delta_decel) {
        return previous - max_delta_decel;
    }
    return target;  /* 在范围内，直接使用 */
}

bool MotionControl_Init(MotionControl_t *ctrl,
                       EncoderInterface_t *encoder,
                       MotorInterface_t *motor) {
    if (ctrl == NULL || encoder == NULL || motor == NULL) {
        return false;
    }

    /* Validate encoder callbacks */
    if (encoder->getCount == NULL || encoder->resetCount == NULL) {
        return false;
    }

    /* Validate motor callbacks */
    if (motor->setDifferentialPWM == NULL || motor->stop == NULL || motor->init == NULL) {
        return false;
    }

    ctrl->encoder = encoder;
    ctrl->motor   = motor;

    /* 运动学模块 */
    DiffKin_Init(WHEEL_BASE, WHEEL_RADIUS);

    /* 状态估计器 */
    StateEst_Init(&ctrl->state_estimator,
                  encoder,
                  WHEEL_RADIUS,
                  ENCODER_PPR,
                  PID_CONTROL_FREQ_HZ);  /* StateEst_Update runs with the 100 Hz PID loop. */

    /* 左右轮速度控制器（前馈+反馈） */
    WheelController_Init(&ctrl->wheel_left,
                        SPEED_KP, SPEED_KI,
                        FF_K_ACCEL, FF_K_FRICTION, FF_K_STATIC);
    WheelController_Init(&ctrl->wheel_right,
                        SPEED_KP, SPEED_KI,
                        FF_K_ACCEL, FF_K_FRICTION, FF_K_STATIC);

    /* 状态变量 */
    ctrl->state        = CONTROL_STATE_IDLE;
    ctrl->cmd.v_linear = 0.0f;
    ctrl->cmd.omega    = 0.0f;
    ctrl->smoothed_v   = 0.0f;
    ctrl->smoothed_omega = 0.0f;
    ctrl->prev_limited_v = 0.0f;
    ctrl->gyro_z_radps = 0.0f;
    ctrl->gyro_z_valid = false;
    ctrl->yaw_rate_error_radps = 0.0f;
    ctrl->steering_pwm = 0.0f;
    ctrl->wheel_pi_steering_pwm = 0.0f;
    ctrl->loop_count   = 0;
    ctrl->max_exec_time = 0;
    ctrl->last_stamp   = 0;

    return true;
}

/* ============================================================================
 * 主控制循环
 * ---------------------------------------------------------------------------
 * 频率: 100Hz (每10ms执行一次)
 * 调用位置: track_control_app.c (每5个主循环周期调用一次)
 * ============================================================================ */

void MotionControl_Update(MotionControl_t *ctrl) {
    if (ctrl->state != CONTROL_STATE_RUNNING) {
        return;
    }

    /* PID控制周期: 10ms (100Hz) - 匹配执行器响应时间 */
    const float dt = CONTROL_PERIOD_S;  /* 0.01s (100Hz) */

#if ENABLE_PERFORMANCE_MONITOR
    /* 记录开始时间 */
    uint32_t start_stamp = ctrl->last_stamp; /* 由定时器或其他计时源更新 */
    (void)start_stamp;
#endif

    /* ------------------------------------------------------------
     * 步骤1: 状态估计 - 读取编码器，计算实际轮速
     * ---------------------------------------------------------- */
    StateEst_Update(&ctrl->state_estimator);
    float v_left_actual  = StateEst_GetLeftSpeed(&ctrl->state_estimator);
    float v_right_actual = StateEst_GetRightSpeed(&ctrl->state_estimator);

    /* ------------------------------------------------------------
     * 步骤2: 指令平滑 - 对速度指令施加一阶低通滤波
     * ---------------------------------------------------------- */
    SmoothCommand(ctrl);

    /* ------------------------------------------------------------
     * 步骤3: 加速度限幅
     * ---------------------------------------------------------- */
    /* Use instance-owned previous state for acceleration limiting */
    float v_limited = ClampAcceleration(ctrl->smoothed_v, ctrl->prev_limited_v, dt);
    ctrl->prev_limited_v = v_limited;

    /* Speed limits */
    v_limited = clamp_float(v_limited, -MAX_SPEED, MAX_SPEED);
    float omega_limited = clamp_float(ctrl->smoothed_omega, -MAX_OMEGA, MAX_OMEGA);

    /* ------------------------------------------------------------
     * 步骤4: 逆运动学 - (v, ω) → (vL_target, vR_target)
     * ---------------------------------------------------------- */
    float v_left_target = 0.0f;
    float v_right_target = 0.0f;
    DiffKin_Inverse(v_limited, omega_limited, &v_left_target, &v_right_target);

    /* ------------------------------------------------------------
     * 步骤5: 轮速控制 - 前馈+反馈
     * ---------------------------------------------------------- */
    float wheel_feedback_left = 0.0f;
    float wheel_feedback_right = 0.0f;
    int16_t wheel_pwm_left = WheelController_Update(&ctrl->wheel_left,
                                                    v_left_target,
                                                    v_left_actual,
                                                    dt,
                                                    &wheel_feedback_left);
    int16_t wheel_pwm_right = WheelController_Update(&ctrl->wheel_right,
                                                     v_right_target,
                                                     v_right_actual,
                                                     dt,
                                                     &wheel_feedback_right);

    /*
     * Cascaded steering:
     *   IR position outer loop -> omega target
     *   ICM42688 gyro_z inner loop -> differential PWM
     *
     * The mean of the wheel PI outputs controls propulsion.  Steering is
     * allocated afterwards with priority, preventing common-mode saturation
     * from swallowing the left/right correction.
     */
    float common_pwm = 0.5f * ((float)wheel_pwm_left + (float)wheel_pwm_right);
    ctrl->yaw_rate_error_radps =
        ctrl->gyro_z_valid ? (omega_limited - ctrl->gyro_z_radps) : 0.0f;
    float steering_request =
        STEERING_FF_PWM_PER_RADPS * omega_limited;
    if (ctrl->gyro_z_valid) {
        steering_request +=
            YAW_RATE_KP_PWM_PER_RADPS * ctrl->yaw_rate_error_radps;
    }
#if INDEPENDENT_WHEEL_PI_TEST_PROFILE
    /*
     * Keep the differential part of the two independent speed PI loops.
     * Do not add the full wheel-PWM difference: its feed-forward component
     * would duplicate STEERING_FF_PWM_PER_RADPS above.
     */
    float wheel_pi_turn_raw =
        0.5f * (wheel_feedback_right - wheel_feedback_left);
    ctrl->wheel_pi_steering_pwm +=
        WHEEL_PI_STEERING_FILTER_ALPHA *
        (wheel_pi_turn_raw - ctrl->wheel_pi_steering_pwm);
    ctrl->wheel_pi_steering_pwm =
        clamp_float(ctrl->wheel_pi_steering_pwm,
                    -WHEEL_PI_STEERING_LIMIT_PWM,
                    WHEEL_PI_STEERING_LIMIT_PWM);
    steering_request += ctrl->wheel_pi_steering_pwm;
#else
    ctrl->wheel_pi_steering_pwm = 0.0f;
#endif

    int16_t pwm_left;
    int16_t pwm_right;
    MixSteeringPriority(common_pwm, steering_request,
                        v_limited >= 0.0f,
                        &pwm_left, &pwm_right);

    /* Telemetry must report the PWM actually sent to the motor bridge. */
    ctrl->wheel_left.pwm_output = pwm_left;
    ctrl->wheel_right.pwm_output = pwm_right;
    ctrl->steering_pwm = 0.5f * ((float)pwm_right - (float)pwm_left);

    /* ------------------------------------------------------------
     * 步骤6: 输出到电机
     * ---------------------------------------------------------- */
    ctrl->motor->setDifferentialPWM(pwm_left, pwm_right);

    /* 性能监控 */
    ctrl->loop_count++;

#if ENABLE_PERFORMANCE_MONITOR
    // uint32_t exec_time = now - start_stamp;
    // if (exec_time > ctrl->max_exec_time) {
    //     ctrl->max_exec_time = exec_time;
    // }
#endif
}

/* ============================================================================
 * 参数设置API
 * ============================================================================ */

void MotionControl_SetVelocityCommand(MotionControl_t *ctrl,
                                      float v_linear,
                                      float omega) {
    ctrl->cmd.v_linear = v_linear;
    ctrl->cmd.omega    = omega;
}

void MotionControl_SetYawRateFeedback(MotionControl_t *ctrl,
                                      float gyro_z_radps,
                                      bool valid) {
    if (ctrl == NULL) {
        return;
    }
    ctrl->gyro_z_radps = valid ? gyro_z_radps : 0.0f;
    ctrl->gyro_z_valid = valid;
}

void MotionControl_SetSpeedPI(MotionControl_t *ctrl,
                              float kp, float ki) {
    PID_SetGains(&ctrl->wheel_left.pid,  kp, ki, 0.0f);
    PID_SetGains(&ctrl->wheel_right.pid, kp, ki, 0.0f);
}

void MotionControl_SetFeedforward(MotionControl_t *ctrl,
                                  float k_accel,
                                  float k_friction,
                                  float k_static) {
    Feedforward_SetParams(&ctrl->wheel_left.ff,  k_accel, k_friction, k_static);
    Feedforward_SetParams(&ctrl->wheel_right.ff, k_accel, k_friction, k_static);
}

/* ============================================================================
 * 控制状态管理
 * ============================================================================ */

void MotionControl_Start(MotionControl_t *ctrl) {
    /* 只允许从 IDLE 或 EMERGENCY 启动 */
    if (ctrl->state != CONTROL_STATE_IDLE && ctrl->state != CONTROL_STATE_EMERGENCY) {
        return;
    }

    /* 复位所有控制器 */
    WheelController_Reset(&ctrl->wheel_left);
    WheelController_Reset(&ctrl->wheel_right);
    StateEst_Reset(&ctrl->state_estimator);

    /* 复位平滑状态，使用当前指令值初始化 */
    ctrl->smoothed_v     = ctrl->cmd.v_linear;
    ctrl->smoothed_omega = ctrl->cmd.omega;
    ctrl->prev_limited_v = 0.0f;
    ctrl->gyro_z_radps = 0.0f;
    ctrl->gyro_z_valid = false;
    ctrl->yaw_rate_error_radps = 0.0f;
    ctrl->steering_pwm = 0.0f;
    ctrl->wheel_pi_steering_pwm = 0.0f;

    ctrl->state = CONTROL_STATE_RUNNING;
}

void MotionControl_Stop(MotionControl_t *ctrl) {
    if (ctrl->motor != NULL) {
        ctrl->motor->stop();
    }

    WheelController_Reset(&ctrl->wheel_left);
    WheelController_Reset(&ctrl->wheel_right);

    ctrl->cmd.v_linear   = 0.0f;
    ctrl->cmd.omega      = 0.0f;
    ctrl->smoothed_v     = 0.0f;
    ctrl->smoothed_omega = 0.0f;
    ctrl->prev_limited_v = 0.0f;
    ctrl->wheel_pi_steering_pwm = 0.0f;

    ctrl->state = CONTROL_STATE_IDLE;
}

void MotionControl_EmergencyStop(MotionControl_t *ctrl) {
    if (ctrl->motor != NULL) {
        ctrl->motor->stop();
    }

    ctrl->state = CONTROL_STATE_EMERGENCY;
}

/* ============================================================================
 * 状态查询API
 * ============================================================================ */

ControlState_t MotionControl_GetState(MotionControl_t *ctrl) {
    return ctrl->state;
}

void MotionControl_GetWheelSpeed(MotionControl_t *ctrl,
                                 float *left, float *right) {
    if (left  != NULL) *left  = ctrl->wheel_left.actual_velocity;
    if (right != NULL) *right = ctrl->wheel_right.actual_velocity;
}

void MotionControl_GetTargetWheelSpeed(MotionControl_t *ctrl,
                                       float *left, float *right) {
    if (left  != NULL) *left  = ctrl->wheel_left.target_velocity;
    if (right != NULL) *right = ctrl->wheel_right.target_velocity;
}

void MotionControl_GetPerformance(MotionControl_t *ctrl,
                                  uint32_t *loop_count,
                                  uint32_t *max_exec_time) {
    if (loop_count    != NULL) *loop_count    = ctrl->loop_count;
    if (max_exec_time != NULL) *max_exec_time = ctrl->max_exec_time;
}
