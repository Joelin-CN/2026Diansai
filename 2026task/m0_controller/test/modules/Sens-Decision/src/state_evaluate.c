/**
 * @file      state_evaluate.c
 * @brief     Source file for state evaluate module.
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 */

#include "../inc/state_evaluate.h"
#include "../inc/utils.h"
#include "../inc/config.h"
#include <math.h>
#include <string.h>

extern sens_decision_config_t g_sens_decision_config;

void state_evaluator_init(state_evaluator_t *evaluator, const sd_ekf_config_t *ekf_config) {
    memset(evaluator, 0, sizeof(*evaluator));
    ekf_init(&evaluator->ekf, ekf_config);
    evaluator->initialized = false;
    evaluator->state.localization_valid = false;
}

sd_status_t state_evaluator_update(state_evaluator_t *evaluator, const sensor_frame_t *frame) {
    float left_speed, right_speed, v_encoder, omega_encoder;
    float observation[SD_EKF_OBSERVATION_COUNT];
    float dt_s;
    sd_status_t status;
    size_t i;
    
    if (evaluator == NULL || frame == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    for (i = 0; i < SD_ENCODER_COUNT; ++i) {
        if (!frame->encoder_valid[i]) {
            evaluator->consecutive_valid_frames = 0;
            evaluator->consecutive_failure_frames++;
            if (evaluator->consecutive_failure_frames >= 
                g_sens_decision_config.behavior.localization_failure_frames) {
                evaluator->state.localization_valid = false;
            }
            return SD_ERR_DATA_INVALID;
        }
    }
    
    if (!frame->imu_valid) {
        evaluator->consecutive_valid_frames = 0;
        evaluator->consecutive_failure_frames++;
        if (evaluator->consecutive_failure_frames >= 
            g_sens_decision_config.behavior.localization_failure_frames) {
            evaluator->state.localization_valid = false;
        }
        return SD_ERR_DATA_INVALID;
    }
    
    if (!evaluator->initialized) {
        evaluator->last_timestamp_us = frame->timestamp_us;
        evaluator->state.timestamp_us = frame->timestamp_us;
        evaluator->initialized = true;
        evaluator->consecutive_valid_frames = 0;
        evaluator->consecutive_failure_frames = 0;
        return SD_OK;
    }
    
    if (frame->timestamp_us <= evaluator->last_timestamp_us) {
        evaluator->consecutive_valid_frames = 0;
        evaluator->consecutive_failure_frames++;
        if (evaluator->consecutive_failure_frames >= 
            g_sens_decision_config.behavior.localization_failure_frames) {
            evaluator->state.localization_valid = false;
        }
        return SD_ERR_DATA_INVALID;
    }
    
    dt_s = (float)(frame->timestamp_us - evaluator->last_timestamp_us) * 1e-6f;
    
    if (dt_s < g_sens_decision_config.ekf.dt_min_s || 
        dt_s > g_sens_decision_config.ekf.dt_max_s) {
        evaluator->consecutive_valid_frames = 0;
        evaluator->consecutive_failure_frames++;
        if (evaluator->consecutive_failure_frames >= 
            g_sens_decision_config.behavior.localization_failure_frames) {
            evaluator->state.localization_valid = false;
        }
        return SD_ERR_DATA_INVALID;
    }
    
    left_speed = (frame->encoders[g_sens_decision_config.vehicle.left_encoder_indices[0]].speed_mps +
                  frame->encoders[g_sens_decision_config.vehicle.left_encoder_indices[1]].speed_mps) / 2.0f;
    right_speed = (frame->encoders[g_sens_decision_config.vehicle.right_encoder_indices[0]].speed_mps +
                   frame->encoders[g_sens_decision_config.vehicle.right_encoder_indices[1]].speed_mps) / 2.0f;
    
    v_encoder = (right_speed + left_speed) / 2.0f;
    omega_encoder = (right_speed - left_speed) / g_sens_decision_config.vehicle.wheel_track_m;
    
    observation[0] = v_encoder;
    observation[1] = omega_encoder;
    observation[2] = frame->imu.gyro_radps[2];
    
    for (i = 0; i < SD_EKF_OBSERVATION_COUNT; ++i) {
        if (!isfinite(observation[i])) {
            evaluator->consecutive_valid_frames = 0;
            evaluator->consecutive_failure_frames++;
            if (evaluator->consecutive_failure_frames >= 
                g_sens_decision_config.behavior.localization_failure_frames) {
                evaluator->state.localization_valid = false;
            }
            return SD_ERR_DATA_INVALID;
        }
    }
    
    ekf_predict(&evaluator->ekf, dt_s);
    status = ekf_update(&evaluator->ekf, observation);
    
    if (status != SD_OK) {
        evaluator->consecutive_valid_frames = 0;
        evaluator->consecutive_failure_frames++;
        if (evaluator->consecutive_failure_frames >= 
            g_sens_decision_config.behavior.localization_failure_frames) {
            evaluator->state.localization_valid = false;
        }
        return status;
    }
    
    evaluator->state.x = evaluator->ekf.state[0];
    evaluator->state.y = evaluator->ekf.state[1];
    evaluator->state.theta = evaluator->ekf.state[2];
    evaluator->state.v = evaluator->ekf.state[3];
    evaluator->state.omega = evaluator->ekf.state[4];
    
    memcpy(evaluator->state.P, evaluator->ekf.covariance, sizeof(evaluator->state.P));
    
    evaluator->state.timestamp_us = frame->timestamp_us;
    evaluator->last_timestamp_us = frame->timestamp_us;
    
    evaluator->consecutive_failure_frames = 0;
    evaluator->consecutive_valid_frames++;
    
    if (evaluator->consecutive_valid_frames >= 
        g_sens_decision_config.behavior.localization_valid_frames) {
        evaluator->state.localization_valid = true;
    }
    
    return SD_OK;
}
