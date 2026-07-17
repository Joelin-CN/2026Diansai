#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "interface.h"
#include "preprocess.h"
#include "utils.h"
#include "state_evaluate.h"
#include "perception.h"
#include "behavior_planner.h"
#include "trajectory_generate.h"

static int g_passed;
static int g_failed;
static bool g_suppress_assertion_log;
static int32_t g_encoder_counts[SD_ENCODER_COUNT];
static imu_raw_data_t g_imu_raw;
static uint16_t g_ir_mask;
static sd_status_t g_encoder_status[SD_ENCODER_COUNT];
static unsigned int g_encoder_read_calls[SD_ENCODER_COUNT];
static sd_status_t g_imu_status;
static sd_status_t g_ir_status;
static const sensor_vtable_t *g_original_vtables[SENSOR_ID_COUNT];
static int g_lifecycle_events[16];
static size_t g_lifecycle_event_count;
static sensor_id_t g_init_failure_id = SENSOR_ID_COUNT;
static const sensor_vtable_t g_injected_vtable;

#define TEST_ASSERT_TRUE(expression)                                           \
    do {                                                                       \
        if (!(expression)) {                                                   \
            SD_LOG_ERROR("assertion failed at %s:%d: %s", __FILE__, __LINE__, \
                         #expression);                                         \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ_INT(expected, actual)                                   \
    do {                                                                       \
        int expected_value = (int)(expected);                                  \
        int actual_value = (int)(actual);                                      \
        if (expected_value != actual_value) {                                  \
            SD_LOG_ERROR("assertion failed at %s:%d: expected %d, got %d",    \
                         __FILE__, __LINE__, expected_value, actual_value);     \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_NEAR(expected, actual, tolerance)                          \
    do {                                                                       \
        float expected_value = (expected);                                     \
        float actual_value = (actual);                                         \
        float tolerance_value = (tolerance);                                   \
        if (!isfinite(expected_value) || !isfinite(actual_value) ||            \
            !isfinite(tolerance_value) || tolerance_value < 0.0f ||            \
            fabsf(expected_value - actual_value) > tolerance_value) {          \
            if (!g_suppress_assertion_log) {                                   \
                SD_LOG_ERROR("assertion failed at %s:%d: expected %.9g, got " \
                             "%.9g", __FILE__, __LINE__,                       \
                             (double)expected_value, (double)actual_value);     \
            }                                                                  \
            return false;                                                      \
        }                                                                      \
    } while (0)

static void run_test(const char *name, bool (*test_function)(void)) {
    if (test_function()) {
        ++g_passed;
        SD_LOG_INFO("test %s passed", name);
    } else {
        ++g_failed;
        SD_LOG_ERROR("test %s failed", name);
    }
}

static sd_status_t virtual_read_encoder(uint8_t index, int32_t *count) {
    if (index >= SD_ENCODER_COUNT || count == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    ++g_encoder_read_calls[index];
    if (g_encoder_status[index] != SD_OK) {
        return g_encoder_status[index];
    }
    *count = g_encoder_counts[index];
    return SD_OK;
}

static sd_status_t virtual_read_imu(imu_raw_data_t *data) {
    if (data == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (g_imu_status != SD_OK) {
        return g_imu_status;
    }
    *data = g_imu_raw;
    return SD_OK;
}

static sd_status_t virtual_read_ir(uint16_t *active_mask) {
    if (active_mask == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (g_ir_status != SD_OK) {
        return g_ir_status;
    }
    *active_mask = g_ir_mask;
    return SD_OK;
}

static const sensor_hal_t g_virtual_hal = {
    virtual_read_encoder, virtual_read_imu, virtual_read_ir
};

static sensor_id_t test_sensor_id(sensor_t *sensor) {
    sensor_id_t id;
    for (id = SENSOR_ID_ENCODER_LEFT_FRONT; id < SENSOR_ID_COUNT;
         id = (sensor_id_t)(id + 1)) {
        if (sensor_get(id) == sensor) {
            return id;
        }
    }
    return SENSOR_ID_COUNT;
}

static sd_status_t injected_init(sensor_t *sensor) {
    sensor_id_t id = test_sensor_id(sensor);
    sd_status_t status;
    g_lifecycle_events[g_lifecycle_event_count++] = (int)id + 1;
    if (id == g_init_failure_id) {
        return SD_ERR_HW_FAULT;
    }
    sensor->vtable = g_original_vtables[id];
    status = sensor->vtable->init(sensor);
    sensor->vtable = &g_injected_vtable;
    return status;
}

static sd_status_t injected_read(sensor_t *sensor, void *output,
                                 uint64_t timestamp_us) {
    sensor_id_t id = test_sensor_id(sensor);
    return g_original_vtables[id]->read(sensor, output, timestamp_us);
}

static sd_status_t injected_write(sensor_t *sensor, const void *input) {
    sensor_id_t id = test_sensor_id(sensor);
    return g_original_vtables[id]->write(sensor, input);
}

static sd_status_t injected_release(sensor_t *sensor) {
    sensor_id_t id = test_sensor_id(sensor);
    sd_status_t status;
    g_lifecycle_events[g_lifecycle_event_count++] = -((int)id + 1);
    sensor->vtable = g_original_vtables[id];
    status = sensor->vtable->release(sensor);
    sensor->vtable = &g_injected_vtable;
    return status;
}

static const sensor_vtable_t g_injected_vtable = {
    injected_init, injected_read, injected_write, injected_release
};

static void reset_virtual_sensors(void) {
    memset(g_encoder_counts, 0, sizeof(g_encoder_counts));
    memset(g_encoder_status, 0, sizeof(g_encoder_status));
    memset(g_encoder_read_calls, 0, sizeof(g_encoder_read_calls));
    memset(&g_imu_raw, 0, sizeof(g_imu_raw));
    g_ir_mask = 0U;
    g_imu_status = SD_OK;
    g_ir_status = SD_OK;
    (void)sensors_release_all();
    sd_config_reset_defaults();
}

static bool test_default_config(void) {
    size_t index;

    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sd_config_validate(&g_sens_decision_config));
    TEST_ASSERT_TRUE(g_sens_decision_config.vehicle.wheel_track_m > 0.0f);
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        TEST_ASSERT_TRUE(
            g_sens_decision_config.encoders[index].wheel_radius_m > 0.0f);
        TEST_ASSERT_TRUE(
            g_sens_decision_config.encoders[index].pulses_per_revolution > 0U);
    }
    TEST_ASSERT_TRUE(g_sens_decision_config.ekf.dt_min_s <
                     g_sens_decision_config.ekf.dt_max_s);
    return true;
}

static bool test_config_validation(void) {
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, sd_config_validate(NULL));

    g_sens_decision_config.vehicle.wheel_track_m = 0.0f;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));

    sd_config_reset_defaults();
    g_sens_decision_config.imu.filter_alpha = 1.1f;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));

    sd_config_reset_defaults();
    g_sens_decision_config.behavior.localization_valid_frames = 0U;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));

    sd_config_reset_defaults();
    g_sens_decision_config.trajectory.max_jerk_mps3 = 0.0f;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));

    sd_config_reset_defaults();
    g_sens_decision_config.vehicle.right_encoder_indices[0] = 1U;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sd_config_validate(&g_sens_decision_config));
    return true;
}

static bool assert_near_nan_expected(void) {
    TEST_ASSERT_NEAR(NAN, 0.0f, 1e-6f);
    return true;
}

