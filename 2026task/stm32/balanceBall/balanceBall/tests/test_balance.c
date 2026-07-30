#include <math.h>
#include <stdio.h>
#include "balance_actuator.h"
#include "balance_loop.h"
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

static void test_supervisor_repeated_fault_preserves_first_reason(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_CAMERA_TIMEOUT);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_EMERGENCY_STOP);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT);
}

static void test_supervisor_ignores_none_fault(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_NONE);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_NONE);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_CAMERA_DATA);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_NONE);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_CAMERA_DATA);
}

static void test_supervisor_blocks_normal_transitions_while_faulted(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_raise_fault(&supervisor, BALANCE_FAULT_MOTOR_COMMUNICATION);
    balance_supervisor_confirm_manual_zero(&supervisor);
    CHECK_TRUE(!balance_supervisor_complete_open_loop(&supervisor, true));
    CHECK_TRUE(!balance_supervisor_start_closed_loop(&supervisor));
    balance_supervisor_stop(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_MOTOR_COMMUNICATION);
}

static void test_supervisor_stop_returns_closed_loop_to_ready(void)
{
    BalanceSupervisor supervisor;
    balance_supervisor_init(&supervisor);
    balance_supervisor_confirm_manual_zero(&supervisor);
    (void)balance_supervisor_complete_open_loop(&supervisor, true);
    (void)balance_supervisor_start_closed_loop(&supervisor);
    balance_supervisor_stop(&supervisor);
    CHECK_TRUE(supervisor.state == BALANCE_STATE_READY);
    CHECK_TRUE(supervisor.fault == BALANCE_FAULT_NONE);
}

static BalanceLoopConfig loop_config(void)
{
    const BalanceLoopConfig config = {
        .observer = { .alpha = 1.0f, .beta = 0.2f,
                      .min_dt_s = 0.010f, .max_dt_s = 0.100f },
        .controller = { .kp = 1.0f, .kv = 0.0f, .ki = 0.0f,
                        .integral_zone_cm = 1.0f, .integral_limit = 1.0f,
                        .output_limit = 4.0f },
        .actuator = { .control_sign = 1.0f, .position_limit = 4.0f,
                      .max_delta_per_frame = 1.0f, .speed = 20.0f,
                      .acceleration = 50.0f },
        .measurement = { .min_position_cm = -11.5f, .max_position_cm = 11.5f,
                         .max_jump_cm = 4.0f, .timeout_ms = 100U },
        .target_rate_cm_s = 2.0f,
        .ball_end_zone_cm = 11.0f,
        .saturation_frame_limit = 3U,
        .saturation_error_min_cm = 1.0f,
    };
    return config;
}

static void prepare_loop(BalanceLoop *loop)
{
    balance_loop_confirm_manual_zero(loop);
    CHECK_TRUE(balance_loop_complete_open_loop(loop, true));
}

static void warm_loop(BalanceLoop *loop, BalanceActuatorCommand *command)
{
    const BalanceMeasurement first = {
        .sequence = 1U, .rx_timestamp_ms = 1000U, .valid = true,
        .position_cm = 1.0f,
    };
    const BalanceMeasurement second = {
        .sequence = 2U, .rx_timestamp_ms = 1030U, .valid = true,
        .position_cm = 1.0f,
    };
    CHECK_TRUE(!balance_loop_process_measurement(loop, &first, command));
    CHECK_TRUE(!balance_loop_process_measurement(loop, &second, command));
}

static void test_loop_emits_one_command_per_fresh_frame(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement third = {
        .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true,
        .position_cm = 1.0f,
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    CHECK_TRUE(!balance_loop_start(&loop));
    warm_loop(&loop, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &third, &command));
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &third, &command));
    CHECK_NEAR(command.position, -1.0f, 0.0001f);
}

static void test_loop_faults_on_camera_timeout_and_clears_control_state(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    warm_loop(&loop, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    loop.controller.integral = 0.75f;
    balance_loop_poll(&loop, 1131U);
    CHECK_TRUE(loop.supervisor.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT);
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
    CHECK_TRUE(!loop.camera_ready);
}

static void test_loop_faults_before_commanding_near_tube_end(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement sample = {
        .sequence = 1U, .rx_timestamp_ms = 1000U, .valid = true,
        .position_cm = 11.1f,
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &sample, &command));
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_BALL_END_ZONE);
}

static void test_loop_faults_after_sustained_output_saturation(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceLoopConfig config = loop_config();
    const BalanceMeasurement samples[] = {
        { .sequence = 1U, .rx_timestamp_ms = 1000U, .valid = true, .position_cm = -2.0f },
        { .sequence = 2U, .rx_timestamp_ms = 1030U, .valid = true, .position_cm = -2.0f },
        { .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true, .position_cm = -2.0f },
        { .sequence = 4U, .rx_timestamp_ms = 1090U, .valid = true, .position_cm = -2.0f },
        { .sequence = 5U, .rx_timestamp_ms = 1120U, .valid = true, .position_cm = -2.0f },
    };
    config.controller.kp = 30.0f;
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    (void)balance_loop_process_measurement(&loop, &samples[0], &command);
    (void)balance_loop_process_measurement(&loop, &samples[1], &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &samples[2], &command));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &samples[3], &command));
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &samples[4], &command));
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_OUTPUT_SATURATION);
}

