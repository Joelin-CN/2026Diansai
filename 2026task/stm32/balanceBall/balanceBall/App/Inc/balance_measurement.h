#ifndef BALANCE_MEASUREMENT_H
#define BALANCE_MEASUREMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t sequence;
    uint32_t rx_timestamp_ms;
    bool valid;
    float position_cm;
} BalanceMeasurement;

typedef struct {
    float min_position_cm;
    float max_position_cm;
    float max_jump_cm;
    uint32_t timeout_ms;
} BalanceMeasurementConfig;

typedef enum {
    BALANCE_MEASUREMENT_ACCEPTED = 0,
    BALANCE_MEASUREMENT_INVALID,
    BALANCE_MEASUREMENT_DUPLICATE,
    BALANCE_MEASUREMENT_STALE,
    BALANCE_MEASUREMENT_OUT_OF_RANGE,
    BALANCE_MEASUREMENT_JUMP,
} BalanceMeasurementResult;

typedef struct {
    BalanceMeasurementConfig config;
    BalanceMeasurement last;
    bool has_sample;
} BalanceMeasurementGuard;

void balance_measurement_guard_init(BalanceMeasurementGuard *guard,
                                    const BalanceMeasurementConfig *config);
BalanceMeasurementResult balance_measurement_accept(BalanceMeasurementGuard *guard,
                                                    const BalanceMeasurement *sample);
bool balance_measurement_timed_out(const BalanceMeasurementGuard *guard,
                                   uint32_t now_ms);

#endif
