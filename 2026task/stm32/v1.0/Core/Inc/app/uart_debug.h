/**
 * @file uart_debug.h
 * @brief UART调试输出接口（printf重定向）
 * @date 2026-07-29
 */

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化调试串口（UART5）
 */
void UartDebug_Init(void);

/**
 * @brief 发送单个字符
 * @param c 要发送的字符
 */
void UartDebug_PutChar(char c);

#ifdef __cplusplus
}
#endif

#endif /* UART_DEBUG_H */
