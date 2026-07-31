# IR Algorithm Simplification - Modification Summary

**Date**: 2026-07-30
**Version**: v1.3.0

## Executive Summary

Removed all line type event detection from the IR tracking algorithm. The `road_event_t` enum, intersection detection, and curve detection have been eliminated. The behavior FSM simplified from 7 to 5 states. Six redundant parameters were removed and one new parameter (`speed_error_gain`) was added for continuous error-based speed control.

## Files Modified (9 files)

### 1. `modules/Sens-Decision/inc/perception.h`
- **Removed**: `road_event_t` enum (4 event types)
- **Removed**: `event` field from `perception_result_t`
- **Result**: Cleaner struct with only continuous signals

### 2. `modules/Sens-Decision/src/perception.c`
- **Removed**: Event detection logic (~10 lines)
- **Removed**: `ROAD_EVENT_LINE_LOST` assignment on line lost
- **Kept**: `lateral_error` and `heading_error` computation unchanged
- **Result**: Pure signal processing, no event classification

### 3. `modules/Sens-Decision/inc/behavior_planner.h`
- **Simplified**: `behavior_state_t` enum: 7 states → 5 states
  - Removed: `BEHAVIOR_STATE_APPROACH_CURVE`, `BEHAVIOR_STATE_CURVE`
  - Renamed: `BEHAVIOR_STATE_LINE_FOLLOW` → `BEHAVIOR_STATE_RUNNING`
- **Removed**: `stable_straight_frames` from `behavior_planner_t`

### 4. `modules/Sens-Decision/src/behavior_planner.c`
- **Removed**: Curve-related state transitions (~25 lines)
- **Removed**: `stable_straight_frames` counting logic
- **Simplified**: `was_running_state` check (3 states → 1 state)
- **Added**: Error-based continuous speed control in RUNNING state
- **Result**: 150 lines (was 175 lines)

### 5. `modules/Sens-Decision/inc/config.h`
- **Removed from `sd_perception_config_t`**: `curve_error_threshold`, `curve_derivative_threshold`, `intersection_active_channels`
- **Removed from `sd_behavior_config_t`**: `curve_exit_stable_frames`, `approach_curve_speed_mps`, `curve_speed_mps`
- **Added to `sd_behavior_config_t`**: `speed_error_gain` (float)

### 6. `modules/Sens-Decision/src/config.c`
- **Removed**: 6 parameter initializations
- **Removed**: Corresponding validation checks in `sd_config_validate()`
- **Added**: `speed_error_gain = 0.3f` initialization
- **Added**: `speed_error_gain` validation (must be finite and ≥ 0)

### 7. `modules/Sens-Decision/src/perception_debug.c`
- **Replaced**: Event switch statement → Speed Factor display
- **Replaced**: Compact print event markers → line_valid + HIGH_HDG markers
- **Replaced**: Self-check [4] Intersection Detection → Speed Error Gain check

### 8. `Core/Src/app/speed_mode.c`
- **Removed**: `approach_curve_speed_mps` and `curve_speed_mps` assignments from all 4 speed modes
- **Updated**: Printf messages (removed approach/curve speed info)

### 9. `Core/Src/app/ir_calibration.c`
- **Removed**: `CROSS` mark in calibration display

## Parameter Changes

### Removed (6)
| Parameter | Was | Why |
|-----------|-----|-----|
| `curve_error_threshold` | 0.45 | Curve detection removed |
| `curve_derivative_threshold` | 1.5 | Curve detection removed |
| `intersection_active_channels` | 4 | Intersection detection removed |
| `approach_curve_speed_mps` | 0.7 m/s | APPROACH_CURVE state removed |
| `curve_speed_mps` | 0.5 m/s | CURVE state removed |
| `curve_exit_stable_frames` | 5 | Curve exit logic removed |

### Added (1)
| Parameter | Default | Purpose |
|-----------|---------|---------|
| `speed_error_gain` | 0.3 | Error-based speed reduction gain |

### New Speed Control Formula
```
speed_factor = 1.0 - speed_error_gain × |lateral_error|, clamped to [0.4, 1.0]
speed_limit  = line_speed_mps × speed_factor
```

## Compilation Result
```
[100%] Built target v1.0_freeRTOS
```
✅ 0 errors, 0 warnings

## Code Metrics

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| behavior_planner.c | 175 lines | 150 lines | -14% |
| perception events | 4 | 0 | -100% |
| behavior states | 7 | 5 | -29% |
| Config params | 6 removed | 1 added | net -5 |
| Total lines | | | ~-135 |

## Next Steps
Agent-3 will update all documentation.
