/**
 * @file main.c
 * @brief RTOS shell: safe startup, task creation, ISR notification, fatal hooks
 * @date 2026-07-18
 */

#include "control_app.h"
#include "motor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"

#define CONTROL_TASK_PRIORITY   (4U)
#define CONTROL_TASK_STACK_WORDS (512U)
#define TARGET_LAPS (3U)

static TaskHandle_t g_controlTask = NULL;

/**
 * @brief Control task: runs ControlApp_RunFastCycle at 500 Hz
 */
static void ControlTask(void *argument)
{
    (void)argument;

    /* Configure timer interrupt priority for FreeRTOS compatibility */
    NVIC_SetPriority(CONTROL_TIMER_INST_INT_IRQN,
                     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(CONTROL_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(CONTROL_TIMER_INST_INT_IRQN);
    
    /* Start the 500 Hz timer (TIMG0) */
    DL_TimerG_startCounter(CONTROL_TIMER_INST);

    /* Main control loop */
    for (;;) {
        /* Block waiting for timer notification (every 2 ms) */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        /* Run one fast cycle */
        ControlApp_RunFastCycle();
        
        /* Periodically record stack high water mark (non-blocking) */
        /* Note: Actual logging would be done outside the 2ms path */
        #if 0
        static uint32_t cycle_count = 0;
        if ((cycle_count++ % 1000) == 0) {
            UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
            /* Store or transmit highWaterMark asynchronously */
            (void)highWaterMark;
        }
        #endif
    }
}

/**
 * @brief Main entry point
 */
int main(void)
{
    /* Initialize hardware */
    SYSCFG_DL_init();
    
    /* Initialize control application */
    if (!ControlApp_Init(TARGET_LAPS)) {
        /* Initialization failed - motors already stopped by ControlApp_Init */
        Motor_Stop();
        for (;;) {
            /* Halt on init failure */
        }
    }
    
    /* Create control task */
    if (xTaskCreate(ControlTask, "control", CONTROL_TASK_STACK_WORDS, NULL,
                    CONTROL_TASK_PRIORITY, &g_controlTask) != pdPASS) {
        ControlApp_EmergencyStop();
        for (;;) {
            /* Halt on task creation failure */
        }
    }
    
    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();
    
    /* Reached only if the scheduler cannot allocate its internal task */
    ControlApp_EmergencyStop();
    for (;;) {
        /* Halt on scheduler failure */
    }
}

/**
 * @brief Timer interrupt handler: notifies control task at 500 Hz
 */
void TIMG0_IRQHandler(void)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (DL_TimerG_getPendingInterrupt(CONTROL_TIMER_INST) ==
        DL_TIMER_IIDX_ZERO) {
        vTaskNotifyGiveFromISR(g_controlTask, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}

/**
 * @brief FreeRTOS malloc failed hook
 */
void vApplicationMallocFailedHook(void)
{
    ControlApp_EmergencyStop();
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* Halt on malloc failure */
    }
}

/**
 * @brief FreeRTOS stack overflow hook
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName)
{
    (void)task;
    (void)taskName;
    ControlApp_EmergencyStop();
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* Halt on stack overflow */
    }
}
