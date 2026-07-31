/**
 * @file ir_uart_diagnostic.h
 * @brief UART4 and IR sensor low-level diagnostic tool
 * @date 2026-07-30
 */

#ifndef IR_UART_DIAGNOSTIC_H_
#define IR_UART_DIAGNOSTIC_H_

#include <stdint.h>

/**
 * @brief Run complete UART4 diagnostic suite
 *
 * Tests:
 * 1. UART4 configuration (baud, GPIO, interrupts)
 * 2. IR sensor communication (send request, check response)
 * 3. Register status dump
 *
 * Prints detailed diagnostic information to debug UART
 */
void IrUartDiag_RunAll(void);

/**
 * @brief Count received bytes (call from UART4 IRQ handler)
 * @param byte Received byte
 */
void IrUartDiag_CountRxByte(uint8_t byte);

#endif // IR_UART_DIAGNOSTIC_H_
