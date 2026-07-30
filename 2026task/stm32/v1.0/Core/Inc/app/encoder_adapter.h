/**
 * @file encoder_adapter.h
 * @brief 编码器适配器接口
 * @date 2026-07-29
 */

#ifndef ENCODER_ADAPTER_H
#define ENCODER_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motion_feedback.h"

/**
 * @brief 获取编码器接口
 * @return 编码器接口指针
 */
EncoderInterface_t *EncoderAdapter_GetInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_ADAPTER_H */
