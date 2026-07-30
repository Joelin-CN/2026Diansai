/**
 * @file      state_evaluate.h
 * @brief     Sens-Decision层的状态评估部分部分的逻辑封装
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 * @note      Sens-Decision层的状态评估部分的逻辑封装
 */
#ifndef STATE_EVALUATE_H
#define STATE_EVALUATE_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "EKF.h"
#include "preprocess.h"

typedef struct {
    float x;
    float y;
    float theta;
    float v;
    float omega;
    float P[SD_EKF_STATE_COUNT][SD_EKF_STATE_COUNT];
    uint64_t timestamp_us;
    bool localization_valid;
} vehicle_state_t;

typedef struct {
    ekf_t ekf;
    vehicle_state_t state;
    uint64_t last_timestamp_us;
    uint16_t consecutive_valid_frames;
    uint16_t consecutive_failure_frames;
    bool initialized;
} state_evaluator_t;

void state_evaluator_init(state_evaluator_t *evaluator, const sd_ekf_config_t *ekf_config);
sd_status_t state_evaluator_update(state_evaluator_t *evaluator, const sensor_frame_t *frame);

#endif // STATE_EVALUATE_H