static bool assert_near_nan_actual(void) {
    TEST_ASSERT_NEAR(0.0f, NAN, 1e-6f);
    return true;
}

static bool assert_near_nan_tolerance(void) {
    TEST_ASSERT_NEAR(0.0f, 0.0f, NAN);
    return true;
}

static bool assert_near_negative_tolerance(void) {
    TEST_ASSERT_NEAR(0.0f, 0.0f, -1.0f);
    return true;
}

static bool test_assertion_guards(void) {
    bool result;

    g_suppress_assertion_log = true;
    result = assert_near_nan_expected();
    g_suppress_assertion_log = false;
    TEST_ASSERT_TRUE(!result);

    g_suppress_assertion_log = true;
    result = assert_near_nan_actual();
    g_suppress_assertion_log = false;
    TEST_ASSERT_TRUE(!result);

    g_suppress_assertion_log = true;
    result = assert_near_nan_tolerance();
    g_suppress_assertion_log = false;
    TEST_ASSERT_TRUE(!result);

    g_suppress_assertion_log = true;
    result = assert_near_negative_tolerance();
    g_suppress_assertion_log = false;
    TEST_ASSERT_TRUE(!result);
    return true;
}

static bool test_utils(void) {
    TEST_ASSERT_NEAR(1.0f, sd_clampf(2.0f, 0.0f, 1.0f), 1e-6f);
    TEST_ASSERT_NEAR(0.0f, sd_clampf(0.5f, 1.0f, 0.0f), 1e-6f);
    TEST_ASSERT_NEAR(0.0f, sd_clampf(NAN, 0.0f, 1.0f), 1e-6f);
    TEST_ASSERT_NEAR(0.0f, sd_normalize_angle(INFINITY), 1e-6f);
    TEST_ASSERT_TRUE(fabsf(sd_normalize_angle(3.5f)) <= SD_PI);
    return true;
}

static bool test_logging(void) {
    SD_LOG_DEBUG("debug message");
    SD_LOG_INFO("info message");
    SD_LOG_WARNING("warning message\ncontinued");
    SD_LOG_ERROR("error message");
    return true;
}

static bool test_sensor_hal_validation(void) {
    sensor_hal_t invalid_hal = g_virtual_hal;
    encoder_data_t encoder;

    reset_virtual_sensors();
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, sensors_configure_hal(NULL));
    invalid_hal.read_encoder_count = NULL;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sensors_configure_hal(&invalid_hal));
    invalid_hal = g_virtual_hal;
    invalid_hal.read_imu_raw = NULL;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sensors_configure_hal(&invalid_hal));
    invalid_hal = g_virtual_hal;
    invalid_hal.read_ir_mask = NULL;
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sensors_configure_hal(&invalid_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_ERR_NOT_INITIALIZED,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 100U));
    TEST_ASSERT_TRUE(sensor_get(SENSOR_ID_COUNT) == NULL);
    TEST_ASSERT_TRUE(sensor_get((sensor_id_t)-1) == NULL);
    return true;
}

static bool test_sensor_api_validation(void) {
    encoder_data_t encoder;
    sensor_t *sensor;

    reset_virtual_sensors();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    sensor = sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT);
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sensor_read(NULL, &encoder, 1U));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, sensor_read(sensor, NULL, 1U));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       sensor_write(NULL, &encoder));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, sensor_write(sensor, NULL));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, sensor_release(NULL));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, preprocess_update(1U, NULL));
    TEST_ASSERT_EQ_INT(SD_OK, sensor_release(sensor));
    TEST_ASSERT_EQ_INT(SD_ERR_NOT_INITIALIZED,
                       sensor_read(sensor, &encoder, 2U));
    return true;
}

static bool test_sensor_lifecycle(void) {
    reset_virtual_sensors();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_ERR_UNSUPPORTED,
                       sensor_write(sensor_get(SENSOR_ID_IMU), &g_imu_raw));
    TEST_ASSERT_EQ_INT(SD_OK, sensor_release(sensor_get(SENSOR_ID_IMU)));
    TEST_ASSERT_EQ_INT(SD_OK, sensor_release(sensor_get(SENSOR_ID_IMU)));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_release_all());
    TEST_ASSERT_EQ_INT(SD_OK, sensors_release_all());
    return true;
}

static bool test_sensor_init_reverse_rollback(void) {
    size_t index;
    sd_status_t status;

    reset_virtual_sensors();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    g_lifecycle_event_count = 0U;
    g_init_failure_id = SENSOR_ID_ENCODER_RIGHT_REAR;
    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        sensor_t *sensor = sensor_get((sensor_id_t)index);
        g_original_vtables[index] = sensor->vtable;
        sensor->vtable = &g_injected_vtable;
    }
    status = sensors_init_all();
    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        sensor_get((sensor_id_t)index)->vtable = g_original_vtables[index];
    }
    g_init_failure_id = SENSOR_ID_COUNT;

    TEST_ASSERT_EQ_INT(SD_ERR_HW_FAULT, status);
    TEST_ASSERT_EQ_INT(7, g_lifecycle_event_count);
    TEST_ASSERT_EQ_INT(1, g_lifecycle_events[0]);
    TEST_ASSERT_EQ_INT(2, g_lifecycle_events[1]);
    TEST_ASSERT_EQ_INT(3, g_lifecycle_events[2]);
    TEST_ASSERT_EQ_INT(4, g_lifecycle_events[3]);
    TEST_ASSERT_EQ_INT(-3, g_lifecycle_events[4]);
    TEST_ASSERT_EQ_INT(-2, g_lifecycle_events[5]);
    TEST_ASSERT_EQ_INT(-1, g_lifecycle_events[6]);
    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        TEST_ASSERT_TRUE(!sensor_get((sensor_id_t)index)->initialized);
    }
    return true;
}

static bool test_sensor_init_mixed_state_rollback(void) {
    encoder_data_t encoder;
    size_t index;
    sd_status_t status;

    reset_virtual_sensors();
    g_encoder_counts[0] = 100;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 100000U));
    for (index = 1U; index < SENSOR_ID_COUNT; ++index) {
        TEST_ASSERT_EQ_INT(SD_OK,
                           sensor_release(sensor_get((sensor_id_t)index)));
    }

    g_lifecycle_event_count = 0U;
    g_init_failure_id = SENSOR_ID_ENCODER_RIGHT_REAR;
    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        sensor_t *sensor = sensor_get((sensor_id_t)index);
        g_original_vtables[index] = sensor->vtable;
        sensor->vtable = &g_injected_vtable;
    }
    status = sensors_init_all();
    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        sensor_get((sensor_id_t)index)->vtable = g_original_vtables[index];
    }
    g_init_failure_id = SENSOR_ID_COUNT;

    TEST_ASSERT_EQ_INT(SD_ERR_HW_FAULT, status);
    TEST_ASSERT_EQ_INT(5, g_lifecycle_event_count);
    TEST_ASSERT_EQ_INT(2, g_lifecycle_events[0]);
    TEST_ASSERT_EQ_INT(3, g_lifecycle_events[1]);
    TEST_ASSERT_EQ_INT(4, g_lifecycle_events[2]);
    TEST_ASSERT_EQ_INT(-3, g_lifecycle_events[3]);
    TEST_ASSERT_EQ_INT(-2, g_lifecycle_events[4]);
    TEST_ASSERT_TRUE(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT)->initialized);
    for (index = 1U; index < SENSOR_ID_COUNT; ++index) {
        TEST_ASSERT_TRUE(!sensor_get((sensor_id_t)index)->initialized);
    }
    g_encoder_counts[0] = 200;
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 200000U));
    TEST_ASSERT_TRUE(encoder.speed_mps > 0.0f);
    return true;
}

