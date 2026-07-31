/**
 * @file      perception.c
 * @brief     Source file for perception module.
 * @author    joelin-CN
 * @version   1.2.0
 * @date      2026-07-31
 * @note      支持黑线ADC高/低两种极性
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
 * @brief 更新感知结果
 *
 * @note 2026-07-31实车串口验证:
 *       当前循迹模块黑线ADC高、白底ADC低，active_high=true。
 *       旧实现固定按“黑线ADC低”计算，导致白底六路被误判为0xE7，
 *       中央黑线两路反而未命中。本实现按active_high选择极性。
 *
 * @algorithm:
 *       decision_level = reference - threshold
 *       active_high=true : line_strength = raw - decision_level
 *       active_high=false: line_strength = decision_level - raw
 *       line_strength > 0且通道数据有效时判为黑线。
 */
sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result) {
    float line_strength[SD_IR_CHANNEL_COUNT];
    float weighted_sum = 0.0f;
    float strength_sum = 0.0f;
    float binary_weight_sum = 0.0f;
    uint16_t detected_mask = 0U;
    uint8_t best_start = 0U;
    uint8_t best_length = 0U;
    float best_energy = 0.0f;
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
    /*
     * active_mask in ir_data describes which channels are electrically valid.
     * result->active_mask must instead describe channels that currently see
     * the black line; otherwise a validity mask of 0xFFFF is mistaken for a
     * transverse finish line.
     */
    result->active_mask = 0U;

    /* 步骤1: 按实车传感器极性计算越过判决边界后的黑线置信度。 */
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        float raw_value = ir_data->values[i];
        float reference = g_sens_decision_config.perception.white_reference[i];
        float threshold = g_sens_decision_config.perception.black_strength_threshold;
        float decision_level = reference - threshold;

        if (g_sens_decision_config.perception.active_high) {
            line_strength[i] = raw_value - decision_level;
        } else {
            line_strength[i] = decision_level - raw_value;
        }

        if (line_strength[i] < 0.0f) {
            line_strength[i] = 0.0f;
        }
    }

    /* 步骤2: 生成黑线命中掩码。 */
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        if (((ir_data->active_mask & (uint16_t)(1U << i)) != 0U) &&
            line_strength[i] > 0.0f) {
            detected_mask |= (uint16_t)(1U << i);
        }
    }

    /*
     * 宽黑线可能同时覆盖中间4路。若其中某一路因器件差异刚好低于阈值，
     * 填补被两侧命中包围的单通道空洞，避免质心突然跳向一边。
     */
    for (i = 1U; i + 1U < SD_IR_CHANNEL_COUNT; ++i) {
        uint16_t bit = (uint16_t)(1U << i);
        bool valid = (ir_data->active_mask & bit) != 0U;
        bool left_on = (detected_mask & (uint16_t)(1U << (i - 1U))) != 0U;
        bool right_on = (detected_mask & (uint16_t)(1U << (i + 1U))) != 0U;
        if (valid && (detected_mask & bit) == 0U && left_on && right_on) {
            float neighbor_min = fminf(line_strength[i - 1U], line_strength[i + 1U]);
            line_strength[i] = 0.5f * neighbor_min;
            detected_mask |= bit;
        }
    }
    result->active_mask = detected_mask;

    /*
     * 找出最长连续黑区作为主轨迹。这样既完整支持中间4路同时激活，
     * 又不会让远端单路反光噪声把质心拉走。同长度时选择模拟能量更强者。
     */
    {
        uint8_t run_start = 0U;
        uint8_t run_length = 0U;
        float run_energy = 0.0f;

        for (i = 0U; i <= SD_IR_CHANNEL_COUNT; ++i) {
            bool on = (i < SD_IR_CHANNEL_COUNT) &&
                      ((detected_mask & (uint16_t)(1U << i)) != 0U);
            if (on) {
                if (run_length == 0U) {
                    run_start = (uint8_t)i;
                    run_energy = 0.0f;
                }
                ++run_length;
                run_energy += line_strength[i];
            } else if (run_length > 0U) {
                if (run_length > best_length ||
                    (run_length == best_length && run_energy > best_energy)) {
                    best_start = run_start;
                    best_length = run_length;
                    best_energy = run_energy;
                }
                run_length = 0U;
                run_energy = 0.0f;
            }
        }
    }

    result->line_valid = (best_length > 0U);

    /* 步骤3: 主连续黑区的横向偏差计算。 */
    if (best_length == 0U) {
        ++perception->lost_count;
        result->lost_count = perception->lost_count;
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;
    } else {
        perception->lost_count = 0;
        result->lost_count = 0;

        for (i = best_start; i < (size_t)(best_start + best_length); ++i) {
            float weight = g_sens_decision_config.perception.weights[i];
            float strength = line_strength[i];
            binary_weight_sum += weight;
            weighted_sum += weight * strength;
            strength_sum += strength;
        }

        /*
         * 宽线时以连续命中区域的几何中心为主(70%)，模拟强度质心为辅(30%)。
         * 中间四路命中时几何中心仍为0，不会被单路ADC增益差异明显拉偏。
         */
        if (strength_sum > 1e-6f) {
            float binary_centroid = binary_weight_sum / (float)best_length;
            float analog_centroid = weighted_sum / strength_sum;
            result->lateral_error =
                0.70f * binary_centroid + 0.30f * analog_centroid;
        } else {
            result->lateral_error = 0.0f;
        }

        /* 步骤4: 航向误差估计；限制宽线边缘切换造成的微分尖峰。 */
        if (perception->initialized) {
            dt_s = (timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
            if (dt_s > 0.0f) {
                derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                if (derivative > 2.0f) derivative = 2.0f;
                if (derivative < -2.0f) derivative = -2.0f;
                perception->heading_error =
                    g_sens_decision_config.perception.heading_filter_alpha * perception->heading_error +
                    (1.0f - g_sens_decision_config.perception.heading_filter_alpha) * derivative;
            }
        } else {
            perception->heading_error = 0.0f;
        }

        result->heading_error = perception->heading_error;
    }

    perception->prev_lateral_error = result->lateral_error;
    perception->prev_timestamp_us = timestamp_us;
    perception->initialized = true;

    return SD_OK;
}
