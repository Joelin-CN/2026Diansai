/**
 * @file encoder_hw_bridge.c
 * @brief 编码器硬件桥接层实现
 * @date 2026-07-29
 */

#include "encoder_hw_bridge.h"
#include "encoder.h"
#include "cmsis_os.h"

int32_t EncoderHwBridge_GetCount(uint8_t physical_id) {
    return Encoder_GetCount(physical_id);
}

void EncoderHwBridge_ResetCount(uint8_t physical_id) {
    taskENTER_CRITICAL();       // ISR 安全
    Encoder_ResetCount(physical_id);
    taskEXIT_CRITICAL();
}
