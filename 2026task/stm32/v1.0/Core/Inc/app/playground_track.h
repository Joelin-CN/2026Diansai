/**
 * @file      playground_track.h
 * @brief     Playground track (操场型循迹) - Public API
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Segment-aware adaptive line-following for competition tasks 2 & 4
 */

#ifndef PLAYGROUND_TRACK_H
#define PLAYGROUND_TRACK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Public Types
 * ============================================================================ */

/**
 * @brief Task selection for playground track competition
 */
typedef enum {
    PLAYGROUND_TASK_LAP,          /**< Task 2: Full lap A→A, ≤20s, stop ≤2cm */
    PLAYGROUND_TASK_AB_STRAIGHT,  /**< Task 4: A→B straight, ≤8s, pendulum-safe */
} playground_task_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Initialize playground track control system
 *
 * Performs hardware initialization sequence:
 * 1. Motor + Encoder
 * 2. IR sensor (8-channel UART)
 * 3. IMU (ICM42688, graceful skip on failure)
 * 4. Sens-Decision perception module
 * 5. MotionControl (differential drive PID)
 *
 * @param task Task selection (PLAYGROUND_TASK_LAP or PLAYGROUND_TASK_AB_STRAIGHT)
 * @return true if initialization succeeded, false otherwise
 *
 * @note On failure, motors are stopped and caller must halt execution
 * @note This function takes ~3 seconds (2s IR warm-up + 1s IMU calibration)
 */
bool PlaygroundTrack_Init(playground_task_t task);

/**
 * @brief Main control cycle - call at 500 Hz
 *
 * Implements layered frequency architecture:
 * - 500 Hz: Encoder sampling
 * - 100 Hz: PID control (every 5 cycles)
 * -  50 Hz: Perception + state machine (every 10 cycles)
 *
 * @note Must be called every 2 ms from FreeRTOS task (osDelay(2))
 * @note Non-blocking execution, typically completes in <200 µs
 */
void PlaygroundTrack_RunFastCycle(void);

/**
 * @brief Check if task is complete
 *
 * @return true if robot has stopped at target position, false otherwise
 *
 * @note Task 2: returns true after stopping at A-line
 * @note Task 4: returns true after stopping at B position
 */
bool PlaygroundTrack_IsComplete(void);

/**
 * @brief Get cumulative distance traveled (debug utility)
 *
 * @return Distance in meters from start position
 *
 * @note Distance is integrated from encoder wheel speeds at 50 Hz
 * @note Accurate for single lap, no EKF drift accumulation
 */
float PlaygroundTrack_GetDistance(void);

#ifdef __cplusplus
}
#endif

#endif /* PLAYGROUND_TRACK_H */
