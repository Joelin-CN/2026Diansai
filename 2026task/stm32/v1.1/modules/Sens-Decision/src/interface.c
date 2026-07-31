#include "interface.h"

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "utils.h"

typedef struct {
    uint8_t index;
    sd_encoder_config_t config;
    int32_t previous_count;
    uint64_t previous_timestamp_us;
    bool has_baseline;
} encoder_private_t;

typedef struct {
    sd_imu_config_t config;
    float filtered_accel[3];
    float filtered_gyro[3];
    bool has_sample;
} imu_private_t;

typedef struct {
    sd_perception_config_t config;
} ir_private_t;

static sensor_hal_t g_hal;
static bool g_hal_configured;
static encoder_private_t g_encoder_private[SD_ENCODER_COUNT];
static imu_private_t g_imu_private;
static ir_private_t g_ir_private;

static sd_status_t encoder_init(sensor_t *sensor);
static sd_status_t encoder_read(sensor_t *sensor, void *output,
                                uint64_t timestamp_us);
static sd_status_t imu_init(sensor_t *sensor);
static sd_status_t imu_read(sensor_t *sensor, void *output,
                            uint64_t timestamp_us);
static sd_status_t ir_init(sensor_t *sensor);
static sd_status_t ir_read(sensor_t *sensor, void *output,
                           uint64_t timestamp_us);
static sd_status_t read_only_write(sensor_t *sensor, const void *input);
static sd_status_t common_release(sensor_t *sensor);
static const char *get_sensor_name(sensor_id_t id);
static const char *get_status_string(sd_status_t status);

static const sensor_vtable_t g_encoder_vtable = {
    encoder_init, encoder_read, read_only_write, common_release
};
static const sensor_vtable_t g_imu_vtable = {
    imu_init, imu_read, read_only_write, common_release
};
static const sensor_vtable_t g_ir_vtable = {
    ir_init, ir_read, read_only_write, common_release
};

/**
 * @brief 传感器对象数组（双轮差速底盘配置）
 *
 * @note 从四轮配置迁移到双轮配置（2026-07-30）
 *       - 索引0: 左轮编码器 (SENSOR_ID_ENCODER_LEFT)
 *       - 索引1: 右轮编码器 (SENSOR_ID_ENCODER_RIGHT)
 *       - 索引2: IMU (SENSOR_ID_IMU)
 *       - 索引3: 红外阵列 (SENSOR_ID_IR_ARRAY)
 *       - 总数: 4个传感器对象
 *
 * @warning 私有数据指针必须指向有效的数组元素:
 *          - g_encoder_private[0..1] (SD_ENCODER_COUNT=2)
 *          - g_imu_private (单个对象)
 *          - g_ir_private (单个对象)
 */
static sensor_t g_sensors[SENSOR_ID_COUNT] = {
    {&g_encoder_vtable, &g_encoder_private[0], false},  /* 左轮编码器 */
    {&g_encoder_vtable, &g_encoder_private[1], false},  /* 右轮编码器 */
    {&g_imu_vtable, &g_imu_private, false},             /* IMU */
    {&g_ir_vtable, &g_ir_private, false}                /* 红外阵列 */
};

