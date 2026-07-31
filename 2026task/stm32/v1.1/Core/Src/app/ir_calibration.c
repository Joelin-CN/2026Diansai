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
    printf("\r\n========== IR Configuration ==========\r\n");
    osDelay(20);

    printf("White Reference:\r\n");
    osDelay(10);
    printf("  Ch: ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%6u ", i);
    }
    printf("\r\n");
    osDelay(10);

    printf("  Val:");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%6.0f ", g_sens_decision_config.perception.white_reference[i]);
    }
    printf("\r\n");
    osDelay(10);

    printf("\r\nThreshold: %.1f\r\n", g_sens_decision_config.perception.black_strength_threshold);
    osDelay(10);

    printf("\r\nWeights:\r\n");
    osDelay(10);
    printf("  Ch: ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%6u ", i);
    }
    printf("\r\n");
    osDelay(10);

    printf("  Wgt:");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%6.2f ", g_sens_decision_config.perception.weights[i]);
    }
    printf("\r\n");
    osDelay(10);

    printf("======================================\r\n");
    osDelay(50);
}

void IrCalibration_Monitor(uint32_t duration_ms, uint32_t interval_ms) {
    printf("\r\n=== IR Monitor ===\r\n");
    osDelay(100);
    printf("Duration: %lu ms, Interval: %lu ms\r\n", (unsigned long)duration_ms, (unsigned long)interval_ms);
    osDelay(100);

    uint32_t elapsed = 0;
    uint32_t sample_count = 0;

    while (elapsed < duration_ms) {
        IrUartSensor_Process();
        uint16_t raw[SD_IR_CHANNEL_COUNT];

        if (IrUartSensor_GetAnalog(raw)) {
            sample_count++;

            // 计算黑线强度和横向偏差
            uint8_t active_count = 0;
            float weighted_sum = 0.0f;
            float strength_sum = 0.0f;

            for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
                float strength = g_sens_decision_config.perception.white_reference[i] - (float)raw[i];
                if (strength < 0.0f) {
                    strength = 0.0f;
                }

                if (strength > g_sens_decision_config.perception.black_strength_threshold) {
                    active_count++;
                    weighted_sum += g_sens_decision_config.perception.weights[i] * strength;
                    strength_sum += strength;
                }
            }

            float lateral_error = 0.0f;
            if (strength_sum > 1e-6f) {
                lateral_error = weighted_sum / strength_sum;
            }

            // 分段输出，每段后延迟
            printf("[%5lu]", (unsigned long)elapsed);
            osDelay(20);

            for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
                printf(" %4u", raw[i]);
                if (i == 3) osDelay(20);  // 中间延迟一次
            }
            osDelay(20);

            printf(" | A:%u E:%+.2f", active_count, lateral_error);
            osDelay(20);

            if (active_count == 0) {
                printf(" LOST");
            }
            printf("\r\n");
            osDelay(50);  // 行尾大延迟
        } else {
            printf("[%5lu] No data\r\n", (unsigned long)elapsed);
            osDelay(50);
        }

        osDelay(interval_ms);
        elapsed += interval_ms;
    }

    printf("Complete. Samples: %lu\r\n", (unsigned long)sample_count);
    osDelay(100);
    printf("==================\r\n");
    osDelay(100);
}

