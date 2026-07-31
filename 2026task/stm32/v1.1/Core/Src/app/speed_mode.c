/**
 * @file      speed_mode.c
 * @brief     Speed mode configuration implementation
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 */

#include "speed_mode.h"
#include "config.h"
#include <stdio.h>

static speed_mode_t g_current_mode = SPEED_MODE_DEBUG;

void speed_mode_set(speed_mode_t mode) {
    g_current_mode = mode;

    switch (mode) {
        case SPEED_MODE_DEBUG:
            g_sens_decision_config.behavior.line_speed_mps = 0.2f;
            printf("[SpeedMode] DEBUG: line=0.2 m/s\n");
            printf("[SpeedMode] Ultra-low speed for sensor verification\n");
            break;

        case SPEED_MODE_SLOW:
            g_sens_decision_config.behavior.line_speed_mps = 0.5f;
            printf("[SpeedMode] SLOW: line=0.5 m/s\n");
            printf("[SpeedMode] Low speed for control tuning\n");
            break;

        case SPEED_MODE_NORMAL:
            g_sens_decision_config.behavior.line_speed_mps = 1.0f;
            printf("[SpeedMode] NORMAL: line=1.0 m/s\n");
            printf("[SpeedMode] Standard speed for validated system\n");
            break;

        case SPEED_MODE_FAST:
            g_sens_decision_config.behavior.line_speed_mps = 1.5f;
            printf("[SpeedMode] FAST: line=1.5 m/s\n");
            printf("[SpeedMode] Competition speed mode\n");
            break;

        default:
            printf("[SpeedMode] ERROR: Unknown mode %d, using DEBUG\n", mode);
            speed_mode_set(SPEED_MODE_DEBUG);
            return;
    }
}

speed_mode_t speed_mode_get(void) {
    return g_current_mode;
}

const char* speed_mode_name(speed_mode_t mode) {
    switch (mode) {
        case SPEED_MODE_DEBUG:  return "DEBUG";
        case SPEED_MODE_SLOW:   return "SLOW";
        case SPEED_MODE_NORMAL: return "NORMAL";
        case SPEED_MODE_FAST:   return "FAST";
        default:                return "UNKNOWN";
    }
}
