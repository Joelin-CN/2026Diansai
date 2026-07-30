#ifndef BALANCE_CONTROLLER_H
#define BALANCE_CONTROLLER_H

#include <stdbool.h>
#include "balance_observer.h"

typedef struct {
    float kp;
    float kv;
    float ki;
    float integral_zone_cm;
    float integral_limit;
    float output_limit;
} BalanceControllerConfig;

typedef struct {
    BalanceControllerConfig config;
    float integral;
} BalanceController;

typedef struct {
    float error_cm;
    float raw;
    float limited;
    bool saturated;
} BalanceControlOutput;

void balance_controller_init(BalanceController *controller,
                             const BalanceControllerConfig *config);
void balance_controller_reset(BalanceController *controller);
BalanceControlOutput balance_controller_step(BalanceController *controller,
                                             float target_cm,
                                             const BalanceEstimate *estimate,
                                             float dt_s,
                                             bool allow_integral);

#endif
