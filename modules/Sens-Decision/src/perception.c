/**
 * @file      perception.c
 * @brief     Source file for perception module.
 * @author    joelin-CN
 * @version   1.1.0
 * @date      2026-07-28
 * @note      v1.1.0 改动（analog perception fix）:
 *   ① 激活阈值降至 0.2，低于阈值的通道不参与重心计算，抑制环境光噪声
 *   ② 重心公式改为标准模拟量加权重心 Σ(w·v)/Σv，真正利用 ADC 归一化值
 *   ③ 丢线时 heading_error 指数衰减（不再冻结旧值），同时记录最后偏向符号
 */

#include "../inc/perception.h"

#include <math.h>
#include <string.h>

/** ① 参与重心计算的最低归一化激活阈值，抑制环境光噪声（原 0.5 数字阈值改为 0.2 模拟阈值） */
#define PERCEPTION_ACTIVATION_THRESHOLD 0.2f

/** ③ 丢线时 heading_error 每帧衰减系数兜底值（当 heading_filter_alpha 未配置时使用） */
#define PERCEPTION_HEADING_DECAY_FALLBACK 0.85f

extern sens_decision_config_t g_sens_decision_config;

void perception_init(perception_t *perception) {
    if (perception == NULL) {
        return;
    }
    memset(perception, 0, sizeof(perception_t));
}

sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result) {
    /* ② 标准模拟加权重心所需的两个累加量 */
    float weighted_sum   = 0.0f;  /* Σ(w_i · v_i) */
    float total_activation = 0.0f; /* Σ v_i，作为分母 */
    uint8_t active_count = 0;
    size_t i;
    float derivative;
    float dt_s;
    float decay_alpha;

    if (perception == NULL || ir_data == NULL || result == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    if (perception->initialized && timestamp_us <= perception->prev_timestamp_us) {
        return SD_ERR_DATA_INVALID;
    }

    memset(result, 0, sizeof(perception_result_t));
    result->active_mask = ir_data->active_mask;

    /* ①② 阈值过滤 + 模拟量加权重心累加 */
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        float v = ir_data->values[i];
        if (v > PERCEPTION_ACTIVATION_THRESHOLD) {   /* ① 低于阈值通道不参与，抑制噪声 */
            float weight = g_sens_decision_config.perception.weights[i];
            weighted_sum     += weight * v;           /* ② Σ(w·v) */
            total_activation += v;                    /* ② Σv     */
            ++active_count;
        }
    }

    result->line_valid = (active_count > 0);

    if (active_count == 0) {
        ++perception->lost_count;
        result->lost_count   = perception->lost_count;
        result->event        = ROAD_EVENT_LINE_LOST;
        result->lateral_error = 0.0f;

        /* ③ 丢线时 heading_error 指数衰减，避免旧值冻结后恢复过冲 */
        decay_alpha = g_sens_decision_config.perception.heading_filter_alpha;
        if (decay_alpha <= 0.0f || decay_alpha >= 1.0f) {
            decay_alpha = PERCEPTION_HEADING_DECAY_FALLBACK;
        }
        perception->heading_error *= decay_alpha;
        result->heading_error = perception->heading_error;
    } else {
        perception->lost_count = 0;
        result->lost_count     = 0;

        /* ② 标准加权重心：Σ(w·v) / Σv */
        result->lateral_error = (total_activation > 0.0f)
                                    ? weighted_sum / total_activation
                                    : 0.0f;

        /* ③ 记录有效信号时的偏向符号，供上层丢线恢复策略使用 */
        if (result->lateral_error > 0.0f) {
            perception->last_lateral_sign = 1.0f;
        } else if (result->lateral_error < 0.0f) {
            perception->last_lateral_sign = -1.0f;
        }
        /* 若误差为 0（正中），保持上次符号不变 */

        /* heading_error：误差时间导数 + 低通滤波 */
        if (perception->initialized) {
            dt_s = (float)(timestamp_us - perception->prev_timestamp_us) / 1000000.0f;
            if (dt_s > 0.0f) {
                derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
                perception->heading_error =
                    g_sens_decision_config.perception.heading_filter_alpha * perception->heading_error +
                    (1.0f - g_sens_decision_config.perception.heading_filter_alpha) * derivative;
            }
        } else {
            dt_s = (timestamp_us > perception->prev_timestamp_us)
                       ? (float)(timestamp_us - perception->prev_timestamp_us) / 1000000.0f
                       : 0.0f;
            perception->heading_error = (dt_s > 0.0f)
                ? (result->lateral_error - perception->prev_lateral_error) / dt_s
                : 0.0f;
        }

        result->heading_error = perception->heading_error;

        if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
            result->event = ROAD_EVENT_INTERSECTION;
        } else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
                   fabsf(result->heading_error)  >= g_sens_decision_config.perception.curve_derivative_threshold) {
            result->event = ROAD_EVENT_CURVE_ENTRY;
        } else {
            result->event = ROAD_EVENT_NONE;
        }
    }

    perception->prev_lateral_error = result->lateral_error;
    perception->prev_timestamp_us  = timestamp_us;
    perception->initialized        = true;

    return SD_OK;
}
