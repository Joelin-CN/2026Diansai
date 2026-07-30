/**
 * @file test_encoder_super_minimal.c
 * @brief Super minimal encoder test - no time functions
 * @date 2026-07-27
 */

#include "encoder.h"
#include "motor.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TEST_SPEED 500  // 50% PWM

static uint32_t g_loopCounter = 0;
static bool g_initialized = false;

/**
 * @brief Simple delay using busy loop
 */
static void SimpleDelay(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        /* Busy wait */
    }
}

/**
 * @brief Main test loop - super minimal version
 */
void test_encoder_super_minimal_main_loop(void)
{
    g_loopCounter++;

    /* Initialize once */
    if (!g_initialized) {
        printf("\n=== Super Minimal Encoder Test ===\n");
        printf("No time functions used\n");
        printf("Initializing...\n");

        Motor_Init();
        Encoder_Init();

        printf("Starting M1 motor at 50%% PWM...\n");
        Motor_SetFour(TEST_SPEED, 0, 0, 0);

        g_initialized = true;

        printf("Initialization complete. Starting loop...\n");
        return;
    }

    /* Print status every 5000 loops (roughly 5 seconds at 1ms per loop) */
    if ((g_loopCounter % 5000) == 0) {
        int32_t enc1 = Encoder_GetCount(ENCODER_M1);
        int32_t enc2 = Encoder_GetCount(ENCODER_M2);
        int32_t enc3 = Encoder_GetCount(ENCODER_M3);
        int32_t enc4 = Encoder_GetCount(ENCODER_M4);

        printf("[Loop %lu] Enc: M1=%ld M2=%ld M3=%ld M4=%ld\n",
               (unsigned long)g_loopCounter,
               (long)enc1, (long)enc2, (long)enc3, (long)enc4);
    }

    /* Stop after 50000 loops (roughly 50 seconds) */
    if (g_loopCounter >= 50000) {
        Motor_SetFour(0, 0, 0, 0);
        printf("\n=== Test Complete ===\n");
        printf("Total loops: %lu\n", (unsigned long)g_loopCounter);

        int32_t enc1 = Encoder_GetCount(ENCODER_M1);
        printf("Final M1 encoder count: %ld\n", (long)enc1);

        /* Stay in done state */
        for (;;) {
            /* Halt */
        }
    }

    /* Simple delay - roughly 1ms at 32MHz */
    SimpleDelay(8000);
}
