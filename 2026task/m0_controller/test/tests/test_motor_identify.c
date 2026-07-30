/**
 * @file test_motor_identify.c
 * @brief Identify each motor one by one (debug version with detailed output)
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

void test_motor_identify_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        uart_print("\r\n[DEBUG] Starting test...\r\n");

        uart_print("[DEBUG] Calling Motor_Init()...\r\n");
        Motor_Init();
        uart_print("[DEBUG] Motor_Init() returned\r\n");

        uart_print("[DEBUG] Calling Motor_Stop()...\r\n");
        Motor_Stop();
        uart_print("[DEBUG] Motor_Stop() returned\r\n");

        uart_print("\r\n=== MOTOR IDENTIFICATION TEST ===\r\n");
        uart_print("Each motor will run for 3 seconds.\r\n");
        uart_print("Note which physical wheel moves.\r\n\r\n");

        /* Phase 0: ALL STOP */
        uart_print("Phase 0: ALL STOP (2s)\r\n");
        Motor_Stop();
        uart_print("[DEBUG] Starting 2s delay...\r\n");
        DelayMs(2000);
        uart_print("[DEBUG] 2s delay complete\r\n");

        /* Phase 1: M1 ONLY */
        uart_print("Phase 1: M1 ONLY at 30%% (3s)\r\n");
        uart_print("[DEBUG] Calling Motor_SetFour(300, 0, 0, 0)...\r\n");
        Motor_SetFour(300, 0, 0, 0);
        uart_print("[DEBUG] Motor_SetFour returned\r\n");
        uart_print("[DEBUG] Starting 3s delay...\r\n");
        DelayMs(3000);
        uart_print("[DEBUG] 3s delay complete\r\n");

        /* Phase 2: STOP */
        uart_print("Phase 2: STOP (1s)\r\n");
        Motor_Stop();
        DelayMs(1000);

        /* Phase 3: M2 ONLY */
        uart_print("Phase 3: M2 ONLY at 30%% (3s)\r\n");
        Motor_SetFour(0, 300, 0, 0);
        DelayMs(3000);

        /* Phase 4: STOP */
        uart_print("Phase 4: STOP (1s)\r\n");
        Motor_Stop();
        DelayMs(1000);

        /* Phase 5: M3 ONLY */
        uart_print("Phase 5: M3 ONLY at 30%% (3s)\r\n");
        Motor_SetFour(0, 0, 300, 0);
        DelayMs(3000);

        /* Phase 6: STOP */
        uart_print("Phase 6: STOP (1s)\r\n");
        Motor_Stop();
        DelayMs(1000);

        /* Phase 7: M4 ONLY */
        uart_print("Phase 7: M4 ONLY at 30%% (3s)\r\n");
        Motor_SetFour(0, 0, 0, 300);
        DelayMs(3000);

        /* Phase 8: STOP */
        uart_print("Phase 8: STOP\r\n\r\n");
        Motor_Stop();

        uart_print("=== TEST COMPLETE ===\r\n");
        uart_print("Record which wheel moved in each phase.\r\n");
        uart_print("Press RESET to run again.\r\n\r\n");

        initialized = true;
    }

    /* Idle loop - test complete */
    __WFI();
}
