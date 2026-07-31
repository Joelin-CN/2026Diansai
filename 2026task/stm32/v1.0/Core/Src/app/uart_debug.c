/**
 * @file uart_debug.c
 * @brief UART调试输入/输出实现（printf/scanf重定向到UART5）
 * @note  fputc/fgetc  → Keil MDK (MicroLIB)
 *        _write/_read → GCC/newlib (VSCode / STM32CubeIDE)
 * @date 2026-07-29
 */

#include "uart_debug.h"
#include "usart.h"
#include <stdio.h>

void UartDebug_Init(void) {
    /* CubeMX已初始化UART5，此处为空实现 */
}

/* ============================================================================
 * 输出重定向
 * ============================================================================ */

/* ---- Keil MicroLIB 重定向 ---- */
int fputc(int ch, FILE *f) {
    (void)f;
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ---- GCC / newlib / Picolibc 重定向 ---- */
int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int fd, char *ptr, int len) {
    (void)fd;
    HAL_UART_Transmit(&huart5, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}

void UartDebug_PutChar(char c) {
    HAL_UART_Transmit(&huart5, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

/* ============================================================================
 * 输入重定向
 * ============================================================================ */

/* ---- Keil MicroLIB 重定向 ---- */
int fgetc(FILE *f) {
    (void)f;
    uint8_t c;
    HAL_UART_Receive(&huart5, &c, 1, HAL_MAX_DELAY);
    return (int)c;
}

/* ---- GCC / newlib / Picolibc 重定向 ---- */
int __io_getchar(void) {
    uint8_t c;
    if (HAL_UART_Receive(&huart5, &c, 1, 100) == HAL_OK) {  // 100ms timeout
        return (int)c;
    }
    return -1;  // Timeout or error
}
