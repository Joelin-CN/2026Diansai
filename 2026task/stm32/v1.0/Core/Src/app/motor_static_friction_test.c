/**
 * @file motor_static_friction_test.c
 * @brief 静摩擦补偿参数FF_K_STATIC手动标定测试实现
 * @date 2026-07-31
 */

#include "motor_static_friction_test.h"
#include "motor.h"
#include "encoder.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ============================================================================
 * 私有变量
 * ============================================================================ */

// 当前PWM状态
static int16_t s_current_left_pwm = 0;
static int16_t s_current_right_pwm = 0;

// 命令缓冲区
#define CMD_BUFFER_SIZE 32
static char s_cmd_buffer[CMD_BUFFER_SIZE];
static volatile uint8_t s_cmd_index = 0;

// 串口接收缓冲区
#define RX_BUFFER_SIZE 64
static uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t s_rx_write_idx = 0;
static volatile uint16_t s_rx_read_idx = 0;
static uint8_t s_uart_rx_byte;  // UART接收字节缓冲

/* ============================================================================
 * 私有函数声明
 * ============================================================================ */

static void ShowHelp(void);
static void ProcessCommand(char *cmd);

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

/**
 * @brief 从接收缓冲区读取一个字符
 * @return 字符，如果缓冲区为空返回-1
 */
static int GetCharFromBuffer(void) {
    if (s_rx_read_idx != s_rx_write_idx) {
        uint8_t c = s_rx_buffer[s_rx_read_idx];
        s_rx_read_idx = (s_rx_read_idx + 1) % RX_BUFFER_SIZE;
        return (int)c;
    }
    return -1;
}

void Motor_StaticFriction_InteractiveTest(void) {
    printf("\n\n");
    printf("========================================\n");
    printf("  FF_K_STATIC Calibration Test\n");
    printf("  Static Friction Manual Test Mode\n");
    printf("========================================\n");

    ShowHelp();

    printf("\nReady> ");
    fflush(stdout);

    // 启动UART5中断接收（每次接收1字节）
    extern UART_HandleTypeDef huart5;
    HAL_UART_Receive_IT(&huart5, &s_uart_rx_byte, 1);

    // 主循环：持续处理接收到的字符
    while (1) {
        // 从缓冲区读取字符
        int c = GetCharFromBuffer();

        if (c >= 0) {
            // 收到字符
            if (c == '\n' || c == '\r') {
                // 回车键：处理命令
                if (s_cmd_index > 0) {
                    s_cmd_buffer[s_cmd_index] = '\0';
                    ProcessCommand(s_cmd_buffer);
                    s_cmd_index = 0;
                }
                printf("\nReady> ");
                fflush(stdout);
            }
            else if (c == '\b' || c == 127) {
                // 退格键：删除字符
                if (s_cmd_index > 0) {
                    s_cmd_index--;
                    printf("\b \b");  // 回退、空格覆盖、再回退
                    fflush(stdout);
                }
            }
            else if (s_cmd_index < CMD_BUFFER_SIZE - 1 && c >= 32 && c <= 126) {
                // 存储可打印字符并回显
                s_cmd_buffer[s_cmd_index++] = (char)c;
                printf("%c", c);
                fflush(stdout);
            }
        }

        // 周期性调用编码器轮询（重要！）
        Encoder_Poll();

        // 短暂延时
        HAL_Delay(10);
    }
}

/**
 * @brief UART5接收完成回调（在中断中调用）
 * @note 需要在usart.c中调用此函数
 */
void Motor_StaticFriction_UART_RxCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART5) {
        // 将接收到的字节存入环形缓冲区
        s_rx_buffer[s_rx_write_idx] = s_uart_rx_byte;
        s_rx_write_idx = (s_rx_write_idx + 1) % RX_BUFFER_SIZE;

        // 继续接收下一个字节
        HAL_UART_Receive_IT(huart, &s_uart_rx_byte, 1);
    }
}

/* ============================================================================
 * 私有函数实现
 * ============================================================================ */

/**
 * @brief 显示帮助信息
 */
