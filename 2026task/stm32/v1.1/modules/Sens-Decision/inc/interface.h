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
    sd_status_t (*read_ir)(uint16_t *active_mask, float values[SD_IR_CHANNEL_COUNT]);
} sensor_hal_t;

/**
 * @brief 传感器ID枚举（双轮差速底盘配置）
 *
 * @note 从四轮配置迁移到双轮配置（2026-07-30）
 *       - 移除: ENCODER_LEFT_REAR, ENCODER_RIGHT_REAR (四轮后轮编码器)
 *       - 保留: 2个编码器（左轮+右轮）+ IMU + 红外阵列
 *
 * @hardware 物理传感器映射:
 *   - SENSOR_ID_ENCODER_LEFT:  TIM3编码器（左轮电机）
 *   - SENSOR_ID_ENCODER_RIGHT: TIM4编码器（右轮电机）
 *   - SENSOR_ID_IMU:           ICM42688（6轴IMU）
 *   - SENSOR_ID_IR_ARRAY:      8通道红外传感器阵列
 */
typedef enum {
    SENSOR_ID_ENCODER_LEFT,   /* 左轮编码器（物理索引0） */
    SENSOR_ID_ENCODER_RIGHT,  /* 右轮编码器（物理索引1） */
    SENSOR_ID_IMU,            /* IMU传感器 */
    SENSOR_ID_IR_ARRAY,       /* 红外传感器阵列 */
    SENSOR_ID_COUNT           /* 传感器总数：4 */
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

/**
 * @brief Run sensor diagnostic report
 * @note Prints detailed status of each sensor for debugging
 */
void sensors_diagnostic_report(void);

#endif
