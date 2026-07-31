/**
 * @file ir_uart_diagnostic.c
 * @brief UART4 and IR sensor low-level diagnostic tool
 * @date 2026-07-30
 */

#include "ir_uart_diagnostic.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

// Test statistics
static uint32_t rx_byte_count = 0;
static uint32_t tx_byte_count = 0;
static uint8_t last_rx_bytes[16] = {0};
static uint8_t last_rx_index = 0;

/**
 * @brief Test byte counter (call from UART4 IRQ handler)
 */
void IrUartDiag_CountRxByte(uint8_t byte)
{
    rx_byte_count++;
    last_rx_bytes[last_rx_index] = byte;
    last_rx_index = (last_rx_index + 1) % 16;
}

/**
 * @brief Print UART4 configuration
 */
static void IrUartDiag_PrintConfig(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("   USART2 Configuration Diagnostic     \r\n");
    printf("========================================\r\n");

    // Check if USART2 is initialized
    if (huart2.Instance == NULL) {
        printf("❌ ERROR: USART2 not initialized!\r\n");
        return;
    }

    printf("Instance:     USART2 (0x%08lX)\r\n", (uint32_t)huart2.Instance);
    printf("Baud Rate:    %lu\r\n", huart2.Init.BaudRate);
    printf("Word Length:  ");
    switch (huart2.Init.WordLength) {
        case UART_WORDLENGTH_8B: printf("8 bits\r\n"); break;
        case UART_WORDLENGTH_9B: printf("9 bits\r\n"); break;
        default: printf("Unknown\r\n");
    }
    printf("Stop Bits:    ");
    switch (huart2.Init.StopBits) {
        case UART_STOPBITS_1: printf("1\r\n"); break;
        case UART_STOPBITS_2: printf("2\r\n"); break;
        default: printf("Unknown\r\n");
    }
    printf("Parity:       ");
    switch (huart2.Init.Parity) {
        case UART_PARITY_NONE: printf("None\r\n"); break;
        case UART_PARITY_EVEN: printf("Even\r\n"); break;
        case UART_PARITY_ODD: printf("Odd\r\n"); break;
        default: printf("Unknown\r\n");
    }
    printf("Mode:         ");
    if (huart2.Init.Mode == (UART_MODE_TX_RX)) printf("TX + RX\r\n");
    else if (huart2.Init.Mode == UART_MODE_TX) printf("TX only\r\n");
    else if (huart2.Init.Mode == UART_MODE_RX) printf("RX only\r\n");
    else printf("Unknown\r\n");

    printf("Flow Control: ");
    switch (huart2.Init.HwFlowCtl) {
        case UART_HWCONTROL_NONE: printf("None\r\n"); break;
        case UART_HWCONTROL_RTS: printf("RTS\r\n"); break;
        case UART_HWCONTROL_CTS: printf("CTS\r\n"); break;
        case UART_HWCONTROL_RTS_CTS: printf("RTS+CTS\r\n"); break;
        default: printf("Unknown\r\n");
    }

    // Check GPIO configuration
    printf("\r\nGPIO Configuration:\r\n");
    printf("PA2 (TX): Mode=%lu, AF=%lu\r\n",
           (GPIOA->MODER >> (2*2)) & 0x3,
           (GPIOA->AFR[0] >> (2*4)) & 0xF);
    printf("PA3 (RX): Mode=%lu, AF=%lu\r\n",
           (GPIOA->MODER >> (3*2)) & 0x3,
           (GPIOA->AFR[0] >> (3*4)) & 0xF);

    // Check UART registers
    printf("\r\nUSART2 Register Status:\r\n");
    printf("CR1:  0x%08lX ", huart2.Instance->CR1);
    if (huart2.Instance->CR1 & USART_CR1_UE) printf("[UE=ON] ");
    if (huart2.Instance->CR1 & USART_CR1_TE) printf("[TE=ON] ");
    if (huart2.Instance->CR1 & USART_CR1_RE) printf("[RE=ON] ");
    if (huart2.Instance->CR1 & USART_CR1_RXNEIE) {
        printf("[RXNEIE=ON] ");
    } else {
        printf("[RXNEIE=OFF!] ⚠️  ");
    }
    printf("\r\n");

    printf("SR:   0x%08lX ", huart2.Instance->SR);
    if (huart2.Instance->SR & USART_SR_RXNE) printf("[RXNE] ");
    if (huart2.Instance->SR & USART_SR_TC) printf("[TC] ");
    if (huart2.Instance->SR & USART_SR_TXE) printf("[TXE] ");
    if (huart2.Instance->SR & USART_SR_ORE) printf("[ORE!] ");
    if (huart2.Instance->SR & USART_SR_FE) printf("[FE!] ");
    if (huart2.Instance->SR & USART_SR_PE) printf("[PE!] ");
    printf("\r\n");

    // Check NVIC
    printf("\r\nNVIC Configuration:\r\n");
    uint32_t irq_enabled = NVIC->ISER[USART2_IRQn / 32] & (1 << (USART2_IRQn % 32));
    uint32_t irq_pending = NVIC->ISPR[USART2_IRQn / 32] & (1 << (USART2_IRQn % 32));
    printf("USART2_IRQn (%d): %s, %s\r\n",
           USART2_IRQn,
           irq_enabled ? "ENABLED" : "DISABLED",
           irq_pending ? "PENDING" : "NOT PENDING");

    printf("========================================\r\n\r\n");
}