sd_status_t sensors_configure_hal(const sensor_hal_t *hal) {
    if (hal == NULL || hal->read_encoder_count == NULL ||
        hal->read_imu_raw == NULL || hal->read_ir == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    g_hal = *hal;
    g_hal_configured = true;
    return SD_OK;
}

sd_status_t sensors_init_all(void) {
    bool initialized_this_call[SENSOR_ID_COUNT] = {false};
    size_t index;
    sd_status_t status;

    printf("[SensDecision] ===== Sensor Initialization Start =====\n");

    if (!g_hal_configured) {
        printf("[ERROR] HAL not configured - call sensors_configure_hal() first\n");
        return SD_ERR_NOT_INITIALIZED;
    }
    printf("[SensDecision] HAL configuration: OK\n");

    status = sd_config_validate(&g_sens_decision_config);
    if (status != SD_OK) {
        printf("[ERROR] Config validation failed: %s\n", get_status_string(status));
        printf("[ERROR] Check encoder indices and parameter ranges\n");
        return status;
    }
    printf("[SensDecision] Config validation: OK\n");

    for (index = 0U; index < SENSOR_ID_COUNT; ++index) {
        if (g_sensors[index].initialized) {
            printf("[SensDecision] Sensor %u (%s): Already initialized\n",
                   (unsigned)index, get_sensor_name((sensor_id_t)index));
            continue;
        }

        printf("[SensDecision] Initializing sensor %u (%s)...\n",
               (unsigned)index, get_sensor_name((sensor_id_t)index));

        status = g_sensors[index].vtable->init(&g_sensors[index]);
        if (status != SD_OK) {
            printf("[ERROR] Sensor %u (%s) initialization failed: %s\n",
                   (unsigned)index, get_sensor_name((sensor_id_t)index),
                   get_status_string(status));

            /* Rollback: Release all sensors initialized in this call */
            printf("[SensDecision] Rolling back initialization...\n");
            while (index > 0U) {
                --index;
                if (initialized_this_call[index]) {
                    (void)sensor_release(&g_sensors[index]);
                    printf("[SensDecision] Released sensor %u (%s)\n",
                           (unsigned)index, get_sensor_name((sensor_id_t)index));
                }
            }
            return status;
        }

        printf("[SensDecision] Sensor %u (%s): OK\n",
               (unsigned)index, get_sensor_name((sensor_id_t)index));
        g_sensors[index].initialized = true;
        initialized_this_call[index] = true;
    }

    printf("[SensDecision] ===== All sensors initialized successfully =====\n");
    return SD_OK;
}

sensor_t *sensor_get(sensor_id_t id) {
    if ((int)id < 0 || id >= SENSOR_ID_COUNT) {
        return NULL;
    }
    return &g_sensors[id];
}

sd_status_t sensor_read(sensor_t *sensor, void *output, uint64_t timestamp_us) {
    if (sensor == NULL || output == NULL || sensor->vtable == NULL ||
        sensor->vtable->read == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (!sensor->initialized) {
        return SD_ERR_NOT_INITIALIZED;
    }
    return sensor->vtable->read(sensor, output, timestamp_us);
}

sd_status_t sensor_write(sensor_t *sensor, const void *input) {
    if (sensor == NULL || input == NULL || sensor->vtable == NULL ||
        sensor->vtable->write == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (!sensor->initialized) {
        return SD_ERR_NOT_INITIALIZED;
    }
    return sensor->vtable->write(sensor, input);
}

sd_status_t sensor_release(sensor_t *sensor) {
    if (sensor == NULL || sensor->vtable == NULL ||
        sensor->vtable->release == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    return sensor->vtable->release(sensor);
}

sd_status_t sensors_release_all(void) {
    size_t index = SENSOR_ID_COUNT;
    sd_status_t first_status = SD_OK;

    while (index > 0U) {
        sd_status_t status;
        --index;
        status = sensor_release(&g_sensors[index]);
        if (first_status == SD_OK && status != SD_OK) {
            first_status = status;
        }
    }
    return first_status;
}

static sd_status_t encoder_init(sensor_t *sensor) {
    encoder_private_t *data = (encoder_private_t *)sensor->private_data;
    size_t index = (size_t)(data - g_encoder_private);

    data->index = (uint8_t)index;
    data->config = g_sens_decision_config.encoders[index];
    data->has_baseline = false;
    data->previous_count = 0;
    data->previous_timestamp_us = 0U;
    return SD_OK;
}

static sd_status_t encoder_read(sensor_t *sensor, void *output,
                                uint64_t timestamp_us) {
    encoder_private_t *data = (encoder_private_t *)sensor->private_data;
    encoder_data_t *result = (encoder_data_t *)output;
    int32_t count;
    sd_status_t status;

    if (data->has_baseline && timestamp_us <= data->previous_timestamp_us) {
        return SD_ERR_DATA_INVALID;
    }
    status = g_hal.read_encoder_count(data->index, &count);
    if (status != SD_OK) {
        return status;
    }
    result->count = count;
    result->timestamp_us = timestamp_us;
    result->speed_mps = 0.0f;
    if (data->has_baseline) {
        uint32_t difference = (uint32_t)count - (uint32_t)data->previous_count;
        int64_t delta_count = difference <= INT32_MAX
                                  ? (int64_t)difference
                                  : (int64_t)difference - INT64_C(4294967296);
        float dt_s = (float)(timestamp_us - data->previous_timestamp_us) /
                     1000000.0f;
        result->speed_mps = (float)data->config.direction *
                            (float)delta_count * 2.0f * SD_PI *
                            data->config.wheel_radius_m /
                            (float)data->config.pulses_per_revolution / dt_s;
    }
    data->previous_count = count;
    data->previous_timestamp_us = timestamp_us;
    data->has_baseline = true;
    return SD_OK;
}

static sd_status_t imu_init(sensor_t *sensor) {
    imu_private_t *data = (imu_private_t *)sensor->private_data;
    data->config = g_sens_decision_config.imu;
    memset(data->filtered_accel, 0, sizeof(data->filtered_accel));
    memset(data->filtered_gyro, 0, sizeof(data->filtered_gyro));
    data->has_sample = false;
    return SD_OK;
}

static sd_status_t imu_read(sensor_t *sensor, void *output,
                            uint64_t timestamp_us) {
    imu_private_t *data = (imu_private_t *)sensor->private_data;
    imu_data_t *result = (imu_data_t *)output;
    imu_raw_data_t raw;
    sd_status_t status = g_hal.read_imu_raw(&raw);
    size_t axis;

    if (status != SD_OK) {
        return status;
    }
    for (axis = 0U; axis < 3U; ++axis) {
        float accel = (float)raw.accel[axis] *
                          data->config.accel_scale_mps2_per_lsb -
                      data->config.accel_bias_mps2[axis];
        float gyro = (float)raw.gyro[axis] *
                         data->config.gyro_scale_radps_per_lsb -
                     data->config.gyro_bias_radps[axis];
        if (data->has_sample) {
            float alpha = data->config.filter_alpha;
            accel = alpha * data->filtered_accel[axis] +
                    (1.0f - alpha) * accel;
            gyro = alpha * data->filtered_gyro[axis] +
                   (1.0f - alpha) * gyro;
        }
        data->filtered_accel[axis] = accel;
        data->filtered_gyro[axis] = gyro;
        result->accel_mps2[axis] = accel;
        result->gyro_radps[axis] = gyro;
    }
    data->has_sample = true;
    /* Simple host/HAL contract: raw 0 is 25 C and 132 LSB is 1 C. */
    result->temperature_c = (float)raw.temperature / 132.0f + 25.0f;
    result->timestamp_us = timestamp_us;
    return SD_OK;
}

static sd_status_t ir_init(sensor_t *sensor) {
    ir_private_t *data = (ir_private_t *)sensor->private_data;
    data->config = g_sens_decision_config.perception;
    return SD_OK;
}

static sd_status_t ir_read(sensor_t *sensor, void *output,
                           uint64_t timestamp_us) {
    ir_array_data_t *result = (ir_array_data_t *)output;
    sd_status_t status;
    (void)sensor;

    status = g_hal.read_ir(&result->active_mask, result->values);
    if (status != SD_OK) {
        return status;
    }
    result->active_mask &= (uint16_t)((UINT16_C(1) << SD_IR_CHANNEL_COUNT) - 1U);
    result->timestamp_us = timestamp_us;
    return SD_OK;
}

static sd_status_t read_only_write(sensor_t *sensor, const void *input) {
    (void)sensor;
    (void)input;
    return SD_ERR_UNSUPPORTED;
}

static sd_status_t common_release(sensor_t *sensor) {
    if (sensor->private_data == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    if (sensor->vtable == &g_encoder_vtable) {
        encoder_private_t *data = (encoder_private_t *)sensor->private_data;
        data->has_baseline = false;
        data->previous_count = 0;
        data->previous_timestamp_us = 0U;
    } else if (sensor->vtable == &g_imu_vtable) {
        imu_private_t *data = (imu_private_t *)sensor->private_data;
        data->has_sample = false;
        memset(data->filtered_accel, 0, sizeof(data->filtered_accel));
        memset(data->filtered_gyro, 0, sizeof(data->filtered_gyro));
    }
    sensor->initialized = false;
    return SD_OK;
}

static const char *get_sensor_name(sensor_id_t id) {
    switch (id) {
        case SENSOR_ID_ENCODER_LEFT:
            return "Encoder_Left";
        case SENSOR_ID_ENCODER_RIGHT:
            return "Encoder_Right";
        case SENSOR_ID_IMU:
            return "IMU";
        case SENSOR_ID_IR_ARRAY:
            return "IR_Array";
        default:
            return "Unknown";
    }
}

static const char *get_status_string(sd_status_t status) {
    switch (status) {
        case SD_OK:
            return "OK";
        case SD_ERR_INVALID_ARGUMENT:
            return "Invalid argument";
        case SD_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case SD_ERR_UNSUPPORTED:
            return "Unsupported operation";
        case SD_ERR_DATA_INVALID:
            return "Invalid data";
        case SD_ERR_NULL_POINTER:
            return "NULL pointer";
        default:
            return "Unknown error";
    }
}

void sensors_diagnostic_report(void) {
    printf("\n========== SENSOR DIAGNOSTIC REPORT ==========\n");

    /* Check HAL configuration */
    if (!g_hal_configured) {
        printf("[FAIL] HAL not configured\n");
        printf("==============================================\n\n");
        return;
    }
    printf("[OK] HAL configured\n");

    /* Check config validation */
    sd_status_t status = sd_config_validate(&g_sens_decision_config);
    printf("Config validation: %s\n", status == SD_OK ? "[PASS]" : "[FAIL]");
    if (status != SD_OK) {
        printf("  Error: %s\n", get_status_string(status));
    }

    /* Test individual sensors */
    printf("\nSensor Status:\n");

    for (size_t i = 0; i < SENSOR_ID_COUNT; i++) {
        sensor_id_t id = (sensor_id_t)i;
        const char *name = get_sensor_name(id);
        sensor_t *sensor = &g_sensors[i];

        printf("  %u. %s: ", (unsigned)i, name);

        if (!sensor->initialized) {
            printf("[NOT INITIALIZED]\n");
            continue;
        }

        /* Try to read sensor data */
        if (id == SENSOR_ID_ENCODER_LEFT || id == SENSOR_ID_ENCODER_RIGHT) {
            encoder_data_t data;
            status = sensor_read(sensor, &data, 0);
            if (status == SD_OK) {
                printf("[OK] count=%ld\n", (long)data.count);
            } else {
                printf("[FAIL] %s\n", get_status_string(status));
            }
        } else if (id == SENSOR_ID_IMU) {
            imu_data_t data;
            status = sensor_read(sensor, &data, 0);
            if (status == SD_OK) {
                printf("[OK] accel=(%.2f, %.2f, %.2f) m/s^2\n",
                       data.accel_mps2[0], data.accel_mps2[1], data.accel_mps2[2]);
            } else {
                printf("[FAIL] %s\n", get_status_string(status));
            }
        } else if (id == SENSOR_ID_IR_ARRAY) {
            ir_array_data_t data;
            status = sensor_read(sensor, &data, 0);
            if (status == SD_OK) {
                printf("[OK] mask=0x%04X, values=[", data.active_mask);
                for (int j = 0; j < SD_IR_CHANNEL_COUNT; j++) {
                    printf("%.0f%s", data.values[j], j < SD_IR_CHANNEL_COUNT - 1 ? ", " : "");
                }
                printf("]\n");
            } else {
                printf("[FAIL] %s\n", get_status_string(status));
            }
        }
    }

    printf("==============================================\n\n");
}
