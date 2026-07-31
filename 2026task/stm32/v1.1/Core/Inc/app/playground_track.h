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
    PLAYGROUND_TASK_2_LAP_FAST,   /**< Task 2: proven fast full lap, stop at A */
    PLAYGROUND_TASK_4_AB_6S,      /**< Task 4: A→B, target about 6 seconds */
    PLAYGROUND_TASK_5_LAP_25S,    /**< Task 5: slowed full lap, target about 25 seconds */
} playground_task_t;

/* Backward-compatible names used by earlier v1.1 code and notes. */
#define PLAYGROUND_TASK_LAP          PLAYGROUND_TASK_2_LAP_FAST
#define PLAYGROUND_TASK_AB_STRAIGHT  PLAYGROUND_TASK_4_AB_6S

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
 * @note Typical measured design time is about 1.2-1.5 seconds; the exact
 *       value is printed at boot because SPI/IMU response time can vary.
 */
bool PlaygroundTrack_Init(playground_task_t task);

/**
 * @brief Start one configured driving task without repeating hardware/IMU init.
 *
 * @note Call only after PlaygroundTrack_Init() has completed.
 */
bool PlaygroundTrack_StartTask(playground_task_t task);

/** Immediately stop the motors and return to the key-ready idle state. */
void PlaygroundTrack_Abort(void);

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

/** Return true if the active run stopped because of a safety fault. */
bool PlaygroundTrack_IsFault(void);

/** Return true while task 2, 4 or 5 is actively driving/stopping. */
bool PlaygroundTrack_IsRunning(void);

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
