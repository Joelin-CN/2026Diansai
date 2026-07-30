#include <math.h>
#include <stdio.h>
#include "balance_actuator.h"
#include "balance_controller.h"
#include "balance_measurement.h"
#include "balance_observer.h"
#include "balance_supervisor.h"
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

static BalanceMeasurementConfig measurement_config(void)
{
    const BalanceMeasurementConfig config = {
        .min_position_cm = -11.5f,
        .max_position_cm = 11.5f,
        .max_jump_cm = 4.0f,
        .timeout_ms = 100U,
    };
    return config;
}

static void test_measurement_guard_rejects_invalid_duplicate_and_out_of_range(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = measurement_config();
    BalanceMeasurement sample = {
        .sequence = 1U,
        .rx_timestamp_ms = 1000U,
        .valid = true,
        .position_cm = 0.0f,
    };

    balance_measurement_guard_init(&guard, &config);
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_ACCEPTED);
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_DUPLICATE);
    sample.sequence = 0U;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_STALE);
    sample.sequence = 2U;
    sample.position_cm = 20.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_OUT_OF_RANGE);
    sample.sequence = 3U;
    sample.position_cm = 5.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_JUMP);
    sample.sequence = 4U;
    sample.valid = false;
    sample.position_cm = 0.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_INVALID);
}

static void test_measurement_guard_rejects_non_finite_positions(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = measurement_config();
    BalanceMeasurement sample = {
        .sequence = 1U,
        .rx_timestamp_ms = 1000U,
        .valid = true,
        .position_cm = NAN,
    };

    balance_measurement_guard_init(&guard, &config);
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_INVALID);
    sample.position_cm = INFINITY;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_INVALID);
}

static void test_measurement_sequence_order_is_wrap_safe(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = measurement_config();
    BalanceMeasurement sample = {
        .sequence = UINT32_MAX,
        .rx_timestamp_ms = 1000U,
        .valid = true,
        .position_cm = 0.0f,
    };

    balance_measurement_guard_init(&guard, &config);
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_ACCEPTED);
    sample.sequence = 0U;
    sample.position_cm = 1.0f;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_ACCEPTED);
    sample.sequence = UINT32_MAX;
    CHECK_TRUE(balance_measurement_accept(&guard, &sample)
               == BALANCE_MEASUREMENT_STALE);
}

static void test_measurement_timeout_uses_last_accepted_frame(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = measurement_config();
    BalanceMeasurement sample = {
        .sequence = 1U,
        .rx_timestamp_ms = 1000U,
        .valid = true,
        .position_cm = 0.0f,
    };

    balance_measurement_guard_init(&guard, &config);
    CHECK_TRUE(!balance_measurement_timed_out(&guard, 5000U));
    (void)balance_measurement_accept(&guard, &sample);
    sample.sequence = 2U;
    sample.rx_timestamp_ms = 1090U;
    sample.position_cm = 20.0f;
    (void)balance_measurement_accept(&guard, &sample);
    CHECK_TRUE(!balance_measurement_timed_out(&guard, 1100U));
    CHECK_TRUE(balance_measurement_timed_out(&guard, 1101U));
}

static void test_measurement_timeout_is_timestamp_wrap_safe(void)
{
    BalanceMeasurementGuard guard;
    const BalanceMeasurementConfig config = measurement_config();
    const BalanceMeasurement sample = {
        .sequence = 1U,
        .rx_timestamp_ms = UINT32_MAX - 49U,
        .valid = true,
        .position_cm = 0.0f,
    };

    balance_measurement_guard_init(&guard, &config);
    (void)balance_measurement_accept(&guard, &sample);
    CHECK_TRUE(!balance_measurement_timed_out(&guard, 50U));
    CHECK_TRUE(balance_measurement_timed_out(&guard, 51U));
}

static void test_supervisor_requires_zero_and_open_loop_approval(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO);
    CHECK_TRUE(!balance_supervisor_start_closed_loop(&supervisor));
    balance_supervisor_confirm_manual_zero(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_OPEN_LOOP_CHECK);
    CHECK_TRUE(!balance_supervisor_complete_open_loop(&supervisor, false));
    CHECK_TRUE(balance_supervisor_complete_open_loop(&supervisor, true));
    CHECK_TRUE(supervisor.state == BALANCE_STATE_READY);
    CHECK_TRUE(balance_supervisor_start_closed_loop(&supervisor));
    CHECK_TRUE(supervisor.state == BALANCE_STATE_CLOSED_LOOP);
}

static void test_supervisor_fault_requires_new_manual_zero(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_CAMERA_TIMEOUT);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT);
    balance_supervisor_acknowledge_fault(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_NONE);
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
    test_measurement_guard_rejects_invalid_duplicate_and_out_of_range();
    test_measurement_guard_rejects_non_finite_positions();
    test_measurement_sequence_order_is_wrap_safe();
    test_measurement_timeout_uses_last_accepted_frame();
    test_measurement_timeout_is_timestamp_wrap_safe();
    test_supervisor_requires_zero_and_open_loop_approval();
    test_supervisor_fault_requires_new_manual_zero();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
