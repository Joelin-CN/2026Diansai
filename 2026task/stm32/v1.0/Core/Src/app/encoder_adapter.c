/**
 * @file encoder_adapter.c
 * @brief 编码器适配器实现（2轮）
 * @date 2026-07-29
 */

#include "encoder_adapter.h"
#include "encoder_hw_bridge.h"
#include "motion_feedback.h"

static int32_t adapter_getCount(EncoderId_t id) {
    if (id == ENCODER_LEFT)  return EncoderHwBridge_GetCount(0);
    if (id == ENCODER_RIGHT) return EncoderHwBridge_GetCount(1);
    return 0;
}

static void adapter_resetCount(EncoderId_t id) {
    if (id == ENCODER_LEFT)  { EncoderHwBridge_ResetCount(0); return; }
    if (id == ENCODER_RIGHT) { EncoderHwBridge_ResetCount(1); return; }
}

static EncoderInterface_t s_iface = {
    .getCount    = adapter_getCount,
    .resetCount  = adapter_resetCount,
};

EncoderInterface_t *EncoderAdapter_GetInterface(void) {
    return &s_iface;
}
