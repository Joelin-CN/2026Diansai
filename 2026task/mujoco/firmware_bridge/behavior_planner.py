"""
Behavior planner - Python port of behavior_planner.c
FSM for high-level robot behavior states.
"""

from enum import IntEnum

# Behavior states (from behavior_planner.h)
class BehaviorState(IntEnum):
    IDLE = 0
    LINE_FOLLOW = 1
    APPROACH_CURVE = 2
    CURVE = 3
    LINE_LOST_DEGRADED = 4
    STOPPED = 5
    FAULT = 6


# Speed limits per state (from config.c)
SPEED_IDLE = 0.0
SPEED_LINE_FOLLOW = 1.0
SPEED_APPROACH_CURVE = 0.7
SPEED_CURVE = 0.5
SPEED_DEGRADED = 0.25

# Frame thresholds (from config.c)
LINE_LOST_STOP_FRAMES = 20   # ~0.4s at 50Hz
CURVE_EXIT_STABLE_FRAMES = 5  # ~0.1s


class BehaviorPlannerState:
    """Internal state for behavior planner FSM"""
    def __init__(self):
        self.state = BehaviorState.IDLE
        self.frame_counter = 0
        self.enabled = False


def behavior_planner_update(state: BehaviorPlannerState,
                             perception_result: dict,
                             enable: bool) -> tuple:
    """
    Update behavior FSM based on perception and enable signal.

    Args:
        state: BehaviorPlannerState object
        perception_result: dict from perception_update()
        enable: bool, system enable flag

    Returns:
        Tuple of (behavior_state, speed_limit):
        - behavior_state: BehaviorState enum
        - speed_limit: float (m/s)
    """
    from firmware_bridge.perception import RoadEvent

    line_valid = perception_result['line_valid']
    event = perception_result['event']
    lost_count = perception_result['lost_count']

    prev_state = state.state

    # Enable/disable logic
    if not enable:
        state.state = BehaviorState.IDLE
        state.frame_counter = 0
        state.enabled = False
    else:
        state.enabled = True

    if not state.enabled:
        return state.state, SPEED_IDLE

    # FSM transitions
    if state.state == BehaviorState.IDLE:
        if line_valid:
            state.state = BehaviorState.LINE_FOLLOW
            state.frame_counter = 0

    elif state.state == BehaviorState.LINE_FOLLOW:
        if not line_valid:
            state.state = BehaviorState.LINE_LOST_DEGRADED
            state.frame_counter = 0
        elif event == RoadEvent.ROAD_EVENT_CURVE_ENTRY:
            state.state = BehaviorState.APPROACH_CURVE
            state.frame_counter = 0

    elif state.state == BehaviorState.APPROACH_CURVE:
        if not line_valid:
            state.state = BehaviorState.LINE_LOST_DEGRADED
            state.frame_counter = 0
        elif event == RoadEvent.ROAD_EVENT_CURVE_ENTRY:
            # Stay in approach or enter curve
            state.state = BehaviorState.CURVE
            state.frame_counter = 0
        elif event == RoadEvent.ROAD_EVENT_NONE:
            # Curve ended, back to line follow
            state.state = BehaviorState.LINE_FOLLOW
            state.frame_counter = 0

    elif state.state == BehaviorState.CURVE:
        if not line_valid:
            state.state = BehaviorState.LINE_LOST_DEGRADED
            state.frame_counter = 0
        elif event == RoadEvent.ROAD_EVENT_NONE:
            # Check if stable exit
            state.frame_counter += 1
            if state.frame_counter >= CURVE_EXIT_STABLE_FRAMES:
                state.state = BehaviorState.LINE_FOLLOW
                state.frame_counter = 0

    elif state.state == BehaviorState.LINE_LOST_DEGRADED:
        if line_valid:
            # Recovered
            state.state = BehaviorState.LINE_FOLLOW
            state.frame_counter = 0
        else:
            state.frame_counter += 1
            if state.frame_counter >= LINE_LOST_STOP_FRAMES:
                state.state = BehaviorState.STOPPED
                state.frame_counter = 0

    elif state.state == BehaviorState.STOPPED:
        # Stay stopped unless re-enabled or line recovered
        if line_valid:
            state.state = BehaviorState.LINE_FOLLOW
            state.frame_counter = 0

    # Map state to speed limit
    speed_map = {
        BehaviorState.IDLE: SPEED_IDLE,
        BehaviorState.LINE_FOLLOW: SPEED_LINE_FOLLOW,
        BehaviorState.APPROACH_CURVE: SPEED_APPROACH_CURVE,
        BehaviorState.CURVE: SPEED_CURVE,
        BehaviorState.LINE_LOST_DEGRADED: SPEED_DEGRADED,
        BehaviorState.STOPPED: 0.0,
        BehaviorState.FAULT: 0.0,
    }

    speed_limit = speed_map.get(state.state, 0.0)

    return state.state, speed_limit
