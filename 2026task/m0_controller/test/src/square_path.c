/**
 * @file      square_path.c
 * @brief     50 Hz hybrid square tracking implementation
 * @author    Subagent Task 6
 * @version   1.0.0
 * @date      2026-07-18
 */

#include <math.h>
#include "../inc/square_path.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 1m × 1m square, 4m perimeter, 20mm max spacing → 201 points (200 segments) */
#define SQUARE_POINT_COUNT 201U
#define SQUARE_SIDE_LENGTH 1.0f
#define POINTS_PER_SIDE 50U

static path_point_t g_square_path[SQUARE_POINT_COUNT];
static bool g_path_initialized = false;

/**
 * @brief Initialize square path with CCW traversal A→C→D→B→A
 * Coordinate frame: A=(0,0), C=(0,1), D=(1,1), B=(1,0)
 * Per spec, this is CCW (robot convention with y-forward/down)
 */
static void init_square_path(void) {
    size_t idx = 0;
    
    /* Side A→C: (0,0) to (0,1), heading = +π/2 (north) */
    for (size_t i = 0; i < POINTS_PER_SIDE; ++i) {
        float t = (float)i / (float)POINTS_PER_SIDE;
        g_square_path[idx].x = 0.0f;
        g_square_path[idx].y = t * SQUARE_SIDE_LENGTH;
        g_square_path[idx].heading = (float)(M_PI / 2.0);
        g_square_path[idx].curvature = 0.0f;
        idx++;
    }
    
    /* Side C→D: (0,1) to (1,1), heading = 0 (east) */
    for (size_t i = 0; i < POINTS_PER_SIDE; ++i) {
        float t = (float)i / (float)POINTS_PER_SIDE;
        g_square_path[idx].x = t * SQUARE_SIDE_LENGTH;
        g_square_path[idx].y = SQUARE_SIDE_LENGTH;
        g_square_path[idx].heading = 0.0f;
        g_square_path[idx].curvature = 0.0f;
        idx++;
    }
    
    /* Side D→B: (1,1) to (1,0), heading = -π/2 (south) */
    for (size_t i = 0; i < POINTS_PER_SIDE; ++i) {
        float t = (float)i / (float)POINTS_PER_SIDE;
        g_square_path[idx].x = SQUARE_SIDE_LENGTH;
        g_square_path[idx].y = SQUARE_SIDE_LENGTH - t * SQUARE_SIDE_LENGTH;
        g_square_path[idx].heading = (float)(-M_PI / 2.0);
        g_square_path[idx].curvature = 0.0f;
        idx++;
    }
    
    /* Side B→A: (1,0) to (0,0), heading = ±π (west) */
    for (size_t i = 0; i <= POINTS_PER_SIDE; ++i) {  /* Include endpoint */
        float t = (float)i / (float)POINTS_PER_SIDE;
        g_square_path[idx].x = SQUARE_SIDE_LENGTH - t * SQUARE_SIDE_LENGTH;
        g_square_path[idx].y = 0.0f;
        g_square_path[idx].heading = (float)M_PI;
        g_square_path[idx].curvature = 0.0f;
        idx++;
        if (idx >= SQUARE_POINT_COUNT) break;
    }
    
    g_path_initialized = true;
}

const path_point_t *SquarePath_GetPoints(void) {
    if (!g_path_initialized) {
        init_square_path();
    }
    return g_square_path;
}

size_t SquarePath_GetPointCount(void) {
    if (!g_path_initialized) {
        init_square_path();
    }
    return SQUARE_POINT_COUNT;
}

float SquarePath_CorrectOmega(float nominal_omega, float lateral_error,
                              float heading_error,
                              const square_path_config_t *config) {
    if (config == NULL) {
        return nominal_omega;
    }
    
    /* Lateral correction: negative error (left) → positive omega (turn right) */
    float lateral_correction = -lateral_error * config->lateral_gain;
    
    /* Heading correction: positive error (pointing right) → negative omega (turn left) */
    float heading_correction = -heading_error * config->heading_gain;
    
    float corrected = nominal_omega + lateral_correction + heading_correction;
    
    /* Clamp to [-max_omega, +max_omega] */
    if (corrected > config->max_omega_radps) {
        corrected = config->max_omega_radps;
    } else if (corrected < -config->max_omega_radps) {
        corrected = -config->max_omega_radps;
    }
    
    return corrected;
}

bool SquarePath_UpdateLap(lap_counter_t *counter, size_t nearest_index,
                          size_t path_count, uint8_t target_laps) {
    if (counter == NULL || path_count == 0) {
        return false;
    }
    
    /* Validate target_laps: must be in range 1-5 */
    if (target_laps < 1 || target_laps > 5) {
        return false;
    }
    
    /* Guard zone is first 5% of path */
    size_t guard_threshold = path_count / 20;
    if (guard_threshold < 5) {
        guard_threshold = 5;
    }
    
    bool in_start_zone = (nearest_index < guard_threshold);
    
    /* If not in start zone, mark as having left */
    if (!in_start_zone) {
        counter->left_start_guard = true;
        return false;
    }
    
    /* In start zone: only increment if we've left before */
    if (counter->left_start_guard) {
        counter->completed_laps++;
        counter->left_start_guard = false;  /* Reset guard */
        
        /* Check if target reached */
        if (counter->completed_laps >= target_laps) {
            counter->target_reached = true;
        }
        
        return true;
    }
    
    return false;
}
