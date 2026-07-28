/**
 * @file test_motor_encoder_hardware.c
 * @brief Motor and Encoder Hardware Validation Test
 *
 * Test sequence:
 * 1. Encoder count reading (passive test)
 * 2. Motor basic control (open-loop PWM test)
 * 3. Motor + Encoder integration (closed-loop feedback test)
 */

#include <stdio.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"

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

/* Test state machine */
typedef enum {
    TEST_STATE_INIT,
    TEST_STATE_ENCODER_PASSIVE,
    TEST_STATE_MOTOR_BASIC,
    TEST_STATE_MOTOR_ENCODER_LOOP,
    TEST_STATE_DONE
} TestState;

static TestState g_test_state = TEST_STATE_INIT;
static uint32_t g_test_counter = 0;
static int32_t g_encoder_snapshot[ENCODER_ID_COUNT];

/* Test configuration */
#define TEST_MOTOR_LOW_SPEED    200   /* 20% duty cycle */
#define TEST_MOTOR_MED_SPEED    500   /* 50% duty cycle */
#define TEST_MOTOR_HIGH_SPEED   800   /* 80% duty cycle */

/**
 * Test 1: Encoder Passive Reading
 * Read encoder counts without driving motors
 * User should manually rotate wheels to generate pulses
 */
static void Test_EncoderPassive(void)
{
    static uint32_t start_time = 0;
    static uint32_t last_print_time = 0;
    static bool test_started = false;

    if (!test_started) {
        printf("\n=== TEST 1: Encoder Passive Reading ===\n");
        printf("Manually rotate each wheel to generate encoder pulses.\n");
        printf("Monitoring for 10 seconds...\n\n");
        printf("Motor | Count | Delta/s | Status\n");
        printf("------|-------|---------|-------\n");

        /* Take initial snapshot */
        for (int i = 0; i < ENCODER_ID_COUNT; i++) {
            g_encoder_snapshot[i] = Encoder_GetCount((Encoder_Id)i);
        }

        start_time = PlatformTime_GetUs64() / 1000U;  /* Convert to ms */
        last_print_time = start_time;
        test_started = true;
    }

    uint32_t current_time = PlatformTime_GetUs64() / 1000U;
    uint32_t elapsed_total = current_time - start_time;
    uint32_t elapsed_since_print = current_time - last_print_time;

    /* Update every 500ms */
    if (elapsed_since_print >= 500) {
        for (int i = 0; i < ENCODER_ID_COUNT; i++) {
            int32_t current = Encoder_GetCount((Encoder_Id)i);
            int32_t delta = current - g_encoder_snapshot[i];

            /* Calculate delta per second */
            float delta_per_sec = (delta * 1000.0f) / (elapsed_since_print > 0 ? elapsed_since_print : 1);

            const char *status = (delta != 0) ? "ACTIVE" : "idle";

            printf("  M%d  | %6d | %7.1f | %s\n",
                   i + 1, current, delta_per_sec, status);

            g_encoder_snapshot[i] = current;
        }
        printf("\n");

        /* Update last print time */
        last_print_time = current_time;
    }

    /* Test runs for 10 seconds */
    if (elapsed_total > 10000) {
        printf("=== Encoder Test Complete ===\n");
        printf("Result: Check if encoder counts changed when you rotated wheels.\n");
        printf("  - If counts changed: ✓ Encoders working\n");
        printf("  - If counts stayed zero: ✗ Check encoder wiring\n\n");

        /* Move to next test */
        g_test_state = TEST_STATE_MOTOR_BASIC;
        g_test_counter = 0;
        test_started = false;
    }
}

/**
 * Test 2: Motor Basic Control
 * Drive each motor individually at different speeds
 * No encoder feedback, just open-loop PWM
 */
