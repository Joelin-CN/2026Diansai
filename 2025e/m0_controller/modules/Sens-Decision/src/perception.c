/**
 * @file      perception.c
 * @brief     Source file for perception module.
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
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

sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result) {
    float weighted_sum = 0.0f;
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
    
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        float weight = g_sens_decision_config.perception.weights[i];
        float abs_weight = fabsf(weight);
        if (abs_weight > max_abs_weight) {
            max_abs_weight = abs_weight;
        }
        if (ir_data->values[i] > 0.5f) {
            weighted_sum += weight;
            ++active_count;
        }
    }
    
    result->line_valid = (active_count > 0);
    
    if (active_count == 0) {
        ++perception->lost_count;
        result->lost_count = perception->lost_count;
        result->event = ROAD_EVENT_LINE_LOST;
        result->lateral_error = 0.0f;
        result->heading_error = perception->heading_error;
    } else {
        perception->lost_count = 0;
        result->lost_count = 0;
        
        if (max_abs_weight > 0.0f) {
            result->lateral_error = weighted_sum / max_abs_weight;
        } else {
            result->lateral_error = 0.0f;
        }
        
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
