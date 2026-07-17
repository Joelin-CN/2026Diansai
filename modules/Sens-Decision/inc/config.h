#ifndef SENS_DECISION_CONFIG_H
#define SENS_DECISION_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define SD_ENCODER_COUNT 4U
#define SD_IR_CHANNEL_COUNT 12U
#define SD_EKF_STATE_COUNT 5U
#define SD_EKF_OBSERVATION_COUNT 3U

typedef enum {
    SD_OK = 0,
    SD_ERR_INVALID_ARGUMENT = -1,
    SD_ERR_NOT_INITIALIZED = -2,
    SD_ERR_READ = -3,
    SD_ERR_TIMEOUT = -4,
    SD_ERR_HW_FAULT = -5,
    SD_ERR_UNSUPPORTED = -6,
    SD_ERR_DATA_INVALID = -7,
    SD_ERR_NUMERIC = -8
} sd_status_t;

typedef struct {
    float x_m;
    float y_m;
    float z_m;
} sd_position_t;

typedef struct {
    float wheel_track_m;
    uint8_t left_encoder_indices[2];
    uint8_t right_encoder_indices[2];
} sd_vehicle_config_t;

typedef struct {
    float wheel_radius_m;
    uint32_t pulses_per_revolution;
    int8_t direction;
    sd_position_t position;
} sd_encoder_config_t;

typedef struct {
    float accel_scale_mps2_per_lsb;
    float gyro_scale_radps_per_lsb;
    float accel_bias_mps2[3];
    float gyro_bias_radps[3];
    float filter_alpha;
    sd_position_t position;
} sd_imu_config_t;

typedef struct {
    bool active_high;
    float weights[SD_IR_CHANNEL_COUNT];
    sd_position_t position;
    float heading_filter_alpha;
    float curve_error_threshold;
    float curve_derivative_threshold;
    uint8_t intersection_active_channels;
} sd_perception_config_t;

typedef struct {
    float initial_covariance_diag[SD_EKF_STATE_COUNT];
    float process_noise_diag[SD_EKF_STATE_COUNT];
    float observation_noise_diag[SD_EKF_OBSERVATION_COUNT];
    float dt_min_s;
    float dt_max_s;
} sd_ekf_config_t;

typedef struct {
    uint16_t localization_valid_frames;
    uint16_t localization_failure_frames;
    uint16_t line_recovery_frames;
    uint16_t line_lost_stop_frames;
    uint16_t critical_failure_frames;
    uint16_t curve_exit_stable_frames;
    float idle_speed_mps;
    float line_speed_mps;
    float approach_curve_speed_mps;
    float curve_speed_mps;
    float degraded_speed_mps;
} sd_behavior_config_t;

typedef struct {
    float lookahead_distance_m;
    float curvature_speed_gain;
    float max_speed_mps;
    float max_accel_mps2;
    float max_decel_mps2;
    float max_jerk_mps3;
    uint16_t forward_search_points;
} sd_trajectory_config_t;

typedef struct {
    sd_vehicle_config_t vehicle;
    sd_encoder_config_t encoders[SD_ENCODER_COUNT];
    sd_imu_config_t imu;
    sd_perception_config_t perception;
    sd_ekf_config_t ekf;
    sd_behavior_config_t behavior;
    sd_trajectory_config_t trajectory;
} sens_decision_config_t;

extern sens_decision_config_t g_sens_decision_config;

void sd_config_reset_defaults(void);
sd_status_t sd_config_validate(const sens_decision_config_t *config);

#endif
