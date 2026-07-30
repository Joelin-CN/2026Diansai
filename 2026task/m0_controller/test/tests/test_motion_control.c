/**
 * @file test_motion_control.c
 * @brief Motion Control behavior tests
 * @date 2026-07-18
 * 
 * Tests:
 * - Callback validation
 * - Zero velocity command
 * - Negative velocity command (reverse motion)
 * - Emergency stop
 * - Two independent controller instances
 * - PI anti-windup behavior
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* Include Motion Control headers */
#include "../modules/Motion Control/inc/motion_control.h"
#include "../modules/Motion Control/inc/motion_config.h"

/* ============================================================================
 * Test Infrastructure: Fake Encoder and Motor Interfaces
 * ============================================================================ */

static int16_t last_left_pwm = 0;
static int16_t last_right_pwm = 0;
static uint32_t stop_call_count = 0;

static int32_t fake_encoder_counts[ENCODER_COUNT] = {0};

static int32_t FakeEncoder_GetCount(EncoderId_t id) {
    if (id >= ENCODER_COUNT) return 0;
    return fake_encoder_counts[id];
}

static void FakeEncoder_ResetCount(EncoderId_t id) {
    if (id >= ENCODER_COUNT) return;
    fake_encoder_counts[id] = 0;
}

static void FakeMotor_SetDifferentialPWM(int16_t left, int16_t right) {
    last_left_pwm = left;
    last_right_pwm = right;
}

static void FakeMotor_Stop(void) {
    stop_call_count++;
    last_left_pwm = 0;
    last_right_pwm = 0;
}

static void FakeMotor_Init(void) {
    /* No-op for testing */
}

static EncoderInterface_t encoder_vtable = {
    .getCount = FakeEncoder_GetCount,
    .resetCount = FakeEncoder_ResetCount
};

static MotorInterface_t motor_vtable = {
    .setDifferentialPWM = FakeMotor_SetDifferentialPWM,
    .stop = FakeMotor_Stop,
    .init = FakeMotor_Init
};

/* Incomplete vtables for validation testing */
static EncoderInterface_t incomplete_encoder = {
    .getCount = NULL,
    .resetCount = FakeEncoder_ResetCount
};

static MotorInterface_t incomplete_motor = {
    .setDifferentialPWM = NULL,
    .stop = FakeMotor_Stop,
    .init = FakeMotor_Init
};

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static void reset_test_state(void) {
    last_left_pwm = 0;
    last_right_pwm = 0;
    stop_call_count = 0;
    memset(fake_encoder_counts, 0, sizeof(fake_encoder_counts));
}

static void run_cycles(MotionControl_t *ctrl, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        MotionControl_Update(ctrl);
    }
}

/* Simulate encoder counts for constant velocity */
static void simulate_velocity(float v_left_mps, float v_right_mps) {
    /* Convert velocity to encoder delta per control cycle (500 Hz = 0.002s) */
    float dt = CONTROL_PERIOD_S;
    float wheel_circ = 2.0f * 3.14159265f * WHEEL_RADIUS;
    
    /* Delta counts = (velocity * dt) / (wheel_circ) * ENCODER_PPR */
    int32_t delta_left = (int32_t)((v_left_mps * dt / wheel_circ) * ENCODER_PPR);
    int32_t delta_right = (int32_t)((v_right_mps * dt / wheel_circ) * ENCODER_PPR);
    
    fake_encoder_counts[ENCODER_LEFT_FRONT] += delta_left;
    fake_encoder_counts[ENCODER_LEFT_REAR] += delta_left;
    fake_encoder_counts[ENCODER_RIGHT_FRONT] += delta_right;
    fake_encoder_counts[ENCODER_RIGHT_REAR] += delta_right;
}

/* ============================================================================
 * Test Cases
 * ============================================================================ */

static void test_callback_validation(void) {
    printf("Test: Callback validation\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    
    /* Test 1: Missing encoder callback */
    bool result = MotionControl_Init(&ctrl, &incomplete_encoder, &motor_vtable);
    assert(!result && "Init should fail with incomplete encoder vtable");
    
    /* Test 2: Missing motor callback */
    result = MotionControl_Init(&ctrl, &encoder_vtable, &incomplete_motor);
    assert(!result && "Init should fail with incomplete motor vtable");
    
    /* Test 3: Valid callbacks */
    result = MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable);
    assert(result && "Init should succeed with valid callbacks");
    
    printf("  PASS\n");
}

