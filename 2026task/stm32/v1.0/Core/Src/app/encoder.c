/**
 * @file encoder.c
 * @brief 2轮编码器实现（TIM3/TIM4硬件编码器，含溢出扩展）
 * @date 2026-07-29
 */

#include "encoder.h"
#include "tim.h"

static int32_t  s_count[2]    = {0, 0};
static uint16_t s_last[2]     = {0, 0};
static TIM_HandleTypeDef *const s_htim[2] = {&htim3, &htim4};

void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    s_last[0] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    s_last[1] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    s_count[0] = 0;
    s_count[1] = 0;
}

/* 在 500Hz 控制任务中调用，更新累积计数 */
void Encoder_Poll(void) {
    for (int i = 0; i < 2; i++) {
        uint16_t now   = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim[i]);
        int16_t  delta = (int16_t)(now - s_last[i]);  // 有符号强制转换自动处理溢出
        s_count[i]    += (int32_t)delta;
        s_last[i]      = now;
    }
}

int32_t Encoder_GetCount(uint8_t id) {
    if (id >= 2U) return 0;
    return s_count[id];
}

void Encoder_ResetCount(uint8_t id) {
    if (id >= 2U) return;
    s_count[id] = 0;
    s_last[id]  = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim[id]);
}