static bool test_release_resets_sensor_history(void) {
    encoder_data_t encoder;
    imu_data_t imu;

    reset_virtual_sensors();
    g_encoder_counts[0] = 100;
    g_imu_raw.accel[0] = 2048;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 100000U));
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_IMU), &imu, 100000U));
    g_encoder_counts[0] = 200;
    g_imu_raw.accel[0] = 0;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_release_all());
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 200000U));
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_IMU), &imu, 200000U));
    TEST_ASSERT_NEAR(0.0f, encoder.speed_mps, 0.0f);
    TEST_ASSERT_NEAR(0.0f, imu.accel_mps2[0], 0.0f);
    return true;
}

static bool test_encoder_conversion(void) {
    sensor_frame_t frame;
    float expected;
    size_t index;

    reset_virtual_sensors();
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        g_encoder_counts[index] = (int32_t)(1000 + index * 100);
    }
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        TEST_ASSERT_TRUE(frame.encoder_valid[index]);
        TEST_ASSERT_NEAR(0.0f, frame.encoders[index].speed_mps, 1e-7f);
        g_encoder_counts[index] += 100;
    }
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1100000U, &frame));
    expected = 100.0f * 2.0f * SD_PI * 0.0325f / 1024.0f / 0.1f;
    TEST_ASSERT_NEAR(expected, frame.encoders[0].speed_mps, 1e-6f);
    TEST_ASSERT_NEAR(expected, frame.encoders[1].speed_mps, 1e-6f);
    TEST_ASSERT_NEAR(-expected, frame.encoders[2].speed_mps, 1e-6f);
    TEST_ASSERT_NEAR(-expected, frame.encoders[3].speed_mps, 1e-6f);
    return true;
}

static bool test_encoder_time_and_wrap(void) {
    encoder_data_t encoder;
    sensor_t *sensor;
    float expected;

    reset_virtual_sensors();
    g_encoder_counts[0] = INT32_MAX;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    sensor = sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT);
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &encoder, 100000U));
    TEST_ASSERT_EQ_INT(1, g_encoder_read_calls[0]);
    TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID,
                       sensor_read(sensor, &encoder, 100000U));
    TEST_ASSERT_EQ_INT(1, g_encoder_read_calls[0]);
    g_encoder_counts[0] = INT32_MIN;
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &encoder, 200000U));
    expected = 2.0f * SD_PI * 0.0325f / 1024.0f / 0.1f;
    TEST_ASSERT_NEAR(expected, encoder.speed_mps, 1e-7f);
    TEST_ASSERT_EQ_INT(SD_OK, sensor_release(sensor));
    g_encoder_counts[0] = INT32_MIN;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &encoder, 300000U));
    g_encoder_counts[0] = INT32_MAX;
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &encoder, 400000U));
    TEST_ASSERT_NEAR(-expected, encoder.speed_mps, 1e-7f);
    return true;
}

static bool test_active_sensor_config_snapshot(void) {
    encoder_data_t encoder;
    imu_data_t imu;
    float expected_speed;

    reset_virtual_sensors();
    g_encoder_counts[0] = 100;
    g_imu_raw.accel[0] = 2048;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 100000U));
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_IMU), &imu, 100000U));

    g_sens_decision_config.encoders[0].wheel_radius_m = 1.0f;
    g_sens_decision_config.encoders[0].pulses_per_revolution = 1U;
    g_sens_decision_config.encoders[0].direction = -1;
    g_sens_decision_config.imu.accel_scale_mps2_per_lsb = 100.0f;
    g_sens_decision_config.imu.accel_bias_mps2[0] = 1000.0f;
    g_sens_decision_config.imu.filter_alpha = 0.0f;
    g_encoder_counts[0] = 200;

    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_ENCODER_LEFT_FRONT),
                                   &encoder, 200000U));
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_IMU), &imu, 200000U));
    expected_speed = 100.0f * 2.0f * SD_PI * 0.0325f / 1024.0f / 0.1f;
    TEST_ASSERT_NEAR(expected_speed, encoder.speed_mps, 1e-6f);
    TEST_ASSERT_NEAR(9.80665f, imu.accel_mps2[0], 1e-5f);
    return true;
}

static bool test_imu_conversion_filter(void) {
    imu_data_t first;
    imu_data_t second;
    sensor_t *sensor;
    float first_accel;

    reset_virtual_sensors();
    g_sens_decision_config.imu.accel_bias_mps2[0] = 1.0f;
    g_sens_decision_config.imu.gyro_bias_radps[2] = 0.1f;
    g_imu_raw.accel[0] = 2048;
    g_imu_raw.gyro[2] = 164;
    g_imu_raw.temperature = 132;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    sensor = sensor_get(SENSOR_ID_IMU);
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &first, 1000U));
    first_accel = 9.80665f - 1.0f;
    TEST_ASSERT_NEAR(first_accel, first.accel_mps2[0], 1e-5f);
    TEST_ASSERT_NEAR(0.0745329f, first.gyro_radps[2], 1e-5f);
    TEST_ASSERT_NEAR(26.0f, first.temperature_c, 1e-6f);
    g_imu_raw.accel[0] = 0;
    TEST_ASSERT_EQ_INT(SD_OK, sensor_read(sensor, &second, 2000U));
    TEST_ASSERT_NEAR(0.25f * first_accel + 0.75f * -1.0f,
                     second.accel_mps2[0], 1e-5f);
    TEST_ASSERT_TRUE(isfinite(second.gyro_radps[2]));
    return true;
}

static bool test_ir_mapping(void) {
    ir_array_data_t ir;
    size_t index;

    reset_virtual_sensors();
    g_ir_mask = 0xF805U;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK,
                       sensor_read(sensor_get(SENSOR_ID_IR_ARRAY), &ir, 44U));
    TEST_ASSERT_EQ_INT(0x0805, ir.active_mask);
    for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
        float expected = (index == 0U || index == 2U || index == 11U)
                             ? 1.0f : 0.0f;
        TEST_ASSERT_NEAR(expected, ir.values[index], 0.0f);
    }
    return true;
}

static bool test_preprocess_partial_failure(void) {
    sensor_frame_t frame;
    size_t index;

    reset_virtual_sensors();
    g_encoder_status[1] = SD_ERR_READ;
    g_imu_status = SD_ERR_HW_FAULT;
    g_ir_status = SD_ERR_TIMEOUT;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_ERR_READ, preprocess_update(777U, &frame));
    TEST_ASSERT_TRUE(frame.encoder_valid[0]);
    TEST_ASSERT_TRUE(!frame.encoder_valid[1]);
    TEST_ASSERT_TRUE(frame.encoder_valid[2]);
    TEST_ASSERT_TRUE(frame.encoder_valid[3]);
    TEST_ASSERT_TRUE(!frame.imu_valid);
    TEST_ASSERT_TRUE(!frame.ir_valid);
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        if (index != 1U) {
            TEST_ASSERT_TRUE(frame.encoders[index].timestamp_us == 777U);
        }
    }
    return true;
}

