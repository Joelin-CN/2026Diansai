#include "balance_observer.h"

void balance_observer_init(BalanceObserver *observer,
                           const BalanceObserverConfig *config)
{
    observer->config = *config;
    balance_observer_reset(observer);
}

void balance_observer_reset(BalanceObserver *observer)
{
    observer->estimate.position_cm = 0.0f;
    observer->estimate.velocity_cm_s = 0.0f;
    observer->last_timestamp_ms = 0U;
    observer->initialized = false;
}

BalanceObserverResult balance_observer_update(BalanceObserver *observer,
                                              uint32_t timestamp_ms,
                                              float position_cm,
                                              BalanceEstimate *estimate)
{
    if (!observer->initialized) {
        observer->estimate.position_cm = position_cm;
        observer->estimate.velocity_cm_s = 0.0f;
        observer->last_timestamp_ms = timestamp_ms;
        observer->initialized = true;
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_RESET;
    }

    const float dt_s = (float)(timestamp_ms - observer->last_timestamp_ms) * 0.001f;
    if (dt_s < observer->config.min_dt_s) {
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_REJECTED;
    }
    if (dt_s > observer->config.max_dt_s) {
        observer->estimate.position_cm = position_cm;
        observer->estimate.velocity_cm_s = 0.0f;
        observer->last_timestamp_ms = timestamp_ms;
        *estimate = observer->estimate;
        return BALANCE_OBSERVER_RESET;
    }

    const float predicted_position = observer->estimate.position_cm
                                   + observer->estimate.velocity_cm_s * dt_s;
    const float residual = position_cm - predicted_position;
    observer->estimate.position_cm = predicted_position
                                  + observer->config.alpha * residual;
    observer->estimate.velocity_cm_s += observer->config.beta * residual / dt_s;
    observer->last_timestamp_ms = timestamp_ms;
    *estimate = observer->estimate;
    return BALANCE_OBSERVER_UPDATED;
}
