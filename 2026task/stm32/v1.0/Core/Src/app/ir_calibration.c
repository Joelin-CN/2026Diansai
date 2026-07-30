/**
 * @file      ir_calibration.c
 * @brief     红外传感器校准工具实现
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-30
 */

#include "ir_calibration.h"
#include "ir_uart_sensor.h"
#include "config.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>

extern sens_decision_config_t g_sens_decision_config;

void IrCalibration_WhiteBalance(void) {
    printf("\r\n========== IR White Balance Calibration ==========\r\n");
    printf("[INFO] Place robot on WHITE surface (no black line)\r\n");
    printf("[INFO] Stabilizing... (1 second)\r\n");

    osDelay(1000);  // 等待稳定

    float sum[SD_IR_CHANNEL_COUNT];
    memset(sum, 0, sizeof(sum));
    const uint16_t samples = 100;
    uint16_t successful_reads = 0;

    printf("[INFO] Sampling %u times (interval: 10ms)...\r\n", samples);

    for (uint16_t n = 0; n < samples; n++) {
        // 驱动帧解析器
        IrUartSensor_Process();

        uint16_t raw[SD_IR_CHANNEL_COUNT];
        if (IrUartSensor_GetAnalog(raw)) {
            for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
                sum[i] += (float)raw[i];
            }
            successful_reads++;
        }

        osDelay(10);  // 10ms采样间隔
    }

    if (successful_reads < samples / 2) {
        printf("[ERROR] Calibration failed: only %u/%u successful reads\r\n",
               successful_reads, samples);
        printf("[ERROR] Check IR sensor connection and try again\r\n");
        return;
    }

    // 计算平均值并更新配置
    printf("[INFO] Calibration successful (%u/%u samples)\r\n", successful_reads, samples);
    printf("[INFO] White reference values:\r\n");
    printf("  Channel:  ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", i);
    }
    printf("\r\n");

    printf("  Value:    ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        g_sens_decision_config.perception.white_reference[i] = sum[i] / successful_reads;
        printf("%7.1f ", g_sens_decision_config.perception.white_reference[i]);
    }
    printf("\r\n");

    printf("[SUCCESS] White balance calibration complete!\r\n");
    printf("===================================================\r\n\r\n");
}

void IrCalibration_BlackThreshold(void) {
    printf("\r\n========== IR Black Threshold Calibration ==========\r\n");
    printf("[INFO] Place robot CENTERED on BLACK line\r\n");
    printf("[INFO] Stabilizing... (1 second)\r\n");

    osDelay(1000);

    // 读取当前传感器值
    IrUartSensor_Process();
    uint16_t raw[SD_IR_CHANNEL_COUNT];

    if (!IrUartSensor_GetAnalog(raw)) {
        printf("[ERROR] Failed to read IR sensor\r\n");
        printf("[ERROR] Check sensor connection and try again\r\n");
        return;
    }

    // 计算黑线强度
    printf("[INFO] Current sensor readings:\r\n");
    printf("  Channel:       ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", i);
    }
    printf("\r\n");

    printf("  Raw value:     ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", raw[i]);
    }
    printf("\r\n");

    printf("  White ref:     ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.0f ", g_sens_decision_config.perception.white_reference[i]);
    }
    printf("\r\n");

    printf("  Black strength:");
    float max_strength = 0.0f;
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        float strength = g_sens_decision_config.perception.white_reference[i] - (float)raw[i];
        if (strength < 0.0f) {
            strength = 0.0f;
        }
        printf("%7.0f ", strength);

        if (strength > max_strength) {
            max_strength = strength;
        }
    }
    printf("\r\n");

    if (max_strength < 20.0f) {
        printf("[WARNING] Maximum black strength too low (%.1f)\r\n", max_strength);
        printf("[WARNING] Possible issues:\r\n");
        printf("  - Robot not on black line\r\n");
        printf("  - White balance not calibrated\r\n");
        printf("  - Black line contrast too low\r\n");
    }

    // 设置阈值为最大强度的50%
    g_sens_decision_config.perception.black_strength_threshold = max_strength * 0.5f;

    printf("[INFO] Maximum black strength: %.1f\r\n", max_strength);
    printf("[INFO] Threshold set to: %.1f (50%% of max)\r\n",
           g_sens_decision_config.perception.black_strength_threshold);

    printf("[SUCCESS] Black threshold calibration complete!\r\n");
    printf("====================================================\r\n\r\n");
}

void IrCalibration_PrintConfig(void) {
    printf("\r\n========== IR Calibration Configuration ==========\r\n");

    printf("White Reference Values:\r\n");
    printf("  Channel: ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", i);
    }
    printf("\r\n");

    printf("  Value:   ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.1f ", g_sens_decision_config.perception.white_reference[i]);
    }
    printf("\r\n");

    printf("\r\nBlack Strength Threshold: %.1f\r\n",
           g_sens_decision_config.perception.black_strength_threshold);

    printf("\r\nSensor Weights:\r\n");
    printf("  Channel: ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", i);
    }
    printf("\r\n");

    printf("  Weight:  ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.2f ", g_sens_decision_config.perception.weights[i]);
    }
    printf("\r\n");

    printf("===================================================\r\n\r\n");
}

void IrCalibration_Monitor(uint32_t duration_ms, uint32_t interval_ms) {
    printf("\r\n========== IR Sensor Real-Time Monitor ==========\r\n");
    printf("[INFO] Monitoring for %u ms (interval: %u ms)\r\n", duration_ms, interval_ms);
    printf("[INFO] Press Ctrl+C to stop (if supported)\r\n\r\n");

    uint32_t elapsed = 0;
    uint32_t sample_count = 0;

    while (elapsed < duration_ms) {
        IrUartSensor_Process();
        uint16_t raw[SD_IR_CHANNEL_COUNT];

        if (IrUartSensor_GetAnalog(raw)) {
            sample_count++;

            printf("[%6u ms] Sample #%u:\r\n", elapsed, sample_count);

            // 原始值
            printf("  Raw:      ");
            for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
                printf("%4u ", raw[i]);
            }
            printf("\r\n");

            // 黑线强度
            printf("  Strength: ");
            uint8_t active_count = 0;
            float weighted_sum = 0.0f;
            float strength_sum = 0.0f;

            for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
                float strength = g_sens_decision_config.perception.white_reference[i] - (float)raw[i];
                if (strength < 0.0f) {
                    strength = 0.0f;
                }
                printf("%4.0f ", strength);

                if (strength > g_sens_decision_config.perception.black_strength_threshold) {
                    active_count++;
                    weighted_sum += g_sens_decision_config.perception.weights[i] * strength;
                    strength_sum += strength;
                }
            }
            printf("\r\n");

            // 状态
            float lateral_error = 0.0f;
            if (strength_sum > 1e-6f) {
                lateral_error = weighted_sum / strength_sum;
            }

            printf("  Active channels: %u/8\r\n", active_count);
            printf("  Lateral error: %+.3f\r\n", lateral_error);

            if (active_count == 0) {
                printf("  Status: LINE LOST\r\n");
            } else if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
                printf("  Status: INTERSECTION DETECTED\r\n");
            } else {
                printf("  Status: LINE TRACKING\r\n");
            }

            printf("\r\n");
        } else {
            printf("[%6u ms] No data available\r\n", elapsed);
        }

        osDelay(interval_ms);
        elapsed += interval_ms;
    }

    printf("[INFO] Monitoring complete. Total samples: %u\r\n", sample_count);
    printf("==================================================\r\n\r\n");
}
