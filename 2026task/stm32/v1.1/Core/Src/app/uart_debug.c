/**
 * @file uart_debug.c
 * @brief UART调试输出实现（printf重定向到UART5）
 * @note  fputc  → Keil MDK (MicroLIB)
 *        _write → GCC/newlib (VSCode / STM32CubeIDE)
 * @date 2026-07-29
 */

#include "uart_debug.h"
#include "usart.h"
#include <stdio.h>

void UartDebug_Init(void) {
    /* CubeMX已初始化UART5，此处为空实现 */
}

/* ---- Keil MicroLIB 重定向 ---- */
int fputc(int ch, FILE *f) {
    (void)f;
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ---- GCC / newlib 重定向（VSCode、STM32CubeIDE） ---- */
int _write(int fd, char *ptr, int len) {
    (void)fd;
    HAL_UART_Transmit(&huart5, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}

void UartDebug_PutChar(char c) {
    HAL_UART_Transmit(&huart5, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}