static bool test_vtable_frame_metadata_coherence(void) {
    sensor_frame_t frame;
    size_t index;

    reset_virtual_sensors();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(123456789U, &frame));
    TEST_ASSERT_TRUE(frame.timestamp_us == 123456789U);
    for (index = 0U; index < SD_ENCODER_COUNT; ++index) {
        TEST_ASSERT_TRUE(frame.encoders[index].timestamp_us == frame.timestamp_us);
    }
    TEST_ASSERT_TRUE(frame.imu.timestamp_us == frame.timestamp_us);
    TEST_ASSERT_TRUE(frame.ir.timestamp_us == frame.timestamp_us);
    return true;
}

static bool test_ekf_first_frame_baseline(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    TEST_ASSERT_TRUE(evaluator.initialized);
    TEST_ASSERT_EQ_INT(1000000U, evaluator.last_timestamp_us);
    TEST_ASSERT_TRUE(!evaluator.state.localization_valid);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.x, 1e-6f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.y, 1e-6f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.theta, 1e-6f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.v, 1e-6f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.omega, 1e-6f);
    
    return true;
}

static bool test_ekf_straight_travel(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    size_t step;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    for (step = 1; step <= 11; ++step) {
        g_encoder_counts[0] = (int32_t)(step * 512);
        g_encoder_counts[1] = (int32_t)(step * 512);
        g_encoder_counts[2] = (int32_t)(-step * 512);
        g_encoder_counts[3] = (int32_t)(-step * 512);
        g_imu_raw.gyro[2] = 0;
        
        TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U + step * 100000U, &frame));
        TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    }
    
    TEST_ASSERT_NEAR(1.0f, evaluator.state.x, 0.15f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.y, 0.1f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.theta, 0.1f);
    TEST_ASSERT_NEAR(1.0f, evaluator.state.v, 0.1f);
    TEST_ASSERT_NEAR(0.0f, evaluator.state.omega, 0.1f);
    
    return true;
}

static bool test_ekf_rotation_in_place(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    size_t step;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    for (step = 1; step <= 5; ++step) {
        g_encoder_counts[0] = (int32_t)(step * 200);
        g_encoder_counts[1] = (int32_t)(step * 200);
        g_encoder_counts[2] = (int32_t)(step * 200);
        g_encoder_counts[3] = (int32_t)(step * 200);
        g_imu_raw.gyro[2] = 656;
        
        TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U + step * 100000U, &frame));
        TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    }
    
    TEST_ASSERT_NEAR(0.0f, evaluator.state.v, 0.2f);
    TEST_ASSERT_TRUE(fabsf(evaluator.state.omega) > 0.5f);
    
    return true;
}

static bool test_ekf_gyro_correction(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    float omega_before;
    float omega_after;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    for (i = 0; i < SD_ENCODER_COUNT; ++i) {
        g_encoder_counts[i] = 50;
    }
    g_imu_raw.gyro[2] = 0;
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1100000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    omega_before = evaluator.state.omega;
    
    g_encoder_counts[0] += 50;
    g_encoder_counts[1] += 50;
    g_encoder_counts[2] -= 50;
    g_encoder_counts[3] -= 50;
    g_imu_raw.gyro[2] = 656;
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1200000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    omega_after = evaluator.state.omega;
    
    TEST_ASSERT_TRUE(fabsf(omega_after - omega_before) > 0.01f);
    
    return true;
}

static bool test_ekf_covariance_properties(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    size_t i, j;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    g_encoder_counts[0] = 50;
    g_encoder_counts[1] = 50;
    g_encoder_counts[2] = -50;
    g_encoder_counts[3] = -50;
    g_imu_raw.gyro[2] = 164;
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1100000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
        for (j = 0; j < SD_EKF_STATE_COUNT; ++j) {
            TEST_ASSERT_TRUE(isfinite(evaluator.state.P[i][j]));
            if (i != j) {
                TEST_ASSERT_NEAR(evaluator.state.P[i][j], evaluator.state.P[j][i], 1e-6f);
            }
        }
        TEST_ASSERT_TRUE(evaluator.state.P[i][i] >= 0.0f);
    }
    
    return true;
}

static bool test_ekf_timestamp_rollback(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(2000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    g_encoder_counts[0] = 10;
    g_encoder_counts[1] = 10;
    g_encoder_counts[2] = -10;
    g_encoder_counts[3] = -10;
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(2100000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    evaluator.last_timestamp_us = 3000000U;
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(2200000U, &frame));
    TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID, state_evaluator_update(&evaluator, &frame));
    
    return true;
}

static bool test_ekf_excessive_dt(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(10000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID, state_evaluator_update(&evaluator, &frame));
    
    return true;
}

static bool test_ekf_nan_observation(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(1000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    
    frame.encoder_valid[0] = false;
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(2000000U, &frame));
    TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID, state_evaluator_update(&evaluator, &frame));
    
    return true;
}

static bool test_ekf_repeated_failures_clear_valid(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    g_sens_decision_config.behavior.localization_valid_frames = 2U;
    g_sens_decision_config.behavior.localization_failure_frames = 3U;
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    
    for (i = 0; i < 5; ++i) {
        g_encoder_counts[0] = (int32_t)(i * 10);
        g_encoder_counts[1] = (int32_t)(i * 10);
        g_encoder_counts[2] = (int32_t)(-i * 10);
        g_encoder_counts[3] = (int32_t)(-i * 10);
        TEST_ASSERT_EQ_INT(SD_OK, preprocess_update((i + 1) * 100000U, &frame));
        TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    }
    
    TEST_ASSERT_TRUE(evaluator.state.localization_valid);
    
    for (i = 0; i < 3; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, preprocess_update((i + 6) * 100000U, &frame));
        frame.encoder_valid[0] = false;
        TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID, state_evaluator_update(&evaluator, &frame));
    }
    
    TEST_ASSERT_TRUE(!evaluator.state.localization_valid);
    
    return true;
}

static bool test_perception_center_masks(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x060;
    ir_data.values[5] = 1.0f;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    TEST_ASSERT_NEAR(0.0f, result.lateral_error, 0.1f);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x108;
    ir_data.values[3] = 1.0f;
    ir_data.values[8] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 2000000U, &result));
    TEST_ASSERT_NEAR(0.0f, result.lateral_error, 0.1f);
    
    return true;
}

static bool test_perception_lateral_signs(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    float left_error, right_error;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x001;
    ir_data.values[0] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    left_error = result.lateral_error;
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x800;
    ir_data.values[11] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 2000000U, &result));
    right_error = result.lateral_error;
    
    TEST_ASSERT_TRUE(left_error * right_error < 0.0f);
    
    return true;
}

static bool test_perception_intersection_event(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    g_sens_decision_config.perception.intersection_active_channels = 8U;
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0xFFF;
    for (i = 0; i < SD_IR_CHANNEL_COUNT; ++i) {
        ir_data.values[i] = 1.0f;
    }
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    TEST_ASSERT_EQ_INT(ROAD_EVENT_INTERSECTION, result.event);
    
    return true;
}

static bool test_perception_curve_entry_event(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    g_sens_decision_config.perception.curve_error_threshold = 0.3f;
    g_sens_decision_config.perception.curve_derivative_threshold = 5.0f;
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x060;
    ir_data.values[5] = 1.0f;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x001;
    ir_data.values[0] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1010000U, &result));
    TEST_ASSERT_EQ_INT(ROAD_EVENT_CURVE_ENTRY, result.event);
    
    return true;
}

