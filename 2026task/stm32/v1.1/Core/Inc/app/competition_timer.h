#ifndef COMPETITION_TIMER_H
#define COMPETITION_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  COMP_TIMER_INITIALIZING = 0,
  COMP_TIMER_READY,
  COMP_TIMER_RUNNING,
  COMP_TIMER_FINISHED,
  COMP_TIMER_UNAVAILABLE,
  COMP_TIMER_FAULT
} CompetitionTimerState;

typedef struct
{
  uint8_t task_number;
  CompetitionTimerState state;
  uint32_t elapsed_ms;
  uint32_t limit_ms;
  bool within_limit;
} CompetitionTimerSnapshot;

void CompetitionTimer_Init(void);

/** Enable key starts after all boot-time sensor calibration has completed. */
void CompetitionTimer_MarkReady(void);

/**
 * Poll and process debounced menu events at 500 Hz:
 * KEY1 previous task, KEY2 next task, KEY3 confirm/start.
 * KEY4 and KEY5 are intentionally unused.
 */
void CompetitionTimer_Process(void);

/**
 * Start task 2..6 and reset elapsed time.
 * This API can also be called by an autonomous task instead of a button.
 */
bool CompetitionTimer_Start(uint8_t task_number);

/**
 * Freeze the timer if the supplied task is the currently running task.
 * Call when the car reaches A/B or task 3 reaches and stabilizes at -5 cm.
 */
bool CompetitionTimer_Finish(uint8_t task_number);

/** Freeze the current time and show a control/sensor fault. */
bool CompetitionTimer_Fail(uint8_t task_number);

/** Return a consistent snapshot for display/control code. */
void CompetitionTimer_GetSnapshot(CompetitionTimerSnapshot *snapshot);

/** Compatibility hook only; the current build does not use key EXTI. */
void CompetitionTimer_OnButtonInterrupt(uint16_t gpio_pin);

/**
 * Optional control hooks. Override these weak functions in the motion-control
 * application to start or stop the corresponding autonomous task.
 */
/**
 * Start-request hook. Return false for tasks whose actuator control is not
 * implemented; the OLED will show NOT IMPLEMENTED and motors remain stopped.
 */
bool CompetitionTimer_OnTaskStartRequested(uint8_t task_number);
void CompetitionTimer_OnTaskFinished(uint8_t task_number,
                                     uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_TIMER_H */
