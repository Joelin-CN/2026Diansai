#include "config.h"

#include <math.h>
#include <stddef.h>

sens_decision_config_t g_sens_decision_config;

static bool position_is_finite(const sd_position_t *position) {
    return isfinite(position->x_m) && isfinite(position->y_m) &&
           isfinite(position->z_m);
}

static bool positive_finite(float value) {
    return isfinite(value) && value > 0.0f;
}

void sd_config_reset_defaults(void) {
    static const float ir_weights[SD_IR_CHANNEL_COUNT] = {
        -11.0f, -9.0f, -7.0f, -5.0f, -3.0f, -1.0f,
        1.0f,   3.0f,  5.0f,  7.0f,  9.0f,  11.0f
    };
    static const int8_t encoder_directions[SD_ENCODER_COUNT] = {1, 1, -1, -1};
    static const float encoder_x[SD_ENCODER_COUNT] = {0.08f, -0.08f, 0.08f,
                                                       -0.08f};
    static const float encoder_y[SD_ENCODER_COUNT] = {0.075f, 0.075f, -0.075f,
                                                       -0.075f};
    size_t index;

    g_sens_decision_config.vehicle.wheel_track_m = 0.15f;
    g_sens_decision_config.vehicle.left_encoder_indices[0] = 0U;
    g_sens_decision_config.vehicle.left_encoder_indices[1] = 1U;
    g_sens_decision_config.vehicle.right_encoder_indices[0] = 2U;
    g_sens_decision_config.vehicle.right_encoder_indices[1] = 3U;

    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        sd_encoder_config_t *encoder = &g_sens_decision_config.encoders[index];
        encoder->wheel_radius_m = 0.0325f;
        encoder->pulses_per_revolution = 1024U;
        encoder->direction = encoder_directions[index];
        encoder->position.x_m = encoder_x[index];
        encoder->position.y_m = encoder_y[index];
        encoder->position.z_m = 0.0f;
    }

    g_sens_decision_config.imu.accel_scale_mps2_per_lsb = 9.80665f / 2048.0f;
    g_sens_decision_config.imu.gyro_scale_radps_per_lsb =
        0.017453292519943295f / 16.4f;
    for (index = 0U; index < 3U; ++index) {
        g_sens_decision_config.imu.accel_bias_mps2[index] = 0.0f;
        g_sens_decision_config.imu.gyro_bias_radps[index] = 0.0f;
    }
    g_sens_decision_config.imu.filter_alpha = 0.25f;
    g_sens_decision_config.imu.position.x_m = 0.0f;
    g_sens_decision_config.imu.position.y_m = 0.0f;
    g_sens_decision_config.imu.position.z_m = 0.03f;

    g_sens_decision_config.perception.active_high = true;
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        g_sens_decision_config.perception.weights[index] = ir_weights[index];
    }
    g_sens_decision_config.perception.position.x_m = 0.10f;
    g_sens_decision_config.perception.position.y_m = 0.0f;
    g_sens_decision_config.perception.position.z_m = -0.02f;
    g_sens_decision_config.perception.heading_filter_alpha = 0.3f;
    g_sens_decision_config.perception.curve_error_threshold = 0.45f;
    g_sens_decision_config.perception.curve_derivative_threshold = 1.5f;
    g_sens_decision_config.perception.intersection_active_channels = 8U;

    for (index = 0U; index < SD_EKF_STATE_COUNT; ++index) {
        g_sens_decision_config.ekf.initial_covariance_diag[index] = 0.1f;
        g_sens_decision_config.ekf.process_noise_diag[index] = 0.01f;
    }
    for (index = 0U; index < SD_EKF_OBSERVATION_COUNT; ++index) {
        g_sens_decision_config.ekf.observation_noise_diag[index] = 0.05f;
    }
    g_sens_decision_config.ekf.dt_min_s = 0.0001f;
    g_sens_decision_config.ekf.dt_max_s = 0.1f;

    g_sens_decision_config.behavior.localization_valid_frames = 3U;
    g_sens_decision_config.behavior.localization_failure_frames = 3U;
    g_sens_decision_config.behavior.line_recovery_frames = 3U;
    g_sens_decision_config.behavior.line_lost_stop_frames = 20U;
    g_sens_decision_config.behavior.critical_failure_frames = 5U;
    g_sens_decision_config.behavior.curve_exit_stable_frames = 5U;
    g_sens_decision_config.behavior.idle_speed_mps = 0.0f;
    g_sens_decision_config.behavior.line_speed_mps = 1.0f;
    g_sens_decision_config.behavior.approach_curve_speed_mps = 0.7f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.5f;
    g_sens_decision_config.behavior.degraded_speed_mps = 0.25f;

    g_sens_decision_config.trajectory.lookahead_distance_m = 0.25f;
    g_sens_decision_config.trajectory.curvature_speed_gain = 1.0f;
    g_sens_decision_config.trajectory.max_speed_mps = 1.2f;
    g_sens_decision_config.trajectory.max_accel_mps2 = 1.5f;
    g_sens_decision_config.trajectory.max_decel_mps2 = 2.0f;
    g_sens_decision_config.trajectory.max_jerk_mps3 = 5.0f;
    g_sens_decision_config.trajectory.forward_search_points = 32U;
}