static bool test_perception_line_lost_event(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x000;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    TEST_ASSERT_EQ_INT(ROAD_EVENT_LINE_LOST, result.event);
    TEST_ASSERT_EQ_INT(1, result.lost_count);
    TEST_ASSERT_TRUE(!result.line_valid);
    
    return true;
}

static bool test_perception_loss_count_reset(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x000;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    TEST_ASSERT_EQ_INT(1, result.lost_count);
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 2000000U, &result));
    TEST_ASSERT_EQ_INT(2, result.lost_count);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x040;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 3000000U, &result));
    TEST_ASSERT_EQ_INT(0, result.lost_count);
    TEST_ASSERT_TRUE(result.line_valid);
    
    return true;
}

static bool test_perception_timestamp_rollback(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x040;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 2000000U, &result));
    TEST_ASSERT_EQ_INT(SD_ERR_DATA_INVALID, perception_update(&perception, &ir_data, 1000000U, &result));
    
    return true;
}

static bool test_perception_null_input(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x040;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, perception_update(NULL, &ir_data, 1000000U, &result));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, perception_update(&perception, NULL, 1000000U, &result));
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, perception_update(&perception, &ir_data, 1000000U, NULL));
    
    return true;
}

static bool test_perception_heading_derivative(void) {
    perception_t perception;
    perception_result_t result;
    ir_array_data_t ir_data;
    float first_heading, second_heading;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    g_sens_decision_config.perception.heading_filter_alpha = 0.5f;
    
    perception_init(&perception);
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x040;
    ir_data.values[6] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1000000U, &result));
    first_heading = result.heading_error;
    
    memset(&ir_data, 0, sizeof(ir_data));
    ir_data.active_mask = 0x001;
    ir_data.values[0] = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &ir_data, 1100000U, &result));
    second_heading = result.heading_error;
    
    TEST_ASSERT_TRUE(isfinite(first_heading));
    TEST_ASSERT_TRUE(isfinite(second_heading));
    TEST_ASSERT_TRUE(fabsf(second_heading) > 0.0f);
    
    return true;
}

static bool test_behavior_idle_start_gating(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_START;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = false;
    perception.line_valid = true;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_IDLE, output.state);
    
    vehicle.localization_valid = true;
    perception.line_valid = false;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_IDLE, output.state);
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_FOLLOW, output.state);
    
    return true;
}

static bool test_behavior_line_to_approach_curve(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_START;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    perception.event = ROAD_EVENT_NONE;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_FOLLOW, output.state);
    
    input.command = BEHAVIOR_CMD_NONE;
    perception.event = ROAD_EVENT_CURVE_ENTRY;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_APPROACH_CURVE, output.state);
    
    return true;
}

static bool test_behavior_approach_to_curve(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_APPROACH_CURVE;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.1f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    perception.heading_error = 0.05f;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_APPROACH_CURVE, output.state);
    
    perception.heading_error = 0.3f;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_CURVE, output.state);
    
    return true;
}

static bool test_behavior_curve_exit(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    uint16_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_CURVE;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.01f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    perception.heading_error = 0.01f;
    
    for (i = 0; i < g_sens_decision_config.behavior.curve_exit_stable_frames - 1U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
        TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_CURVE, output.state);
    }
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_FOLLOW, output.state);
    
    return true;
}

static bool test_behavior_short_loss_recovery(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    uint16_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_LINE_FOLLOW;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = false;
    perception.lateral_error = 0.5f;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_LOST_DEGRADED, output.state);
    TEST_ASSERT_NEAR(0.5f, output.last_valid_lateral_error, 1e-6f);
    
    for (i = 0; i < g_sens_decision_config.behavior.line_recovery_frames - 1U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
        TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_LOST_DEGRADED, output.state);
    }
    
    perception.line_valid = true;
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_FOLLOW, output.state);
    
    return true;
}

static bool test_behavior_persistent_loss(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    uint16_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_LINE_FOLLOW;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = false;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_LOST_DEGRADED, output.state);
    
    for (i = 0; i < g_sens_decision_config.behavior.line_lost_stop_frames - 1U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
        TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_LINE_LOST_DEGRADED, output.state);
    }
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_STOPPED, output.state);
    
    return true;
}

static bool test_behavior_immediate_stop(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_LINE_FOLLOW;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_STOP;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_STOPPED, output.state);
    TEST_ASSERT_NEAR(0.0f, output.speed_limit_mps, 1e-6f);
    
    return true;
}

static bool test_behavior_fault_on_failures(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    uint16_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_LINE_FOLLOW;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = false;
    perception.line_valid = false;
    
    for (i = 0; i < g_sens_decision_config.behavior.critical_failure_frames - 1U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
        TEST_ASSERT_TRUE(output.state != BEHAVIOR_STATE_FAULT);
    }
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_FAULT, output.state);
    TEST_ASSERT_NEAR(0.0f, output.speed_limit_mps, 1e-6f);
    
    return true;
}

static bool test_behavior_reset_reject_unhealthy(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_FAULT;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_RESET;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = false;
    perception.line_valid = true;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_FAULT, output.state);
    
    return true;
}

static bool test_behavior_reset_to_idle(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    planner.current_state = BEHAVIOR_STATE_FAULT;
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_RESET;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_IDLE, output.state);
    
    return true;
}

static bool test_behavior_no_transition_log(void) {
    behavior_planner_t planner;
    behavior_input_t input;
    behavior_output_t output;
    vehicle_state_t vehicle = {0};
    perception_result_t perception = {0};
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    behavior_planner_init(&planner);
    
    input.vehicle = &vehicle;
    input.perception = &perception;
    input.command = BEHAVIOR_CMD_NONE;
    input.path_curvature = 0.0f;
    
    vehicle.localization_valid = true;
    perception.line_valid = true;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_IDLE, output.state);
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &input, &output));
    TEST_ASSERT_EQ_INT(BEHAVIOR_STATE_IDLE, output.state);
    
    return true;
}

static bool test_trajectory_empty_path(void) {
    trajectory_generator_t generator;
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT, 
                       trajectory_set_path(&generator, NULL, 0));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.v = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
    
    return true;
}

static bool test_trajectory_nan_path(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = NAN;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       trajectory_set_path(&generator, path, 2));
    
    path[0].curvature = 0.0f;
    path[1].x = NAN;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       trajectory_set_path(&generator, path, 2));
    
    return true;
}

static bool test_trajectory_straight_path_progress(void) {
    trajectory_generator_t generator;
    path_point_t path[10];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    size_t i;
    size_t last_index;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    for (i = 0U; i < 10U; ++i) {
        path[i].x = (float)i * 0.5f;
        path[i].y = 0.0f;
        path[i].heading = 0.0f;
        path[i].curvature = 0.0f;
    }
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 10));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.5f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
    last_index = generator.last_nearest_index;
    
    for (i = 0U; i < 20U; ++i) {
        vehicle.x += 0.2f;
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
        TEST_ASSERT_TRUE(generator.last_nearest_index >= last_index);
        last_index = generator.last_nearest_index;
    }
    
    return true;
}

