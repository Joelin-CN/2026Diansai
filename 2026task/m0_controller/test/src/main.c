/**
 * @file main.c
 * @brief RTOS shell: safe startup, task creation, ISR notification, fatal hooks
 * @date 2026-07-18
 */

/* ============================================================================
 * TEST MODE SELECTION
 * ============================================================================ */
#define TEST_MODE_CONTROL_APP    0  // Full control application with FreeRTOS
#define TEST_MODE_ICM42688       1  // ICM42688 gyroscope hardware test (bare metal)
#define TEST_MODE_MOTOR_ENCODER  2  // Motor & Encoder hardware test (bare metal)
#define TEST_MODE_ENCODER_SIMPLE 3  // Simple encoder test (no interrupts)
#define TEST_MODE_MOTOR_BASIC    4  // Basic motor test (just drive motors)
#define TEST_MODE_MOTOR_PWM30    5  // Fixed 30% PWM test (validated reference)
#define TEST_MODE_MOTOR_SIMPLE   6  // Ultra-simple motor test (motor.c interface)
#define TEST_MODE_MOTOR_DEBUG    7  // Motor debug test (show PWM and GPIO values)
#define TEST_MODE_MOTOR_IDENTIFY 8  // Motor identification test (one by one)
#define TEST_MODE_MOTOR_M2_DEBUG 9  // M2 hardware debug test
#define TEST_MODE_MOTOR_FINAL_VERIFY 10  // Final direction verification
#define TEST_MODE_ENCODER_AUTO   11  // Automatic encoder test (motors drive themselves)
#define TEST_MODE_ENCODER_MINIMAL 12  // Minimal encoder test (simplified debugging)
#define TEST_MODE_ENCODER_SUPER_MINIMAL 13  // Super minimal encoder test (no time functions)
#define TEST_MODE_ENCODER_NO_READ 14  // Test without encoder reading
#define TEST_MODE_ENCODER_NO_INTERRUPT 15  // Test encoder without interrupts
#define TEST_MODE_IR_TRACKER 16  // 8-way IR-tracker UART analog test (bare metal)

#define ACTIVE_TEST_MODE  TEST_MODE_ENCODER_AUTO  // <-- Change this to select test

/* ============================================================================ */

#include "control_app.h"
#include "motor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ti_msp_dl_config.h"
#include "uart_debug.h"
#include "platform_time.h"
#include "../modules/IR-tracker/inc/ir_uart_sensor.h"
#include <stdio.h>

#define CONTROL_TASK_PRIORITY   (4U)
#define CONTROL_TASK_STACK_WORDS (512U)
#define TARGET_LAPS (3U)

/* Test function declarations */
#if (ACTIVE_TEST_MODE == TEST_MODE_ICM42688)
extern void test_icm42688_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_ENCODER)
extern void test_motor_encoder_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_SIMPLE)
extern void test_encoder_simple_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_BASIC)
extern void test_motor_basic_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_PWM30)
extern void test_motor_pwm30_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_SIMPLE)
extern void test_motor_simple_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_DEBUG)
extern void test_motor_debug_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_IDENTIFY)
extern void test_motor_identify_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_M2_DEBUG)
extern void test_motor_m2_debug_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_FINAL_VERIFY)
extern void test_motor_final_verify_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_AUTO)
extern void test_encoder_auto_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_MINIMAL)
extern void test_encoder_minimal_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_SUPER_MINIMAL)
extern void test_encoder_super_minimal_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_NO_READ)
extern void test_encoder_no_read_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_NO_INTERRUPT)
extern void test_encoder_no_interrupt_main_loop(void);
#elif (ACTIVE_TEST_MODE == TEST_MODE_IR_TRACKER)
extern void test_ir_tracker_main_loop(void);
#endif

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
 * @brief Simple delay function (blocking)
 */
static void DelayMs(uint32_t ms)
{
    /* Approximate delay: 32 MHz CPU, ~8000 cycles per ms */
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++) {
            /* Empty loop */
        }
    }
}

/**
 * @brief Main entry point
 */
