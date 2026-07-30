#ifndef BALANCE_ACTUATOR_H
#define BALANCE_ACTUATOR_H

#include <stdbool.h>

typedef struct {
    float control_sign;
    float position_limit;
    float max_delta_per_frame;
    float speed;
    float acceleration;
} BalanceActuatorConfig;

typedef struct {
    BalanceActuatorConfig config;
    float previous_position;
} BalanceActuator;

typedef struct {
    float position;
    float speed;
    float acceleration;
    bool position_limited;
    bool slew_limited;
} BalanceActuatorCommand;

void balance_actuator_init(BalanceActuator *actuator,
                           const BalanceActuatorConfig *config);
void balance_actuator_reset(BalanceActuator *actuator);
BalanceActuatorCommand balance_actuator_limit(BalanceActuator *actuator,
                                              float controller_output);

#endif
