/**
 * @file encoder.h
 * @brief 2轮编码器接口（TIM3/TIM4硬件编码器模式）
 * @date 2026-07-29
 */

#ifndef ENCODER_H
#define ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化编码器（启动TIM3/TIM4）
 */
void Encoder_Init(void);

/**
 * @brief 轮询更新编码器累积计数（500Hz调用）
 */
void Encoder_Poll(void);

/**
 * @brief 获取编码器累积计数
 * @param id 编码器ID（0=左，1=右）
 * @return 累积计数值（32位）
 */
int32_t Encoder_GetCount(uint8_t id);

/**
 * @brief 复位编码器累积计数
 * @param id 编码器ID（0=左，1=右）
 */
void Encoder_ResetCount(uint8_t id);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */
