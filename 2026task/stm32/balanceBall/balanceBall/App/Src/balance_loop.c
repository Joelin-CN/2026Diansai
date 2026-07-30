#include "balance_loop.h"

#include <math.h>

static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void reset_saturation_monitor(BalanceLoop *loop)
{
    loop->saturation_frames = 0U;
    loop->previous_saturation_abs_error_cm = 0.0f;
    loop->has_saturation_error = false;
}

static void reset_logical_state(BalanceLoop *loop)
{
    balance_observer_reset(&loop->observer);
    balance_controller_reset(&loop->controller);
    balance_target_reset(&loop->target);
    reset_saturation_monitor(loop);
    loop->camera_ready = false;
    loop->telemetry.active = false;
    if (!loop->telemetry.valid) {
        loop->telemetry = (BalanceTelemetry){0};
    }
}

static void reset_to_software_zero(BalanceLoop *loop)
{
    reset_logical_state(loop);
    balance_actuator_reset(&loop->actuator);
}

static bool config_is_valid(const BalanceLoopConfig *config)
{
    return isfinite(config->observer.alpha)
        && isfinite(config->observer.beta)
        && isfinite(config->observer.min_dt_s)
        && isfinite(config->observer.max_dt_s)
        && isfinite(config->controller.kp)
        && isfinite(config->controller.kv)
        && isfinite(config->controller.ki)
        && isfinite(config->controller.integral_zone_cm)
        && isfinite(config->controller.integral_limit)
        && isfinite(config->controller.output_limit)
        && isfinite(config->actuator.control_sign)
        && isfinite(config->actuator.position_limit)
        && isfinite(config->actuator.max_delta_per_frame)
        && isfinite(config->actuator.speed)
        && isfinite(config->actuator.acceleration)
        && isfinite(config->measurement.min_position_cm)
        && isfinite(config->measurement.max_position_cm)
        && isfinite(config->measurement.max_jump_cm)
        && isfinite(config->target_rate_cm_s)
        && isfinite(config->ball_end_zone_cm)
        && isfinite(config->saturation_error_min_cm)
        && (config->actuator.control_sign == 1.0f
            || config->actuator.control_sign == -1.0f);
}

static bool config_ranges_are_valid(const BalanceLoopConfig *config)
{
    const float negative_extent = -config->measurement.min_position_cm;
    const float symmetric_extent =
        negative_extent < config->measurement.max_position_cm
            ? negative_extent : config->measurement.max_position_cm;

    return config->observer.alpha >= 0.0f && config->observer.alpha <= 1.0f
        && config->observer.beta >= 0.0f && config->observer.beta <= 1.0f
        && config->observer.min_dt_s > 0.0f
        && config->observer.max_dt_s >= config->observer.min_dt_s
        && config->measurement.min_position_cm
               < config->measurement.max_position_cm
        && config->measurement.min_position_cm <= -5.0f
        && config->measurement.max_position_cm >= 5.0f
        && config->measurement.max_jump_cm > 0.0f
        && config->measurement.timeout_ms > 0U
        && config->target_rate_cm_s > 0.0f
        && config->ball_end_zone_cm > 5.0f
        && config->ball_end_zone_cm <= symmetric_extent
        && config->actuator.position_limit > 0.0f
        && config->actuator.max_delta_per_frame > 0.0f
        && config->controller.integral_limit > 0.0f
        && config->controller.output_limit > 0.0f
        && config->controller.kp >= 0.0f
        && config->controller.kv >= 0.0f
        && config->controller.ki >= 0.0f
        && config->controller.integral_zone_cm >= 0.0f
        && config->actuator.speed >= 0.0f
        && config->actuator.acceleration >= 0.0f
        && config->saturation_error_min_cm >= 0.0f;
}

static void raise_fault(BalanceLoop *loop, BalanceFault fault)
{
    balance_supervisor_raise_fault(&loop->supervisor, fault);
    reset_to_software_zero(loop);
}

