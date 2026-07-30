#ifndef BALANCE_OBSERVER_H
#define BALANCE_OBSERVER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float alpha;
    float beta;
    float min_dt_s;
    float max_dt_s;
} BalanceObserverConfig;

typedef struct {
    float position_cm;
    float velocity_cm_s;
} BalanceEstimate;

typedef enum {
    BALANCE_OBSERVER_REJECTED = 0,
    BALANCE_OBSERVER_RESET,
    BALANCE_OBSERVER_UPDATED,
} BalanceObserverResult;

typedef struct {
    BalanceObserverConfig config;
    BalanceEstimate estimate;
    uint32_t last_timestamp_ms;
    bool initialized;
} BalanceObserver;

void balance_observer_init(BalanceObserver *observer,
                           const BalanceObserverConfig *config);
void balance_observer_reset(BalanceObserver *observer);
BalanceObserverResult balance_observer_update(BalanceObserver *observer,
                                              uint32_t timestamp_ms,
                                              float position_cm,
                                              BalanceEstimate *estimate);

#endif
