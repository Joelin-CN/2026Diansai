"""
Perception module - Python port of perception.c
Converts IR sensor data to lateral/heading error and road events.
"""

import numpy as np
from enum import IntEnum

# Parameters from config.c: sd_config_reset_defaults()
IR_WEIGHTS = np.array([-7.0, -5.0, -3.0, -1.0, 1.0, 3.0, 5.0, 7.0], dtype=np.float32)
IR_THRESHOLD = 0.5                    # Reflectance threshold for "on line"
HEADING_FILTER_ALPHA = 0.3            # IIR filter coefficient
CURVE_ERROR_THRESHOLD = 0.45          # Lateral error threshold for curve detection
CURVE_DERIVATIVE_THRESHOLD = 1.5      # Heading error threshold for curve detection
INTERSECTION_ACTIVE_CHANNELS = 4      # Min active channels for intersection


class RoadEvent(IntEnum):
    """Road event types (from perception.h)"""
    ROAD_EVENT_NONE = 0
    ROAD_EVENT_CURVE_ENTRY = 1
    ROAD_EVENT_INTERSECTION = 2
    ROAD_EVENT_LINE_LOST = 3


class PerceptionState:
    """Internal state for perception module"""
    def __init__(self):
        self.initialized = False
        self.prev_lateral_error = 0.0
        self.heading_error = 0.0
        self.prev_timestamp_us = 0
        self.lost_count = 0


def perception_update(state: PerceptionState,
                      reflectance: np.ndarray,
                      timestamp_us: int,
                      dt_s: float) -> dict:
    """
    Update perception from IR reflectance data.

    This is a direct port of perception.c: perception_update()

    Args:
        state: Perception state object
        reflectance: float[8] reflectance values (0-1, 0=white, 1=black)
        timestamp_us: Current timestamp in microseconds
        dt_s: Time step in seconds (used if timestamp not advancing)

    Returns:
        dict with keys:
        - lateral_error: float, normalized -1 to +1
        - heading_error: float, filtered derivative of lateral error
        - active_mask: uint8[8], 1 where reflectance > threshold
        - line_valid: bool
        - event: RoadEvent
        - lost_count: int
    """
    # Validate timestamp (monotonic)
    if state.initialized and timestamp_us <= state.prev_timestamp_us:
        # Use dt_s as fallback
        dt_actual = dt_s
    else:
        dt_actual = (timestamp_us - state.prev_timestamp_us) / 1e6 if state.prev_timestamp_us > 0 else dt_s

    # Compute weighted centroid
    weighted_sum = np.sum(IR_WEIGHTS * reflectance)
    max_abs_weight = 7.0  # max(abs(IR_WEIGHTS))

    # Active channel detection
    active_mask = (reflectance > IR_THRESHOLD).astype(np.uint8)
    active_count = int(active_mask.sum())

    line_valid = active_count > 0

    if not line_valid:
        # Line lost
        state.lost_count += 1
        lateral_error = 0.0  # Hold last valid value or zero
        event = RoadEvent.ROAD_EVENT_LINE_LOST
    else:
        # Line detected
        state.lost_count = 0
        lateral_error = weighted_sum / max_abs_weight

        # Compute heading error (filtered derivative)
        if dt_actual > 0.0:
            derivative = (lateral_error - state.prev_lateral_error) / dt_actual

            if state.initialized:
                # IIR filter: y[n] = α*y[n-1] + (1-α)*x[n]
                state.heading_error = (HEADING_FILTER_ALPHA * state.heading_error +
                                       (1.0 - HEADING_FILTER_ALPHA) * derivative)
            else:
                # First update: directly assign
                state.heading_error = derivative
        else:
            # dt too small, skip derivative update
            pass

        # Classify road event
        if active_count >= INTERSECTION_ACTIVE_CHANNELS:
            event = RoadEvent.ROAD_EVENT_INTERSECTION
        elif (abs(lateral_error) >= CURVE_ERROR_THRESHOLD and
              abs(state.heading_error) >= CURVE_DERIVATIVE_THRESHOLD):
            event = RoadEvent.ROAD_EVENT_CURVE_ENTRY
        else:
            event = RoadEvent.ROAD_EVENT_NONE

    # Update state
    state.prev_lateral_error = lateral_error
    state.prev_timestamp_us = timestamp_us
    state.initialized = True

    return {
        'lateral_error': lateral_error,
        'heading_error': state.heading_error,
        'active_mask': active_mask,
        'line_valid': line_valid,
        'event': event,
        'lost_count': state.lost_count,
    }
