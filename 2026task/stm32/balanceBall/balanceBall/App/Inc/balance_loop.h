#ifndef BALANCE_LOOP_H
#define BALANCE_LOOP_H

#include "balance_actuator.h"
#include "balance_controller.h"
#include "balance_measurement.h"
#include "balance_observer.h"
#include "balance_supervisor.h"
#include "balance_target.h"

typedef struct {
    BalanceObserverConfig observer;
    BalanceControllerConfig controller;
    BalanceActuatorConfig actuator;
    BalanceMeasurementConfig measurement;
    float target_rate_cm_s;
    float ball_end_zone_cm;
    uint16_t saturation_frame_limit;
    float saturation_error_min_cm;
} BalanceLoopConfig;

typedef struct {
    bool valid;
    bool active;
    uint32_t timestamp_ms;
    float raw_position_cm;
    float target_cm;
    BalanceEstimate estimate;
    BalanceControlOutput control;
    BalanceActuatorCommand command;
    BalanceState state;
    BalanceFault fault;
} BalanceTelemetry;

typedef struct {
    BalanceLoopConfig config;
    BalanceObserver observer;
    BalanceController controller;
    BalanceActuator actuator;
    BalanceMeasurementGuard measurement;
    BalanceSupervisor supervisor;
    BalanceTarget target;
    uint16_t saturation_frames;
    float previous_saturation_abs_error_cm;
    bool has_saturation_error;
    bool camera_ready;
    bool config_valid;
    BalanceTelemetry telemetry;
} BalanceLoop;

void balance_loop_init(BalanceLoop *loop, const BalanceLoopConfig *config);
void balance_loop_confirm_manual_zero(BalanceLoop *loop);
bool balance_loop_complete_open_loop(BalanceLoop *loop, bool approved);
bool balance_loop_start(BalanceLoop *loop);
void balance_loop_stop(BalanceLoop *loop);
bool balance_loop_select_target(BalanceLoop *loop, float target_cm);
bool balance_loop_process_measurement(BalanceLoop *loop,
                                      const BalanceMeasurement *measurement,
                                      BalanceActuatorCommand *command);
void balance_loop_poll(BalanceLoop *loop, uint32_t now_ms);
void balance_loop_raise_motor_fault(BalanceLoop *loop);
void balance_loop_emergency_stop(BalanceLoop *loop);
void balance_loop_acknowledge_fault(BalanceLoop *loop);
void balance_loop_get_telemetry(const BalanceLoop *loop,
                                BalanceTelemetry *telemetry);

#endif
