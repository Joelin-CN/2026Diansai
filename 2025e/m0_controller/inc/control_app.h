/**
 * @file control_app.h
 * @brief Application coordinator: initializes all modules and runs 500/50 Hz control loops
 * @date 2026-07-18
 * 
 * Architecture:
 *   - Fast cycle: 500 Hz (every 2 ms) - Motion Control update
 *   - Decision cycle: 50 Hz (every 10th fast cycle) - Sens-Decision pipeline
 *   - Fail-safe initialization: Any mandatory failure stops motors and returns false
 *   - Persistent start command: Keep sending BEHAVIOR_CMD_START until planner enters running
 */

#ifndef CONTROL_APP_H
#define CONTROL_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize all control subsystems
 * 
 * Initialization order:
 *   1. Motor_Init -> Motor_Stop
 *   2. Encoder_Init
 *   3. MCP23017_Init and read
 *   4. Reset Sens-Decision configuration defaults
 *   5. Start the TIMG12-backed platform timebase
 *   6. Bind the MSPM0 SPI adapter and initialize the ICM42688
 *   7. Calibrate the gyro from 100 samples over approximately one second
 *   8. Synchronize ICM42688 scales and gyro bias into SI-unit sensor config
 *   9. Configure and initialize the sensor HAL
 *  10. Initialize Sens-Decision objects/path
 *  11. Initialize and start Motion Control
 *
 * The robot must remain stationary during the approximately one-second gyro
 * calibration so the synchronized bias is valid.
 * 
 * @param target_laps Target lap count (1-5)
 * @return true if all mandatory components initialized successfully, false otherwise
 */
bool ControlApp_Init(uint8_t target_laps);

/**
 * @brief Run one fast control cycle (500 Hz)
 * 
 * On every 10th call:
 *   - Runs Sens-Decision pipeline (preprocess, state evaluation, perception,
 *     behavior planning, trajectory generation) at 50 Hz with dt=0.020f
 *   - Skips every downstream stage if any stage fails
 *   - Applies IR corrections and submits a new velocity command only after the
 *     complete pipeline succeeds
 *   - Resets the consecutive critical-failure count only after complete success
 * 
 * On every call:
 *   - Updates Motion Control at 500 Hz
 *
 * After each successful decision pipeline, handles lap completion. After each
 * failed pipeline, tracks critical failures and emergency-stops at 3 consecutive.
 */
void ControlApp_RunFastCycle(void);

/**
 * @brief Emergency stop - immediately halt all motors
 * 
 * Called on critical system failures or user request.
 */
void ControlApp_EmergencyStop(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_APP_H */