/**
 * @brief Test UART4 loopback (TX -> RX)
 */
static void IrUartDiag_LoopbackTest(void)
{
    printf("Loopback Test: Connect PC10 to PC11 physically\r\n");
    printf("Sending: HELLO\r\n");

    // Clear RX buffer
    rx_byte_count = 0;
    memset(last_rx_bytes, 0, sizeof(last_rx_bytes));
    last_rx_index = 0;

    // Send test string
    const char *test_str = "HELLO";
    HAL_UART_Transmit(&huart4, (uint8_t *)test_str, strlen(test_str), 1000);
    tx_byte_count += strlen(test_str);

    // Wait for reception
    HAL_Delay(100);

    printf("TX: %lu bytes, RX: %lu bytes\r\n", tx_byte_count, rx_byte_count);
    if (rx_byte_count > 0) {
        printf("RX Data: ");
        for (int i = 0; i < (rx_byte_count < 16 ? rx_byte_count : 16); i++) {
            printf("%c", last_rx_bytes[i]);
        }
        printf("\r\n");
    }
    printf("\r\n");
}

/**
 * @brief Test IR sensor communication
 */
static void IrUartDiag_SensorTest(void)
{
    printf("IR Sensor Communication Test\r\n");

    // CRITICAL: Force enable RXNEIE before test
    printf("Forcing RXNEIE enable...\r\n");
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    // Verify it's set
    if (huart2.Instance->CR1 & USART_CR1_RXNEIE) {
        printf("✅ RXNEIE now enabled (CR1=0x%08lX)\r\n", huart2.Instance->CR1);
    } else {
        printf("❌ RXNEIE still disabled! (CR1=0x%08lX)\r\n", huart2.Instance->CR1);
    }

    printf("Sending analog mode request: $0,1,0#\r\n");

    // Clear counters
    rx_byte_count = 0;
    tx_byte_count = 0;
    memset(last_rx_bytes, 0, sizeof(last_rx_bytes));
    last_rx_index = 0;

    // Send request
    const char *cmd = "$0,1,0#";
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), 1000);
    tx_byte_count += strlen(cmd);

    printf("Waiting 2 seconds for sensor response...\r\n");

    // Monitor for 2 seconds
    for (int i = 0; i < 20; i++) {
        HAL_Delay(100);
        if (rx_byte_count > 0) {
            printf("  [%d00ms] RX: %lu bytes\r\n", i+1, rx_byte_count);
        }
    }

    printf("\r\nResult: TX=%lu bytes, RX=%lu bytes\r\n", tx_byte_count, rx_byte_count);

    if (rx_byte_count == 0) {
        printf("❌ No response from sensor\r\n");
        printf("Check:\r\n");
        printf("  1. Sensor power (usually 5V)\r\n");
        printf("  2. TX/RX not swapped (STM32 TX->Sensor RX)\r\n");
        printf("  3. Common ground connected\r\n");
        printf("  4. Sensor firmware supports this protocol\r\n");
    } else {
        printf("✅ Received %lu bytes\r\n", rx_byte_count);
        printf("Raw data: ");
        for (int i = 0; i < (rx_byte_count < 16 ? rx_byte_count : 16); i++) {
            if (last_rx_bytes[i] >= 32 && last_rx_bytes[i] < 127) {
                printf("%c", last_rx_bytes[i]);
            } else {
                printf("[0x%02X]", last_rx_bytes[i]);
            }
        }
        printf("\r\n");
    }
    printf("\r\n");
}

/**
 * @brief Run complete UART4 diagnostic
 */
void IrUartDiag_RunAll(void)
{
    printf("\r\n");
    printf("************************************************\r\n");
    printf("*   USART2 / IR SENSOR DIAGNOSTIC SUITE       *\r\n");
    printf("************************************************\r\n");

    // Step 1: Configuration check
    IrUartDiag_PrintConfig();
    HAL_Delay(500);

    // Step 2: Sensor test
    IrUartDiag_SensorTest();
    HAL_Delay(500);

    printf("************************************************\r\n");
    printf("*   DIAGNOSTIC COMPLETE                        *\r\n");
    printf("************************************************\r\n\r\n");
}
