/**
 * @file motion_config.h
 * @brief 运动控制层参数配置
 * @date 2026-07-14
 * 
 * 集中管理所有物理参数、控制参数、约束参数
 */

#ifndef MOTION_CONFIG_H
#define MOTION_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 物理参数配置
 * ============================================================================ */

/** @brief 轮距 (m) - 左右轮中心距离 */
#define WHEEL_BASE              0.150f

/** @brief 轮半径 (m) - 标定值 */
#define WHEEL_RADIUS            0.033f

/** @brief 编码器每转脉冲数 (4倍频后) */
#define ENCODER_PPR             334

/** @brief 电机减速比 */
#define GEAR_RATIO              28.0f

/** @brief 轮周长 (m) */
#define WHEEL_CIRCUMFERENCE     (2.0f * 3.14159265f * WHEEL_RADIUS)

/* ============================================================================
 * 控制频率配置
 * ============================================================================ */

/** @brief 主控制循环频率 (Hz) */
#define CONTROL_FREQ_HZ         500

/** @brief 控制周期 (秒) */
#define CONTROL_PERIOD_S        (1.0f / CONTROL_FREQ_HZ)

/** @brief 外环频率分频系数 (外环 = 主频/分频系数) */
#define OUTER_LOOP_DIVIDER      5

/** @brief 外环频率 (Hz) */
#define OUTER_LOOP_FREQ_HZ      (CONTROL_FREQ_HZ / OUTER_LOOP_DIVIDER)

/* ============================================================================
 * 外环：横向位置PID参数
 * ============================================================================ */

/** @brief 横向位置控制比例增益 */
#define LATERAL_KP              0.30f

/** @brief 横向位置控制积分增益 */
#define LATERAL_KI              0.00f

/** @brief 横向位置控制微分增益 */
#define LATERAL_KD              0.15f

/** @brief 横向位置控制输出限幅 (rad/s) */
#define LATERAL_OUTPUT_MAX      3.0f
#define LATERAL_OUTPUT_MIN      (-3.0f)

/* ============================================================================
 * 中环：角速度PI参数
 * ============================================================================ */

/** @brief 角速度控制比例增益 */
#define OMEGA_KP                0.80f

/** @brief 角速度控制积分增益 */
#define OMEGA_KI                0.10f

/** @brief 角速度控制输出限幅 (差速 m/s) */
#define OMEGA_OUTPUT_MAX        0.30f
#define OMEGA_OUTPUT_MIN        (-0.30f)

/* ============================================================================
 * 内环：轮速PI参数
 * ============================================================================ */

/** @brief 轮速控制比例增益 */
#define SPEED_KP                200.0f

/** @brief 轮速控制积分增益 */
#define SPEED_KI                50.0f

/** @brief 轮速控制输出限幅 (PWM) */
#define SPEED_OUTPUT_MAX        500.0f
#define SPEED_OUTPUT_MIN        (-500.0f)

/* ============================================================================
 * 前馈控制参数
 * ============================================================================ */

/** @brief 加速度前馈系数 (PWM/(m/s²)) */
#define FF_K_ACCEL              50.0f

/** @brief 摩擦前馈系数 (PWM/(m/s)) */
#define FF_K_FRICTION           300.0f

/** @brief 静摩擦补偿 (PWM) */
#define FF_K_STATIC             80.0f

/* ============================================================================
 * 速度与加速度约束
 * ============================================================================ */

/** @brief 默认基准速度 (m/s) */
#define BASE_SPEED_DEFAULT      0.50f

/** @brief 最大速度 (m/s) */
#define MAX_SPEED               1.00f

/** @brief 最小速度 (m/s) */
#define MIN_SPEED               0.10f

/** @brief 搜索速度 (丢线时) (m/s) */
#define SEARCH_SPEED            0.10f

/** @brief 最大加速度 (m/s²) */
#define MAX_ACCELERATION        2.0f

/* ============================================================================
 * PWM输出约束
 * ============================================================================ */

/** @brief PWM最大值 */
#define PWM_MAX                 1000

/** @brief PWM最小值 */
#define PWM_MIN                 (-1000)

/** @brief 启动最小PWM (克服静摩擦) */
#define PWM_START_MIN           80

/* ============================================================================
 * 循迹传感器配置
 * ============================================================================ */

/** @brief 横向误差最大值 (循迹传感器输出范围) */
#define LATERAL_ERROR_MAX       1100

/** @brief 横向误差归一化系数 */
#define LATERAL_ERROR_NORM      (1.0f / LATERAL_ERROR_MAX)

/* ============================================================================
 * 丢线处理参数
 * ============================================================================ */

/** @brief 丢线搜索超时 (控制周期数) */
#define LINE_LOST_TIMEOUT       250  // 0.5秒 @ 500Hz

/** @brief 丢线时的搜索角速度 (rad/s) */
#define LINE_SEARCH_OMEGA       2.0f

/* ============================================================================
 * 状态估计器配置
 * ============================================================================ */

/** @brief 速度低通滤波器截止频率 (Hz) */
#define SPEED_FILTER_CUTOFF     50.0f

/** @brief 速度低通滤波器系数 (alpha) */
#define SPEED_FILTER_ALPHA      (2.0f * 3.14159265f * SPEED_FILTER_CUTOFF * CONTROL_PERIOD_S / \
                                 (1.0f + 2.0f * 3.14159265f * SPEED_FILTER_CUTOFF * CONTROL_PERIOD_S))

/* ============================================================================
 * 调试与监控配置
 * ============================================================================ */

/** @brief 使能性能监控 */
#define ENABLE_PERFORMANCE_MONITOR  1

/** @brief 使能调试输出 */
#define ENABLE_DEBUG_OUTPUT         0

/** @brief 最大执行时间告警阈值 (us) */
#define MAX_EXEC_TIME_WARNING       500

#ifdef __cplusplus
}
#endif

#endif /* MOTION_CONFIG_H */
