/**
 * @file      EKF.h
 * @brief     Sens-Decision层的扩展卡尔曼滤波部分的逻辑封装
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 * @note      Sens-Decision层的扩展卡尔曼滤波部分的逻辑封装
 */
#ifndef EKF_H
#define EKF_H

#include "config.h"

typedef struct {
    float state[SD_EKF_STATE_COUNT];
    float covariance[SD_EKF_STATE_COUNT][SD_EKF_STATE_COUNT];
    float process_noise[SD_EKF_STATE_COUNT][SD_EKF_STATE_COUNT];
    float observation_noise[SD_EKF_OBSERVATION_COUNT][SD_EKF_OBSERVATION_COUNT];
} ekf_t;

void ekf_init(ekf_t *ekf, const sd_ekf_config_t *config);
void ekf_predict(ekf_t *ekf, float dt);
sd_status_t ekf_update(ekf_t *ekf, const float observation[SD_EKF_OBSERVATION_COUNT]);

#endif