static void Test_MotorBasic(void)
{
    static uint32_t phase = 0;
    static uint32_t phase_start = 0;
    static bool test_started = false;

    if (!test_started) {
        printf("\n=== TEST 2: Motor Basic Control ===\n");
        printf("Testing each motor individually with PWM.\n");
        printf("Listen for motor sounds and observe wheel rotation.\n\n");
        phase = 0;
        phase_start = g_test_counter;
        test_started = true;
    }

    uint32_t phase_time = g_test_counter - phase_start;

    /* Each phase lasts 2 seconds (2000ms) */
    #define PHASE_DURATION 2000

    switch (phase) {
        case 0:  /* M1 forward low speed */
            if (phase_time == 0) {
                printf("[Phase 0] M1 forward @ 20%% PWM\n");
                Motor_SetFour(TEST_MOTOR_LOW_SPEED, 0, 0, 0);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 1:  /* M2 forward low speed */
            if (phase_time == 0) {
                printf("[Phase 1] M2 forward @ 20%% PWM\n");
                Motor_SetFour(0, TEST_MOTOR_LOW_SPEED, 0, 0);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 2:  /* M3 forward low speed */
            if (phase_time == 0) {
                printf("[Phase 2] M3 forward @ 20%% PWM\n");
                Motor_SetFour(0, 0, TEST_MOTOR_LOW_SPEED, 0);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 3:  /* M4 forward low speed */
            if (phase_time == 0) {
                printf("[Phase 3] M4 forward @ 20%% PWM\n");
                Motor_SetFour(0, 0, 0, TEST_MOTOR_LOW_SPEED);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 4:  /* All motors forward medium speed */
            if (phase_time == 0) {
                printf("[Phase 4] All motors forward @ 50%% PWM\n");
                Motor_SetFour(TEST_MOTOR_MED_SPEED, TEST_MOTOR_MED_SPEED,
                             TEST_MOTOR_MED_SPEED, TEST_MOTOR_MED_SPEED);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 5:  /* All motors reverse medium speed */
            if (phase_time == 0) {
                printf("[Phase 5] All motors reverse @ 50%% PWM\n");
                Motor_SetFour(-TEST_MOTOR_MED_SPEED, -TEST_MOTOR_MED_SPEED,
                             -TEST_MOTOR_MED_SPEED, -TEST_MOTOR_MED_SPEED);
            }
            if (phase_time >= PHASE_DURATION) {
                Motor_Stop();
                phase++;
                phase_start = g_test_counter;
            }
            break;

        case 6:  /* Test complete */
            printf("\n=== Motor Basic Test Complete ===\n");
            printf("Result: Check if each motor responded correctly.\n");
            printf("  - If motors ran: ✓ Motor driver working\n");
            printf("  - If no movement: ✗ Check power supply and wiring\n\n");

            g_test_state = TEST_STATE_MOTOR_ENCODER_LOOP;
            g_test_counter = 0;
            test_started = false;
            break;
    }
}

/**
 * Test 3: Motor + Encoder Closed-Loop
 * Run motors and monitor encoder feedback
 * This verifies the complete sensing + actuation loop
 */
static void Test_MotorEncoderLoop(void)
{
    static uint32_t phase = 0;
    static uint32_t phase_start = 0;
    static bool test_started = false;

    if (!test_started) {
        printf("\n=== TEST 3: Motor + Encoder Closed-Loop ===\n");
        printf("Running motors and monitoring encoder feedback.\n\n");

        /* Reset all encoder counts */
        for (int i = 0; i < ENCODER_ID_COUNT; i++) {
            Encoder_ResetCount((Encoder_Id)i);
        }

        phase = 0;
        phase_start = g_test_counter;
        test_started = true;
    }

    uint32_t phase_time = g_test_counter - phase_start;

    switch (phase) {
        case 0:  /* Run all motors forward for 3 seconds */
            if (phase_time == 0) {
                printf("[Phase 0] All motors forward @ 50%% - monitoring encoders\n");
                printf("Time(s) |   M1   |   M2   |   M3   |   M4   |\n");
                printf("--------|--------|--------|--------|--------|\n");
                Motor_SetFour(TEST_MOTOR_MED_SPEED, TEST_MOTOR_MED_SPEED,
                             TEST_MOTOR_MED_SPEED, TEST_MOTOR_MED_SPEED);
            }

            /* Print encoder counts every 500ms */
            if ((phase_time % 500) == 0) {
                printf("  %2u.%1u  | %6d | %6d | %6d | %6d |\n",
                       phase_time / 1000, (phase_time % 1000) / 100,
                       Encoder_GetCount(ENCODER_M1),
                       Encoder_GetCount(ENCODER_M2),
                       Encoder_GetCount(ENCODER_M3),
                       Encoder_GetCount(ENCODER_M4));
            }

            if (phase_time >= 3000) {
                Motor_Stop();
                printf("\n");
                phase++;
                phase_start = g_test_counter;

                /* Wait 500ms before next phase */
                while ((g_test_counter - phase_start) < 500) {
                    g_test_counter++;
                    DelayUs(1000);  /* 1ms */
                }
                phase_start = g_test_counter;
            }
            break;

        case 1:  /* Run all motors reverse for 3 seconds */
            if (phase_time == 0) {
                printf("[Phase 1] All motors reverse @ 50%% - monitoring encoders\n");
                printf("Time(s) |   M1   |   M2   |   M3   |   M4   |\n");
                printf("--------|--------|--------|--------|--------|\n");
                Motor_SetFour(-TEST_MOTOR_MED_SPEED, -TEST_MOTOR_MED_SPEED,
                             -TEST_MOTOR_MED_SPEED, -TEST_MOTOR_MED_SPEED);
            }

            if ((phase_time % 500) == 0) {
                printf("  %2u.%1u  | %6d | %6d | %6d | %6d |\n",
                       phase_time / 1000, (phase_time % 1000) / 100,
                       Encoder_GetCount(ENCODER_M1),
                       Encoder_GetCount(ENCODER_M2),
                       Encoder_GetCount(ENCODER_M3),
                       Encoder_GetCount(ENCODER_M4));
            }

            if (phase_time >= 3000) {
                Motor_Stop();
                printf("\n");
                phase++;
            }
            break;

        case 2:  /* Analysis and completion */
            printf("=== Motor + Encoder Test Complete ===\n\n");

            printf("Final encoder counts:\n");
            for (int i = 0; i < ENCODER_ID_COUNT; i++) {
                int32_t count = Encoder_GetCount((Encoder_Id)i);
                printf("  M%d: %d counts\n", i + 1, count);
            }

            printf("\nResult interpretation:\n");
            printf("  - Counts increased during forward phase: ✓ Correct direction\n");
            printf("  - Counts decreased during reverse phase: ✓ Correct direction\n");
            printf("  - Counts near zero at end: ✓ Good symmetry\n");
            printf("  - Counts stayed zero: ✗ Encoder not connected or motor not moving\n");

            g_test_state = TEST_STATE_DONE;
            break;
    }
}

/**
 * Main test loop
 * Called repeatedly from main()
 */
void test_motor_encoder_main_loop(void)
{
    /* Delay 1ms per iteration */
    DelayUs(1000);
    g_test_counter++;

    switch (g_test_state) {
        case TEST_STATE_INIT:
            printf("\n");
            printf("=====================================\n");
            printf(" Motor & Encoder Hardware Test      \n");
            printf("=====================================\n");
            printf("Board: MSPM0G3507\n");
            printf("Motors: 4x TB6612 channels\n");
            printf("Encoders: 4x Quadrature\n");
            printf("\n");
            printf("⚠ WARNING: Ensure motors have adequate power supply!\n");
            printf("⚠ Wheels should be free to rotate.\n");
            printf("\n");

            /* Initialize hardware */
            Motor_Init();
            Encoder_Init();

            printf("✓ Motor driver initialized\n");
            printf("✓ Encoder interrupts enabled\n");
            printf("\n");

            g_test_state = TEST_STATE_ENCODER_PASSIVE;
            g_test_counter = 0;
            break;

        case TEST_STATE_ENCODER_PASSIVE:
            Test_EncoderPassive();
            break;

        case TEST_STATE_MOTOR_BASIC:
            Test_MotorBasic();
            break;

        case TEST_STATE_MOTOR_ENCODER_LOOP:
            Test_MotorEncoderLoop();
            break;

        case TEST_STATE_DONE:
            printf("\n");
            printf("=====================================\n");
            printf(" All Tests Complete!                \n");
            printf("=====================================\n");
            printf("\n");
            printf("Press RESET to run tests again.\n");

            /* Stop everything */
            Motor_Stop();

            /* Idle forever */
            for (;;) {
                DelayUs(1000000);  /* 1 second */
            }
            break;
    }
}