static bool test_trajectory_curvature_speed_limit(void) {
    trajectory_generator_t generator;
    path_point_t path[4];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float expected_v_curve;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.7854f;
    path[1].curvature = 1.0f;
    
    path[2].x = 1.0f;
    path[2].y = 1.0f;
    path[2].heading = 1.5708f;
    path[2].curvature = 1.0f;
    
    path[3].x = 0.0f;
    path[3].y = 1.0f;
    path[3].heading = 2.3562f;
    path[3].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 4));
    
    vehicle.x = 1.0f;
    vehicle.y = 0.5f;
    vehicle.theta = 1.5708f;
    vehicle.v = 0.0f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_CURVE;
    behavior.speed_limit_mps = 2.0f;
    
    for (i = 0U; i < 100U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
        vehicle.v = output.v;
    }
    
    expected_v_curve = sqrtf(g_sens_decision_config.trajectory.curvature_speed_gain / 
                             (1.0f + 1e-6f));
    
    TEST_ASSERT_TRUE(output.v < behavior.speed_limit_mps);
    TEST_ASSERT_NEAR(expected_v_curve, output.v, 0.3f);
    
    return true;
}

static bool test_trajectory_accel_constraint(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float dt = 0.02f;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 10.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.0f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 2.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output));
    TEST_ASSERT_TRUE(output.a <= g_sens_decision_config.trajectory.max_accel_mps2);
    TEST_ASSERT_TRUE(output.a >= -g_sens_decision_config.trajectory.max_decel_mps2);
    
    return true;
}

static bool test_trajectory_decel_constraint(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float dt = 0.02f;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 10.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 1.5f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 0.2f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output));
    TEST_ASSERT_TRUE(output.a <= g_sens_decision_config.trajectory.max_accel_mps2);
    TEST_ASSERT_TRUE(output.a >= -g_sens_decision_config.trajectory.max_decel_mps2);
    
    return true;
}

static bool test_trajectory_jerk_constraint(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output1, output2;
    float dt = 0.02f;
    float delta_a;
    float jerk;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 10.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.0f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 2.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output1));
    
    vehicle.v = output1.v;
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output2));
    
    delta_a = output2.a - output1.a;
    jerk = fabsf(delta_a / dt);
    
    TEST_ASSERT_TRUE(jerk <= g_sens_decision_config.trajectory.max_jerk_mps3 + 0.01f);
    
    return true;
}

static bool test_trajectory_angular_velocity(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float expected_omega;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.5f;
    path[1].x = 2.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.5f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.0f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.5f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_CURVE;
    behavior.speed_limit_mps = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
    
    expected_omega = output.v * output.curvature;
    TEST_ASSERT_NEAR(expected_omega, output.omega, 0.05f);
    
    return true;
}

static bool test_trajectory_stopped_state(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.1f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_STOPPED;
    behavior.speed_limit_mps = 0.0f;
    
    for (i = 0U; i < 50U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
        vehicle.v = output.v;
        vehicle.omega = output.omega;
    }
    
    TEST_ASSERT_TRUE(isfinite(output.v));
    TEST_ASSERT_TRUE(isfinite(output.omega));
    TEST_ASSERT_NEAR(0.0f, output.v, 0.01f);
    TEST_ASSERT_NEAR(0.0f, output.omega, 0.01f);
    
    return true;
}

static bool test_trajectory_fault_state(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.3f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_FAULT;
    behavior.speed_limit_mps = 0.0f;
    
    for (i = 0U; i < 50U; ++i) {
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output));
        vehicle.v = output.v;
        vehicle.omega = output.omega;
    }
    
    TEST_ASSERT_TRUE(isfinite(output.v));
    TEST_ASSERT_TRUE(isfinite(output.omega));
    TEST_ASSERT_NEAR(0.0f, output.v, 0.01f);
    TEST_ASSERT_NEAR(0.0f, output.omega, 0.01f);
    
    return true;
}

static bool test_trajectory_stopped_decel_limit(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float dt = 0.02f;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 1.5f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_STOPPED;
    behavior.speed_limit_mps = 0.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output));
    
    TEST_ASSERT_TRUE(output.a >= -g_sens_decision_config.trajectory.max_decel_mps2 - 0.001f);
    TEST_ASSERT_TRUE(output.a <= 0.0f);
    
    return true;
}

static bool test_trajectory_fault_decel_limit(void) {
    trajectory_generator_t generator;
    path_point_t path[2];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float dt = 0.02f;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 2));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 2.0f;
    vehicle.omega = 0.5f;
    behavior.state = BEHAVIOR_STATE_FAULT;
    behavior.speed_limit_mps = 0.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output));
    
    TEST_ASSERT_TRUE(output.a >= -g_sens_decision_config.trajectory.max_decel_mps2 - 0.001f);
    TEST_ASSERT_TRUE(output.a <= 0.0f);
    
    return true;
}

static bool test_trajectory_alpha_uses_vehicle_omega(void) {
    trajectory_generator_t generator;
    path_point_t path[3];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output;
    float dt = 0.02f;
    float expected_alpha;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 1.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.5f;
    path[2].x = 2.0f;
    path[2].y = 0.0f;
    path[2].heading = 0.0f;
    path[2].curvature = 0.5f;
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 3));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.2f;
    vehicle.theta = 0.1f;
    vehicle.v = 0.8f;
    vehicle.omega = 0.3f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, dt, &output));
    
    expected_alpha = (output.omega - vehicle.omega) / dt;
    TEST_ASSERT_NEAR(expected_alpha, output.alpha, 0.01f);
    
    return true;
}

static bool test_trajectory_zero_length_segment(void) {
    trajectory_generator_t generator;
    path_point_t path[3];
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 0.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    path[2].x = 1.0f;
    path[2].y = 0.0f;
    path[2].heading = 0.0f;
    path[2].curvature = 0.0f;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       trajectory_set_path(&generator, path, 3));
    
    return true;
}

static bool test_trajectory_backward_segment(void) {
    trajectory_generator_t generator;
    path_point_t path[3];
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    
    path[0].x = 0.0f;
    path[0].y = 0.0f;
    path[0].heading = 0.0f;
    path[0].curvature = 0.0f;
    path[1].x = 2.0f;
    path[1].y = 0.0f;
    path[1].heading = 0.0f;
    path[1].curvature = 0.0f;
    path[2].x = 1.0f;
    path[2].y = 0.0f;
    path[2].heading = 3.14159f;
    path[2].curvature = 0.0f;
    
    TEST_ASSERT_EQ_INT(SD_ERR_INVALID_ARGUMENT,
                       trajectory_set_path(&generator, path, 3));
    
    return true;
}

static bool test_trajectory_line_lost_frozen_target(void) {
    trajectory_generator_t generator;
    path_point_t path[5];
    vehicle_state_t vehicle = {0};
    behavior_output_t behavior = {0};
    trajectory_point_t output1, output2;
    size_t i;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    
    for (i = 0U; i < 5U; ++i) {
        path[i].x = (float)i * 0.5f;
        path[i].y = 0.0f;
        path[i].heading = 0.0f;
        path[i].curvature = 0.0f;
    }
    
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 5));
    
    vehicle.x = 0.5f;
    vehicle.y = 0.0f;
    vehicle.theta = 0.0f;
    vehicle.v = 0.5f;
    vehicle.omega = 0.0f;
    behavior.state = BEHAVIOR_STATE_LINE_FOLLOW;
    behavior.speed_limit_mps = 1.0f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output1));
    
    behavior.state = BEHAVIOR_STATE_LINE_LOST_DEGRADED;
    behavior.speed_limit_mps = 0.5f;
    vehicle.x = 1.5f;
    
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior, 0.02f, &output2));
    
    TEST_ASSERT_TRUE(output1.x == output2.x);
    TEST_ASSERT_TRUE(output1.y == output2.y);
    
    TEST_ASSERT_TRUE(output2.v <= behavior.speed_limit_mps + 0.01f);
    
    return true;
}

