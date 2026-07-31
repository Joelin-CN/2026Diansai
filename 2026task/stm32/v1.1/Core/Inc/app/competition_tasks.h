#ifndef COMPETITION_TASKS_H
#define COMPETITION_TASKS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize OLED, timer, motors, encoders, IR and IMU calibration.
 * Motors remain stopped after this returns.
 */
bool CompetitionTasks_Init(void);

/** Run the unified 500 Hz controller, key dispatcher and OLED timer. */
void CompetitionTasks_RunFastCycle(void);

/**
 * Refresh the OLED from a separate low-priority FreeRTOS task.
 * Call around 10 Hz; never call this from the 500 Hz motor-control loop.
 */
void CompetitionTasks_DisplayCycle(void);

/** Measured boot calibration time from the most recent initialization. */
uint32_t CompetitionTasks_GetInitTimeMs(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_TASKS_H */