static void test_loop_exposes_last_control_telemetry(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceTelemetry telemetry;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement third = {
        .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true,
        .position_cm = 1.0f,
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    warm_loop(&loop, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &third, &command));
    balance_loop_get_telemetry(&loop, &telemetry);
    CHECK_TRUE(telemetry.valid);
    CHECK_TRUE(telemetry.timestamp_ms == 1060U);
    CHECK_NEAR(telemetry.raw_position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(telemetry.estimate.position_cm, 1.0f, 0.0001f);
    CHECK_NEAR(telemetry.control.error_cm, -1.0f, 0.0001f);
    CHECK_NEAR(telemetry.command.position, -1.0f, 0.0001f);
    CHECK_TRUE(telemetry.state == BALANCE_STATE_CLOSED_LOOP);
}

static void test_loop_stop_clears_control_state_and_requires_fresh_camera_data(void)
{
    BalanceLoop loop;
    const BalanceLoopConfig config = loop_config();
    balance_loop_init(&loop, &config);
    loop.supervisor.state = BALANCE_STATE_CLOSED_LOOP;
    loop.controller.integral = 0.75f;
    loop.camera_ready = true;
    balance_loop_stop(&loop);
    CHECK_TRUE(loop.supervisor.state == BALANCE_STATE_READY);
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
    CHECK_TRUE(!loop.camera_ready);
    CHECK_TRUE(!balance_loop_start(&loop));
}

static void test_loop_rejects_bad_frame_classes_without_advancing_or_emitting(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceTelemetry before;
    BalanceTelemetry after;
    const BalanceLoopConfig config = loop_config();
    BalanceMeasurement sample = {
        .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true,
        .position_cm = 1.0f,
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    warm_loop(&loop, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &sample, &command));
    balance_loop_get_telemetry(&loop, &before);

    CHECK_TRUE(!balance_loop_process_measurement(&loop, &sample, &command));
    sample.sequence = 2U;
    sample.rx_timestamp_ms = 1090U;
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &sample, &command));
    sample.sequence = 4U;
    sample.valid = false;
    CHECK_TRUE(!balance_loop_process_measurement(&loop, &sample, &command));
    balance_loop_get_telemetry(&loop, &after);
    CHECK_TRUE(loop.supervisor.state == BALANCE_STATE_CLOSED_LOOP);
    CHECK_TRUE(loop.measurement.last.sequence == 3U);
    CHECK_TRUE(after.timestamp_ms == before.timestamp_ms);
    CHECK_NEAR(after.command.position, before.command.position, 0.0001f);
}

static void test_loop_target_selection_clears_integral_only_when_accepted(void)
{
    BalanceLoop loop;
    const BalanceLoopConfig config = loop_config();
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    loop.controller.integral = 0.75f;
    CHECK_TRUE(!balance_loop_select_target(&loop, 3.0f));
    CHECK_NEAR(loop.controller.integral, 0.75f, 0.0001f);
    CHECK_TRUE(balance_loop_select_target(&loop, 5.0f));
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
}

static void test_loop_preserves_first_fault_and_resets_control_state(void)
{
    BalanceLoop loop;
    const BalanceLoopConfig config = loop_config();
    balance_loop_init(&loop, &config);
    loop.controller.integral = 0.75f;
    loop.camera_ready = true;
    balance_loop_raise_motor_fault(&loop);
    balance_loop_emergency_stop(&loop);
    CHECK_TRUE(loop.supervisor.fault == BALANCE_FAULT_MOTOR_COMMUNICATION);
    CHECK_NEAR(loop.controller.integral, 0.0f, 0.0001f);
    CHECK_TRUE(!loop.camera_ready);
}

static void test_loop_fault_preserves_last_emitted_telemetry(void)
{
    BalanceLoop loop;
    BalanceActuatorCommand command;
    BalanceTelemetry telemetry;
    const BalanceLoopConfig config = loop_config();
    const BalanceMeasurement third = {
        .sequence = 3U, .rx_timestamp_ms = 1060U, .valid = true,
        .position_cm = 1.0f,
    };
    balance_loop_init(&loop, &config);
    prepare_loop(&loop);
    warm_loop(&loop, &command);
    CHECK_TRUE(balance_loop_start(&loop));
    CHECK_TRUE(balance_loop_process_measurement(&loop, &third, &command));
    balance_loop_raise_motor_fault(&loop);
    balance_loop_get_telemetry(&loop, &telemetry);
    CHECK_TRUE(telemetry.valid);
    CHECK_TRUE(telemetry.timestamp_ms == 1060U);
    CHECK_NEAR(telemetry.command.position, command.position, 0.0001f);
    CHECK_TRUE(telemetry.state == BALANCE_STATE_FAULT);
    CHECK_TRUE(telemetry.fault == BALANCE_FAULT_MOTOR_COMMUNICATION);
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
    test_supervisor_repeated_fault_preserves_first_reason();
    test_supervisor_ignores_none_fault();
    test_supervisor_blocks_normal_transitions_while_faulted();
    test_supervisor_stop_returns_closed_loop_to_ready();
    test_loop_emits_one_command_per_fresh_frame();
    test_loop_faults_on_camera_timeout_and_clears_control_state();
    test_loop_faults_before_commanding_near_tube_end();
    test_loop_faults_after_sustained_output_saturation();
    test_loop_exposes_last_control_telemetry();
    test_loop_stop_clears_control_state_and_requires_fresh_camera_data();
    test_loop_rejects_bad_frame_classes_without_advancing_or_emitting();
    test_loop_target_selection_clears_integral_only_when_accepted();
    test_loop_preserves_first_fault_and_resets_control_state();
    test_loop_fault_preserves_last_emitted_telemetry();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
