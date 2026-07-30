#include <math.h>
#include <stdio.h>
#include "balance_actuator.h"
#include "balance_controller.h"
#include "balance_observer.h"
#include "balance_target.h"

static int failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

#define CHECK_NEAR(actual, expected, tolerance) \
    CHECK_TRUE(fabsf((actual) - (expected)) <= (tolerance))

static void test_observer_initializes_from_first_sample(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 0.75f,
        .beta = 0.20f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    CHECK_TRUE(balance_observer_update(&observer, 1000U, 2.5f, &estimate)
               == BALANCE_OBSERVER_RESET);
    CHECK_NEAR(estimate.position_cm, 2.5f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 0.0f, 0.0001f);
}

static void test_observer_uses_actual_frame_interval(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 1.0f,
        .beta = 0.5f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    balance_observer_update(&observer, 1000U, 0.0f, &estimate);
    CHECK_TRUE(balance_observer_update(&observer, 1040U, 1.0f, &estimate)
               == BALANCE_OBSERVER_UPDATED);
    CHECK_NEAR(estimate.position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 12.5f, 0.001f);
}

static void test_observer_rejects_duplicate_and_resets_after_gap(void)
{
    BalanceObserver observer;
    BalanceEstimate estimate;
    const BalanceObserverConfig config = {
        .alpha = 0.75f,
        .beta = 0.20f,
        .min_dt_s = 0.010f,
        .max_dt_s = 0.100f,
    };

    balance_observer_init(&observer, &config);
    balance_observer_update(&observer, 1000U, 0.0f, &estimate);
    CHECK_TRUE(balance_observer_update(&observer, 1005U, 1.0f, &estimate)
               == BALANCE_OBSERVER_REJECTED);
    CHECK_TRUE(balance_observer_update(&observer, 1200U, 4.0f, &estimate)
               == BALANCE_OBSERVER_RESET);
    CHECK_NEAR(estimate.position_cm, 4.0f, 0.0001f);
    CHECK_NEAR(estimate.velocity_cm_s, 0.0f, 0.0001f);
}

static void test_target_rejects_non_competition_position(void)
{
    BalanceTarget target;
    balance_target_init(&target, 2.0f);
    CHECK_TRUE(!balance_target_select(&target, 3.0f));
    CHECK_NEAR(target.requested_cm, 0.0f, 0.0001f);
}

static void test_target_ramps_without_overshoot(void)
{
    BalanceTarget target;
    balance_target_init(&target, 2.0f);
    CHECK_TRUE(balance_target_select(&target, 5.0f));
    CHECK_NEAR(balance_target_step(&target, 0.5f), 1.0f, 0.0001f);
    CHECK_NEAR(balance_target_step(&target, 2.0f), 5.0f, 0.0001f);
    CHECK_NEAR(balance_target_step(&target, 0.5f), 5.0f, 0.0001f);
}

static BalanceControllerConfig controller_config(void)
{
    const BalanceControllerConfig config = {
        .kp = 2.0f,
        .kv = 0.5f,
        .ki = 1.0f,
        .integral_zone_cm = 1.0f,
        .integral_limit = 2.0f,
        .output_limit = 10.0f,
    };
    return config;
}

static void test_controller_uses_velocity_as_damping(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    const BalanceControllerConfig config = controller_config();
    const BalanceEstimate estimate = { .position_cm = 1.0f, .velocity_cm_s = 2.0f };
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 0.03f, false);
    CHECK_NEAR(output.error_cm, -1.0f, 0.0001f);
    CHECK_NEAR(output.limited, -3.0f, 0.0001f);
}

static void test_controller_integrates_only_inside_zone(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    const BalanceControllerConfig config = controller_config();
    BalanceEstimate estimate = { .position_cm = -0.5f, .velocity_cm_s = 0.0f };
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_NEAR(controller.integral, 0.5f, 0.0001f);
    estimate.position_cm = -2.0f;
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_NEAR(controller.integral, 0.5f, 0.0001f);
    (void)output;
}

static void test_controller_does_not_wind_up_into_saturation(void)
{
    BalanceController controller;
    BalanceControlOutput output;
    BalanceControllerConfig config = controller_config();
    const BalanceEstimate estimate = { .position_cm = -0.5f, .velocity_cm_s = 0.0f };
    config.kp = 30.0f;
    balance_controller_init(&controller, &config);
    output = balance_controller_step(&controller, 0.0f, &estimate, 1.0f, true);
    CHECK_TRUE(output.saturated);
    CHECK_NEAR(controller.integral, 0.0f, 0.0001f);
    CHECK_NEAR(output.limited, 10.0f, 0.0001f);
}

static void test_actuator_applies_direction_absolute_and_slew_limits(void)
{
    BalanceActuator actuator;
    BalanceActuatorCommand command;
    const BalanceActuatorConfig config = {
        .control_sign = -1.0f,
        .position_limit = 4.0f,
        .max_delta_per_frame = 1.0f,
        .speed = 20.0f,
        .acceleration = 50.0f,
    };

    balance_actuator_init(&actuator, &config);
    command = balance_actuator_limit(&actuator, 10.0f);
    CHECK_NEAR(command.position, -1.0f, 0.0001f);
    CHECK_TRUE(command.position_limited);
    CHECK_TRUE(command.slew_limited);
    CHECK_NEAR(command.speed, 20.0f, 0.0001f);
    CHECK_NEAR(command.acceleration, 50.0f, 0.0001f);
    command = balance_actuator_limit(&actuator, 10.0f);
    CHECK_NEAR(command.position, -2.0f, 0.0001f);
}

static void test_actuator_reset_returns_command_state_to_zero(void)
{
    BalanceActuator actuator;
    const BalanceActuatorConfig config = {
        .control_sign = 1.0f, .position_limit = 4.0f,
        .max_delta_per_frame = 1.0f, .speed = 20.0f, .acceleration = 50.0f,
    };
    balance_actuator_init(&actuator, &config);
    (void)balance_actuator_limit(&actuator, 3.0f);
    balance_actuator_reset(&actuator);
    CHECK_NEAR(actuator.previous_position, 0.0f, 0.0001f);
}

int main(void)
{
    test_observer_initializes_from_first_sample();
    test_observer_uses_actual_frame_interval();
    test_observer_rejects_duplicate_and_resets_after_gap();
    test_target_rejects_non_competition_position();
    test_target_ramps_without_overshoot();
    test_controller_uses_velocity_as_damping();
    test_controller_integrates_only_inside_zone();
    test_controller_does_not_wind_up_into_saturation();
    test_actuator_applies_direction_absolute_and_slew_limits();
    test_actuator_reset_returns_command_state_to_zero();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
