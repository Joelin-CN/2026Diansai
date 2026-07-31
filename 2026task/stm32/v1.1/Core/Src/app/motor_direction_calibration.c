/**
 * @file motor_direction_calibration.c
 * @brief Motor direction calibration tool implementation
 */

#include "motor_direction_calibration.h"
#include "motor.h"
#include "encoder.h"
#include "platform_time.h"
#include <stdio.h>

#define CALIB_PWM 20           // 20% PWM for calibration
#define CALIB_DURATION_MS 2000 // 2 seconds per test

// Helper to get milliseconds
static inline uint32_t PlatformTime_GetMillis(void) {
    return PlatformTime_GetUs32() / 1000;
}

static void test_motor_direction(int16_t left_pwm, int16_t right_pwm, const char* description) {
    printf("\n========================================\n");
    printf("Test: %s\n", description);
    printf("Left PWM: %d%%  Right PWM: %d%%\n", left_pwm, right_pwm);
    printf("========================================\n");

    // Get initial encoder counts
    int32_t start_left = Encoder_GetCount(0);
    int32_t start_right = Encoder_GetCount(1);

    printf("Starting - Left: %ld  Right: %ld\n", start_left, start_right);
    printf("Running for %d seconds...\n", CALIB_DURATION_MS / 1000);

    // Set motor speed
    Motor_SetSpeed(left_pwm, right_pwm);

    // Run test
    uint32_t start_time = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - start_time < CALIB_DURATION_MS) {
        Encoder_Poll();
    }

    // Stop motors
    Motor_SetSpeed(0, 0);

    // Get final encoder counts
    Encoder_Poll();
    int32_t end_left = Encoder_GetCount(0);
    int32_t end_right = Encoder_GetCount(1);

    int32_t delta_left = end_left - start_left;
    int32_t delta_right = end_right - start_right;

    printf("\nResults:\n");
    printf("  Left:  %ld -> %ld (change: %ld)\n", start_left, end_left, delta_left);
    printf("  Right: %ld -> %ld (change: %ld)\n", start_right, end_right, delta_right);

    // Analyze results
    printf("\nAnalysis:\n");

    if (left_pwm > 0 && delta_left > 0) {
        printf("  ✅ Left motor FORWARD: encoder increases (CORRECT)\n");
    } else if (left_pwm > 0 && delta_left < 0) {
        printf("  ❌ Left motor FORWARD: encoder decreases (REVERSED)\n");
        printf("     --> Need to swap motor wires OR invert in config\n");
    } else if (left_pwm < 0 && delta_left < 0) {
        printf("  ✅ Left motor BACKWARD: encoder decreases (CORRECT)\n");
    } else if (left_pwm < 0 && delta_left > 0) {
        printf("  ❌ Left motor BACKWARD: encoder increases (REVERSED)\n");
        printf("     --> Need to swap motor wires OR invert in config\n");
    } else if (left_pwm == 0) {
        if (delta_left == 0) {
            printf("  ✅ Left motor STOPPED: no encoder change (CORRECT)\n");
        } else {
            printf("  ⚠️  Left motor STOPPED but encoder changed: %ld\n", delta_left);
        }
    }

    if (right_pwm > 0 && delta_right > 0) {
        printf("  ✅ Right motor FORWARD: encoder increases (CORRECT)\n");
    } else if (right_pwm > 0 && delta_right < 0) {
        printf("  ❌ Right motor FORWARD: encoder decreases (REVERSED)\n");
        printf("     --> Need to swap motor wires OR invert in config\n");
    } else if (right_pwm < 0 && delta_right < 0) {
        printf("  ✅ Right motor BACKWARD: encoder decreases (CORRECT)\n");
    } else if (right_pwm < 0 && delta_right > 0) {
        printf("  ❌ Right motor BACKWARD: encoder increases (REVERSED)\n");
        printf("     --> Need to swap motor wires OR invert in config\n");
    } else if (right_pwm == 0) {
        if (delta_right == 0) {
            printf("  ✅ Right motor STOPPED: no encoder change (CORRECT)\n");
        } else {
            printf("  ⚠️  Right motor STOPPED but encoder changed: %ld\n", delta_right);
        }
    }

    printf("\nPress any key or wait 2 seconds for next test...\n");
    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 2000) {
        // Wait
    }
}

void MotorDirectionCalibration_Run(void) {
    printf("\n");
    printf("========================================\n");
    printf("  Motor Direction Calibration Tool\n");
    printf("========================================\n");
    printf("\n");
    printf("This tool will test each motor at 20%% PWM\n");
    printf("to verify correct rotation direction.\n");
    printf("\n");
    printf("Test sequence:\n");
    printf("  1. Left motor FORWARD\n");
    printf("  2. Left motor BACKWARD\n");
    printf("  3. Right motor FORWARD\n");
    printf("  4. Right motor BACKWARD\n");
    printf("  5. Both motors FORWARD\n");
    printf("  6. Both motors BACKWARD\n");
    printf("\n");
    printf("Expected behavior:\n");
    printf("  - FORWARD: encoder count should INCREASE (+)\n");
    printf("  - BACKWARD: encoder count should DECREASE (-)\n");
    printf("  - If reversed, we'll note which motor needs fixing\n");
    printf("\n");
    printf("Starting in 3 seconds...\n\n");

    uint32_t wait = PlatformTime_GetMillis();
    while (PlatformTime_GetMillis() - wait < 3000) {
        // Wait
    }

    // Test 1: Left motor forward
    test_motor_direction(CALIB_PWM, 0, "Left Motor FORWARD");

    // Test 2: Left motor backward
    test_motor_direction(-CALIB_PWM, 0, "Left Motor BACKWARD");

    // Test 3: Right motor forward
    test_motor_direction(0, CALIB_PWM, "Right Motor FORWARD");

    // Test 4: Right motor backward
    test_motor_direction(0, -CALIB_PWM, "Right Motor BACKWARD");

    // Test 5: Both motors forward
    test_motor_direction(CALIB_PWM, CALIB_PWM, "Both Motors FORWARD");

    // Test 6: Both motors backward
    test_motor_direction(-CALIB_PWM, -CALIB_PWM, "Both Motors BACKWARD");

    printf("\n");
    printf("========================================\n");
    printf("  Direction Calibration Complete!\n");
    printf("========================================\n");
    printf("\n");
    printf("Summary:\n");
    printf("If any motor direction is reversed:\n");
    printf("\n");
    printf("Option 1: Swap motor wires physically\n");
    printf("  - Swap the two wires on the reversed motor\n");
    printf("  - This is the easiest hardware fix\n");
    printf("\n");
    printf("Option 2: Invert in software (config.c)\n");
    printf("  - Change encoder_directions[] array:\n");
    printf("    static const int8_t encoder_directions[2] = {1, -1};\n");
    printf("  - Change 1 to -1 or -1 to 1 for reversed motor\n");
    printf("\n");
    printf("Recommendation: Use Option 1 (swap wires) for simplicity\n");
    printf("\n");
}
