/**
 * @file motor_speed_test.c
 * @brief Motor speed test with multiple PWM levels
 */

#include "motor_speed_test.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include <stdio.h>

#define TEST_DURATION_MS 3000  // 3 seconds per test

// Helper to get milliseconds from microseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

static void test_pwm_level(int16_t pwm_percent, const char* description) {
    printf("\n========================================\n");
    printf("Testing: %s\n", description);
    printf("PWM: %d%%\n", pwm_percent);
    printf("========================================\n");

    // Reset encoder counts
    int32_t start_left = Encoder_GetCount(0);
    int32_t start_right = Encoder_GetCount(1);

    printf("Starting encoder - Left: %ld  Right: %ld\n", start_left, start_right);

    // Set motor speed
    Motor_SetSpeed(pwm_percent, pwm_percent);

    // Run for test duration
    uint32_t start_time = PlatformTime_GetMillis();
    uint32_t last_print = start_time;

    while (PlatformTime_GetMillis() - start_time < TEST_DURATION_MS) {
        Encoder_Poll();

        // Print status every 500ms
        uint32_t now = PlatformTime_GetMillis();
        if (now - last_print >= 500) {
            int32_t left = Encoder_GetCount(0);
            int32_t right = Encoder_GetCount(1);
            printf("[%lu ms] Left: %ld  Right: %ld\n",
                   now - start_time, left - start_left, right - start_right);
            last_print = now;
        }
    }

    // Stop motors
    Motor_SetSpeed(0, 0);

    // Final encoder reading
    Encoder_Poll();
    int32_t end_left = Encoder_GetCount(0);
    int32_t end_right = Encoder_GetCount(1);

    int32_t delta_left = end_left - start_left;
    int32_t delta_right = end_right - start_right;

    printf("\nResults after %d ms:\n", TEST_DURATION_MS);
    printf("  Left encoder:  %ld counts (%ld counts/sec)\n",
           delta_left, delta_left * 1000 / TEST_DURATION_MS);
    printf("  Right encoder: %ld counts (%ld counts/sec)\n",
           delta_right, delta_right * 1000 / TEST_DURATION_MS);

    // Calculate speed balance
    if (delta_left != 0 && delta_right != 0) {
        float balance = (float)delta_right / (float)delta_left * 100.0f;
        printf("  Speed balance: Right/Left = %.1f%%\n", balance);
        if (balance < 95.0f || balance > 105.0f) {
            printf("  ⚠️  Speed difference > 5%% (需要调参补偿)\n");
        } else {
            printf("  ✅ Speed balanced\n");
        }
    }

    printf("\nWaiting 2 seconds before next test...\n");
    uint32_t wait_start = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait_start < 2000) {
        // Wait
    }
}

void MotorSpeedTest_Run(void) {
    printf("\n");
    printf("========================================\n");
    printf("  Motor Speed Test Tool\n");
    printf("========================================\n");
    printf("\n");
    printf("This tool will test motors at different PWM levels\n");
    printf("and measure encoder feedback.\n");
    printf("\n");
    printf("Test sequence:\n");
    printf("  1. 10%% PWM (very low speed)\n");
    printf("  2. 20%% PWM (low speed)\n");
    printf("  3. 30%% PWM (medium-low speed)\n");
    printf("  4. 50%% PWM (medium speed - MAX)\n");
    printf("\n");
    printf("Each test runs for %d seconds.\n", TEST_DURATION_MS / 1000);
    printf("\n");
    printf("Starting in 3 seconds...\n\n");

    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        // Wait
    }

    // Test 1: 10% PWM
    test_pwm_level(10, "Very Low Speed (10%)");

    // Test 2: 20% PWM
    test_pwm_level(20, "Low Speed (20%)");

    // Test 3: 30% PWM
    test_pwm_level(30, "Medium-Low Speed (30%)");

    // Test 4: 50% PWM (MAX)
    test_pwm_level(50, "Medium Speed (50% - MAX)");

    printf("\n");
    printf("========================================\n");
    printf("  Speed Test Complete!\n");
    printf("========================================\n");
    printf("\n");
    printf("Summary:\n");
    printf("  - If encoder counts are 0, check encoder wiring\n");
    printf("  - If speed seems slow, check VM voltage (should be 6-12V)\n");
    printf("  - If left/right difference >5%%, tune PID parameters\n");
    printf("\n");
    printf("Expected encoder counts (approx):\n");
    printf("  10%% PWM: ~300-600 counts/sec\n");
    printf("  20%% PWM: ~600-1200 counts/sec\n");
    printf("  30%% PWM: ~900-1800 counts/sec\n");
    printf("  50%% PWM: ~1500-3000 counts/sec\n");
    printf("\n");
}
