/**
 * @file test_motor_simple.c
 * @brief Ultra-simple motor test using motor.c interface
 * @date 2026-07-24
 *
 * This test uses the existing motor.c interface to verify:
 * - Motor_Init() works
 * - Motor_SetFour() works
 * - Motors can be driven
 */

#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "motor.h"

/**
 * @brief Simple delay using busy-wait loop
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
 * @brief Direct UART output without printf
 */
static void uart_print(const char *text)
{
    while (*text != '\0') {
        while (DL_UART_isTXFIFOFull(UART0_INST));
        DL_UART_Main_transmitData(UART0_INST, (uint8_t)*text++);
    }
}

/**
 * Very simple motor test
 */
void test_motor_simple_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        /* Step 1: Print startup message */
        uart_print("\r\n=== MOTOR SIMPLE TEST ===\r\n");
        uart_print("Step 1: Startup OK\r\n");

        /* Step 2: Initialize motor driver */
        uart_print("Step 2: Calling Motor_Init()...\r\n");
        Motor_Init();
        uart_print("Step 2: Motor_Init() returned\r\n");

        /* Step 3: Stop motors */
        uart_print("Step 3: Calling Motor_Stop()...\r\n");
        Motor_Stop();
        uart_print("Step 3: Motors stopped\r\n");

        /* Step 4: Wait 1 second */
        uart_print("Step 4: Waiting 1 second...\r\n");
        DelayMs(1000);
        uart_print("Step 4: Wait complete\r\n");

        /* Step 5: Start motors at 30% */
        uart_print("Step 5: Starting motors at 30%% (speed=300)...\r\n");
        Motor_SetFour(300, 300, 300, 300);
        uart_print("Step 5: Motors started!\r\n");

        uart_print("\r\n*** Motors should be running now ***\r\n");
        uart_print("Press RESET to stop.\r\n\r\n");

        initialized = true;
    }

    /* Idle loop - motors keep running */
    __WFI();
}
