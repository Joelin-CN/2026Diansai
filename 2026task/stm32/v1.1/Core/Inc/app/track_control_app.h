/**
 * @file      track_control_app.h
 * @brief     Track path control application header
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Control application for 400m track-shaped path
 */
#ifndef TRACK_CONTROL_APP_H
#define TRACK_CONTROL_APP_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize track control application
 * @param target_laps Number of laps to complete (1-10)
 * @return true if initialization successful, false otherwise
 */
bool TrackControlApp_Init(uint8_t target_laps);

/**
 * @brief Run one control cycle (call at 500 Hz)
 * @note Executes motion control every cycle, sens-decision every 10th cycle
 */
void TrackControlApp_RunFastCycle(void);

/**
 * @brief Check if target laps completed
 * @return true if target reached, false otherwise
 */
bool TrackControlApp_IsComplete(void);

/**
 * @brief Get current lap count
 * @return Number of completed laps
 */
uint8_t TrackControlApp_GetCompletedLaps(void);

#endif // TRACK_CONTROL_APP_H
