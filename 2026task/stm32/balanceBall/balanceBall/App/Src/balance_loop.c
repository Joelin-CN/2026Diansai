#include "balance_loop.h"

static float absf(float value)
{
    return value < 0.0f ? -value : value;
}

static void reset_control_state(BalanceLoop *loop)
{
    balance_observer_reset(&loop->observer);
    balance_controller_reset(&loop->controller);
    balance_actuator_reset(&loop->actuator);
    balance_target_reset(&loop->target);
    loop->saturation_frames = 0U;
    loop->camera_ready = false;
}

static void raise_fault(BalanceLoop *loop, BalanceFault fault)
{
    balance_supervisor_raise_fault(&loop->supervisor, fault);
    reset_control_state(loop);
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
    loop->saturation_frames = 0U;
    loop->camera_ready = false;
    loop->telemetry.valid = false;
}

void balance_loop_confirm_manual_zero(BalanceLoop *loop)
{
    reset_control_state(loop);
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
        reset_control_state(loop);
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
    uint32_t previous_timestamp_ms;
    float dt_s;
    float target_cm;

    if (loop->supervisor.state != BALANCE_STATE_READY
        && loop->supervisor.state != BALANCE_STATE_CLOSED_LOOP) {
        return false;
    }

    accepted = balance_measurement_accept(&loop->measurement, measurement);
    if (accepted != BALANCE_MEASUREMENT_ACCEPTED) {
        if (accepted == BALANCE_MEASUREMENT_OUT_OF_RANGE
            || accepted == BALANCE_MEASUREMENT_JUMP) {
            raise_fault(loop, BALANCE_FAULT_CAMERA_DATA);
        }
        return false;
    }
    if (absf(measurement->position_cm) >= loop->config.ball_end_zone_cm) {
        raise_fault(loop, BALANCE_FAULT_BALL_END_ZONE);
        return false;
    }

    previous_timestamp_ms = loop->observer.last_timestamp_ms;
    observer_result = balance_observer_update(&loop->observer,
                                              measurement->rx_timestamp_ms,
                                              measurement->position_cm,
                                              &estimate);
    if (observer_result != BALANCE_OBSERVER_UPDATED) {
        return false;
    }
    loop->camera_ready = true;
    if (loop->supervisor.state == BALANCE_STATE_READY) {
        return false;
    }

    dt_s = (float)(measurement->rx_timestamp_ms - previous_timestamp_ms) * 0.001f;
    target_cm = balance_target_step(&loop->target, dt_s);
    control = balance_controller_step(&loop->controller, target_cm, &estimate,
                                      dt_s, true);
    *command = balance_actuator_limit(&loop->actuator, control.limited);

    if ((control.saturated || command->position_limited)
        && absf(control.error_cm) >= loop->config.saturation_error_min_cm) {
        loop->saturation_frames++;
    } else {
        loop->saturation_frames = 0U;
    }
    if (loop->saturation_frames >= loop->config.saturation_frame_limit) {
        raise_fault(loop, BALANCE_FAULT_OUTPUT_SATURATION);
        return false;
    }

    loop->telemetry.valid = true;
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
    if (loop->supervisor.state == BALANCE_STATE_CLOSED_LOOP
        && balance_measurement_timed_out(&loop->measurement, now_ms)) {
        raise_fault(loop, BALANCE_FAULT_CAMERA_TIMEOUT);
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
    reset_control_state(loop);
    balance_measurement_guard_init(&loop->measurement, &loop->config.measurement);
    balance_supervisor_acknowledge_fault(&loop->supervisor);
}

void balance_loop_get_telemetry(const BalanceLoop *loop,
                                BalanceTelemetry *telemetry)
{
    *telemetry = loop->telemetry;
    telemetry->state = loop->supervisor.state;
    telemetry->fault = loop->supervisor.fault;
}
