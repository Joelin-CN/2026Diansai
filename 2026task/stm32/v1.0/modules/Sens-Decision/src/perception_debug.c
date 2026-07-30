/**
 * @file      perception_debug.c
 * @brief     感知模块调试工具实现
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-30
 */

#include "perception_debug.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

extern sens_decision_config_t g_sens_decision_config;

void perception_debug_print(const ir_array_data_t *ir_data,
                           const perception_result_t *result) {
    if (ir_data == NULL || result == NULL) {
        printf("[Perception Debug] NULL pointer error\r\n");
        return;
    }

    const sd_perception_config_t *config = &g_sens_decision_config.perception;

    printf("\r\n========== Perception Debug ==========\r\n");

    // 通道编号
    printf("Channel:       ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7u ", i);
    }
    printf("\r\n");

    // 原始值
    printf("Raw ADC:       ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.0f ", ir_data->values[i]);
    }
    printf("\r\n");

    // 白色参考
    printf("White Ref:     ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.0f ", config->white_reference[i]);
    }
    printf("\r\n");

    // 黑线强度
    printf("Black Strength:");
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        float strength = config->white_reference[i] - ir_data->values[i];
        if (strength < 0.0f) {
            strength = 0.0f;
        }
        printf("%7.0f ", strength);

        if (strength > config->black_strength_threshold) {
            active_count++;
        }
    }
    printf("\r\n");

    // 权重
    printf("Weights:       ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%7.2f ", config->weights[i]);
    }
    printf("\r\n");

    // 激活掩码（二进制）
    printf("Active Mask:   0x%04X (binary: ", result->active_mask);
    for (int8_t i = 7; i >= 0; i--) {
        printf("%c", (active_count > i) ? '1' : '0');
        if (i == 4) printf(" ");
    }
    printf(")\r\n");

    printf("--------------------------------------\r\n");
    printf("Threshold:     %.1f\r\n", config->black_strength_threshold);
    printf("Active:        %u / %u channels\r\n", active_count, SD_IR_CHANNEL_COUNT);
    printf("Lateral Error: %+.3f\r\n", result->lateral_error);
    printf("Heading Error: %+.3f\r\n", result->heading_error);
    printf("Line Valid:    %s\r\n", result->line_valid ? "YES" : "NO");
    printf("Lost Count:    %u\r\n", result->lost_count);

    // 道路事件
    printf("Event:         ");
    switch (result->event) {
        case ROAD_EVENT_NONE:
            printf("NORMAL TRACKING\r\n");
            break;
        case ROAD_EVENT_CURVE_ENTRY:
            printf("CURVE ENTRY\r\n");
            break;
        case ROAD_EVENT_INTERSECTION:
            printf("INTERSECTION\r\n");
            break;
        case ROAD_EVENT_LINE_LOST:
            printf("LINE LOST\r\n");
            break;
        default:
            printf("UNKNOWN\r\n");
            break;
    }

    printf("======================================\r\n\r\n");
}

void perception_debug_print_compact(const ir_array_data_t *ir_data,
                                   const perception_result_t *result) {
    if (ir_data == NULL || result == NULL) {
        return;
    }

    const sd_perception_config_t *config = &g_sens_decision_config.perception;

    // 原始值（紧凑）
    printf("[IR] Raw: ");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        printf("%3.0f ", ir_data->values[i]);
    }

    // 黑线强度（紧凑）
    printf("| Str: ");
    uint8_t active_count = 0;
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        float strength = config->white_reference[i] - ir_data->values[i];
        if (strength < 0.0f) {
            strength = 0.0f;
        }
        printf("%3.0f ", strength);

        if (strength > config->black_strength_threshold) {
            active_count++;
        }
    }

    // 状态
    printf("| Act: %u/8 | Err: %+.3f", active_count, result->lateral_error);

    // 事件标记
    if (result->event == ROAD_EVENT_INTERSECTION) {
        printf(" | INTERSECTION");
    } else if (result->event == ROAD_EVENT_LINE_LOST) {
        printf(" | LINE_LOST");
    } else if (result->event == ROAD_EVENT_CURVE_ENTRY) {
        printf(" | CURVE");
    }

    printf("\r\n");
}

bool perception_debug_selfcheck(void) {
    const sd_perception_config_t *config = &g_sens_decision_config.perception;
    bool passed = true;

    printf("\r\n========== Perception Self-Check ==========\r\n");

    // 检查1: 白色参考值
    printf("[1] White Reference Values:\r\n");
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        float ref = config->white_reference[i];
        if (ref < 50.0f || ref > 500.0f) {
            printf("  ❌ Channel %u: %.1f (out of range 50-500)\r\n", i, ref);
            passed = false;
        } else {
            printf("  ✓ Channel %u: %.1f\r\n", i, ref);
        }
    }

    // 检查2: 黑线强度阈值
    printf("\r\n[2] Black Strength Threshold:\r\n");
    float threshold = config->black_strength_threshold;
    if (threshold < 5.0f || threshold > 250.0f) {
        printf("  ❌ Threshold: %.1f (out of range 5-250)\r\n", threshold);
        passed = false;
    } else {
        printf("  ✓ Threshold: %.1f\r\n", threshold);
    }

    // 检查3: 权重配置
    printf("\r\n[3] Sensor Weights:\r\n");
    bool has_negative = false;
    bool has_positive = false;
    float weight_sum = 0.0f;

    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        float w = config->weights[i];
        printf("  Channel %u: %+.4f\r\n", i, w);

        if (fabsf(w) < 1e-6f) {
            printf("    ⚠️  Warning: weight is zero\r\n");
        }

        if (w < 0.0f) has_negative = true;
        if (w > 0.0f) has_positive = true;
        weight_sum += w;
    }

    if (!has_negative || !has_positive) {
        printf("  ❌ Weights must have both positive and negative values\r\n");
        passed = false;
    } else {
        printf("  ✓ Weights have both signs\r\n");
    }

    if (fabsf(weight_sum) > 1.0f) {
        printf("  ⚠️  Warning: weight sum = %.3f (should be close to 0)\r\n", weight_sum);
    } else {
        printf("  ✓ Weight sum ≈ 0 (%.3f)\r\n", weight_sum);
    }

    // 检查4: 路口检测配置
    printf("\r\n[4] Intersection Detection:\r\n");
    uint8_t int_channels = config->intersection_active_channels;
    if (int_channels < 4 || int_channels > 8) {
        printf("  ❌ Active channels threshold: %u (should be 4-8)\r\n", int_channels);
        passed = false;
    } else {
        printf("  ✓ Active channels threshold: %u\r\n", int_channels);
    }

    // 检查5: 滤波器参数
    printf("\r\n[5] Filter Parameters:\r\n");
    float alpha = config->heading_filter_alpha;
    if (alpha < 0.0f || alpha > 1.0f) {
        printf("  ❌ Heading filter alpha: %.3f (out of range 0-1)\r\n", alpha);
        passed = false;
    } else {
        printf("  ✓ Heading filter alpha: %.3f\r\n", alpha);
    }

    // 总结
    printf("\r\n===========================================\r\n");
    if (passed) {
        printf("✅ All checks PASSED\r\n");
    } else {
        printf("❌ Some checks FAILED - please review configuration\r\n");
    }
    printf("===========================================\r\n\r\n");

    return passed;
}
