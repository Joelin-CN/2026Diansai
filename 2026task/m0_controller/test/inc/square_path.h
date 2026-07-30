/**
 * @file      square_path.h
 * @brief     50 Hz hybrid square tracking and lap counting
 * @author    Subagent Task 6
 * @version   1.0.0
 * @date      2026-07-18
 * @note      Pure Pursuit feedforward + IR feedback corrections + lap guard
 */
#ifndef SQUARE_PATH_H
#define SQUARE_PATH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../modules/Sens-Decision/inc/trajectory_generate.h"

/**
 * @brief Square path configuration
 */
typedef struct {
    float lateral_gain;      /**< Lateral error correction gain (rad/m) */
    float heading_gain;      /**< Heading error correction gain (dimensionless) */
    float max_omega_radps;   /**< Maximum angular velocity (rad/s) */
    uint8_t target_laps;     /**< Target lap count (1-5) */
} square_path_config_t;

/**
 * @brief Lap counter state machine
 */
typedef struct {
    uint8_t completed_laps;   /**< Number of completed laps */
    bool left_start_guard;    /**< Has left the start guard zone */
    bool target_reached;      /**< Has reached target lap count */
} lap_counter_t;

/**
 * @brief Get pointer to static CCW 1m square path
 * @return Pointer to path points (A→C→D→B→A)
 */
const path_point_t *SquarePath_GetPoints(void);

/**
 * @brief Get number of points in square path
 * @return Point count (≥200 for ≤20mm spacing)
 */
size_t SquarePath_GetPointCount(void);

/**
 * @brief Apply hybrid correction to nominal omega
 * @param nominal_omega Pure Pursuit omega (rad/s)
 * @param lateral_error IR lateral error (m, positive = right of line)
 * @param heading_error IR heading error (rad, positive = pointing right)
 * @param config Correction gains and limits
 * @return Corrected omega, clamped to [-max_omega, +max_omega]
 */
float SquarePath_CorrectOmega(float nominal_omega, float lateral_error,
                              float heading_error,
                              const square_path_config_t *config);

/**
 * @brief Update lap counter state machine
 * @param counter Lap counter state (modified in place)
 * @param nearest_index Current nearest path point index
 * @param path_count Total path point count
 * @param target_laps Target lap count (1-5, values outside rejected)
 * @return true if lap incremented, false otherwise
 */
bool SquarePath_UpdateLap(lap_counter_t *counter, size_t nearest_index,
                          size_t path_count, uint8_t target_laps);

#endif // SQUARE_PATH_H
