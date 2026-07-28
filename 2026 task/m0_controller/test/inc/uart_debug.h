/**
 * @file uart_debug.h
 * @brief UART0 debug logging utilities
 * @date 2026-07-23
 *
 * This module provides:
 * 1. Direct UART output functions (UartDebug_*)
 * 2. printf() retargeting to UART0 (automatic for Arm Compiler 6)
 *
 * Usage:
 *   - Include this header and call SYSCFG_DL_init() to initialize UART0
 *   - Use printf(), SD_LOG_*(), or UartDebug_*() functions
 *   - All output automatically goes to UART0 at 115200 baud
 */

#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART debug output (already initialized by SYSCFG_DL_init)
 * @note This is a no-op; UART0 is configured by ti_msp_dl_config
 */
void UartDebug_Init(void);

/**
 * @brief Send a single character via UART0
 * @param c Character to send
 */
void UartDebug_PutChar(char c);

/**
 * @brief Send a null-terminated string via UART0
 * @param str String to send
 */
void UartDebug_Print(const char *str);

/**
 * @brief Send a string with newline via UART0
 * @param str String to send
 */
void UartDebug_Println(const char *str);

/**
 * @brief Send an unsigned integer in decimal format
 * @param value Value to send
 */
void UartDebug_PrintU32(uint32_t value);

/**
 * @brief Send a signed integer in decimal format
 * @param value Value to send
 */
void UartDebug_PrintI32(int32_t value);

/**
 * @brief Send an unsigned integer in hexadecimal format
 * @param value Value to send
 * @param prefix If true, prepend "0x"
 */
void UartDebug_PrintHex(uint32_t value, bool prefix);

/**
 * @brief Send a float in decimal format (fixed 3 decimal places)
 * @param value Value to send
 */
void UartDebug_PrintFloat(float value);

#ifdef __cplusplus
}
#endif

#endif /* UART_DEBUG_H */
