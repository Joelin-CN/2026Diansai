#ifndef BALANCE_SUPERVISOR_H
#define BALANCE_SUPERVISOR_H

#include <stdbool.h>

typedef enum {
    BALANCE_STATE_WAIT_MANUAL_ZERO = 0,
    BALANCE_STATE_OPEN_LOOP_CHECK,
    BALANCE_STATE_READY,
    BALANCE_STATE_CLOSED_LOOP,
    BALANCE_STATE_FAULT,
} BalanceState;

typedef enum {
    BALANCE_FAULT_NONE = 0,
    BALANCE_FAULT_CAMERA_TIMEOUT,
    BALANCE_FAULT_CAMERA_DATA,
    BALANCE_FAULT_MOTOR_COMMUNICATION,
    BALANCE_FAULT_OUTPUT_SATURATION,
    BALANCE_FAULT_BALL_END_ZONE,
    BALANCE_FAULT_EMERGENCY_STOP,
    BALANCE_FAULT_INVALID_CONFIGURATION,
} BalanceFault;

typedef struct {
    BalanceState state;
    BalanceFault fault;
} BalanceSupervisor;

void balance_supervisor_init(BalanceSupervisor *supervisor);
void balance_supervisor_confirm_manual_zero(BalanceSupervisor *supervisor);
bool balance_supervisor_complete_open_loop(BalanceSupervisor *supervisor, bool approved);
bool balance_supervisor_start_closed_loop(BalanceSupervisor *supervisor);
void balance_supervisor_stop(BalanceSupervisor *supervisor);
void balance_supervisor_raise_fault(BalanceSupervisor *supervisor, BalanceFault fault);
void balance_supervisor_acknowledge_fault(BalanceSupervisor *supervisor);

#endif
