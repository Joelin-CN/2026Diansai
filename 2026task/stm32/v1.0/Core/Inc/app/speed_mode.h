/**
 * @file      speed_mode.h
 * @brief     Speed mode configuration for track control application
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Provides convenient speed mode switching for different debugging stages
 */

#ifndef SPEED_MODE_H
#define SPEED_MODE_H

#include <stdint.h>

/**
 * @brief Speed mode enumeration
 *
 * Defines predefined speed profiles for different stages:
 * - DEBUG: Ultra-low speed for first-time debugging (sensor validation)
 * - SLOW: Low speed for regular tuning (PID parameter adjustment)
 * - NORMAL: Standard speed for validated control (after successful tuning)
 * - FAST: High speed for competition mode (optimized performance)
 */
typedef enum {
    SPEED_MODE_DEBUG,      /**< 0.2/0.15 m/s - Ultra-low speed for sensor verification */
    SPEED_MODE_SLOW,       /**< 0.5/0.3 m/s - Low speed for control tuning */
    SPEED_MODE_NORMAL,     /**< 1.0/0.5 m/s - Normal speed for validated system */
    SPEED_MODE_FAST,       /**< 1.5/0.8 m/s - Competition speed */
} speed_mode_t;

/**
 * @brief Set speed mode
 *
 * Updates the global speed configuration in Sens-Decision layer.
 * This affects the behavior planner's speed limits.
 *
 * @param mode Speed mode to set
 *
 * @note Changes take effect immediately in the next control cycle
 * @note Prints the selected speed configuration to UART
 */
void speed_mode_set(speed_mode_t mode);

/**
 * @brief Get current speed mode
 *
 * @return Current speed mode
 */
speed_mode_t speed_mode_get(void);

/**
 * @brief Get speed mode name string
 *
 * @param mode Speed mode
 * @return Human-readable mode name
 */
const char* speed_mode_name(speed_mode_t mode);

#endif /* SPEED_MODE_H */
