/**
 * @file imu_debug.h
 * @brief IMU调试工具 - 测试ICM42688数据读取和AHRS解算
 * @date 2026-07-29
 */

#ifndef IMU_DEBUG_H
#define IMU_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化IMU调试模块
 * @return true: 成功, false: 失败
 */
bool ImuDebug_Init(void);

/**
 * @brief IMU调试循环（周期性调用）
 * @param print_interval_ms 打印间隔（毫秒）
 */
void ImuDebug_Run(uint32_t print_interval_ms);

#endif /* IMU_DEBUG_H */
