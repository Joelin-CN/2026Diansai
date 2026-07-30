/**
 * @file encoder_hw_bridge.h
 * @brief 编码器硬件桥接层接口
 * @date 2026-07-29
 */

#ifndef ENCODER_HW_BRIDGE_H
#define ENCODER_HW_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 获取物理编码器计数
 * @param physical_id 物理编码器ID（0=左，1=右）
 * @return 编码器计数值
 */
int32_t EncoderHwBridge_GetCount(uint8_t physical_id);

/**
 * @brief 复位物理编码器计数
 * @param physical_id 物理编码器ID（0=左，1=右）
 */
void EncoderHwBridge_ResetCount(uint8_t physical_id);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_HW_BRIDGE_H */
