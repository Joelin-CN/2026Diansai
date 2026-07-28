#ifndef SENS_DECISION_PREPROCESS_H
#define SENS_DECISION_PREPROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "interface.h"

typedef struct {
    uint64_t timestamp_us;
    encoder_data_t encoders[SD_ENCODER_COUNT];
    bool encoder_valid[SD_ENCODER_COUNT];
    imu_data_t imu;
    bool imu_valid;
    ir_array_data_t ir;
    bool ir_valid;
} sensor_frame_t;

sd_status_t preprocess_update(uint64_t timestamp_us, sensor_frame_t *frame);

#endif