static void ShowHelp(void) {
    printf("\n");
    printf("========================================\n");
    printf("  Available Commands\n");
    printf("========================================\n");
    printf("  L <pwm>  - Test LEFT wheel (0-100)\n");
    printf("  R <pwm>  - Test RIGHT wheel (0-100)\n");
    printf("  B <pwm>  - Test BOTH wheels (0-100)\n");
    printf("  S        - STOP all motors\n");
    printf("  E        - Read encoder counts\n");
    printf("  C        - Clear encoder counts\n");
    printf("  H        - Show this help\n");
    printf("========================================\n");
    printf("\nExample:\n");
    printf("  L 65     -> Left wheel PWM = 65\n");
    printf("  E        -> Read encoders\n");
    printf("  S        -> Stop motors\n");
    printf("  C        -> Clear encoders\n");
    printf("========================================\n");
}

/**
 * @brief 处理用户命令
 * @param cmd 命令字符串
 */
static void ProcessCommand(char *cmd) {
    char command;
    int16_t pwm_value = 0;

    // 跳过前导空格
    while (*cmd == ' ') cmd++;

    // 空命令
    if (*cmd == '\0') {
        return;
    }

    // 解析命令
    command = toupper(cmd[0]);

    // 解析PWM值（如果有）
    if (sscanf(cmd + 1, "%hd", &pwm_value) < 1) {
        pwm_value = 0;  // 没有PWM参数的命令
    }

    switch (command) {
        case 'L':  // 左轮测试
            if (pwm_value >= 0 && pwm_value <= 100) {
                s_current_left_pwm = pwm_value;
                s_current_right_pwm = 0;
                Motor_SetSpeed(s_current_left_pwm, s_current_right_pwm);
                printf("[OK] Left wheel PWM = %d\n", s_current_left_pwm);
            } else {
                printf("[ERROR] PWM must be 0-100\n");
            }
            break;

        case 'R':  // 右轮测试
            if (pwm_value >= 0 && pwm_value <= 100) {
                s_current_left_pwm = 0;
                s_current_right_pwm = pwm_value;
                Motor_SetSpeed(s_current_left_pwm, s_current_right_pwm);
                printf("[OK] Right wheel PWM = %d\n", s_current_right_pwm);
            } else {
                printf("[ERROR] PWM must be 0-100\n");
            }
            break;

        case 'B':  // 双轮同时测试
            if (pwm_value >= 0 && pwm_value <= 100) {
                s_current_left_pwm = pwm_value;
                s_current_right_pwm = pwm_value;
                Motor_SetSpeed(s_current_left_pwm, s_current_right_pwm);
                printf("[OK] Both wheels PWM = %d\n", pwm_value);
            } else {
                printf("[ERROR] PWM must be 0-100\n");
            }
            break;

        case 'S':  // 停止
            s_current_left_pwm = 0;
            s_current_right_pwm = 0;
            Motor_Stop();
            printf("[OK] All motors stopped\n");
            break;

        case 'E':  // 读取编码器
            {
                int32_t left_count = Encoder_GetCount(0);   // 0=左轮
                int32_t right_count = Encoder_GetCount(1);  // 1=右轮
                printf("[Encoder] Left: %ld, Right: %ld\n", left_count, right_count);

                // 判断是否转动（阈值：20个脉冲）
                if (abs(left_count) >= 20) {
                    printf("          Left: ROTATED ✓\n");
                } else {
                    printf("          Left: NOT rotated\n");
                }

                if (abs(right_count) >= 20) {
                    printf("          Right: ROTATED ✓\n");
                } else {
                    printf("          Right: NOT rotated\n");
                }
            }
            break;

        case 'C':  // 清零编码器
            Encoder_ResetCount(0);  // 左轮
            Encoder_ResetCount(1);  // 右轮
            printf("[OK] Encoders cleared\n");
            break;

        case 'H':  // 帮助
            ShowHelp();
            break;

        default:
            printf("[ERROR] Unknown command '%c'\n", command);
            printf("Type 'H' for help\n");
            break;
    }
}
