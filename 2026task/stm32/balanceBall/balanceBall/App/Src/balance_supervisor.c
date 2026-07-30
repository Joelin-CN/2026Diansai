#include "balance_supervisor.h"

void balance_supervisor_init(BalanceSupervisor *supervisor)
{
    supervisor->state = BALANCE_STATE_WAIT_MANUAL_ZERO;
    supervisor->fault = BALANCE_FAULT_NONE;
}

void balance_supervisor_confirm_manual_zero(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_WAIT_MANUAL_ZERO) {
        supervisor->state = BALANCE_STATE_OPEN_LOOP_CHECK;
    }
}

bool balance_supervisor_complete_open_loop(BalanceSupervisor *supervisor, bool approved)
{
    if (supervisor->state != BALANCE_STATE_OPEN_LOOP_CHECK || !approved) {
        return false;
    }
    supervisor->state = BALANCE_STATE_READY;
    return true;
}

bool balance_supervisor_start_closed_loop(BalanceSupervisor *supervisor)
{
    if (supervisor->state != BALANCE_STATE_READY) {
        return false;
    }
    supervisor->state = BALANCE_STATE_CLOSED_LOOP;
    return true;
}

void balance_supervisor_stop(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_CLOSED_LOOP) {
        supervisor->state = BALANCE_STATE_READY;
    }
}

void balance_supervisor_raise_fault(BalanceSupervisor *supervisor, BalanceFault fault)
{
    supervisor->fault = fault;
    supervisor->state = BALANCE_STATE_FAULT;
}

void balance_supervisor_acknowledge_fault(BalanceSupervisor *supervisor)
{
    if (supervisor->state == BALANCE_STATE_FAULT) {
        balance_supervisor_init(supervisor);
    }
}
