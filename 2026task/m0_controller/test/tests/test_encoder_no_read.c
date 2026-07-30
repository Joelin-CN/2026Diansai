/**
 * @file test_encoder_no_read.c
 * @brief Test without reading encoder - isolate the issue
 * @date 2026-07-27
 */

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
 * @brief Main test loop - NO encoder reading at all
 */
void test_encoder_no_read_main_loop(void)
{
    g_loopCounter++;

    /* Initialize once */
    if (!g_initialized) {
        printf("\n=== Test Without Encoder Reading ===\n");
        printf("Only motor control, NO encoder functions called\n");
        printf("Initializing motor...\n");

        Motor_Init();
        /* NOT calling Encoder_Init() */

        printf("Starting M1 motor at 50%% PWM...\n");
        Motor_SetFour(TEST_SPEED, 0, 0, 0);

        g_initialized = true;

        printf("Initialization complete. Starting loop...\n");
        return;
    }

    /* Print status every 1000 loops */
    if ((g_loopCounter % 1000) == 0) {
        printf("[Loop %lu] Motor running, no encoder read\n",
               (unsigned long)g_loopCounter);
    }

    /* Stop after 10000 loops */
    if (g_loopCounter >= 10000) {
        Motor_SetFour(0, 0, 0, 0);
        printf("\n=== Test Complete ===\n");
        printf("Total loops: %lu\n", (unsigned long)g_loopCounter);
        printf("If you see this, the main loop works fine!\n");
        printf("Problem is related to Encoder functions.\n");

        /* Stay in done state */
        for (;;) {
            /* Halt */
        }
    }

    /* Simple delay - roughly 1ms at 32MHz */
    SimpleDelay(8000);
}
