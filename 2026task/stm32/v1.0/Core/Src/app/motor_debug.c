/**
 * @file motor_debug.c
 * @brief Motor and encoder debug tool implementation
 * @date 2026-07-29
 */

#include "motor_debug.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include "motor_hw_diagnostic.h"
#include "tim1_register_dump.h"
#include "motor_speed_test.h"
#include "motor_direction_calibration.h"
#include <stdio.h>

// Test sequence states
typedef enum {
    TEST_INIT = 0,
    TEST_PWM_OUTPUT,
    TEST_LEFT_FORWARD,
    TEST_LEFT_BACKWARD,
    TEST_LEFT_STOP,
    TEST_RIGHT_FORWARD,
    TEST_RIGHT_BACKWARD,
    TEST_RIGHT_STOP,
    TEST_BOTH_FORWARD,
    TEST_BOTH_BACKWARD,
    TEST_BOTH_STOP,
    TEST_ENCODER_CHECK,
    TEST_COMPLETE
} TestState_t;

static TestState_t s_test_state = TEST_INIT;
static uint32_t s_state_start_time = 0;
static int32_t s_encoder_start[2] = {0, 0};

// Test parameters
#define TEST_DURATION_MS     2000  // Each test runs for 2 seconds
#define TEST_PWM_LOW         30    // 30% PWM for safety
#define TEST_PWM_MEDIUM      50    // 50% PWM
#define TEST_PWM_HIGH        70    // 70% PWM (max for test)

// Encoder and motor IDs (match encoder.h and motor.h interfaces)
#define ENCODER_LEFT   0
#define ENCODER_RIGHT  1
#define MOTOR_LEFT     0
#define MOTOR_RIGHT    1

// Helper to get milliseconds from microseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

/**
 * @brief Print current encoder values
 */
static void print_encoder_status(const char* label) {
    int32_t left = Encoder_GetCount(ENCODER_LEFT);
    int32_t right = Encoder_GetCount(ENCODER_RIGHT);

    printf("[ENCODER] %s - Left: %ld  Right: %ld\n", label, left, right);
}

/**
 * @brief Print motor command
 */
static void print_motor_command(const char* label, int16_t left_pwm, int16_t right_pwm) {
    printf("\n========================================\n");
    printf("[MOTOR] %s\n", label);
    printf("  Left PWM:  %d%%\n", left_pwm);
    printf("  Right PWM: %d%%\n", right_pwm);
    printf("========================================\n");
    print_encoder_status("Before");
}

/**
 * @brief Check if current test duration has elapsed
 */
static uint8_t is_test_timeout(void) {
    uint32_t elapsed = PlatformTime_GetMillis() - s_state_start_time;
    return (elapsed >= TEST_DURATION_MS);
}

/**
 * @brief Transition to next test state
 */
static void next_test_state(void) {
    s_test_state++;
    s_state_start_time = PlatformTime_GetMillis();

    // Save encoder counts for comparison
    s_encoder_start[0] = Encoder_GetCount(ENCODER_LEFT);
    s_encoder_start[1] = Encoder_GetCount(ENCODER_RIGHT);
}

int MotorDebug_Init(void) {
    printf("\n");
    printf("========================================\n");
    printf("    Motor & Encoder Debug Tool v1.0\n");
    printf("========================================\n");
    printf("\n");

    // Initialize platform timer
    printf("[1/4] Initializing platform timer...\n");
    PlatformTime_Init();

    // Initialize encoders
    printf("[2/4] Initializing encoders...\n");
    Encoder_Init();

    // Initialize motors
    printf("[3/4] Initializing motors (TB6612)...\n");
    Motor_Init();

    // Ensure motors are stopped
    printf("[4/4] Ensuring motors are stopped...\n");
    Motor_SetSpeed(0, 0);

    printf("\n");
    printf("========================================\n");
    printf("  Initialization Complete!\n");
    printf("========================================\n");
    printf("\n");

    // Run direction calibration at 20% PWM
    MotorDirectionCalibration_Run();

    printf("Test Sequence:\n");
    printf("  1. PWM output test (%d%%)\n", TEST_PWM_LOW);
    printf("  2. Left motor forward/backward\n");
    printf("  3. Right motor forward/backward\n");
    printf("  4. Both motors forward/backward\n");
    printf("  5. Encoder verification\n");
    printf("\n");
    printf("Each test runs for %d seconds.\n", TEST_DURATION_MS / 1000);
    printf("Press RESET to stop at any time.\n");
    printf("\n");

    // Print initial encoder values
    print_encoder_status("Initial");
    printf("\n");

    // Wait 2 seconds before starting
    printf("Starting in 2 seconds...\n");
    uint32_t start = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - start < 2000) {
        // Wait
    }

    s_test_state = TEST_INIT;
    s_state_start_time = PlatformTime_GetMillis();

    return 0;
}

