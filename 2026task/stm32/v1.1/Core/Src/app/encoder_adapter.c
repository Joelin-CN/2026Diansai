/**
 * @file encoder_adapter.c
 * @brief 编码器适配器实现（2轮）
 * @date 2026-07-29
 */

#include "encoder_adapter.h"
#include "encoder_hw_bridge.h"
#include "motion_feedback.h"

/*
 * Physical forward direction measured on the B-left/C-right drivetrain.
 * Both raw hardware counts increase in the vehicle-forward direction.
 *
 * 2026-07-31 wiring verification:
 *   TIM3/raw encoder 0 is physically connected to motor C (right wheel).
 *   TIM4/raw encoder 1 is physically connected to motor B (left wheel).
 *
 * Keep the public MotionControl IDs logical (LEFT/RIGHT) and swap only the
 * raw hardware indices here. Without this mapping, the independent wheel PI
 * applies positive feedback and excites a large yaw oscillation.
 */
#define LEFT_ENCODER_DIRECTION  (1)
#define RIGHT_ENCODER_DIRECTION (1)
#define LEFT_ENCODER_RAW_INDEX  (1U)  /* TIM4 -> physical B/left */
#define RIGHT_ENCODER_RAW_INDEX (0U)  /* TIM3 -> physical C/right */

static int32_t adapter_getCount(EncoderId_t id) {
    if (id == ENCODER_LEFT) {
        return LEFT_ENCODER_DIRECTION *
               EncoderHwBridge_GetCount(LEFT_ENCODER_RAW_INDEX);
    }
    if (id == ENCODER_RIGHT) {
        return RIGHT_ENCODER_DIRECTION *
               EncoderHwBridge_GetCount(RIGHT_ENCODER_RAW_INDEX);
    }
    return 0;
}

static void adapter_resetCount(EncoderId_t id) {
    if (id == ENCODER_LEFT) {
        EncoderHwBridge_ResetCount(LEFT_ENCODER_RAW_INDEX);
        return;
    }
    if (id == ENCODER_RIGHT) {
        EncoderHwBridge_ResetCount(RIGHT_ENCODER_RAW_INDEX);
        return;
    }
}

static EncoderInterface_t s_iface = {
    .getCount    = adapter_getCount,
    .resetCount  = adapter_resetCount,
};

EncoderInterface_t *EncoderAdapter_GetInterface(void) {
    return &s_iface;
}
