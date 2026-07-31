/**
 * @file platform_time.h
 * @brief 平台时间抽象层（基于DWT计数器）
 * @date 2026-07-29
 */

#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化平台时间模块（启用DWT计数器）
 */
void PlatformTime_Init(void);

/**
 * @brief 获取32位微秒时间戳
 * @return 当前时间（微秒），约25.6秒溢出
 */
uint32_t PlatformTime_GetUs32(void);

/**
 * @brief 获取64位微秒时间戳（带溢出扩展）
 * @return 当前时间（微秒），需在单任务/临界区调用
 */
uint64_t PlatformTime_GetUs64(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_TIME_H */
