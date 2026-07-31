/**
 * @file motor_interactive_test.c
 * @brief 电机交互式串口测试实现
 * @date 2026-07-30
 */

#include "motor_interactive_test.h"
#include "motor_static_friction_test.h"
#include "motor.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RX_BUFFER_SIZE 64

static uint8_t rx_byte;
static char rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_index = 0;
static volatile uint8_t rx_complete = 0;

/**
 * @brief UART接收完成回调
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    // 先调用静摩擦测试模块的回调（如果在该模式下）
    Motor_StaticFriction_UART_RxCallback(huart);

    // 原有的交互式测试回调逻辑
    if (huart->Instance == UART5) {
        // 收到一个字符
        if (rx_byte == '\r' || rx_byte == '\n') {
            // 回车/换行：命令结束
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                rx_complete = 1;
            }
        } else if (rx_byte == 8 || rx_byte == 127) {
            // 退格键
            if (rx_index > 0) {
                rx_index--;
                printf("\b \b");  // 回显退格
            }
        } else if (rx_index < RX_BUFFER_SIZE - 1) {
            // 正常字符
            rx_buffer[rx_index++] = rx_byte;
            printf("%c", rx_byte);  // 回显字符
        }

        // 重新启动接收
        HAL_UART_Receive_IT(&huart5, &rx_byte, 1);
    }
}

void MotorInteractiveTest_Init(void) {
    printf("\r\n");
    printf("========================================\r\n");
    printf("    Motor Interactive Test Mode\r\n");
    printf("========================================\r\n");
    printf("Usage: Enter speed [-100 to +100]\r\n");
    printf("  Positive = Forward\r\n");
    printf("  Negative = Backward\r\n");
    printf("  0 = Stop\r\n");
    printf("Example: 50 (50%% forward)\r\n");
    printf("         -30 (30%% backward)\r\n");
    printf("========================================\r\n");
    printf("NOTE: Input ASCII text like \"20\" then press ENTER\r\n");
    printf("      NOT hex like 0x20\r\n");
    printf("========================================\r\n");
    printf("\r\n");
    printf("Enter speed> ");
    fflush(stdout);

    // 启动中断接收
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(&huart5, &rx_byte, 1);
    if (status != HAL_OK) {
        printf("\r\n[ERROR] Failed to start UART RX interrupt: %d\r\n", status);
    }
}

void MotorInteractiveTest_Loop(void) {
    // 轮询模式备选方案：如果中断不工作，用轮询接收
    static uint32_t last_check = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_check > 50) {  // 每50ms检查一次
        last_check = now;

        // 尝试读取一个字节（非阻塞）
        uint8_t byte;
        if (HAL_UART_Receive(&huart5, &byte, 1, 0) == HAL_OK) {
            // 收到字符，手动调用处理逻辑
            if (byte == '\r' || byte == '\n') {
                if (rx_index > 0) {
                    rx_buffer[rx_index] = '\0';
                    rx_complete = 1;
                    printf("\r\n[DEBUG] Received command: '%s'\r\n", rx_buffer);
                }
            } else if (byte == 8 || byte == 127) {
                if (rx_index > 0) {
                    rx_index--;
                    printf("\b \b");
                }
            } else if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = byte;
                printf("%c", byte);
            }
        }
    }

    if (rx_complete) {
        rx_complete = 0;

        // 解析输入
        int speed = atoi(rx_buffer);

        printf("[DEBUG] Parsed speed: %d\r\n", speed);

        // 范围检查
        if (speed < -100 || speed > 100) {
            printf("[ERROR] Speed out of range: %d (valid: -100 to +100)\r\n", speed);
        } else {
            // 设置电机速度（两个轮子一起转）
            Motor_SetSpeed(speed, speed);

            if (speed > 0) {
                printf("[OK] Both motors FORWARD at %d%%\r\n", speed);
            } else if (speed < 0) {
                printf("[OK] Both motors BACKWARD at %d%%\r\n", -speed);
            } else {
                printf("[OK] Both motors STOPPED\r\n");
            }
        }

        // 清空缓冲区
        rx_index = 0;
        memset(rx_buffer, 0, RX_BUFFER_SIZE);

        printf("\r\nEnter speed> ");
        fflush(stdout);
    }
}
