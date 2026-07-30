/**
 * @file      perception.h
 * @brief     Sens-Decision层的感知部分的逻辑封装
 * @author    joelin-CN
 * @version   1.0.0
 * @date      2026-07-14
 * @note      Sens-Decision层的感知部分的逻辑封装
 */
#ifndef PERCEPTION_H
#define PERCEPTION_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "interface.h"

typedef enum {
    ROAD_EVENT_NONE,
    ROAD_EVENT_CURVE_ENTRY,
    ROAD_EVENT_INTERSECTION,
    ROAD_EVENT_LINE_LOST
} road_event_t;

typedef struct {
    float lateral_error;
    float heading_error;
    uint16_t active_mask;
    uint16_t lost_count;
    road_event_t event;
    bool line_valid;
} perception_result_t;

typedef struct {
    float prev_lateral_error;
    uint64_t prev_timestamp_us;
    float heading_error;
    uint16_t lost_count;
    bool initialized;
} perception_t;

void perception_init(perception_t *perception);
sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result);

#endif // PERCEPTION_H