/**
 * @file test_motor_basic.c
 * @brief Basic motor hardware test - directly drive motors
 */

#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "motor.h"

static uint32_t g_counter = 0;

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
 * Very simple motor test
 */
void test_motor_basic_main_loop(void)
{
    static bool initialized = false;
    static uint32_t phase = 0;

    if (!initialized) {
        printf("\n");
        printf("=====================================\n");
        printf(" Basic Motor Hardware Test          \n");
        printf("=====================================\n");
        printf("\n");
        printf("Testing motor driver directly.\n");
        printf("You should hear motors running.\n");
        printf("\n");

        /* Initialize motor driver */
        Motor_Init();
        Motor_Stop();

        printf("Motor driver initialized.\n");
        printf("Starting motor test sequence...\n\n");

        initialized = true;
        g_counter = 0;
        phase = 0;
    }

    /* Run test phases */
    switch (phase) {
        case 0:
            if (g_counter == 0) {
                printf("[Phase 0] All motors STOP\n");
                Motor_Stop();
            }
            if (g_counter >= 2000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 1:
            if (g_counter == 0) {
                printf("[Phase 1] All motors FORWARD @ 30%% (speed=300)\n");
                Motor_SetFour(300, 300, 300, 300);
            }
            if (g_counter >= 3000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 2:
            if (g_counter == 0) {
                printf("[Phase 2] All motors STOP\n");
                Motor_Stop();
            }
            if (g_counter >= 1000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 3:
            if (g_counter == 0) {
                printf("[Phase 3] All motors REVERSE @ 30%% (speed=-300)\n");
                Motor_SetFour(-300, -300, -300, -300);
            }
            if (g_counter >= 3000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 4:
            if (g_counter == 0) {
                printf("[Phase 4] All motors STOP\n");
                Motor_Stop();
            }
            if (g_counter >= 1000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 5:
            if (g_counter == 0) {
                printf("[Phase 5] M1 only @ 50%% (speed=500)\n");
                Motor_SetFour(500, 0, 0, 0);
            }
            if (g_counter >= 2000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 6:
            if (g_counter == 0) {
                printf("[Phase 6] M2 only @ 50%%\n");
                Motor_SetFour(0, 500, 0, 0);
            }
            if (g_counter >= 2000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 7:
            if (g_counter == 0) {
                printf("[Phase 7] M3 only @ 50%%\n");
                Motor_SetFour(0, 0, 500, 0);
            }
            if (g_counter >= 2000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 8:
            if (g_counter == 0) {
                printf("[Phase 8] M4 only @ 50%%\n");
                Motor_SetFour(0, 0, 0, 500);
            }
            if (g_counter >= 2000) {
                phase++;
                g_counter = 0;
            }
            break;

        case 9:
            if (g_counter == 0) {
                printf("[Phase 9] All motors STOP\n");
                Motor_Stop();
                printf("\n=== Test Complete ===\n");
                printf("Did you hear the motors?\n");
                printf("Press RESET to run again.\n\n");
            }
            /* Stay in this phase forever */
            break;
    }

    g_counter++;
    DelayMs(1);
}
