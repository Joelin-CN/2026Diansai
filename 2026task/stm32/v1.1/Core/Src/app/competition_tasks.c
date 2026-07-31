#include "competition_tasks.h"

#include "competition_display.h"
#include "competition_timer.h"
#include "main.h"
#include "oled.h"
#include "playground_track.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>

static uint8_t g_active_task;
static uint32_t g_init_time_ms;
static bool g_oled_ready;

static void CompetitionTasks_UpdateDisplay(void)
{
    CompetitionTimerSnapshot snapshot;

    if (!g_oled_ready) {
        return;
    }

    CompetitionTimer_GetSnapshot(&snapshot);
    CompetitionDisplay_Render(&snapshot);
    (void)OLED_Update();
}

bool CompetitionTasks_Init(void)
{
    const uint32_t init_start = HAL_GetTick();

    g_active_task = 0U;
    g_init_time_ms = 0U;

    CompetitionTimer_Init();
    g_oled_ready = OLED_Init();
    CompetitionTasks_UpdateDisplay();

    printf("\r\n");
    printf("============================================================\r\n");
    printf(" H-PROBLEM TASK SELECTOR: K1=LEFT K2=RIGHT K3=START\r\n");
    printf(" Selects Task 2..6; KEY4/KEY5 are disabled\r\n");
    printf(" Task 2 = proven precision-stop lap\r\n");
    printf(" Task 4 = A-B trapezoid, target about 6 s\r\n");
    printf(" Task 5 = slowed stable lap, target about 25 s\r\n");
    printf(" Task 3/6 = reserved until ball-control loop is integrated\r\n");
    printf("============================================================\r\n");

    if (!PlaygroundTrack_Init(PLAYGROUND_TASK_2_LAP_FAST)) {
        printf("[Competition] Boot calibration failed; motors remain stopped\r\n");
        return false;
    }

    g_init_time_ms = HAL_GetTick() - init_start;
    CompetitionTimer_MarkReady();
    printf("[Competition] READY after %lu ms. A key now starts immediately.\r\n",
           (unsigned long)g_init_time_ms);
    printf("[KEY] idle raw PC0=%u PC1=%u PC2=%u (all must be 1 when released)\r\n",
           HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_SET ? 1U : 0U,
           HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_SET ? 1U : 0U,
           HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_SET ? 1U : 0U);
    return true;
}

void CompetitionTasks_RunFastCycle(void)
{
    CompetitionTimerSnapshot snapshot;

    CompetitionTimer_Process();
    PlaygroundTrack_RunFastCycle();
    CompetitionTimer_GetSnapshot(&snapshot);

    if ((snapshot.state == COMP_TIMER_RUNNING) &&
        (snapshot.task_number == g_active_task)) {
        if (PlaygroundTrack_IsComplete()) {
            (void)CompetitionTimer_Finish(g_active_task);
            printf("[Competition] Task %u complete in %lu ms, distance=%.3f m\r\n",
                   (unsigned)g_active_task,
                   (unsigned long)snapshot.elapsed_ms,
                   PlaygroundTrack_GetDistance());
        } else if (PlaygroundTrack_IsFault()) {
            (void)CompetitionTimer_Fail(g_active_task);
            printf("[Competition] Task %u FAULT at %lu ms, distance=%.3f m\r\n",
                   (unsigned)g_active_task,
                   (unsigned long)snapshot.elapsed_ms,
                   PlaygroundTrack_GetDistance());
        }
    }

}

void CompetitionTasks_DisplayCycle(void)
{
    CompetitionTasks_UpdateDisplay();
}

uint32_t CompetitionTasks_GetInitTimeMs(void)
{
    return g_init_time_ms;
}

bool CompetitionTimer_OnTaskStartRequested(uint8_t task_number)
{
    playground_task_t drive_task;

    switch (task_number) {
        case 2U:
            drive_task = PLAYGROUND_TASK_2_LAP_FAST;
            break;
        case 4U:
            drive_task = PLAYGROUND_TASK_4_AB_6S;
            break;
        case 5U:
            drive_task = PLAYGROUND_TASK_5_LAP_25S;
            break;
        case 3U:
        case 6U:
        default:
            printf("[Competition] Task %u is reserved: ball-position control "
                   "has not been integrated, motors stay stopped\r\n",
                   (unsigned)task_number);
            PlaygroundTrack_Abort();
            g_active_task = 0U;
            return false;
    }

    if (!PlaygroundTrack_StartTask(drive_task)) {
        printf("[Competition] Task %u start rejected: controller busy/not ready\r\n",
               (unsigned)task_number);
        return false;
    }

    g_active_task = task_number;
    return true;
}

void CompetitionTimer_OnTaskFinished(uint8_t task_number,
                                     uint32_t elapsed_ms)
{
    /*
     * Autonomous completion already stopped the controller. A same-key press
     * while running reaches this hook first and acts as an emergency abort.
     */
    if (!PlaygroundTrack_IsComplete()) {
        PlaygroundTrack_Abort();
        printf("[Competition] Task %u manually stopped at %lu ms\r\n",
               (unsigned)task_number, (unsigned long)elapsed_ms);
    }
}
