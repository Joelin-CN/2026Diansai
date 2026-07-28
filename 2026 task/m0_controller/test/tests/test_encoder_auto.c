/**
 * @file test_encoder_auto.c
 * @brief Automatic encoder test: motors drive themselves and encoder values are read
 * @date 2026-07-27
 */

#include "encoder.h"
#include "motor.h"
#include "platform_time.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Simple delay function using busy-wait
 * @param us Microseconds to delay
 */
static void DelayUs(uint32_t us)
{
    uint64_t start = PlatformTime_GetUs64();
    while ((PlatformTime_GetUs64() - start) < us) {
        /* Busy wait */
    }
}

/* Test parameters */
#define TEST_SPEED_LOW      200   // 20% PWM
#define TEST_SPEED_MED      500   // 50% PWM
#define TEST_DURATION_MS    2000  // 2 seconds per phase

/* Test state machine */
typedef enum {
    STATE_INIT,
    STATE_TEST_M1,
    STATE_STOP_M1,
    STATE_TEST_M2,
    STATE_STOP_M2,
    STATE_TEST_M3,
    STATE_STOP_M3,
    STATE_TEST_M4,
    STATE_STOP_M4,
    STATE_TEST_ALL_FORWARD,
    STATE_STOP_ALL,
    STATE_TEST_ALL_REVERSE,
    STATE_DONE
} TestState;

static TestState g_state = STATE_INIT;
static uint32_t g_phaseStartMs = 0;
static uint32_t g_testCounter = 0;

/* Encoder counts at phase start */
static int32_t g_startCount[4];

/**
 * @brief Initialize test
 */
static void Test_Init(void)
{
    printf("\n");
    printf("=====================================\n");
    printf(" Automatic Encoder Test             \n");
    printf("=====================================\n");
    printf("Board: MSPM0G3507\n");
    printf("Test: Motor drives, encoder reads\n");
    printf("\n");

    /* Initialize motor and encoder */
    Motor_Init();
    Encoder_Init();

    printf("✓ Motor driver initialized\n");
    printf("✓ Encoder polling ready (no interrupts)\n");
    printf("\n");

    /* Stop all motors initially */
    Motor_SetFour(0, 0, 0, 0);

    /* Reset all encoder counts */
    Encoder_ResetCount(ENCODER_M1);
    Encoder_ResetCount(ENCODER_M2);
    Encoder_ResetCount(ENCODER_M3);
    Encoder_ResetCount(ENCODER_M4);

    printf("Starting test sequence...\n");
    printf("\n");
}

/**
 * @brief Print encoder values
 */
static void Print_EncoderValues(void)
{
    int32_t m1 = Encoder_GetCount(ENCODER_M1);
    int32_t m2 = Encoder_GetCount(ENCODER_M2);
    int32_t m3 = Encoder_GetCount(ENCODER_M3);
    int32_t m4 = Encoder_GetCount(ENCODER_M4);

    printf("  M1: %6ld | M2: %6ld | M3: %6ld | M4: %6ld\n", m1, m2, m3, m4);
}

/**
 * @brief Save encoder start counts
 */
static void Save_StartCounts(void)
{
    g_startCount[0] = Encoder_GetCount(ENCODER_M1);
    g_startCount[1] = Encoder_GetCount(ENCODER_M2);
    g_startCount[2] = Encoder_GetCount(ENCODER_M3);
    g_startCount[3] = Encoder_GetCount(ENCODER_M4);
}

/**
 * @brief Print encoder delta since phase start
 */
static void Print_EncoderDelta(void)
{
    int32_t d1 = Encoder_GetCount(ENCODER_M1) - g_startCount[0];
    int32_t d2 = Encoder_GetCount(ENCODER_M2) - g_startCount[1];
    int32_t d3 = Encoder_GetCount(ENCODER_M3) - g_startCount[2];
    int32_t d4 = Encoder_GetCount(ENCODER_M4) - g_startCount[3];

    printf("  Delta: M1=%6ld, M2=%6ld, M3=%6ld, M4=%6ld\n", d1, d2, d3, d4);
}

/**
 * @brief Main test loop
 */
