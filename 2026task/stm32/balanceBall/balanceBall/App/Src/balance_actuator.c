#include "balance_actuator.h"

static float clampf(float value, float limit)
{
    if (value > limit) return limit;
    if (value < -limit) return -limit;
    return value;
}

void balance_actuator_init(BalanceActuator *actuator,
                           const BalanceActuatorConfig *config)
{
    actuator->config = *config;
    balance_actuator_reset(actuator);
}

void balance_actuator_reset(BalanceActuator *actuator)
{
    actuator->previous_position = 0.0f;
}

BalanceActuatorCommand balance_actuator_limit(BalanceActuator *actuator,
                                              float controller_output)
{
    BalanceActuatorCommand command = {0};
    const float signed_target = actuator->config.control_sign * controller_output;
    const float bounded_target = clampf(signed_target, actuator->config.position_limit);
    const float delta = bounded_target - actuator->previous_position;
    const float bounded_delta = clampf(delta, actuator->config.max_delta_per_frame);
    command.position = actuator->previous_position + bounded_delta;
    command.speed = actuator->config.speed;
    command.acceleration = actuator->config.acceleration;
    command.position_limited = signed_target != bounded_target;
    command.slew_limited = delta != bounded_delta;
    actuator->previous_position = command.position;
    return command;
}
