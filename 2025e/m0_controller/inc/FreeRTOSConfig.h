#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Scheduler and clock configuration. CPUCLK_FREQ is currently 32 MHz. */
#define configENABLE_MPU                       0
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TIME_SLICING                   0
#define configCPU_CLOCK_HZ                       ((uint32_t) 32000000U)
#define configTICK_RATE_HZ                       ((TickType_t) 1000U)
#define configMAX_PRIORITIES                     5U
#define configMINIMAL_STACK_SIZE                 ((uint16_t) 128U)
#define configMAX_TASK_NAME_LEN                  12U
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  0

/* Keep the first integration deliberately small for the 32 KB SRAM device. */
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configTOTAL_HEAP_SIZE                    ((size_t) (4U * 1024U))
#define configAPPLICATION_ALLOCATED_HEAP         0

/* Kernel features used by the application. */
#define configUSE_TASK_NOTIFICATIONS             1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configUSE_QUEUE_SETS                     0
#define configUSE_CO_ROUTINES                    0
#define configUSE_TIMERS                         0
#define configUSE_TICKLESS_IDLE                  0

/* Hooks and diagnostics. */
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_TRACE_FACILITY                 0
#define configUSE_APPLICATION_TASK_TAG           0
#define configENABLE_BACKWARD_COMPATIBILITY      0

#define INCLUDE_vTaskPrioritySet                 0
#define INCLUDE_uxTaskPriorityGet                0
#define INCLUDE_vTaskDelete                      0
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    0
#define INCLUDE_xTaskResumeFromISR               0
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xSemaphoreGetMutexHolder         0
#define INCLUDE_xTimerPendFunctionCall           0

/* MSPM0G3507 implements four NVIC priority levels. */
#ifdef __NVIC_PRIO_BITS
#define configPRIO_BITS __NVIC_PRIO_BITS
#else
#define configPRIO_BITS 2
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY     3
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 1
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define configASSERT(condition)                 \
    do {                                        \
        if ((condition) == 0) {                 \
            taskDISABLE_INTERRUPTS();           \
            for (;;) {                          \
            }                                   \
        }                                       \
    } while (0)

/* Bind the Cortex-M0+ port handlers to the CMSIS vector names. */
#define xPortPendSVHandler  PendSV_Handler
#define vPortSVCHandler     SVC_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
