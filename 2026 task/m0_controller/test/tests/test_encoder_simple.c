/**
 * @file test_encoder_simple.c
 * @brief Simple encoder test without interrupts
 */

#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "encoder.h"
#include "platform_time.h"

/**
 * @brief Simple delay using busy-wait
 */
static void DelayMs(uint32_t ms)
{
    uint64_t start = PlatformTime_GetUs64();
    uint64_t target = start + (ms * 1000ULL);
    while (PlatformTime_GetUs64() < target) {
        /* Busy wait */
    }
}

/**
 * Simple encoder test - poll mode only
 */
void test_encoder_simple_main_loop(void)
{
    static bool initialized = false;
    static uint32_t counter = 0;

    if (!initialized) {
        printf("\n");
        printf("=====================================\n");
        printf(" Simple Encoder Test (No Interrupts)\n");
        printf("=====================================\n");
        printf("\n");
        printf("Testing encoder reading without GPIO interrupts.\n");
        printf("Rotate wheels and watch the counts.\n");
        printf("\n");

        /* Initialize encoder WITHOUT enabling interrupts */
        for (int i = 0; i < ENCODER_ID_COUNT; i++) {
            Encoder_ResetCount((Encoder_Id)i);
        }

        printf("Press RESET to restart.\n");
        printf("\n");
        printf("Time(s) |   M1   |   M2   |   M3   |   M4   |\n");
        printf("--------|--------|--------|--------|--------|\n");

        initialized = true;
        counter = 0;
    }

    /* Print every 500ms */
    if ((counter % 500) == 0) {
        printf("  %2u.%1u  | %6d | %6d | %6d | %6d |\n",
               counter / 1000, (counter % 1000) / 100,
               Encoder_GetCount(ENCODER_M1),
               Encoder_GetCount(ENCODER_M2),
               Encoder_GetCount(ENCODER_M3),
               Encoder_GetCount(ENCODER_M4));
    }

    counter++;
    DelayMs(1);
}
