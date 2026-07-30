/**
 * @file      perception.c
 * @brief     Source file for perception module.
 * @author    joelin-CN
 * @version   1.0.1
 * @date      2026-07-30
 * @note      修复红外传感器黑线检测算法（黑线强度反转）
 */

#include "../inc/perception.h"

#include <math.h>
#include <string.h>

extern sens_decision_config_t g_sens_decision_config;

void perception_init(perception_t *perception) {
    if (perception == NULL) {
        return;
    }
    memset(perception, 0, sizeof(perception_t));
}

/**
 * @brief 更新感知结果（修复版：黑线强度反转算法）
 *
 * @note 核心修复（2026-07-30）:
 *       1. 黑线强度反转: black_strength = white_reference - raw_value
 *       2. 阈值判断修正: 基于黑线强度而非原始值
 *       3. 质心计算修正: 使用黑线强度加权，而非原始反射值
 *
 * @algorithm 黑线检测逻辑:
 *       - 白色背景: raw ≈ 270, black_strength ≈ 0 → 不激活
 *       - 黑色线条: raw ≈ 100, black_strength ≈ 170 → 激活
 *       - 阈值: black_strength > threshold (默认50) → 检测到黑线
 *
 * @see docs/IR_SENSOR_FIX_2026-07-30.md - 完整修复方案文档
 */
sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result) {
    float black_strength[SD_IR_CHANNEL_COUNT];
    float weighted_sum = 0.0f;
    float strength_sum = 0.0f;
    float max_abs_weight = 0.0f;
    uint8_t active_count = 0;
    size_t i;
    float derivative;
    float dt_s;

    if (perception == NULL || ir_data == NULL || result == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    if (perception->initialized && timestamp_us <= perception->prev_timestamp_us) {
        return SD_ERR_DATA_INVALID;
    }

    memset(result, 0, sizeof(perception_result_t));
    result->active_mask = ir_data->active_mask;

    /* 步骤1: 计算黑线强度（核心修复）*/
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        float raw_value = ir_data->values[i];
        float white_ref = g_sens_decision_config.perception.white_reference[i];

        /* 黑线强度 = 白色参考 - 当前值
         * - 白色区域: 强度接近0
         * - 黑线区域: 强度高（约170）*/
        black_strength[i] = white_ref - raw_value;

        /* 防止负值（传感器读数异常高于白色参考）*/
        if (black_strength[i] < 0.0f) {
            black_strength[i] = 0.0f;
        }
    }

    /* 步骤2: 黑线检测和质心计算 */
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        float weight = g_sens_decision_config.perception.weights[i];
        float abs_weight = fabsf(weight);
        float strength = black_strength[i];

        if (abs_weight > max_abs_weight) {
            max_abs_weight = abs_weight;
        }

        /* 阈值判断：黑线强度高于阈值才激活 */
        if (strength > g_sens_decision_config.perception.black_strength_threshold) {
            ++active_count;

            /* 加权质心计算：使用黑线强度而非原始值 */
            weighted_sum += weight * strength;
            strength_sum += strength;
        }
    }

    result->line_valid = (active_count > 0);

    /* 步骤3: 横向偏差计算 */
    if (active_count == 0) {
        ++perception->lost_count;
        result->lost_count = perception->lost_count;
        result->event = ROAD_EVENT_LINE_LOST;
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;
    } else {
        perception->lost_count = 0;
        result->lost_count = 0;

        /* 使用黑线强度总和归一化（物理意义明确）*/
        if (strength_sum > 1e-6f) {
            result->lateral_error = weighted_sum / strength_sum;
        } else {
            result->lateral_error = 0.0f;
        }

        /* 步骤4: 航向误差估计（时间导数）*/
        if (perception->initialized) {
            dt_s = (timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
            if (dt_s > 0.0f) {
                derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                perception->heading_error =
                    g_sens_decision_config.perception.heading_filter_alpha * perception->heading_error +
                    (1.0f - g_sens_decision_config.perception.heading_filter_alpha) * derivative;
            }
        } else {
            if (timestamp_us > perception->prev_timestamp_us) {
                dt_s = (timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
                if (dt_s > 0.0f) {
                    derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                    perception->heading_error = derivative;
                } else {
                    perception->heading_error = 0.0f;
                }
            } else {
                perception->heading_error = 0.0f;
            }
        }

        result->heading_error = perception->heading_error;

        /* 步骤5: 道路事件检测 */
        if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
            result->event = ROAD_EVENT_INTERSECTION;
        } else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
                   fabsf(result->heading_error) >= g_sens_decision_config.perception.curve_derivative_threshold) {
            result->event = ROAD_EVENT_CURVE_ENTRY;
        } else {
            result->event = ROAD_EVENT_NONE;
        }
    }

    perception->prev_lateral_error = result->lateral_error;
    perception->prev_timestamp_us = timestamp_us;
    perception->initialized = true;

    return SD_OK;
}
