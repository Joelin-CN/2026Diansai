/**
 * @file motor_adapter.c
 * @brief 电机适配器实现（2轮）
 * @date 2026-07-29
 */

#include "motor_adapter.h"
#include "motor.h"

static void adapter_init(void) {
    Motor_Init();
}

static void adapter_setDifferentialPWM(int16_t left, int16_t right) {
    Motor_SetSpeed(left, right);
}

static void adapter_stop(void) {
    Motor_Stop();
}

static MotorInterface_t s_iface = {
    .init                = adapter_init,
    .setDifferentialPWM  = adapter_setDifferentialPWM,
    .stop                = adapter_stop,
};

MotorInterface_t *MotorAdapter_GetInterface(void) {
    return &s_iface;
}
