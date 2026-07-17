#ifndef SENS_DECISION_INTERFACE_H
#define SENS_DECISION_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef struct {
    int16_t accel[3];
    int16_t gyro[3];
    int16_t temperature;
} imu_raw_data_t;

typedef struct {
    sd_status_t (*read_encoder_count)(uint8_t index, int32_t *count);
    sd_status_t (*read_imu_raw)(imu_raw_data_t *data);
    sd_status_t (*read_ir_mask)(uint16_t *active_mask);
} sensor_hal_t;

typedef enum {
    SENSOR_ID_ENCODER_LEFT_FRONT,
    SENSOR_ID_ENCODER_LEFT_REAR,
    SENSOR_ID_ENCODER_RIGHT_FRONT,
    SENSOR_ID_ENCODER_RIGHT_REAR,
    SENSOR_ID_IMU,
    SENSOR_ID_IR_ARRAY,
    SENSOR_ID_COUNT
} sensor_id_t;

typedef struct sensor sensor_t;

typedef struct {
    sd_status_t (*init)(sensor_t *sensor);
    sd_status_t (*read)(sensor_t *sensor, void *output, uint64_t timestamp_us);
    sd_status_t (*write)(sensor_t *sensor, const void *input);
    sd_status_t (*release)(sensor_t *sensor);
} sensor_vtable_t;

struct sensor {
    const sensor_vtable_t *vtable;
    void *private_data;
    bool initialized;
};

typedef struct {
    int32_t count;
    float speed_mps;
    uint64_t timestamp_us;
} encoder_data_t;

typedef struct {
    float accel_mps2[3];
    float gyro_radps[3];
    float temperature_c;
    uint64_t timestamp_us;
} imu_data_t;

typedef struct {
    uint16_t active_mask;
    float values[SD_IR_CHANNEL_COUNT];
    uint64_t timestamp_us;
} ir_array_data_t;

sd_status_t sensors_configure_hal(const sensor_hal_t *hal);
sd_status_t sensors_init_all(void);
sensor_t *sensor_get(sensor_id_t id);
sd_status_t sensor_read(sensor_t *sensor, void *output, uint64_t timestamp_us);
sd_status_t sensor_write(sensor_t *sensor, const void *input);
sd_status_t sensor_release(sensor_t *sensor);
sd_status_t sensors_release_all(void);

#endif
