#ifndef BALANCE_TARGET_H
#define BALANCE_TARGET_H

#include <stdbool.h>

typedef struct {
    float requested_cm;
    float ramped_cm;
    float max_rate_cm_s;
} BalanceTarget;

void balance_target_init(BalanceTarget *target, float max_rate_cm_s);
bool balance_target_select(BalanceTarget *target, float requested_cm);
float balance_target_step(BalanceTarget *target, float dt_s);
void balance_target_reset(BalanceTarget *target);

#endif