void balance_loop_init(BalanceLoop *loop, const BalanceLoopConfig *config)
{
    loop->config = *config;
    balance_observer_init(&loop->observer, &config->observer);
    balance_controller_init(&loop->controller, &config->controller);
    balance_actuator_init(&loop->actuator, &config->actuator);
    balance_measurement_guard_init(&loop->measurement, &config->measurement);
    balance_supervisor_init(&loop->supervisor);
    balance_target_init(&loop->target, config->target_rate_cm_s);
    reset_saturation_monitor(loop);
    loop->camera_ready = false;
    loop->config_valid = config_is_valid(config) && config_ranges_are_valid(config);
    loop->telemetry = (BalanceTelemetry){0};
    if (!loop->config_valid) {
        balance_supervisor_raise_fault(&loop->supervisor,
                                       BALANCE_FAULT_INVALID_CONFIGURATION);
    }
}

void balance_loop_confirm_manual_zero(BalanceLoop *loop)
{
    reset_to_software_zero(loop);
    balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    balance_supervisor_confirm_manual_zero(&loop->supervisor);
}

bool balance_loop_complete_open_loop(BalanceLoop *loop, bool approved)
{
    return balance_supervisor_complete_open_loop(&loop->supervisor, approved);
}

bool balance_loop_start(BalanceLoop *loop)
{
    if (!loop->camera_ready) {
        return false;
    }
    return balance_supervisor_start_closed_loop(&loop->supervisor);
}

void balance_loop_stop(BalanceLoop *loop)
{
    balance_supervisor_stop(&loop->supervisor);
    if (loop->supervisor.state == BALANCE_STATE_READY) {
        reset_logical_state(loop);
        balance_measurement_guard_init(&loop->measurement,
                                       &loop->config.measurement);
    }
}

bool balance_loop_select_target(BalanceLoop *loop, float target_cm)
{
    if (loop->supervisor.state != BALANCE_STATE_READY
        && loop->supervisor.state != BALANCE_STATE_CLOSED_LOOP) {
        return false;
    }
    if (!balance_target_select(&loop->target, target_cm)) {
        return false;
    }
    balance_controller_reset(&loop->controller);
    reset_saturation_monitor(loop);
    return true;
}

