/**
 * @file test_encoder_minimal.c
 * @brief Minimal encoder test - simplified version to diagnose the issue
 * @date 2026-07-27
 */

#include "encoder.h"
#include "motor.h"
#include "platform_time.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TEST_SPEED 500  // 50% PWM

static uint32_t g_loopCounter = 0;
static uint32_t g_startTimeMs = 0;
static bool g_initialized = false;

/**
 * @brief Main test loop - minimal version
 */
void test_encoder_minimal_main_loop(void)
{
    g_loopCounter++;

    /* Initialize once */
    if (!g_initialized) {
        printf("\n=== Minimal Encoder Test ===\n");
        printf("Initializing...\n");

        Motor_Init();
        Encoder_Init();

        printf("Starting M1 motor at 50%% PWM...\n");
        Motor_SetFour(TEST_SPEED, 0, 0, 0);

        g_startTimeMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
        g_initialized = true;

        printf("Initialization complete. Loop starting...\n");
        return;
    }

    /* Get current time */
    uint32_t currentMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
    uint32_t elapsedMs = currentMs - g_startTimeMs;

    /* Print status every 500ms */
    static uint32_t lastPrintMs = 0;
    if (currentMs - lastPrintMs >= 500) {
        int32_t enc1 = Encoder_GetCount(ENCODER_M1);
        int32_t enc2 = Encoder_GetCount(ENCODER_M2);
        int32_t enc3 = Encoder_GetCount(ENCODER_M3);
        int32_t enc4 = Encoder_GetCount(ENCODER_M4);

        printf("[%lu ms] Loop=%lu, Enc: M1=%ld M2=%ld M3=%ld M4=%ld\n",
               (unsigned long)elapsedMs,
               (unsigned long)g_loopCounter,
               (long)enc1, (long)enc2, (long)enc3, (long)enc4);

        lastPrintMs = currentMs;
    }

    /* Stop after 5 seconds */
    if (elapsedMs >= 5000) {
        Motor_SetFour(0, 0, 0, 0);
        printf("\n=== Test Complete ===\n");
        printf("Total loops executed: %lu\n", (unsigned long)g_loopCounter);

        /* Stay in done state */
        for (;;) {
            /* Halt */
        }
    }

    /* Small delay to prevent CPU overload */
    uint64_t delayStart = PlatformTime_GetUs64();
    while ((PlatformTime_GetUs64() - delayStart) < 1000) {
        /* Busy wait 1ms */
    }
}
