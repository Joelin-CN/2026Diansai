#include "balance_measurement.h"

#include <math.h>

void balance_measurement_guard_init(BalanceMeasurementGuard *guard,
                                    const BalanceMeasurementConfig *config)
{
    guard->config = *config;
    guard->has_sample = false;
}

BalanceMeasurementResult balance_measurement_accept(BalanceMeasurementGuard *guard,
                                                    const BalanceMeasurement *sample)
{
    if (!sample->valid || !isfinite(sample->position_cm)) {
        return BALANCE_MEASUREMENT_INVALID;
    }
    if (guard->has_sample && sample->sequence == guard->last.sequence) {
        return BALANCE_MEASUREMENT_DUPLICATE;
    }
    if (guard->has_sample
        && sample->sequence - guard->last.sequence > INT32_MAX) {
        return BALANCE_MEASUREMENT_STALE;
    }
    if (sample->position_cm < guard->config.min_position_cm
        || sample->position_cm > guard->config.max_position_cm) {
        return BALANCE_MEASUREMENT_OUT_OF_RANGE;
    }
    if (guard->has_sample
        && fabsf(sample->position_cm - guard->last.position_cm)
               > guard->config.max_jump_cm) {
        return BALANCE_MEASUREMENT_JUMP;
    }

    guard->last = *sample;
    guard->has_sample = true;
    return BALANCE_MEASUREMENT_ACCEPTED;
}

bool balance_measurement_timed_out(const BalanceMeasurementGuard *guard,
                                   uint32_t now_ms)
{
    return guard->has_sample
        && (uint32_t)(now_ms - guard->last.rx_timestamp_ms)
               > guard->config.timeout_ms;
}