static bool test_complete_driving_sequence(void) {
    state_evaluator_t evaluator;
    perception_t perception;
    behavior_planner_t planner;
    trajectory_generator_t generator;
    path_point_t path[10];
    sensor_frame_t frame;
    perception_result_t perc_result;
    behavior_input_t behavior_input;
    behavior_output_t behavior_output;
    vehicle_state_t vehicle;
    trajectory_point_t trajectory_output;
    size_t i, frame_idx;
    size_t last_path_index;
    bool saw_curve_entry = false;
    bool saw_line_lost = false;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    for (i = 0U; i < 10U; ++i) {
        path[i].x = (float)i * 0.5f;
        path[i].y = 0.0f;
        path[i].heading = 0.0f;
        path[i].curvature = 0.0f;
    }
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    perception_init(&perception);
    behavior_planner_init(&planner);
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 10));
    
    for (frame_idx = 0; frame_idx <= 150; ++frame_idx) {
        uint64_t timestamp_us = frame_idx * 10000U;
        
        if (frame_idx <= 1) {
            g_encoder_counts[0] = 0;
            g_encoder_counts[1] = 0;
            g_encoder_counts[2] = 0;
            g_encoder_counts[3] = 0;
            g_imu_raw.gyro[2] = 0;
            g_ir_mask = 0x060;
        } else if (frame_idx <= 50) {
            g_encoder_counts[0] = (int32_t)(frame_idx * 20);
            g_encoder_counts[1] = (int32_t)(frame_idx * 20);
            g_encoder_counts[2] = (int32_t)(-frame_idx * 20);
            g_encoder_counts[3] = (int32_t)(-frame_idx * 20);
            g_imu_raw.gyro[2] = 0;
            g_ir_mask = 0x060;
        } else if (frame_idx <= 100) {
            int32_t base = 50 * 20 + (int32_t)(frame_idx - 50) * 15;
            g_encoder_counts[0] = base;
            g_encoder_counts[1] = base + (int32_t)(frame_idx - 50) * 5;
            g_encoder_counts[2] = -base;
            g_encoder_counts[3] = -base - (int32_t)(frame_idx - 50) * 5;
            g_imu_raw.gyro[2] = 328;
            g_ir_mask = 0x800;
        } else if (frame_idx <= 110) {
            int32_t base = 50 * 20 + 50 * 15 + (int32_t)(frame_idx - 100) * 10;
            g_encoder_counts[0] = base;
            g_encoder_counts[1] = base;
            g_encoder_counts[2] = -base;
            g_encoder_counts[3] = -base;
            g_imu_raw.gyro[2] = 0;
            g_ir_mask = 0x000;
        } else if (frame_idx <= 120) {
            int32_t base = 50 * 20 + 50 * 15 + 10 * 10 + (int32_t)(frame_idx - 110) * 10;
            g_encoder_counts[0] = base;
            g_encoder_counts[1] = base;
            g_encoder_counts[2] = -base;
            g_encoder_counts[3] = -base;
            g_imu_raw.gyro[2] = 0;
            g_ir_mask = 0x040;
        } else {
            int32_t base = 50 * 20 + 50 * 15 + 10 * 10 + 10 * 10 + (int32_t)(frame_idx - 120) * 5;
            g_encoder_counts[0] = base;
            g_encoder_counts[1] = base;
            g_encoder_counts[2] = -base;
            g_encoder_counts[3] = -base;
            g_imu_raw.gyro[2] = 0;
            g_ir_mask = 0x000;
        }
        
        TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(timestamp_us, &frame));
        TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
        TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &frame.ir, timestamp_us, &perc_result));
        
        TEST_ASSERT_TRUE(isfinite(evaluator.state.x));
        TEST_ASSERT_TRUE(isfinite(evaluator.state.y));
        TEST_ASSERT_TRUE(isfinite(evaluator.state.theta));
        TEST_ASSERT_TRUE(isfinite(evaluator.state.v));
        TEST_ASSERT_TRUE(isfinite(evaluator.state.omega));
        
        for (i = 0; i < SD_EKF_STATE_COUNT; ++i) {
            TEST_ASSERT_TRUE(evaluator.state.P[i][i] >= 0.0f);
        }
        
        if (perc_result.event == ROAD_EVENT_CURVE_ENTRY) {
            saw_curve_entry = true;
        }
        if (perc_result.event == ROAD_EVENT_LINE_LOST) {
            saw_line_lost = true;
        }
        
        behavior_input.vehicle = &evaluator.state;
        behavior_input.perception = &perc_result;
        behavior_input.path_curvature = 0.0f;
        
        if (frame_idx == 0) {
            behavior_input.command = BEHAVIOR_CMD_NONE;
        } else if (frame_idx == 1) {
            behavior_input.command = BEHAVIOR_CMD_START;
        } else if (frame_idx == 150) {
            behavior_input.command = BEHAVIOR_CMD_STOP;
        } else {
            behavior_input.command = BEHAVIOR_CMD_NONE;
        }
        
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &behavior_input, &behavior_output));
        
        memcpy(&vehicle, &evaluator.state, sizeof(vehicle_state_t));
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior_output, 0.01f, &trajectory_output));
        
        TEST_ASSERT_TRUE(isfinite(trajectory_output.x));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.y));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.v));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.omega));
        
        if (frame_idx > 0) {
            TEST_ASSERT_TRUE(generator.last_nearest_index >= last_path_index);
            last_path_index = generator.last_nearest_index;
        } else {
            last_path_index = generator.last_nearest_index;
        }
    }
    
    TEST_ASSERT_TRUE(saw_curve_entry);
    TEST_ASSERT_TRUE(saw_line_lost);
    TEST_ASSERT_TRUE(trajectory_output.v < 0.5f);
    
    return true;
}

