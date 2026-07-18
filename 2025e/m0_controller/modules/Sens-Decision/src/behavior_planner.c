/**
 * @file      behavior_planner.c
 * @brief     Source file for behavior planner module.
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 */

#include "../inc/behavior_planner.h"
#include "../inc/utils.h"

#include <math.h>
#include <stddef.h>

static const char *behavior_state_names[] = {
    "IDLE",
    "LINE_FOLLOW",
    "APPROACH_CURVE",
    "CURVE",
    "LINE_LOST_DEGRADED",
    "STOPPED",
    "FAULT"
};

static const char *get_state_name(behavior_state_t state) {
    if (state >= 0 && state < (behavior_state_t)(sizeof(behavior_state_names) / sizeof(behavior_state_names[0]))) {
        return behavior_state_names[state];
    }
    return "UNKNOWN";
}

void behavior_planner_init(behavior_planner_t *planner) {
    if (planner == NULL) {
        return;
    }
    planner->current_state = BEHAVIOR_STATE_IDLE;
    planner->previous_running_state = BEHAVIOR_STATE_IDLE;
    planner->line_lost_frames = 0U;
    planner->critical_failure_count = 0U;
    planner->stable_straight_frames = 0U;
    planner->last_valid_lateral_error = 0.0f;
    planner->initialized = true;
}

sd_status_t behavior_planner_update(behavior_planner_t *planner,
                                   const behavior_input_t *input,
                                   behavior_output_t *output) {
    behavior_state_t new_state;
    bool critical_failure;
    bool was_running_state;
    
    if (planner == NULL || input == NULL || output == NULL ||
        input->vehicle == NULL || input->perception == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    new_state = planner->current_state;
    
    was_running_state = (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW ||
                        planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE ||
                        planner->current_state == BEHAVIOR_STATE_CURVE);
    
    if (was_running_state) {
        planner->last_valid_lateral_error = input->perception->lateral_error;
    }
    
    critical_failure = !input->vehicle->localization_valid && !input->perception->line_valid;
    
    if (critical_failure) {
        planner->critical_failure_count++;
    } else {
        planner->critical_failure_count = 0U;
    }
    
    if (input->command == BEHAVIOR_CMD_STOP) {
        new_state = BEHAVIOR_STATE_STOPPED;
    } else if (planner->current_state != BEHAVIOR_STATE_IDLE &&
               planner->current_state != BEHAVIOR_STATE_STOPPED &&
               planner->current_state != BEHAVIOR_STATE_FAULT &&
               planner->critical_failure_count >= g_sens_decision_config.behavior.critical_failure_frames) {
        new_state = BEHAVIOR_STATE_FAULT;
    } else if (input->command == BEHAVIOR_CMD_RESET &&
               input->vehicle->localization_valid &&
               input->perception->line_valid) {
        new_state = BEHAVIOR_STATE_IDLE;
    } else if (!input->perception->line_valid) {
        if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW ||
            planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE ||
            planner->current_state == BEHAVIOR_STATE_CURVE) {
            
            planner->previous_running_state = planner->current_state;
            planner->line_lost_frames = 1U;
            new_state = BEHAVIOR_STATE_LINE_LOST_DEGRADED;
        } else if (planner->current_state == BEHAVIOR_STATE_LINE_LOST_DEGRADED) {
            planner->line_lost_frames++;
            if (planner->line_lost_frames > g_sens_decision_config.behavior.line_lost_stop_frames) {
                new_state = BEHAVIOR_STATE_STOPPED;
            }
        }
    } else {
        if (planner->current_state == BEHAVIOR_STATE_LINE_LOST_DEGRADED &&
            planner->line_lost_frames <= g_sens_decision_config.behavior.line_recovery_frames) {
            new_state = planner->previous_running_state;
            planner->line_lost_frames = 0U;
        } else if (planner->current_state == BEHAVIOR_STATE_IDLE &&
                   input->command == BEHAVIOR_CMD_START &&
                   input->vehicle->localization_valid &&
                   input->perception->line_valid) {
            new_state = BEHAVIOR_STATE_LINE_FOLLOW;
        } else if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW &&
                   input->perception->event == ROAD_EVENT_CURVE_ENTRY) {
            new_state = BEHAVIOR_STATE_APPROACH_CURVE;
        } else if (planner->current_state == BEHAVIOR_STATE_APPROACH_CURVE &&
                   (fabsf(input->perception->heading_error) >= 0.2f ||
                    fabsf(input->path_curvature) >= 0.2f)) {
            new_state = BEHAVIOR_STATE_CURVE;
            planner->stable_straight_frames = 0U;
        } else if (planner->current_state == BEHAVIOR_STATE_CURVE) {
            if (fabsf(input->perception->heading_error) < 0.1f &&
                fabsf(input->path_curvature) < 0.1f) {
                planner->stable_straight_frames++;
                if (planner->stable_straight_frames >= g_sens_decision_config.behavior.curve_exit_stable_frames) {
                    new_state = BEHAVIOR_STATE_LINE_FOLLOW;
                    planner->stable_straight_frames = 0U;
                }
            } else {
                planner->stable_straight_frames = 0U;
            }
        }
    }
    
    if (new_state != planner->current_state) {
        SD_LOG_WARNING("behavior changed from %s to %s",
                      get_state_name(planner->current_state),
                      get_state_name(new_state));
        planner->current_state = new_state;
    }
    
    output->state = planner->current_state;
    
    switch (planner->current_state) {
        case BEHAVIOR_STATE_IDLE:
            output->speed_limit_mps = g_sens_decision_config.behavior.idle_speed_mps;
            break;
        case BEHAVIOR_STATE_LINE_FOLLOW:
            output->speed_limit_mps = g_sens_decision_config.behavior.line_speed_mps;
            break;
        case BEHAVIOR_STATE_APPROACH_CURVE:
            output->speed_limit_mps = g_sens_decision_config.behavior.approach_curve_speed_mps;
            break;
        case BEHAVIOR_STATE_CURVE:
            output->speed_limit_mps = g_sens_decision_config.behavior.curve_speed_mps;
            break;
        case BEHAVIOR_STATE_LINE_LOST_DEGRADED:
            output->speed_limit_mps = g_sens_decision_config.behavior.degraded_speed_mps *
                                     (1.0f - (float)planner->line_lost_frames /
                                     (float)g_sens_decision_config.behavior.line_lost_stop_frames);
            if (output->speed_limit_mps < 0.0f) {
                output->speed_limit_mps = 0.0f;
            }
            break;
        case BEHAVIOR_STATE_STOPPED:
        case BEHAVIOR_STATE_FAULT:
            output->speed_limit_mps = 0.0f;
            break;
        default:
            output->speed_limit_mps = 0.0f;
            break;
    }
    
    output->last_valid_lateral_error = planner->last_valid_lateral_error;
    
    return SD_OK;
}
