/**
 * @file      trajectory_generate.c
 * @brief     Source file for trajectory generate module.
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 */

#include <math.h>

#include "../inc/trajectory_generate.h"
#include "../inc/utils.h"

void trajectory_generator_init(trajectory_generator_t *generator,
                              const sd_trajectory_config_t *config) {
    if (generator == NULL || config == NULL) {
        return;
    }
    generator->config = config;
    generator->path = NULL;
    generator->path_count = 0U;
    generator->last_nearest_index = 0U;
    generator->last_valid_target_index = 0U;
    generator->last_acceleration = 0.0f;
    generator->initialized = true;
}

sd_status_t trajectory_set_path(trajectory_generator_t *generator,
                               const path_point_t *path,
                               size_t count) {
    size_t i;
    float dx, dy;
    float segment_length_sq;
    float dot_product;
    float cos_angle;
    
    if (generator == NULL || path == NULL || count == 0U) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    for (i = 0U; i < count; ++i) {
        if (!isfinite(path[i].x) || !isfinite(path[i].y) ||
            !isfinite(path[i].heading) || !isfinite(path[i].curvature)) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    
    for (i = 0U; i < count - 1U; ++i) {
        dx = path[i + 1U].x - path[i].x;
        dy = path[i + 1U].y - path[i].y;
        segment_length_sq = dx * dx + dy * dy;
        
        if (segment_length_sq < 1e-6f) {
            return SD_ERR_INVALID_ARGUMENT;
        }
        
        cos_angle = cosf(path[i].heading);
        dot_product = dx * cos_angle + dy * sinf(path[i].heading);
        
        if (dot_product < 0.0f) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    
    generator->path = path;
    generator->path_count = count;
    generator->last_nearest_index = 0U;
    return SD_OK;
}

static size_t find_nearest_point(const trajectory_generator_t *generator,
                                 const vehicle_state_t *vehicle) {
    size_t start_index = generator->last_nearest_index;
    size_t end_index = start_index + generator->config->forward_search_points;
    size_t i;
    size_t nearest_index = start_index;
    float min_distance_sq = INFINITY;
    
    if (end_index > generator->path_count) {
        end_index = generator->path_count;
    }
    
    for (i = start_index; i < end_index; ++i) {
        float dx = generator->path[i].x - vehicle->x;
        float dy = generator->path[i].y - vehicle->y;
        float distance_sq = dx * dx + dy * dy;
        
        if (distance_sq < min_distance_sq) {
            min_distance_sq = distance_sq;
            nearest_index = i;
        }
    }
    
    return nearest_index;
}

static size_t find_lookahead_point(const trajectory_generator_t *generator,
                                   size_t start_index) {
    float accumulated_distance = 0.0f;
    size_t i;
    
    for (i = start_index; i < generator->path_count - 1U; ++i) {
        float dx = generator->path[i + 1U].x - generator->path[i].x;
        float dy = generator->path[i + 1U].y - generator->path[i].y;
        float segment_distance = sqrtf(dx * dx + dy * dy);
        
        accumulated_distance += segment_distance;
        
        if (accumulated_distance >= generator->config->lookahead_distance_m) {
            return i + 1U;
        }
    }
    
    return generator->path_count - 1U;
}

sd_status_t trajectory_generate(trajectory_generator_t *generator,
                               const vehicle_state_t *vehicle,
                               const behavior_output_t *behavior,
                               float dt,
                               trajectory_point_t *output) {
    size_t nearest_index;
    size_t target_index;
    const path_point_t *target_point;
    float v_target;
    float v_curve;
    float a_desired;
    float a_clamped;
    float delta_a;
    float max_delta_a;
    float v_new;
    
    if (generator == NULL || vehicle == NULL || behavior == NULL || output == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    if (behavior->state == BEHAVIOR_STATE_STOPPED || 
        behavior->state == BEHAVIOR_STATE_FAULT) {
        v_target = 0.0f;
        a_desired = (v_target - vehicle->v) / dt;
        
        if (a_desired < -generator->config->max_decel_mps2) {
            a_desired = -generator->config->max_decel_mps2;
        }
        
        max_delta_a = generator->config->max_jerk_mps3 * dt;
        delta_a = a_desired - generator->last_acceleration;
        
        if (delta_a > max_delta_a) {
            a_clamped = generator->last_acceleration + max_delta_a;
        } else if (delta_a < -max_delta_a) {
            a_clamped = generator->last_acceleration - max_delta_a;
        } else {
            a_clamped = a_desired;
        }
        
        v_new = vehicle->v + a_clamped * dt;
        
        if (v_new < 0.0f) {
            v_new = 0.0f;
            a_clamped = (v_new - vehicle->v) / dt;
        }
        
        output->x = vehicle->x;
        output->y = vehicle->y;
        output->theta = vehicle->theta;
        output->v = v_new;
        output->omega = 0.0f;
        output->a = a_clamped;
        output->alpha = -vehicle->omega / (dt + 1e-9f);
        output->curvature = 0.0f;
        
        generator->last_acceleration = a_clamped;
        
        return SD_OK;
    }
    
    if (generator->path == NULL || generator->path_count == 0U) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    if (behavior->state == BEHAVIOR_STATE_LINE_LOST_DEGRADED) {
        target_index = generator->last_valid_target_index;
    } else {
        nearest_index = find_nearest_point(generator, vehicle);
        generator->last_nearest_index = nearest_index;
        
        target_index = find_lookahead_point(generator, nearest_index);
        generator->last_valid_target_index = target_index;
    }
    
    target_point = &generator->path[target_index];
    
    v_curve = sqrtf(generator->config->curvature_speed_gain / 
                    (fabsf(target_point->curvature) + 1e-6f));
    
    v_target = (v_curve < behavior->speed_limit_mps) ? v_curve : behavior->speed_limit_mps;
    
    if (v_target > generator->config->max_speed_mps) {
        v_target = generator->config->max_speed_mps;
    }
    
    a_desired = (v_target - vehicle->v) / dt;
    
    if (a_desired > generator->config->max_accel_mps2) {
        a_desired = generator->config->max_accel_mps2;
    } else if (a_desired < -generator->config->max_decel_mps2) {
        a_desired = -generator->config->max_decel_mps2;
    }
    
    max_delta_a = generator->config->max_jerk_mps3 * dt;
    delta_a = a_desired - generator->last_acceleration;
    
    if (delta_a > max_delta_a) {
        a_clamped = generator->last_acceleration + max_delta_a;
    } else if (delta_a < -max_delta_a) {
        a_clamped = generator->last_acceleration - max_delta_a;
    } else {
        a_clamped = a_desired;
    }
    
    v_new = vehicle->v + a_clamped * dt;
    
    if ((a_clamped > 0.0f && v_new > v_target) || 
        (a_clamped < 0.0f && v_new < v_target)) {
        v_new = v_target;
        a_clamped = (v_new - vehicle->v) / dt;
    }
    
    if (v_new < 0.0f) {
        v_new = 0.0f;
        a_clamped = (v_new - vehicle->v) / dt;
    }
    
    output->x = target_point->x;
    output->y = target_point->y;
    output->theta = target_point->heading;
    output->v = v_new;
    output->curvature = target_point->curvature;
    output->omega = v_new * target_point->curvature;
    output->a = a_clamped;
    
    output->alpha = (output->omega - vehicle->omega) / dt;
    
    generator->last_acceleration = a_clamped;
    
    return SD_OK;
}