bool balance_loop_process_measurement(BalanceLoop *loop,
                                      const BalanceMeasurement *measurement,
                                      BalanceActuatorCommand *command)
{
    BalanceEstimate estimate;
    BalanceControlOutput control;
    BalanceMeasurementResult accepted;
    BalanceObserverResult observer_result;
    BalanceMeasurementGuard candidate_guard;
    uint32_t previous_timestamp_ms;
    float dt_s;
    float target_cm;
    float previous_integral;

    if (loop->supervisor.state != BALANCE_STATE_READY
        && loop->supervisor.state != BALANCE_STATE_CLOSED_LOOP) {
        return false;
    }

    candidate_guard = loop->measurement;
    accepted = balance_measurement_accept(&candidate_guard, measurement);
    if (accepted != BALANCE_MEASUREMENT_ACCEPTED) {
        if (accepted == BALANCE_MEASUREMENT_OUT_OF_RANGE
            || accepted == BALANCE_MEASUREMENT_JUMP) {
            raise_fault(loop, BALANCE_FAULT_CAMERA_DATA);
        }
        return false;
    }
    if (loop->supervisor.state == BALANCE_STATE_CLOSED_LOOP
        && loop->measurement.has_sample
        && (uint32_t)(measurement->rx_timestamp_ms
                      - loop->measurement.last.rx_timestamp_ms)
               > loop->config.measurement.timeout_ms) {
        raise_fault(loop, BALANCE_FAULT_CAMERA_TIMEOUT);
        return false;
    }
    loop->measurement = candidate_guard;
    if (absf(measurement->position_cm) >= loop->config.ball_end_zone_cm) {
        raise_fault(loop, BALANCE_FAULT_BALL_END_ZONE);
        return false;
    }

    previous_timestamp_ms = loop->observer.last_timestamp_ms;
    observer_result = balance_observer_update(&loop->observer,
                                              measurement->rx_timestamp_ms,
                                              measurement->position_cm,
                                              &estimate);
    if (observer_result == BALANCE_OBSERVER_RESET) {
        balance_controller_reset(&loop->controller);
        reset_saturation_monitor(loop);
        if (loop->supervisor.state == BALANCE_STATE_READY) {
            loop->camera_ready = false;
        }
    }
    if (observer_result != BALANCE_OBSERVER_UPDATED) {
        return false;
    }
    loop->camera_ready = true;
    if (loop->supervisor.state == BALANCE_STATE_READY) {
        return false;
    }

    dt_s = (float)(measurement->rx_timestamp_ms - previous_timestamp_ms) * 0.001f;
    target_cm = balance_target_step(&loop->target, dt_s);
    previous_integral = loop->controller.integral;
    control = balance_controller_step(&loop->controller, target_cm, &estimate,
                                       dt_s, true);
    *command = balance_actuator_limit(&loop->actuator, control.limited);
    if (command->position_limited) {
        const float integral_actuator_delta = loop->config.actuator.control_sign
            * loop->config.controller.ki
            * (loop->controller.integral - previous_integral);
        const float saturated_actuator_direction =
            loop->config.actuator.control_sign * control.limited;
        if (integral_actuator_delta * saturated_actuator_direction > 0.0f) {
            loop->controller.integral = previous_integral;
        }
    }

    if (loop->config.saturation_frame_limit > 0U) {
        const float abs_error = absf(control.error_cm);
        const bool saturated = control.saturated || command->position_limited;
        if (saturated && abs_error >= loop->config.saturation_error_min_cm) {
            if (loop->has_saturation_error
                && abs_error >= loop->previous_saturation_abs_error_cm) {
                loop->saturation_frames++;
            } else {
                loop->saturation_frames = 0U;
            }
            loop->previous_saturation_abs_error_cm = abs_error;
            loop->has_saturation_error = true;
        } else {
            reset_saturation_monitor(loop);
        }
        if (loop->saturation_frames >= loop->config.saturation_frame_limit) {
            raise_fault(loop, BALANCE_FAULT_OUTPUT_SATURATION);
            return false;
        }
    } else {
        reset_saturation_monitor(loop);
    }

    loop->telemetry.valid = true;
    loop->telemetry.active = true;
    loop->telemetry.timestamp_ms = measurement->rx_timestamp_ms;
    loop->telemetry.raw_position_cm = measurement->position_cm;
    loop->telemetry.target_cm = target_cm;
    loop->telemetry.estimate = estimate;
    loop->telemetry.control = control;
    loop->telemetry.command = *command;
    loop->telemetry.state = loop->supervisor.state;
    loop->telemetry.fault = loop->supervisor.fault;
    return true;
}

void balance_loop_poll(BalanceLoop *loop, uint32_t now_ms)
{
    if (balance_measurement_timed_out(&loop->measurement, now_ms)) {
        if (loop->supervisor.state == BALANCE_STATE_CLOSED_LOOP) {
            raise_fault(loop, BALANCE_FAULT_CAMERA_TIMEOUT);
        } else if (loop->supervisor.state == BALANCE_STATE_READY) {
            balance_observer_reset(&loop->observer);
            balance_controller_reset(&loop->controller);
            reset_saturation_monitor(loop);
            loop->camera_ready = false;
        }
    }
}

void balance_loop_raise_motor_fault(BalanceLoop *loop)
{
    raise_fault(loop, BALANCE_FAULT_MOTOR_COMMUNICATION);
}

void balance_loop_emergency_stop(BalanceLoop *loop)
{
    raise_fault(loop, BALANCE_FAULT_EMERGENCY_STOP);
}

void balance_loop_acknowledge_fault(BalanceLoop *loop)
{
    reset_to_software_zero(loop);
    balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    if (loop->config_valid) {
        balance_supervisor_acknowledge_fault(&loop->supervisor);
    }
}

void balance_loop_get_telemetry(const BalanceLoop *loop,
                                BalanceTelemetry *telemetry)
{
    *telemetry = loop->telemetry;
    telemetry->state = loop->supervisor.state;
    telemetry->fault = loop->supervisor.fault;
}