int main(void)
{
    /* Initialize hardware */
    SYSCFG_DL_init();

#if (ACTIVE_TEST_MODE == TEST_MODE_CONTROL_APP)
    /* ========================================================================
     * MODE 0: Full Control Application with FreeRTOS
     * ======================================================================== */

    /* Print startup banner */
    printf("\n");
    printf("=====================================\n");
    printf("MSPM0G3507 Controller Firmware\n");
    printf("=====================================\n");
    printf("UART0 Debug: Enabled (115200 baud)\n");
    printf("Target Laps: %u\n", (unsigned int)TARGET_LAPS);
    printf("Initializing...\n\n");

    /* Initialize control application */
    if (!ControlApp_Init(TARGET_LAPS)) {
        /* Initialization failed - motors already stopped by ControlApp_Init */
        printf("[ERROR] ControlApp_Init failed!\n");
        Motor_Stop();
        for (;;) {
            /* Halt on init failure */
        }
    }

    printf("[INFO] ControlApp initialized successfully\n");

    /* Create control task */
    if (xTaskCreate(ControlTask, "control", CONTROL_TASK_STACK_WORDS, NULL,
                    CONTROL_TASK_PRIORITY, &g_controlTask) != pdPASS) {
        printf("[ERROR] Failed to create control task!\n");
        ControlApp_EmergencyStop();
        for (;;) {
            /* Halt on task creation failure */
        }
    }

    printf("[INFO] Control task created\n");
    printf("[INFO] Starting FreeRTOS scheduler...\n\n");

    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Reached only if the scheduler cannot allocate its internal task */
    printf("[FATAL] Scheduler failed to start!\n");
    ControlApp_EmergencyStop();
    for (;;) {
        /* Halt on scheduler failure */
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ICM42688)
    /* ========================================================================
     * MODE 1: ICM42688 Gyroscope Hardware Test (Bare Metal)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'S');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for calibration delays */
    PlatformTime_Init();

    /* Print startup message */
    printf("ICM42688 Test Starting...\n");

    /* Run ICM42688 test loop */
    for (;;) {
        test_icm42688_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_ENCODER)
    /* ========================================================================
     * MODE 2: Motor & Encoder Hardware Test (Bare Metal)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'M');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for test timing */
    PlatformTime_Init();

    /* Print startup message */
    printf("Motor & Encoder Test Starting...\n");

    /* Run motor encoder test loop */
    for (;;) {
        test_motor_encoder_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_SIMPLE)
    /* ========================================================================
     * MODE 3: Simple Encoder Test (No Interrupts)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'E');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for test timing */
    PlatformTime_Init();

    /* Print startup message */
    printf("Simple Encoder Test Starting...\n");

    /* Run simple encoder test loop */
    for (;;) {
        test_encoder_simple_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_BASIC)
    /* ========================================================================
     * MODE 4: Basic Motor Test
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'M');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Print startup message */
    printf("Basic Motor Test Starting...\n");

    /* Run basic motor test loop */
    for (;;) {
        test_motor_basic_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_PWM30)
    /* ========================================================================
     * MODE 5: Fixed 30% PWM Motor Test (Validated Reference)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'P');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run PWM30 motor test loop */
    for (;;) {
        test_motor_pwm30_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_SIMPLE)
    /* ========================================================================
     * MODE 6: Ultra-Simple Motor Test (motor.c interface)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'S');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run simple motor test loop */
    for (;;) {
        test_motor_simple_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_DEBUG)
    /* ========================================================================
     * MODE 7: Motor Debug Test (show PWM and GPIO values)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'D');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run motor debug test loop */
    for (;;) {
        test_motor_debug_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_IDENTIFY)

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run motor identification test loop */
    for (;;) {
        test_motor_identify_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_M2_DEBUG)

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run M2 debug test loop */
    for (;;) {
        test_motor_m2_debug_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_MOTOR_FINAL_VERIFY)

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Run final verification test loop */
    for (;;) {
        test_motor_final_verify_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_AUTO)
    /* ========================================================================
     * MODE 11: Automatic Encoder Test (motors drive themselves)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'A');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for test timing */
    PlatformTime_Init();

    /* Print startup message */
    printf("Automatic Encoder Test Starting...\n");

    /* Run automatic encoder test loop */
    for (;;) {
        test_encoder_auto_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_MINIMAL)
    /* ========================================================================
     * MODE 12: Minimal Encoder Test (simplified debugging)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'M');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for test timing */
    PlatformTime_Init();

    /* Print startup message */
    printf("Minimal Encoder Test Starting...\n");

    /* Run minimal encoder test loop */
    for (;;) {
        test_encoder_minimal_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_SUPER_MINIMAL)
    /* ========================================================================
     * MODE 13: Super Minimal Encoder Test (no time functions)
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'S');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Print startup message */
    printf("Super Minimal Encoder Test Starting...\n");

    /* Run super minimal encoder test loop - NO PlatformTime functions */
    for (;;) {
        test_encoder_super_minimal_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_NO_READ)
    /* ========================================================================
     * MODE 14: Test Without Encoder Reading
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'N');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Print startup message */
    printf("Test Without Encoder Reading Starting...\n");

    /* Run test loop - NO encoder functions at all */
    for (;;) {
        test_encoder_no_read_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_ENCODER_NO_INTERRUPT)
    /* ========================================================================
     * MODE 15: Test Encoder Without Interrupts
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'I');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Print startup message */
    printf("Encoder Test Without Interrupts Starting...\n");

    /* Run test loop - encoder but NO interrupts */
    for (;;) {
        test_encoder_no_interrupt_main_loop();
    }

#elif (ACTIVE_TEST_MODE == TEST_MODE_IR_TRACKER)
    /* ========================================================================
     * MODE 16: 8-way IR-Tracker UART Analog Test
     * ======================================================================== */

    /* Send initial test character to verify UART is working */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'R');
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, '\n');

    /* Small delay for UART to stabilize */
    DelayMs(100);

    /* Initialize platform timer for test timing */
    PlatformTime_Init();

    /* Initialize IR-tracker and request analog stream (~20s warm-up) */
    printf("IR-Tracker Analog Test Starting (warm-up ~20s)...\n");
    IrUartSensor_Init();
    IrUartSensor_RequestAnalogMode();

    /* Run IR-tracker test loop (10 Hz) */
    for (;;) {
        test_ir_tracker_main_loop();
        DelayMs(100);
    }

#else
    #error "Invalid ACTIVE_TEST_MODE selected"
#endif

    return 0;
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