void test_encoder_auto_main_loop(void)
{
    /* Poll encoders at high frequency (this function is called from main loop) */
    Encoder_Poll();

    /* 1ms delay between iterations */
    DelayUs(1000);
    g_testCounter++;

    uint32_t currentMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
    uint32_t elapsedMs = currentMs - g_phaseStartMs;

    /* Global timeout: 20 seconds from first call */
    static uint32_t globalStartMs = 0;
    if (globalStartMs == 0) {
        globalStartMs = currentMs;
    }
    uint32_t totalElapsedMs = currentMs - globalStartMs;

    /* Stop all debug output after 20 seconds */
    if (totalElapsedMs > 20000) {
        /* Only print completion message once */
        static bool completionPrinted = false;
        if (!completionPrinted) {
            printf("\n");
            printf("=====================================\n");
            printf(" 20-Second Test Timeout Reached     \n");
            printf("=====================================\n");
            printf("Final state: %d\n", (int)g_state);
            printf("Total elapsed time: %lu ms\n", (unsigned long)totalElapsedMs);
            printf("\nFinal encoder counts:\n");
            Print_EncoderValues();
            printf("\n[TEST STOPPED - 20s timeout]\n");
            printf("No further debug output will be printed.\n");
            printf("Press RESET to run test again.\n");
            completionPrinted = true;
        }
        /* Stop motors and halt state machine */
        Motor_SetFour(0, 0, 0, 0);
        return;
    }

    /* Debug output every 500ms */
    static uint32_t lastDebugMs = 0;
    if (currentMs - lastDebugMs >= 500) {
        uint32_t intA, intB;
        Encoder_GetInterruptCounts(&intA, &intB);
        printf("[DEBUG] State=%d, elapsed=%lu ms, total=%lu ms, IntA=%lu, IntB=%lu\n",
               (int)g_state, (unsigned long)elapsedMs, (unsigned long)totalElapsedMs,
               (unsigned long)intA, (unsigned long)intB);
        lastDebugMs = currentMs;
    }

    switch (g_state) {
        case STATE_INIT:
            Test_Init();
            g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
            Save_StartCounts();
            printf("=== Test 1: M1 (Left Front) Forward @ 50%% ===\n");
            Motor_SetFour(TEST_SPEED_MED, 0, 0, 0);
            g_state = STATE_TEST_M1;
            break;

        case STATE_TEST_M1:
            if (elapsedMs >= TEST_DURATION_MS) {
                printf("[DEBUG] M1 test complete, elapsed=%lu ms\n", (unsigned long)elapsedMs);
                Print_EncoderDelta();
                Motor_SetFour(0, 0, 0, 0);
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                g_state = STATE_STOP_M1;
            }
            break;

        case STATE_STOP_M1:
            if (elapsedMs >= 500) {  // 500ms stop
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                Save_StartCounts();
                printf("\n=== Test 2: M2 (Left Rear) Forward @ 50%% ===\n");
                Motor_SetFour(0, TEST_SPEED_MED, 0, 0);
                g_state = STATE_TEST_M2;
            }
            break;

        case STATE_TEST_M2:
            if (elapsedMs >= TEST_DURATION_MS) {
                printf("[DEBUG] M2 test complete, transitioning to STATE_STOP_M2\n");
                printf("[DEBUG] About to call Print_EncoderDelta...\n");
                Print_EncoderDelta();
                printf("[DEBUG] Print_EncoderDelta complete\n");
                printf("[DEBUG] About to stop motors...\n");
                Motor_SetFour(0, 0, 0, 0);
                printf("[DEBUG] Motors stopped\n");
                printf("[DEBUG] About to update timestamp...\n");
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                printf("[DEBUG] About to change state...\n");
                g_state = STATE_STOP_M2;
                printf("[DEBUG] State set to %d (STATE_STOP_M2)\n", (int)g_state);
            }
            break;

        case STATE_STOP_M2:
            if (elapsedMs == 0) {
                printf("[DEBUG] Entered STATE_STOP_M2, waiting 500ms...\n");
            }
            if (elapsedMs >= 500) {
                printf("[DEBUG] Transitioning from STATE_STOP_M2 to STATE_TEST_M3 (elapsed=%lu)\n", (unsigned long)elapsedMs);
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                Save_StartCounts();
                printf("\n=== Test 3: M3 (Right Rear) Forward @ 50%% ===\n");
                Motor_SetFour(0, 0, TEST_SPEED_MED, 0);
                g_state = STATE_TEST_M3;
                printf("[DEBUG] State set to %d (STATE_TEST_M3)\n", (int)g_state);
            } else {
                /* Debug: print every 100ms to see what's happening */
                static uint32_t lastPrintMs = 0;
                if (elapsedMs - lastPrintMs >= 100) {
                    printf("[DEBUG] STATE_STOP_M2: elapsed=%lu ms (waiting for 500ms)\n", (unsigned long)elapsedMs);
                    lastPrintMs = elapsedMs;
                }
            }
            break;

        case STATE_TEST_M3:
            if (elapsedMs >= TEST_DURATION_MS) {
                Print_EncoderDelta();
                Motor_SetFour(0, 0, 0, 0);
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                g_state = STATE_STOP_M3;
            }
            break;

        case STATE_STOP_M3:
            if (elapsedMs >= 500) {
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                Save_StartCounts();
                printf("\n=== Test 4: M4 (Right Front) Forward @ 50%% ===\n");
                Motor_SetFour(0, 0, 0, TEST_SPEED_MED);
                g_state = STATE_TEST_M4;
            }
            break;

        case STATE_TEST_M4:
            if (elapsedMs >= TEST_DURATION_MS) {
                Print_EncoderDelta();
                Motor_SetFour(0, 0, 0, 0);
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                g_state = STATE_STOP_M4;
            }
            break;

        case STATE_STOP_M4:
            if (elapsedMs >= 500) {
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                Save_StartCounts();
                printf("\n=== Test 5: All Motors Forward @ 50%% ===\n");
                Motor_SetFour(TEST_SPEED_MED, TEST_SPEED_MED, TEST_SPEED_MED, TEST_SPEED_MED);
                g_state = STATE_TEST_ALL_FORWARD;
            }
            break;

        case STATE_TEST_ALL_FORWARD:
            /* Print encoder values every 500ms */
            if ((elapsedMs % 500) == 0 && elapsedMs > 0) {
                printf("  [%.1fs] ", elapsedMs / 1000.0f);
                Print_EncoderValues();
            }

            if (elapsedMs >= 3000) {  // 3 seconds
                printf("  Final: ");
                Print_EncoderValues();
                Print_EncoderDelta();
                Motor_SetFour(0, 0, 0, 0);
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                g_state = STATE_STOP_ALL;
            }
            break;

        case STATE_STOP_ALL:
            if (elapsedMs >= 500) {
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
                Save_StartCounts();
                printf("\n=== Test 6: All Motors Reverse @ 50%% ===\n");
                Motor_SetFour(-TEST_SPEED_MED, -TEST_SPEED_MED, -TEST_SPEED_MED, -TEST_SPEED_MED);
                g_state = STATE_TEST_ALL_REVERSE;
            }
            break;

        case STATE_TEST_ALL_REVERSE:
            /* Print encoder values every 500ms */
            if ((elapsedMs % 500) == 0 && elapsedMs > 0) {
                printf("  [%.1fs] ", elapsedMs / 1000.0f);
                Print_EncoderValues();
            }

            if (elapsedMs >= 3000) {  // 3 seconds
                printf("  Final: ");
                Print_EncoderValues();
                Print_EncoderDelta();
                Motor_SetFour(0, 0, 0, 0);
                g_state = STATE_DONE;
                g_phaseStartMs = (uint32_t)(PlatformTime_GetUs64() / 1000ULL);
            }
            break;

        case STATE_DONE:
            if (elapsedMs == 0) {  // Just entered
                printf("\n");
                printf("=====================================\n");
                printf(" Test Complete!                     \n");
                printf("=====================================\n");
                printf("\n");
                printf("Final encoder counts:\n");
                Print_EncoderValues();
                printf("\n");
                printf("Result interpretation:\n");
                printf("  - If counts increased during motor run: ✓ Encoder working\n");
                printf("  - If counts stayed zero: ✗ Encoder not connected or wiring issue\n");
                printf("  - If direction is wrong: Encoder A/B phase may be swapped\n");
                printf("\n");
                printf("Press RESET to run test again.\n");
            }
            /* Stay in DONE state forever */
            break;
    }
}
