/**
 * @file      track_path.h
 * @brief     400m track-shaped path (2 semicircles + 2 straights)
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Track geometry: R=0.5m semicircles, L=1.5m straights
 *            Total perimeter ≈ 6.14m (2πR + 2L)
 */
#ifndef TRACK_PATH_H
#define TRACK_PATH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "trajectory_generate.h"

/**
 * @brief Track path configuration
 */
typedef struct {
    float lateral_gain;      /**< Lateral error correction gain (rad/m) */
    float heading_gain;      /**< Heading error correction gain (dimensionless) */
    float max_omega_radps;   /**< Maximum angular velocity (rad/s) */
    uint8_t target_laps;     /**< Target lap count (1-10) */
    float line_speed_mps;    /**< Speed on straight sections (m/s) */
    float curve_speed_mps;   /**< Speed on curved sections (m/s) */
} track_path_config_t;

/**
 * @brief Lap counter state machine
 */
typedef struct {
    uint8_t completed_laps;   /**< Number of completed laps */
    bool left_start_guard;    /**< Has left the start guard zone */
    bool target_reached;      /**< Has reached target lap count */
} track_lap_counter_t;

/**
 * @brief Get pointer to static track path
 * @return Pointer to path points
 */
const path_point_t *TrackPath_GetPoints(void);

/**
 * @brief Get number of points in track path
 * @return Point count (≥300 for ≤20mm spacing)
 */
size_t TrackPath_GetPointCount(void);

/**
 * @brief Apply hybrid correction to nominal omega
 * @param nominal_omega Pure Pursuit omega (rad/s)
 * @param lateral_error IR lateral error (m, positive = left of line)
 * @param heading_error IR heading error (rad, positive = pointing right)
 * @param config Correction gains and limits
 * @return Corrected omega, clamped to [-max_omega, +max_omega]
 */
float TrackPath_CorrectOmega(float nominal_omega, float lateral_error,
                             float heading_error,
                             const track_path_config_t *config);

/**
 * @brief Update lap counter state machine
 * @param counter Lap counter state (modified in place)
 * @param nearest_index Current nearest path point index
 * @param path_count Total path point count
 * @param target_laps Target lap count (1-10)
 * @return true if lap incremented, false otherwise
 */
bool TrackPath_UpdateLap(track_lap_counter_t *counter, size_t nearest_index,
                         size_t path_count, uint8_t target_laps);

#endif // TRACK_PATH_H
