/**
 * @file motor_static_friction_test.h
 * @brief 静摩擦补偿参数FF_K_STATIC手动标定测试
 * @date 2026-07-31
 */

#ifndef MOTOR_STATIC_FRICTION_TEST_H
#define MOTOR_STATIC_FRICTION_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "usart.h"

/**
 * @brief 启动串口交互式测试模式
 *
 * 功能：通过串口手动发送PWM值，实时控制电机并观察编码器反馈
 *
 * 命令格式：
 *   L <pwm>  - 测试左轮（例如：L 65）
 *   R <pwm>  - 测试右轮（例如：R 70）
 *   B <pwm>  - 测试双轮（例如：B 50）
 *   S        - 停止所有电机
 *   E        - 读取编码器值
 *   C        - 清零编码器
 *   H        - 显示帮助信息
 *
 * @note 此函数永不返回，会进入无限循环
 */
void Motor_StaticFriction_InteractiveTest(void);

/**
 * @brief UART接收完成回调（由HAL库中断调用）
 * @param huart UART句柄
 */
void Motor_StaticFriction_UART_RxCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_STATIC_FRICTION_TEST_H */
