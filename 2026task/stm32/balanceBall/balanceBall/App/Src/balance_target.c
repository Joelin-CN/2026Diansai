#include "balance_target.h"

static float absf(float value) { return value < 0.0f ? -value : value; }

void balance_target_init(BalanceTarget *target, float max_rate_cm_s)
{
    target->max_rate_cm_s = max_rate_cm_s;
    balance_target_reset(target);
}

void balance_target_reset(BalanceTarget *target)
{
    target->requested_cm = 0.0f;
    target->ramped_cm = 0.0f;
}

bool balance_target_select(BalanceTarget *target, float requested_cm)
{
    if (requested_cm != -5.0f && requested_cm != 0.0f && requested_cm != 5.0f) {
        return false;
    }
    target->requested_cm = requested_cm;
    return true;
}

float balance_target_step(BalanceTarget *target, float dt_s)
{
    const float delta = target->requested_cm - target->ramped_cm;
    const float max_delta = target->max_rate_cm_s * dt_s;
    if (absf(delta) <= max_delta) {
        target->ramped_cm = target->requested_cm;
    } else {
        target->ramped_cm += delta > 0.0f ? max_delta : -max_delta;
    }
    return target->ramped_cm;
}