static bool test_imu_failure_cascade(void) {
    state_evaluator_t evaluator;
    perception_t perception;
    behavior_planner_t planner;
    trajectory_generator_t generator;
    path_point_t path[5];
    sensor_frame_t frame;
    perception_result_t perc_result;
    behavior_input_t behavior_input;
    behavior_output_t behavior_output;
    vehicle_state_t vehicle;
    trajectory_point_t trajectory_output;
    size_t i, frame_idx;
    sd_status_t preprocess_status;
    
    reset_virtual_sensors();
    sd_config_reset_defaults();
    TEST_ASSERT_EQ_INT(SD_OK, sensors_configure_hal(&g_virtual_hal));
    TEST_ASSERT_EQ_INT(SD_OK, sensors_init_all());
    
    for (i = 0U; i < 5U; ++i) {
        path[i].x = (float)i * 0.5f;
        path[i].y = 0.0f;
        path[i].heading = 0.0f;
        path[i].curvature = 0.0f;
    }
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    perception_init(&perception);
    behavior_planner_init(&planner);
    trajectory_generator_init(&generator, &g_sens_decision_config.trajectory);
    TEST_ASSERT_EQ_INT(SD_OK, trajectory_set_path(&generator, path, 5));
    
    g_encoder_counts[0] = 0;
    g_encoder_counts[1] = 0;
    g_encoder_counts[2] = 0;
    g_encoder_counts[3] = 0;
    g_imu_raw.gyro[2] = 0;
    g_ir_mask = 0x060;
    g_imu_status = SD_OK;
    
    TEST_ASSERT_EQ_INT(SD_OK, preprocess_update(0U, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, state_evaluator_update(&evaluator, &frame));
    TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &frame.ir, 0U, &perc_result));
    
    behavior_input.vehicle = &evaluator.state;
    behavior_input.perception = &perc_result;
    behavior_input.path_curvature = 0.0f;
    behavior_input.command = BEHAVIOR_CMD_START;
    
    TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &behavior_input, &behavior_output));
    
    g_imu_status = SD_ERR_HW_FAULT;
    g_encoder_status[0] = SD_ERR_HW_FAULT;
    g_ir_status = SD_ERR_HW_FAULT;
    
    for (frame_idx = 1; frame_idx <= 100; ++frame_idx) {
        uint64_t timestamp_us = frame_idx * 10000U;
        
        g_encoder_counts[0] = (int32_t)(frame_idx * 10);
        g_encoder_counts[1] = (int32_t)(frame_idx * 10);
        g_encoder_counts[2] = (int32_t)(-frame_idx * 10);
        g_encoder_counts[3] = (int32_t)(-frame_idx * 10);
        g_ir_mask = 0x000;
        
        preprocess_status = preprocess_update(timestamp_us, &frame);
        TEST_ASSERT_TRUE(preprocess_status != SD_OK);
        TEST_ASSERT_TRUE(!frame.imu_valid);
        
        (void)state_evaluator_update(&evaluator, &frame);
        
        TEST_ASSERT_TRUE(evaluator.consecutive_failure_frames > 0U);
        
        if (frame.ir_valid) {
            TEST_ASSERT_EQ_INT(SD_OK, perception_update(&perception, &frame.ir, timestamp_us, &perc_result));
        } else {
            perc_result.line_valid = false;
        }
        
        behavior_input.command = BEHAVIOR_CMD_NONE;
        TEST_ASSERT_EQ_INT(SD_OK, behavior_planner_update(&planner, &behavior_input, &behavior_output));
        
        memcpy(&vehicle, &evaluator.state, sizeof(vehicle_state_t));
        TEST_ASSERT_EQ_INT(SD_OK, trajectory_generate(&generator, &vehicle, &behavior_output, 0.01f, &trajectory_output));
        
        TEST_ASSERT_TRUE(isfinite(trajectory_output.x));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.y));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.v));
        TEST_ASSERT_TRUE(isfinite(trajectory_output.omega));
    }
    
    TEST_ASSERT_TRUE(trajectory_output.v <= 1.0f);
    
    return true;
}

int main(void) {
    run_test("default_config", test_default_config);
    run_test("config_validation", test_config_validation);
    run_test("assertion_guards", test_assertion_guards);
    run_test("utils", test_utils);
    run_test("logging", test_logging);
    run_test("sensor_hal_validation", test_sensor_hal_validation);
    run_test("sensor_api_validation", test_sensor_api_validation);
    run_test("sensor_lifecycle", test_sensor_lifecycle);
    run_test("sensor_init_reverse_rollback", test_sensor_init_reverse_rollback);
    run_test("sensor_init_mixed_state_rollback",
             test_sensor_init_mixed_state_rollback);
    run_test("release_resets_sensor_history", test_release_resets_sensor_history);
    run_test("encoder_conversion", test_encoder_conversion);
    run_test("encoder_time_and_wrap", test_encoder_time_and_wrap);
    run_test("active_sensor_config_snapshot", test_active_sensor_config_snapshot);
    run_test("imu_conversion_filter", test_imu_conversion_filter);
    run_test("ir_mapping", test_ir_mapping);
    run_test("preprocess_partial_failure", test_preprocess_partial_failure);
    run_test("vtable_frame_metadata_coherence",
             test_vtable_frame_metadata_coherence);
    run_test("ekf_first_frame_baseline", test_ekf_first_frame_baseline);
    run_test("ekf_straight_travel", test_ekf_straight_travel);
    run_test("ekf_rotation_in_place", test_ekf_rotation_in_place);
    run_test("ekf_gyro_correction", test_ekf_gyro_correction);
    run_test("ekf_covariance_properties", test_ekf_covariance_properties);
    run_test("ekf_timestamp_rollback", test_ekf_timestamp_rollback);
    run_test("ekf_excessive_dt", test_ekf_excessive_dt);
    run_test("ekf_nan_observation", test_ekf_nan_observation);
    run_test("ekf_repeated_failures_clear_valid", test_ekf_repeated_failures_clear_valid);
    run_test("perception_center_masks", test_perception_center_masks);
    run_test("perception_lateral_signs", test_perception_lateral_signs);
    run_test("perception_intersection_event", test_perception_intersection_event);
    run_test("perception_curve_entry_event", test_perception_curve_entry_event);
    run_test("perception_line_lost_event", test_perception_line_lost_event);
    run_test("perception_loss_count_reset", test_perception_loss_count_reset);
    run_test("perception_timestamp_rollback", test_perception_timestamp_rollback);
    run_test("perception_null_input", test_perception_null_input);
    run_test("perception_heading_derivative", test_perception_heading_derivative);
    run_test("behavior_idle_start_gating", test_behavior_idle_start_gating);
    run_test("behavior_line_to_approach_curve", test_behavior_line_to_approach_curve);
    run_test("behavior_approach_to_curve", test_behavior_approach_to_curve);
    run_test("behavior_curve_exit", test_behavior_curve_exit);
    run_test("behavior_short_loss_recovery", test_behavior_short_loss_recovery);
    run_test("behavior_persistent_loss", test_behavior_persistent_loss);
    run_test("behavior_immediate_stop", test_behavior_immediate_stop);
    run_test("behavior_fault_on_failures", test_behavior_fault_on_failures);
    run_test("behavior_reset_reject_unhealthy", test_behavior_reset_reject_unhealthy);
    run_test("behavior_reset_to_idle", test_behavior_reset_to_idle);
    run_test("behavior_no_transition_log", test_behavior_no_transition_log);
    run_test("trajectory_empty_path", test_trajectory_empty_path);
    run_test("trajectory_nan_path", test_trajectory_nan_path);
    run_test("trajectory_straight_path_progress", test_trajectory_straight_path_progress);
    run_test("trajectory_curvature_speed_limit", test_trajectory_curvature_speed_limit);
    run_test("trajectory_accel_constraint", test_trajectory_accel_constraint);
    run_test("trajectory_decel_constraint", test_trajectory_decel_constraint);
    run_test("trajectory_jerk_constraint", test_trajectory_jerk_constraint);
    run_test("trajectory_angular_velocity", test_trajectory_angular_velocity);
    run_test("trajectory_stopped_state", test_trajectory_stopped_state);
    run_test("trajectory_fault_state", test_trajectory_fault_state);
    run_test("trajectory_stopped_decel_limit", test_trajectory_stopped_decel_limit);
    run_test("trajectory_fault_decel_limit", test_trajectory_fault_decel_limit);
    run_test("trajectory_alpha_uses_vehicle_omega", test_trajectory_alpha_uses_vehicle_omega);
    run_test("trajectory_zero_length_segment", test_trajectory_zero_length_segment);
    run_test("trajectory_backward_segment", test_trajectory_backward_segment);
    run_test("trajectory_line_lost_frozen_target", test_trajectory_line_lost_frozen_target);
    run_test("complete_driving_sequence", test_complete_driving_sequence);
    run_test("imu_failure_cascade", test_imu_failure_cascade);
    SD_LOG_INFO("test summary: passed=%d, failed=%d", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
