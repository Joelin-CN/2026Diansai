/**
 * @file ir_sensor_test.c
 * @brief IR sensor hardware test and verification implementation
 * @date 2026-07-30
 */

#include "ir_sensor_test.h"
#include "ir_uart_sensor.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

// Test configuration
#define IR_TEST_DURATION_MS     10000   // 10 second test
#define IR_TEST_INTERVAL_MS     200     // Print every 200ms
#define IR_MONITOR_INTERVAL_MS  200     // Monitor print interval

// Statistics
static uint32_t test_frame_count = 0;
static uint32_t test_error_count = 0;
static uint32_t test_start_tick = 0;

/**
 * @brief Print IR sensor values in readable format
 */
static void IrSensorTest_PrintValues(uint16_t values[IR_UART_SENSOR_COUNT], uint32_t elapsed_ms)
{
    printf("[%5lu ms] IR: ", elapsed_ms);

    for (uint8_t i = 0; i < IR_UART_SENSOR_COUNT; i++) {
        printf("%4u ", values[i]);
    }

    printf("| Frames: %lu, Errors: %lu\r\n", test_frame_count, test_error_count);
}

/**
 * @brief Print test header
 */
static void IrSensorTest_PrintHeader(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("   IR Sensor Test (USART2 @ 115200)   \r\n");
    printf("========================================\r\n");
    printf("Hardware: PA2(TX), PA3(RX)\r\n");
    printf("Protocol: $A<data># frame format\r\n");
    printf("Channels: 8-way analog values\r\n");
    printf("Duration: %lu seconds\r\n", IR_TEST_DURATION_MS / 1000);
    printf("========================================\r\n\r\n");
}

/**
 * @brief Print test summary
 */
static void IrSensorTest_PrintSummary(uint32_t duration_ms)
{
    float frame_rate = (float)test_frame_count / (duration_ms / 1000.0f);
    float error_rate = test_frame_count > 0
                       ? (float)test_error_count / test_frame_count * 100.0f
                       : 0.0f;

    printf("\r\n");
    printf("========================================\r\n");
    printf("           Test Summary                \r\n");
    printf("========================================\r\n");
    printf("Duration:    %lu ms\r\n", duration_ms);
    printf("Frames RX:   %lu\r\n", test_frame_count);
    printf("Errors:      %lu\r\n", test_error_count);
    printf("Frame rate:  %.2f Hz\r\n", frame_rate);
    printf("Error rate:  %.2f %%\r\n", error_rate);
    printf("========================================\r\n");

    // Verdict
    if (test_frame_count == 0) {
        printf("❌ FAIL: No frames received\r\n");
        printf("   Check: UART4 wiring, sensor power, baud rate\r\n");
    } else if (error_rate > 10.0f) {
        printf("⚠️  WARN: High error rate\r\n");
        printf("   Check: Signal integrity, EMI, grounding\r\n");
    } else if (frame_rate < 1.0f) {
        printf("⚠️  WARN: Low frame rate\r\n");
        printf("   Expected: ~5-10 Hz in analog mode\r\n");
    } else {
        printf("✅ PASS: IR sensor working normally\r\n");
    }

    printf("========================================\r\n\r\n");
}

void IrSensorTest_Run(void)
{
    uint16_t ir_values[IR_UART_SENSOR_COUNT];
    uint32_t last_print_tick = 0;

    // Reset statistics
    test_frame_count = 0;
    test_error_count = 0;

    IrSensorTest_PrintHeader();

    printf("Initializing IR sensor...\r\n");
    IrUartSensor_Init();
    HAL_Delay(100);  // Wait for sensor to stabilize

    // CRITICAL FIX: Force enable RXNEIE (some HAL functions may disable it)
    extern UART_HandleTypeDef huart2;
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
    printf("RXNEIE forced ON (CR1=0x%08lX)\r\n", huart2.Instance->CR1);

    printf("Requesting analog mode...\r\n");
    IrUartSensor_RequestAnalogMode();
    HAL_Delay(100);

    printf("Starting data acquisition...\r\n\r\n");
    printf("Time     | CH0  CH1  CH2  CH3  CH4  CH5  CH6  CH7  | Stats\r\n");
    printf("----------------------------------------------------------------------\r\n");

    test_start_tick = HAL_GetTick();
    last_print_tick = test_start_tick;

    while ((HAL_GetTick() - test_start_tick) < IR_TEST_DURATION_MS) {
        // Process received data
        ir_uart_sensor_status_t status = IrUartSensor_Process();

        if (status == IR_UART_SENSOR_STATUS_OK) {
            test_frame_count++;
        } else if (status != IR_UART_SENSOR_STATUS_NO_FRAME) {
            test_error_count++;
        }

        // Print values at regular intervals
        if ((HAL_GetTick() - last_print_tick) >= IR_TEST_INTERVAL_MS) {
            if (IrUartSensor_GetAnalog(ir_values)) {
                uint32_t elapsed = HAL_GetTick() - test_start_tick;
                IrSensorTest_PrintValues(ir_values, elapsed);
            } else {
                printf("[%5lu ms] Waiting for first valid frame...\r\n",
                       HAL_GetTick() - test_start_tick);
            }
            last_print_tick = HAL_GetTick();
        }

        HAL_Delay(10);  // Small delay to avoid busy-waiting
    }

    // Print summary
    uint32_t total_duration = HAL_GetTick() - test_start_tick;
    IrSensorTest_PrintSummary(total_duration);
}

void IrSensorTest_Monitor(void)
{
    static uint32_t last_print_tick = 0;
    static bool initialized = false;
    uint16_t ir_values[IR_UART_SENSOR_COUNT];

    // Initialize on first call
    if (!initialized) {
        printf("\r\n=== IR Sensor Monitor Mode ===\r\n");
        printf("Polling every %lu ms...\r\n\r\n", IR_MONITOR_INTERVAL_MS);

        IrUartSensor_Init();
        HAL_Delay(100);
        IrUartSensor_RequestAnalogMode();

        last_print_tick = HAL_GetTick();
        initialized = true;
    }

    // Process incoming data
    IrUartSensor_Process();

    // Print at regular intervals
    if ((HAL_GetTick() - last_print_tick) >= IR_MONITOR_INTERVAL_MS) {
        if (IrUartSensor_GetAnalog(ir_values)) {
            printf("IR: ");
            for (uint8_t i = 0; i < IR_UART_SENSOR_COUNT; i++) {
                printf("%4u ", ir_values[i]);
            }
            printf("\r\n");
        } else {
            printf("IR: (no valid data)\r\n");
        }

        last_print_tick = HAL_GetTick();
    }
}
