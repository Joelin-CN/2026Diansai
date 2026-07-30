/**
 * @file      trajectory_generate.h
 * @brief     Sens-Decision层的轨迹生成部分的逻辑封装
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 * @note      Sens-Decision层的轨迹生成部分的逻辑封装
 */
#ifndef TRAJECTORY_GENERATE_H
#define TRAJECTORY_GENERATE_H

#include <stddef.h>

#include "behavior_planner.h"
#include "config.h"
#include "state_evaluate.h"

typedef struct {
    float x;
    float y;
    float heading;
    float curvature;
} path_point_t;

typedef struct {
    float x;
    float y;
    float theta;
    float v;
    float omega;
    float a;
    float alpha;
    float curvature;
} trajectory_point_t;

typedef struct {
    const sd_trajectory_config_t *config;
    const path_point_t *path;
    size_t path_count;
    size_t last_nearest_index;
    size_t last_valid_target_index;
    float last_acceleration;
    bool initialized;
} trajectory_generator_t;

void trajectory_generator_init(trajectory_generator_t *generator,
                              const sd_trajectory_config_t *config);
sd_status_t trajectory_set_path(trajectory_generator_t *generator,
                               const path_point_t *path,
                               size_t count);
sd_status_t trajectory_generate(trajectory_generator_t *generator,
                               const vehicle_state_t *vehicle,
                               const behavior_output_t *behavior,
                               float dt,
                               trajectory_point_t *output);

#endif // TRAJECTORY_GENERATE_H