sd_status_t sd_config_validate(const sens_decision_config_t *config) {
    uint8_t encoder_index_counts[SD_ENCODER_COUNT] = {0U, 0U, 0U, 0U};
    size_t index;

    if (config == NULL || !positive_finite(config->vehicle.wheel_track_m)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 2U; ++index) {
        if (config->vehicle.left_encoder_indices[index] >= SD_ENCODER_COUNT ||
            config->vehicle.right_encoder_indices[index] >= SD_ENCODER_COUNT) {
            return SD_ERR_INVALID_ARGUMENT;
        }
        ++encoder_index_counts[config->vehicle.left_encoder_indices[index]];
        ++encoder_index_counts[config->vehicle.right_encoder_indices[index]];
    }
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        if (encoder_index_counts[index] != 1U) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        const sd_encoder_config_t *encoder = &config->encoders[index];
        if (!positive_finite(encoder->wheel_radius_m) ||
            encoder->pulses_per_revolution == 0U ||
            (encoder->direction != -1 && encoder->direction != 1) ||
            !position_is_finite(&encoder->position)) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!positive_finite(config->imu.accel_scale_mps2_per_lsb) ||
        !positive_finite(config->imu.gyro_scale_radps_per_lsb) ||
        !isfinite(config->imu.filter_alpha) || config->imu.filter_alpha < 0.0f ||
        config->imu.filter_alpha > 1.0f ||
        !position_is_finite(&config->imu.position)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < 3U; ++index) {
        if (!isfinite(config->imu.accel_bias_mps2[index]) ||
            !isfinite(config->imu.gyro_bias_radps[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!position_is_finite(&config->perception.position) ||
        !isfinite(config->perception.heading_filter_alpha) ||
        config->perception.heading_filter_alpha < 0.0f ||
        config->perception.heading_filter_alpha > 1.0f ||
        !positive_finite(config->perception.curve_error_threshold) ||
        !positive_finite(config->perception.curve_derivative_threshold) ||
        config->perception.intersection_active_channels == 0U ||
        config->perception.intersection_active_channels > SD_IR_CHANNEL_COUNT) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        if (!isfinite(config->perception.weights[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (!positive_finite(config->ekf.dt_min_s) ||
        !positive_finite(config->ekf.dt_max_s) ||
        config->ekf.dt_min_s >= config->ekf.dt_max_s) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < SD_EKF_STATE_COUNT; ++index) {
        if (!positive_finite(config->ekf.initial_covariance_diag[index]) ||
            !positive_finite(config->ekf.process_noise_diag[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < SD_EKF_OBSERVATION_COUNT; ++index) {
        if (!positive_finite(config->ekf.observation_noise_diag[index])) {
            return SD_ERR_INVALID_ARGUMENT;
        }
    }
    if (config->behavior.localization_valid_frames == 0U ||
        config->behavior.localization_failure_frames == 0U ||
        config->behavior.line_recovery_frames == 0U ||
        config->behavior.line_lost_stop_frames == 0U ||
        config->behavior.critical_failure_frames == 0U ||
        config->behavior.curve_exit_stable_frames == 0U ||
        !isfinite(config->behavior.idle_speed_mps) ||
        config->behavior.idle_speed_mps < 0.0f ||
        !positive_finite(config->behavior.line_speed_mps) ||
        !positive_finite(config->behavior.approach_curve_speed_mps) ||
        !positive_finite(config->behavior.curve_speed_mps) ||
        !positive_finite(config->behavior.degraded_speed_mps)) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (!positive_finite(config->trajectory.lookahead_distance_m) ||
        !positive_finite(config->trajectory.curvature_speed_gain) ||
        !positive_finite(config->trajectory.max_speed_mps) ||
        !positive_finite(config->trajectory.max_accel_mps2) ||
        !positive_finite(config->trajectory.max_decel_mps2) ||
        !positive_finite(config->trajectory.max_jerk_mps3) ||
        config->trajectory.forward_search_points == 0U) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    return SD_OK;
}