static void test_zero_velocity(void) {
    printf("Test: Zero velocity command\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    MotionControl_SetVelocityCommand(&ctrl, 0.0f, 0.0f);
    run_cycles(&ctrl, 100U);
    
    /* With zero command and zero feedback, PWM should be zero or near-zero */
    assert(abs(last_left_pwm) < 10 && abs(last_right_pwm) < 10 && 
           "Zero command should produce near-zero PWM");
    
    printf("  PASS (left=%d, right=%d)\n", last_left_pwm, last_right_pwm);
}

static void test_negative_velocity(void) {
    printf("Test: Negative velocity (reverse motion)\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    MotionControl_SetVelocityCommand(&ctrl, -0.20f, 0.0f);
    run_cycles(&ctrl, 100U);
    
    /* Expected: both wheels should have negative PWM for reverse motion */
    printf("  Result: left=%d, right=%d\n", last_left_pwm, last_right_pwm);
    assert(last_left_pwm < 0 && last_right_pwm < 0 && 
           "Negative velocity command should produce negative PWM");
    
    printf("  PASS\n");
}

static void test_emergency_stop(void) {
    printf("Test: Emergency stop\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    MotionControl_SetVelocityCommand(&ctrl, 0.5f, 0.0f);
    run_cycles(&ctrl, 10U);
    
    uint32_t stop_count_before = stop_call_count;
    MotionControl_EmergencyStop(&ctrl);
    
    assert(stop_call_count == stop_count_before + 1 && 
           "Emergency stop should call motor stop exactly once");
    assert(MotionControl_GetState(&ctrl) == CONTROL_STATE_EMERGENCY && 
           "State should be EMERGENCY after emergency stop");
    
    printf("  PASS\n");
}

static void test_two_instances(void) {
    printf("Test: Two independent controller instances\n");
    reset_test_state();
    
    MotionControl_t ctrl1, ctrl2;
    assert(MotionControl_Init(&ctrl1, &encoder_vtable, &motor_vtable));
    assert(MotionControl_Init(&ctrl2, &encoder_vtable, &motor_vtable));
    
    /* Start both controllers with different commands */
    MotionControl_SetVelocityCommand(&ctrl1, 0.3f, 0.0f);
    MotionControl_Start(&ctrl1);
    
    MotionControl_SetVelocityCommand(&ctrl2, 0.5f, 0.0f);
    MotionControl_Start(&ctrl2);
    
    /* Run ctrl1 for 200 cycles to let smoothing settle */
    for (uint32_t i = 0; i < 200; i++) {
        MotionControl_Update(&ctrl1);
    }
    int16_t ctrl1_pwm = last_left_pwm;
    
    /* Run ctrl2 for 200 cycles */
    for (uint32_t i = 0; i < 200; i++) {
        MotionControl_Update(&ctrl2);
    }
    int16_t ctrl2_pwm = last_left_pwm;
    
    /* ctrl2 has higher velocity command, should have higher PWM */
    printf("  ctrl1_pwm=%d, ctrl2_pwm=%d\n", ctrl1_pwm, ctrl2_pwm);
    assert(ctrl2_pwm > ctrl1_pwm && 
           "Higher velocity command should produce higher PWM");
    
    /* Verify smoothing states are independent by running ctrl1 again */
    for (uint32_t i = 0; i < 50; i++) {
        MotionControl_Update(&ctrl1);
    }
    int16_t ctrl1_pwm_2nd = last_left_pwm;
    
    /* ctrl1 should continue from its own smoothing state, not ctrl2's */
    /* After settling, ctrl1 should remain close to its previous value */
    printf("  ctrl1_pwm_2nd=%d\n", ctrl1_pwm_2nd);
    assert(abs(ctrl1_pwm_2nd - ctrl1_pwm) < 50 &&
           "Instance smoothing states should be independent");
    
    printf("  PASS\n");
}

static void test_pi_antiwindup(void) {
    printf("Test: PI anti-windup\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    
    /* Apply saturated error for 2 seconds (500 Hz * 2 = 1000 cycles) */
    /* Command high velocity but simulate zero actual velocity */
    MotionControl_SetVelocityCommand(&ctrl, 0.8f, 0.0f);
    for (uint32_t i = 0; i < 1000; i++) {
        simulate_velocity(0.0f, 0.0f);  /* No actual motion */
        MotionControl_Update(&ctrl);
    }
    
    int16_t pwm_saturated = last_left_pwm;
    printf("  PWM after saturation: %d\n", pwm_saturated);
    
    /* Now remove error: command zero velocity and simulate matching velocity */
    MotionControl_SetVelocityCommand(&ctrl, 0.0f, 0.0f);
    for (uint32_t i = 0; i < 100; i++) {
        simulate_velocity(0.0f, 0.0f);
        MotionControl_Update(&ctrl);
    }
    
    int16_t pwm_after_zero = last_left_pwm;
    printf("  PWM after zero command (100 cycles): %d\n", pwm_after_zero);
    
    /* With proper anti-windup, output should return to near-zero */
    /* If integral is wound up, it will take much longer to decay */
    assert(abs(pwm_after_zero) < abs(pwm_saturated) * 0.3f &&
           "Output should decay quickly with proper anti-windup");
    
    printf("  PASS\n");
}

static void test_forward_motion(void) {
    printf("Test: Forward motion\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    MotionControl_SetVelocityCommand(&ctrl, 0.30f, 0.0f);
    run_cycles(&ctrl, 100U);
    
    printf("  Result: left=%d, right=%d\n", last_left_pwm, last_right_pwm);
    assert(last_left_pwm > 0 && last_right_pwm > 0 && 
           "Forward motion should produce positive PWM");
    
    printf("  PASS\n");
}

static void test_turn_in_place(void) {
    printf("Test: Turn in place (pure rotation)\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    /* Pure rotation: v=0, omega > 0 should make left negative, right positive */
    MotionControl_SetVelocityCommand(&ctrl, 0.0f, 2.0f);
    run_cycles(&ctrl, 100U);
    
    printf("  Result: left=%d, right=%d\n", last_left_pwm, last_right_pwm);
    /* Due to kinematics: v_left = v - (b/2)*omega = 0 - 0.075*2 = -0.15 m/s */
    /* v_right = v + (b/2)*omega = 0 + 0.075*2 = +0.15 m/s */
    assert(last_left_pwm < 0 && last_right_pwm > 0 &&
           "Turn in place should produce opposite PWM signs");
    
    printf("  PASS\n");
}

static void test_restart_after_emergency(void) {
    printf("Test: Restart after emergency stop\n");
    reset_test_state();
    
    MotionControl_t ctrl;
    assert(MotionControl_Init(&ctrl, &encoder_vtable, &motor_vtable));
    
    MotionControl_Start(&ctrl);
    MotionControl_SetVelocityCommand(&ctrl, 0.4f, 0.0f);
    run_cycles(&ctrl, 10U);
    
    MotionControl_EmergencyStop(&ctrl);
    assert(MotionControl_GetState(&ctrl) == CONTROL_STATE_EMERGENCY);
    
    /* Must stop before restarting */
    MotionControl_Stop(&ctrl);
    assert(MotionControl_GetState(&ctrl) == CONTROL_STATE_IDLE);
    
    /* Now can restart */
    MotionControl_Start(&ctrl);
    assert(MotionControl_GetState(&ctrl) == CONTROL_STATE_RUNNING);
    
    printf("  PASS\n");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("========================================\n");
    printf("Motion Control Behavior Tests\n");
    printf("========================================\n\n");
    
    test_callback_validation();
    test_zero_velocity();
    test_forward_motion();
    test_negative_velocity();  /* Expected to FAIL initially */
    test_turn_in_place();
    test_emergency_stop();
    test_restart_after_emergency();
    test_two_instances();      /* Expected to FAIL initially */
    test_pi_antiwindup();      /* May FAIL depending on anti-windup implementation */
    
    printf("\n========================================\n");
    printf("All tests PASSED\n");
    printf("========================================\n");
    
    return 0;
}
