/**
 * @file test_motor_m2_debug.c
 * @brief Debug physical M2 (software M3) that doesn't work
 */

#include "ti_msp_dl_config.h"
#include "motor.h"

static void DelayMs(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++) {
        }
    }
}

static void uart_print(const char *text)
{
    while (*text != '\0') {
        while (DL_UART_isTXFIFOFull(UART0_INST));
        DL_UART_Main_transmitData(UART0_INST, (uint8_t)*text++);
    }
}

void test_motor_m2_debug_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        Motor_Init();
        uart_print("\r\n=== M2 (Physical) DEBUG TEST ===\r\n");
        uart_print("Testing software M3 which controls physical M2\r\n\r\n");

        /* Test 1: 30% speed */
        uart_print("Test 1: M3 at 30%% (3s)\r\n");
        Motor_SetFour(0, 0, 300, 0);
        DelayMs(3000);
        Motor_Stop();
        DelayMs(1000);

        /* Test 2: 50% speed */
        uart_print("Test 2: M3 at 50%% (3s)\r\n");
        Motor_SetFour(0, 0, 500, 0);
        DelayMs(3000);
        Motor_Stop();
        DelayMs(1000);

        /* Test 3: 100% speed */
        uart_print("Test 3: M3 at 100%% (3s)\r\n");
        Motor_SetFour(0, 0, 1000, 0);
        DelayMs(3000);
        Motor_Stop();
        DelayMs(1000);

        /* Test 4: Negative direction 30% */
        uart_print("Test 4: M3 at -30%% (3s)\r\n");
        Motor_SetFour(0, 0, -300, 0);
        DelayMs(3000);
        Motor_Stop();

        uart_print("\r\n=== TEST COMPLETE ===\r\n");
        uart_print("Did physical M2 (left rear wheel) move in any test?\r\n");
        uart_print("If NO, check hardware:\r\n");
        uart_print("  1. M2 motor connection\r\n");
        uart_print("  2. TB6612 wiring for M2\r\n");
        uart_print("  3. PWM signal on M3 channel\r\n\r\n");

        initialized = true;
    }

    __WFI();
}
