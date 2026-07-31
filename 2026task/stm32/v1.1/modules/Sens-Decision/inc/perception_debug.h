/**
 * @file      perception_debug.h
 * @brief     感知模块调试工具
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-30
 * @note      用于实时调试红外传感器黑线检测算法
 */

#ifndef PERCEPTION_DEBUG_H
#define PERCEPTION_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "interface.h"
#include "perception.h"

/**
 * @brief 打印感知调试信息（详细版）
 *
 * @param ir_data 红外传感器数据
 * @param result 感知结果
 *
 * @details 输出内容:
 *   - 原始ADC值（8通道）
 *   - 白色参考值（8通道）
 *   - 黑线强度（8通道）
 *   - 激活掩码（二进制）
 *   - 激活通道数
 *   - 横向偏差
 *   - 航向误差
 *   - 道路事件
 */
void perception_debug_print(const ir_array_data_t *ir_data,
                           const perception_result_t *result);

/**
 * @brief 打印感知调试信息（紧凑版）
 *
 * @param ir_data 红外传感器数据
 * @param result 感知结果
 *
 * @details 单行输出格式:
 *   [IR] Raw: 270 268 105 98 102 272 269 271 | Str: 0 2 165 172 168 0 1 0 | Act: 3/8 | Err: -2.847
 */
void perception_debug_print_compact(const ir_array_data_t *ir_data,
                                   const perception_result_t *result);

/**
 * @brief 验证感知算法正确性（自检）
 *
 * @return true 验证通过
 * @return false 验证失败（打印错误信息）
 *
 * @details 检查项:
 *   1. 白色参考值是否合理（100-500范围）
 *   2. 阈值是否合理（10-200范围）
 *   3. 权重是否非零
 *   4. 配置是否一致
 */
bool perception_debug_selfcheck(void);

#endif // PERCEPTION_DEBUG_H
