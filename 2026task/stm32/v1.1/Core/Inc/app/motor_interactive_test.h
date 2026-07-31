/**
 * @file motor_interactive_test.h
 * @brief 电机交互式串口测试
 * @date 2026-07-30
 */

#ifndef MOTOR_INTERACTIVE_TEST_H
#define MOTOR_INTERACTIVE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化串口交互式测试
 */
void MotorInteractiveTest_Init(void);

/**
 * @brief 串口交互式测试主循环
 * @note 在主循环或任务中周期性调用
 */
void MotorInteractiveTest_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_INTERACTIVE_TEST_H */