void MotorDebug_Run(void) {
    // Update encoder counts (must be called regularly)
    Encoder_Poll();

    switch (s_test_state) {
        case TEST_INIT:
            next_test_state();
            break;

        case TEST_PWM_OUTPUT: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("PWM Output Test (Low Power)", TEST_PWM_LOW, TEST_PWM_LOW);
                Motor_SetSpeed(TEST_PWM_LOW, TEST_PWM_LOW);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_LEFT_FORWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Left Motor FORWARD", TEST_PWM_MEDIUM, 0);
                Motor_SetSpeed(TEST_PWM_MEDIUM, 0);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta = Encoder_GetCount(ENCODER_LEFT) - s_encoder_start[0];
                printf("  --> Left encoder change: %ld counts\n", delta);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_LEFT_BACKWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Left Motor BACKWARD", -TEST_PWM_MEDIUM, 0);
                Motor_SetSpeed(-TEST_PWM_MEDIUM, 0);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta = Encoder_GetCount(ENCODER_LEFT) - s_encoder_start[0];
                printf("  --> Left encoder change: %ld counts\n", delta);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_LEFT_STOP: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                printf("\n[PAUSE] Left motor tests complete. Waiting 1 second...\n\n");
            }

            if (is_test_timeout()) {
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_RIGHT_FORWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Right Motor FORWARD", 0, TEST_PWM_MEDIUM);
                Motor_SetSpeed(0, TEST_PWM_MEDIUM);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta = Encoder_GetCount(ENCODER_RIGHT) - s_encoder_start[1];
                printf("  --> Right encoder change: %ld counts\n", delta);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_RIGHT_BACKWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Right Motor BACKWARD", 0, -TEST_PWM_MEDIUM);
                Motor_SetSpeed(0, -TEST_PWM_MEDIUM);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta = Encoder_GetCount(ENCODER_RIGHT) - s_encoder_start[1];
                printf("  --> Right encoder change: %ld counts\n", delta);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_RIGHT_STOP: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                printf("\n[PAUSE] Right motor tests complete. Waiting 1 second...\n\n");
            }

            if (is_test_timeout()) {
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_BOTH_FORWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Both Motors FORWARD", TEST_PWM_MEDIUM, TEST_PWM_MEDIUM);
                Motor_SetSpeed(TEST_PWM_MEDIUM, TEST_PWM_MEDIUM);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta_l = Encoder_GetCount(ENCODER_LEFT) - s_encoder_start[0];
                int32_t delta_r = Encoder_GetCount(ENCODER_RIGHT) - s_encoder_start[1];
                printf("  --> Left encoder change:  %ld counts\n", delta_l);
                printf("  --> Right encoder change: %ld counts\n", delta_r);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_BOTH_BACKWARD: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                print_motor_command("Both Motors BACKWARD", -TEST_PWM_MEDIUM, -TEST_PWM_MEDIUM);
                Motor_SetSpeed(-TEST_PWM_MEDIUM, -TEST_PWM_MEDIUM);
            }

            if (is_test_timeout()) {
                Motor_SetSpeed(0, 0);
                print_encoder_status("After");
                int32_t delta_l = Encoder_GetCount(ENCODER_LEFT) - s_encoder_start[0];
                int32_t delta_r = Encoder_GetCount(ENCODER_RIGHT) - s_encoder_start[1];
                printf("  --> Left encoder change:  %ld counts\n", delta_l);
                printf("  --> Right encoder change: %ld counts\n", delta_r);
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_BOTH_STOP: {
            static uint8_t entered = 0;
            if (!entered) {
                entered = 1;
                printf("\n[PAUSE] Dual motor tests complete. Waiting 1 second...\n\n");
            }

            if (is_test_timeout()) {
                entered = 0;
                next_test_state();
            }
            break;
        }

        case TEST_ENCODER_CHECK:
            printf("\n");
            printf("========================================\n");
            printf("  Final Encoder Check\n");
            printf("========================================\n");
            print_encoder_status("Final");
            printf("\n");
            printf("Manually rotate each wheel and observe:\n");
            printf("  - Left wheel forward  -> Left count increases\n");
            printf("  - Left wheel backward -> Left count decreases\n");
            printf("  - Right wheel forward -> Right count increases\n");
            printf("  - Right wheel backward -> Right count decreases\n");
            printf("\n");

            // Continuous encoder monitoring
            static uint32_t last_print = 0;
            uint32_t now = PlatformTime_GetMillis();
            if (now - last_print >= 500) {  // Print every 500ms
                print_encoder_status("Current");
                last_print = now;
            }

            // Run forever in this state
            break;

        case TEST_COMPLETE:
            // Should not reach here
            break;
    }
}
