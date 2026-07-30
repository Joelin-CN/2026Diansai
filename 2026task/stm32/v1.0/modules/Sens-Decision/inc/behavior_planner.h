/**
 * @file      behavior_planner.h
 * @brief     Sens-Decision层的行为规划部分的逻辑封装
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 * @note      Sens-Decision层的行为规划部分的逻辑封装
 */
#ifndef BEHAVIOR_PLANNER_H
#define BEHAVIOR_PLANNER_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "perception.h"
#include "state_evaluate.h"

typedef enum {
    BEHAVIOR_STATE_IDLE,
    BEHAVIOR_STATE_LINE_FOLLOW,
    BEHAVIOR_STATE_APPROACH_CURVE,
    BEHAVIOR_STATE_CURVE,
    BEHAVIOR_STATE_LINE_LOST_DEGRADED,
    BEHAVIOR_STATE_STOPPED,
    BEHAVIOR_STATE_FAULT
} behavior_state_t;

typedef enum {
    BEHAVIOR_CMD_NONE,
    BEHAVIOR_CMD_START,
    BEHAVIOR_CMD_STOP,
    BEHAVIOR_CMD_RESET
} behavior_command_t;

typedef struct {
    const vehicle_state_t *vehicle;
    const perception_result_t *perception;
    behavior_command_t command;
    float path_curvature;
} behavior_input_t;

typedef struct {
    behavior_state_t state;
    float speed_limit_mps;
    float last_valid_lateral_error;
} behavior_output_t;

typedef struct {
    behavior_state_t current_state;
    behavior_state_t previous_running_state;
    uint16_t line_lost_frames;
    uint16_t critical_failure_count;
    uint16_t stable_straight_frames;
    float last_valid_lateral_error;
    bool initialized;
} behavior_planner_t;

void behavior_planner_init(behavior_planner_t *planner);
sd_status_t behavior_planner_update(behavior_planner_t *planner,
                                   const behavior_input_t *input,
                                   behavior_output_t *output);

#endif // BEHAVIOR_PLANNER_H