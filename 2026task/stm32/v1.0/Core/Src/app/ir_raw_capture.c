/**
 * @file ir_raw_capture.c
 * @brief IR sensor raw data capture tool
 * @date 2026-07-30
 */

#include "ir_raw_capture.h"
#include "ir_uart_sensor.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define CAPTURE_BUFFER_SIZE 512

static uint8_t capture_buffer[CAPTURE_BUFFER_SIZE];
static uint16_t capture_index = 0;
static bool capture_active = false;

/**
 * @brief Capture raw byte (call from interrupt)
 */
void IrRawCapture_RxByte(uint8_t byte)
{
    if (capture_active && capture_index < CAPTURE_BUFFER_SIZE) {
        capture_buffer[capture_index++] = byte;
    }
}

/**
 * @brief Print byte in readable format
 */
static void print_byte(uint8_t byte)
{
    if (byte >= 32 && byte < 127) {
        printf("%c", byte);
    } else {
        printf("[%02X]", byte);
    }
}

/**
 * @brief Capture and display raw IR sensor data
 */
void IrRawCapture_Run(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("   IR RAW DATA CAPTURE                 \r\n");
    printf("========================================\r\n");
    printf("Duration: 2 seconds\r\n");
    printf("Buffer: %d bytes\r\n", CAPTURE_BUFFER_SIZE);
    printf("========================================\r\n\r\n");

    // Clear buffer
    memset(capture_buffer, 0, sizeof(capture_buffer));
    capture_index = 0;

    // Force enable RXNEIE
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    printf("Starting capture...\r\n");
    capture_active = true;

    // Capture for 2 seconds
    HAL_Delay(2000);

    capture_active = false;
    printf("Capture complete. Received %d bytes\r\n\r\n", capture_index);

    if (capture_index == 0) {
        printf("❌ No data captured\r\n");
        return;
    }

    // Print as ASCII/Hex
    printf("=== Raw Data (ASCII/Hex) ===\r\n");
    for (uint16_t i = 0; i < capture_index; i++) {
        print_byte(capture_buffer[i]);
        if ((i + 1) % 64 == 0) {
            printf("\r\n");
        }
    }
    printf("\r\n\r\n");

    // Print as pure hex
    printf("=== Raw Data (Hex only) ===\r\n");
    for (uint16_t i = 0; i < capture_index; i++) {
        printf("%02X ", capture_buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\r\n");
        }
    }
    printf("\r\n\r\n");

    // Look for frame markers
    printf("=== Frame Analysis ===\r\n");
    uint16_t dollar_count = 0;
    uint16_t hash_count = 0;
    uint16_t newline_count = 0;

    for (uint16_t i = 0; i < capture_index; i++) {
        if (capture_buffer[i] == '$') dollar_count++;
        if (capture_buffer[i] == '#') hash_count++;
        if (capture_buffer[i] == '\n') newline_count++;
    }

    printf("'$' (frame start): %d\r\n", dollar_count);
    printf("'#' (frame end):   %d\r\n", hash_count);
    printf("'\\n' (newline):    %d\r\n", newline_count);

    if (dollar_count > 0 && hash_count > 0) {
        printf("\r\n✅ Frame markers found!\r\n");
        printf("Possible frames: %d\r\n",
               dollar_count < hash_count ? dollar_count : hash_count);
    } else {
        printf("\r\n⚠️  No standard frame markers found\r\n");
    }

    // Try to extract one complete frame
    printf("\r\n=== First Complete Frame ===\r\n");
    int frame_start = -1;
    int frame_end = -1;

    for (uint16_t i = 0; i < capture_index; i++) {
        if (capture_buffer[i] == '$' && frame_start == -1) {
            frame_start = i;
        }
        if (capture_buffer[i] == '#' && frame_start != -1) {
            frame_end = i;
            break;
        }
    }

    if (frame_start != -1 && frame_end != -1) {
        printf("Frame at [%d-%d], length=%d bytes:\r\n",
               frame_start, frame_end, frame_end - frame_start + 1);
        for (int i = frame_start; i <= frame_end; i++) {
            print_byte(capture_buffer[i]);
        }
        printf("\r\n");
    } else {
        printf("No complete $...# frame found\r\n");
    }

    printf("\r\n========================================\r\n");
}
