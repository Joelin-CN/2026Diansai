/**
 * @file test_motor_final_verify.c
 * @brief Final verification test after direction correction
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

void test_motor_final_verify_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        Motor_Init();
        Motor_Stop();

        uart_print("\r\n=== FINAL DIRECTION VERIFICATION ===\r\n");
        uart_print("All motors should now rotate FORWARD\r\n");
        uart_print("(direction for vehicle moving forward)\r\n\r\n");

        /* Test 1: All motors together */
        uart_print("Test 1: ALL MOTORS at 30%% (5s)\r\n");
        uart_print("All four wheels should rotate forward together.\r\n");
        Motor_SetFour(300, 300, 300, 300);
        DelayMs(5000);
        Motor_Stop();
        DelayMs(2000);

        /* Test 2: Individual motors */
        uart_print("\r\nTest 2: INDIVIDUAL MOTORS (each 2s)\r\n");

        uart_print("  M1 (controls physical M2 - left rear)\r\n");
        Motor_SetFour(300, 0, 0, 0);
        DelayMs(2000);
        Motor_Stop();
        DelayMs(500);

        uart_print("  M2 (controls physical M1 - left front)\r\n");
        Motor_SetFour(0, 300, 0, 0);
        DelayMs(2000);
        Motor_Stop();
        DelayMs(500);

        uart_print("  M3 (controls physical M3 - right rear)\r\n");
        Motor_SetFour(0, 0, 300, 0);
        DelayMs(2000);
        Motor_Stop();
        DelayMs(500);

        uart_print("  M4 (controls physical M4 - right front)\r\n");
        Motor_SetFour(0, 0, 0, 300);
        DelayMs(2000);
        Motor_Stop();
        DelayMs(1000);

        /* Test 3: Forward motion */
        uart_print("\r\nTest 3: FORWARD MOTION (3s)\r\n");
        uart_print("Vehicle should move forward.\r\n");
        Motor_SetSpeed(300, 300);  // left speed, right speed
        DelayMs(3000);
        Motor_Stop();

        uart_print("\r\n=== VERIFICATION COMPLETE ===\r\n");
        uart_print("Did all wheels rotate in the correct direction?\r\n");
        uart_print("If YES, direction calibration is complete!\r\n");
        uart_print("If NO, report which motors are still wrong.\r\n\r\n");

        initialized = true;
    }

    __WFI();
}
