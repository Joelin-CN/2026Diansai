#include "encoder.h"
#include "FreeRTOS.h"
#include "line_sensor.h"
#include "mcp23017.h"
#include "motor.h"
#include "task.h"
#include "ti_msp_dl_config.h"

#define BASE_SPEED          (450)
#define MAX_RUNNING_SPEED   (850)
#define SEARCH_SPEED        (260)
#define LOST_STOP_COUNT     (250U)
#define KP_NUMERATOR        (30)
#define KD_NUMERATOR        (15)
#define GAIN_DENOMINATOR    (100)
#define CONTROL_TASK_PRIORITY   (4U)
#define CONTROL_TASK_STACK_WORDS (192U)

static TaskHandle_t g_controlTask;

static int16_t ClampRunningSpeed(int32_t speed)
{
    if (speed > MAX_RUNNING_SPEED) return MAX_RUNNING_SPEED;
    if (speed < -MAX_RUNNING_SPEED) return -MAX_RUNNING_SPEED;
    return (int16_t)speed;
}

static void ControlTask(void *argument)
{
    int16_t error = 0;
    int16_t lastError = 0;
    uint16_t lostCount = LOST_STOP_COUNT;

    (void)argument;

    NVIC_SetPriority(CONTROL_TIMER_INST_INT_IRQN,
                     configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_ClearPendingIRQ(CONTROL_TIMER_INST_INT_IRQN);
    NVIC_EnableIRQ(CONTROL_TIMER_INST_INT_IRQN);
    DL_TimerG_startCounter(CONTROL_TIMER_INST);

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint16_t sensorMask = LineSensor_ReadMask();

        if (LineSensor_GetError(sensorMask, &error)) {
            const int32_t derivative = (int32_t)error - lastError;
            const int32_t correction =
                ((int32_t)KP_NUMERATOR * error + (int32_t)KD_NUMERATOR * derivative) /
                GAIN_DENOMINATOR;
            const int16_t leftSpeed = ClampRunningSpeed(BASE_SPEED + correction);
            const int16_t rightSpeed = ClampRunningSpeed(BASE_SPEED - correction);

            Motor_SetSpeed(leftSpeed, rightSpeed);
            lastError = error;
            lostCount = 0U;
        } else if (lostCount < LOST_STOP_COUNT) {
            /* Briefly turn toward the last known line position. */
            if (lastError < 0) {
                Motor_SetSpeed(-SEARCH_SPEED, SEARCH_SPEED);
            } else {
                Motor_SetSpeed(SEARCH_SPEED, -SEARCH_SPEED);
            }
            lostCount++;
        } else {
            Motor_Stop();
        }
    }
}

int main(void)
{
    SYSCFG_DL_init();
    Encoder_Init();
    (void)MCP23017_Init();
    Motor_Init();

    if (xTaskCreate(ControlTask, "control", CONTROL_TASK_STACK_WORDS, NULL,
                    CONTROL_TASK_PRIORITY, &g_controlTask) != pdPASS) {
        Motor_Stop();
        for (;;) {
        }
    }

    vTaskStartScheduler();

    /* Reached only if the scheduler cannot allocate its internal task. */
    Motor_Stop();
    for (;;) {
    }
}

void TIMG0_IRQHandler(void)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (DL_TimerG_getPendingInterrupt(CONTROL_TIMER_INST) ==
        DL_TIMER_IIDX_ZERO) {
        vTaskNotifyGiveFromISR(g_controlTask, &higherPriorityTaskWoken);
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}

void vApplicationMallocFailedHook(void)
{
    Motor_Stop();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName)
{
    (void)task;
    (void)taskName;
    Motor_Stop();
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
