/**
 * @file motor_adapter.h
 * @brief 电机适配器接口
 * @date 2026-07-29
 */

#ifndef MOTOR_ADAPTER_H
#define MOTOR_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "motion_control.h"

/**
 * @brief 获取电机接口
 * @return 电机接口指针
 */
MotorInterface_t *MotorAdapter_GetInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_ADAPTER_H */