void IrCalibration_OneStep(void) {
    printf("\r\n========== IR One-Step Calibration ==========\r\n");
    osDelay(20);
    printf("[INFO] Place robot on track:\r\n");
    osDelay(10);
    printf("  Sensors 3&4 (center) on BLACK line\r\n");
    osDelay(10);
    printf("  Sensors 0,1,2,5,6,7 on WHITE surface\r\n");
    osDelay(10);
    printf("[INFO] Stabilizing (2 sec)...\r\n");
    osDelay(10);

    osDelay(2000);  // 稳定时间

    // 采样统计
    float sum_white[6];  // 存储白色探头(0,1,2,5,6,7)的累积值
    float sum_black[2];  // 存储黑色探头(3,4)的累积值
    memset(sum_white, 0, sizeof(sum_white));
    memset(sum_black, 0, sizeof(sum_black));

    const uint16_t samples = 100;
    uint16_t successful_reads = 0;

    printf("[INFO] Sampling 100 times...\r\n");
    osDelay(10);

    for (uint16_t n = 0; n < samples; n++) {
        IrUartSensor_Process();
        uint16_t raw[SD_IR_CHANNEL_COUNT];

        if (IrUartSensor_GetAnalog(raw)) {
            // 白色探头累加
            sum_white[0] += (float)raw[0];
            sum_white[1] += (float)raw[1];
            sum_white[2] += (float)raw[2];
            sum_white[3] += (float)raw[5];
            sum_white[4] += (float)raw[6];
            sum_white[5] += (float)raw[7];

            // 黑色探头累加
            sum_black[0] += (float)raw[3];
            sum_black[1] += (float)raw[4];

            successful_reads++;
        }

        osDelay(10);  // 10ms采样间隔
    }

    if (successful_reads < samples / 2) {
        printf("[ERROR] Failed: only %u/%u reads\r\n", successful_reads, samples);
        osDelay(20);
        printf("[ERROR] Check IR sensor connection\r\n");
        osDelay(20);
        return;
    }

    printf("[INFO] Success: %u/%u samples\r\n", successful_reads, samples);
    osDelay(20);

    // 计算白色探头的平均值
    float white_avg = 0.0f;
    for (uint8_t i = 0; i < 6; i++) {
        white_avg += sum_white[i] / successful_reads;
    }
    white_avg /= 6.0f;

    // 计算黑色探头的平均值
    float black_avg = (sum_black[0] + sum_black[1]) / (2.0f * successful_reads);

    // 检测传感器类型：黑色读数更高说明是反向传感器
    bool sensor_inverted = (black_avg > white_avg);

    if (sensor_inverted) {
        // 反向传感器：黑色高值，白色低值
        // white_reference设为黑色读数（作为参考基准）
        for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
            g_sens_decision_config.perception.white_reference[i] = black_avg;
        }
        // 黑线强度 = 黑色值 - 白色值（正值）
        float black_strength = black_avg - white_avg;
        // 阈值：取黑线强度的一半作为判定标准
        // 这样只有strength < 一半时才认为是黑线
        g_sens_decision_config.perception.black_strength_threshold = black_strength * 0.5f;
    } else {
        // 正向传感器：白色高值，黑色低值（原逻辑）
        for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
            g_sens_decision_config.perception.white_reference[i] = white_avg;
        }
        float black_strength = white_avg - black_avg;
        if (black_strength < 0.0f) {
            black_strength = 0.0f;
        }
        g_sens_decision_config.perception.black_strength_threshold = black_strength * 0.6f;
    }

    // 重新计算用于显示
    float black_strength = sensor_inverted ? (black_avg - white_avg) : (white_avg - black_avg);
    if (black_strength < 0.0f) {
        black_strength = 0.0f;
    }

    // 设置阈值为黑线强度的60%
    g_sens_decision_config.perception.black_strength_threshold = black_strength * 0.6f;

    // 打印结果
    printf("\r\n[RESULTS]\r\n");
    osDelay(10);
    printf("  Sensor type: %s\r\n", sensor_inverted ? "INVERTED (black=high)" : "NORMAL (white=high)");
    osDelay(10);
    printf("  White avg: %.1f\r\n", white_avg);
    osDelay(10);
    printf("  Black avg: %.1f\r\n", black_avg);
    osDelay(10);
    printf("  Strength:  %.1f\r\n", black_strength);
    osDelay(10);
    printf("  Threshold: %.1f (%s)\r\n", g_sens_decision_config.perception.black_strength_threshold,
           sensor_inverted ? "50%" : "60%");
    osDelay(10);

    // 验证黑线强度是否足够
    if (black_strength < 20.0f) {
        printf("\r\n[WARNING] Strength too low (%.1f)\r\n", black_strength);
        osDelay(20);
        printf("  Check: Sensors 3&4 on black line?\r\n");
        osDelay(20);
    } else {
        printf("\r\n[SUCCESS] Calibration complete!\r\n");
        osDelay(20);
    }

    printf("=============================================\r\n");
    osDelay(50);
}
