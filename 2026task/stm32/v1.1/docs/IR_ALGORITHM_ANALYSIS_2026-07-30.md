# IR Tracking Algorithm Deep Analysis

**Date**: 2026-07-30  
**Analyzer**: Claude Opus 4.8  
**Project**: STM32 Differential Drive Line Tracking Robot  
**Context**: Playground-type track (straight lines + curves only, no intersections/T-junctions)

## Executive Summary

The current IR tracking algorithm contains **significant over-engineering** for the target application. Analysis reveals three critical issues: (1) invalid intersection detection logic that can never trigger in playground tracks, (2) pseudo curve detection that cannot distinguish actual curves from straight-line deviations due to fundamental sensor limitations, and (3) event-driven behavior state machine that adds unnecessary complexity. **Recommendation**: Simplify to a minimal tracking algorithm using only `lateral_error` and `heading_error` for PD control, eliminating all event classification logic.

---

## Current Algorithm Architecture

### 1. Line Type Detection Logic

**Location**: `perception.c:144-152`

The algorithm implements a three-tier event classification system:

```c
/* Step 5: Road event detection */
if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
    result->event = ROAD_EVENT_INTERSECTION;
} else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
           fabsf(result->heading_error) >= g_sens_decision_config.perception.curve_derivative_threshold) {
    result->event = ROAD_EVENT_CURVE_ENTRY;
} else {
    result->event = ROAD_EVENT_NONE;
}
```

**Detection Hierarchy**:
1. **Priority 1**: Intersection detection (4+ sensors active)
2. **Priority 2**: Curve entry (lateral error ≥ 0.45 AND heading error ≥ 1.5)
3. **Priority 3**: Normal tracking (default state)

### 2. Event Classification

**Definition**: `perception.h:18-23`

```c
typedef enum {
    ROAD_EVENT_NONE,           // Normal tracking
    ROAD_EVENT_CURVE_ENTRY,    // Curve detected
    ROAD_EVENT_INTERSECTION,   // Intersection/crossroad
    ROAD_EVENT_LINE_LOST       // Line lost
} road_event_t;
```

**Judgment Conditions**:

| Event | Trigger Condition | Configuration Parameter |
|-------|------------------|------------------------|
| `ROAD_EVENT_INTERSECTION` | `active_count ≥ 4` | `intersection_active_channels = 4` |
| `ROAD_EVENT_CURVE_ENTRY` | `\|lateral_error\| ≥ 0.45` AND `\|heading_error\| ≥ 1.5` | `curve_error_threshold = 0.45`<br>`curve_derivative_threshold = 1.5` |
| `ROAD_EVENT_LINE_LOST` | `active_count == 0` | N/A (implicit) |
| `ROAD_EVENT_NONE` | None of the above | Default state |

### 3. Control Strategy Mapping

**Location**: `behavior_planner.c:110-129`, `behavior_planner.c:141-169`

The detected events drive a **finite state machine** with speed adjustments:

**State Transition Graph**:
```
IDLE → LINE_FOLLOW → APPROACH_CURVE → CURVE → LINE_FOLLOW
                          ↑               |
                          └───────────────┘
                       (when curve detected)
```

**Speed Mapping**:

| Behavior State | Trigger Event | Speed Limit | Config Parameter |
|---------------|---------------|-------------|------------------|
| `BEHAVIOR_STATE_LINE_FOLLOW` | `ROAD_EVENT_NONE` | 1.0 m/s | `line_speed_mps` |
| `BEHAVIOR_STATE_APPROACH_CURVE` | `ROAD_EVENT_CURVE_ENTRY` | 0.7 m/s | `approach_curve_speed_mps` |
| `BEHAVIOR_STATE_CURVE` | `heading_error ≥ 0.2` or `curvature ≥ 0.2` | 0.5 m/s | `curve_speed_mps` |

**Key Logic** (behavior_planner.c:110-112):
```c
else if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW &&
         input->perception->event == ROAD_EVENT_CURVE_ENTRY) {
    new_state = BEHAVIOR_STATE_APPROACH_CURVE;
```

The curve detection directly triggers a behavior state transition and speed reduction.

---

## Problem Identification

### Problem 1: Invalid Intersection Detection

**Code Location**: `perception.c:145-146`

```c
if (active_count >= g_sens_decision_config.perception.intersection_active_channels) {
    result->event = ROAD_EVENT_INTERSECTION;
```

**Issue**: This detection logic is fundamentally incompatible with playground-type tracks.

**Analysis**:
- **Track Constraint**: Playground tracks consist of **only straight lines and curves** (arc segments). No intersections, T-junctions, or crossroads exist.
- **Sensor Response**: For a standard black line (e.g., 15-20mm width), only 1-3 adjacent sensors activate when the robot is centered on the line.
- **Trigger Condition**: `active_count ≥ 4` requires at least 4 out of 8 sensors to detect black simultaneously.
- **Physical Impossibility**: On a single-line track, 4+ sensors activating simultaneously would require:
  - Line width ≥ 45mm (3 sensors × 11.39mm spacing), OR
  - Robot severely straddling the line (already a tracking failure)

**Misclassification Risk**:
If the robot drifts far off-track and multiple sensors catch the line edge, this could falsely trigger `ROAD_EVENT_INTERSECTION`, causing unexpected behavior.

**Current Usage**:
- `behavior_planner.c`: No state transitions use `ROAD_EVENT_INTERSECTION`
- `perception_debug.c:143`: Only used for debug logging ("INTER")

**Impact**:
- **Code Clutter**: 2 lines of dead code in the detection logic
- **Config Bloat**: 1 unused parameter (`intersection_active_channels = 4`)
- **Maintenance Cost**: Future developers may waste time understanding this "feature"
- **Performance**: Negligible (one integer comparison)

**Recommendation**: **Remove** this detection branch entirely.

---

### Problem 2: Pseudo Curve Detection

**Code Location**: `perception.c:147-149`

```c
else if (fabsf(result->lateral_error) >= g_sens_decision_config.perception.curve_error_threshold &&
         fabsf(result->heading_error) >= g_sens_decision_config.perception.curve_derivative_threshold) {
    result->event = ROAD_EVENT_CURVE_ENTRY;
```

**Issue**: The IR sensor array **cannot distinguish actual curves from straight-line deviations** due to fundamental sensor limitations.

**Fundamental Limitation Analysis**:

**Sensor Characteristics**:
- **Detection Window**: 8 sensors × 11.39mm spacing = 79.86mm total width
- **Temporal Resolution**: Single snapshot per update cycle (no memory of track curvature)
- **Spatial Information**: Only measures **instantaneous lateral position** of the black line

**The Indistinguishability Problem**:

Consider two scenarios:

| Scenario A: Straight Line, Robot Drifted Left | Scenario B: Left Curve, Robot Centered |
|-----------------------------------------------|----------------------------------------|
| Track: Straight (radius = ∞) | Track: Left curve (radius = 300mm) |
| Robot position: 20mm left of line | Robot position: Centered on line |
| **Sensor View**: Left sensors (Ch0-2) active | **Sensor View**: Left sensors (Ch0-2) active |
| **lateral_error**: +2.0 (line on right side) | **lateral_error**: +2.0 (line curves right) |
| **heading_error**: +1.8 (drifting left) | **heading_error**: +1.8 (curve derivative) |

**Identical Sensor Signatures**! The algorithm cannot tell them apart.

**Why This Happens**:
- `lateral_error`: Measures **weighted centroid** of activated sensors (spatial deviation)
- `heading_error`: Measures **time derivative** of lateral_error (rate of change)
- Both metrics respond identically to:
  - Robot drifting on a straight line
  - Robot following a curved line

**Physics of the Problem**:
The IR array sees only a **narrow horizontal slice** of the track (79.86mm window). To distinguish a curve from a drift, the sensor would need to:
- See track curvature **beyond the sensor array** (requires lookahead), OR
- Integrate motion history over **multiple sensor snapshots** (requires mapping/SLAM)

Neither is implemented in this algorithm.

**Misclassification Risk Examples**:

1. **False Positive**: Robot drifts heavily on straight line → Triggers `ROAD_EVENT_CURVE_ENTRY` → Speed reduces to 0.5 m/s → Tracking degrades further
2. **False Negative**: Robot enters gentle curve but tracking is accurate → lateral_error < 0.45 → Stays at 1.0 m/s → May overshoot curve
3. **Oscillation**: Robot wobbles on straight line → lateral_error crosses 0.45 threshold → Behavior flaps between LINE_FOLLOW and APPROACH_CURVE

**Current Usage**:

**Critical Dependency** (behavior_planner.c:110-112):
```c
else if (planner->current_state == BEHAVIOR_STATE_LINE_FOLLOW &&
         input->perception->event == ROAD_EVENT_CURVE_ENTRY) {
    new_state = BEHAVIOR_STATE_APPROACH_CURVE;  // Speed: 1.0 → 0.7 m/s
```

This is the **only place** where curve detection affects control strategy. It triggers:
- State transition: `LINE_FOLLOW` → `APPROACH_CURVE`
- Speed reduction: 1.0 m/s → 0.7 m/s (30% slower)
- Further reduction: `APPROACH_CURVE` → `CURVE` (0.5 m/s if heading_error persists)

**Impact Analysis**:

**Pros** (Intended Benefits):
- Proactive speed reduction before entering sharp curves (safety margin)
- Reduces overshoot on high-curvature sections

**Cons** (Actual Problems):
- **Unreliable Trigger**: Cannot distinguish curves from tracking errors
- **Speed Oscillation**: False positives on straight lines cause unnecessary slowdowns
- **Complexity**: 3-state FSM (LINE_FOLLOW → APPROACH_CURVE → CURVE) with 5 transition conditions
- **Config Overhead**: 2 threshold parameters (`curve_error_threshold`, `curve_derivative_threshold`) that are fundamentally impossible to "tune correctly"

**Alternative Approach**:

Instead of event-driven speed control, use **continuous curvature-based speed adaptation**:
```c
// Direct curvature mapping (already computed in trajectory planner)
speed_limit = max_speed / (1 + k * |path_curvature|)
```

This approach:
- Uses actual path curvature (from trajectory planner, which has lookahead)
- No binary "curve detected" decision → smooth speed transitions
- No false positives from tracking errors
- Simpler logic (one equation vs. 3-state FSM)

**Recommendation**: **Replace event-driven speed control with curvature-based speed scaling**. Remove curve detection entirely.

---

### Problem 3: Control Strategy Coupling

**Code Location**: `behavior_planner.c:110-129`

**Issue**: The behavior state machine introduces unnecessary coupling between perception and control.

**Current Architecture**:
```
Perception (perception.c)
    ↓ road_event_t
Behavior FSM (behavior_planner.c)
    ↓ speed_limit_mps
Trajectory Planning
    ↓ path_curvature
Motion Control
```

**Coupling Problems**:

1. **Redundant Information Flow**:
   - Perception computes `lateral_error` (continuous)
   - Perception classifies to `road_event_t` (discrete)
   - Behavior FSM maps event to speed (discrete)
   - Trajectory planner already computes `path_curvature` (continuous, more accurate)

2. **State Machine Complexity**:
   - 7 behavior states (IDLE, LINE_FOLLOW, APPROACH_CURVE, CURVE, LINE_LOST_DEGRADED, STOPPED, FAULT)
   - 14 transition conditions across 130+ lines of code
   - Only **1 transition** (LINE_FOLLOW → APPROACH_CURVE) depends on curve detection
   - Other transitions use `line_valid`, `localization_valid`, `command` (orthogonal to curve detection)

3. **Exit Condition Complexity** (behavior_planner.c:118-128):
```c
else if (planner->current_state == BEHAVIOR_STATE_CURVE) {
    if (fabsf(input->perception->heading_error) < 0.1f &&
        fabsf(input->path_curvature) < 0.1f) {
        planner->stable_straight_frames++;
        if (planner->stable_straight_frames >= g_sens_decision_config.behavior.curve_exit_stable_frames) {
            new_state = BEHAVIOR_STATE_LINE_FOLLOW;
            planner->stable_straight_frames = 0U;
        }
    } else {
        planner->stable_straight_frames = 0U;
    }
}
```

**Analysis**: The exit condition uses `path_curvature` (from trajectory planner) to detect curve exit, **not** the perception events. This proves the perception-based curve detection is insufficient even within its own control flow.

**Recommendation**: **Flatten the state machine**. Use continuous signals (`lateral_error`, `heading_error`, `path_curvature`) directly for control gain scheduling, eliminating discrete event states.

---

## Redundant Parameters

The following configuration parameters are **only used for line type detection** and would become obsolete if the detection logic is removed:

| Parameter | Current Value | Location | Usage Count |
|-----------|--------------|----------|-------------|
| `curve_error_threshold` | 0.45 | config.c:332 | 1× (perception.c:147) |
| `curve_derivative_threshold` | 1.5 | config.c:333 | 1× (perception.c:148) |
| `intersection_active_channels` | 4 | config.c:334 | 1× (perception.c:145) |
| `approach_curve_speed_mps` | 0.7 m/s | config.c:535 | 1× (behavior_planner.c:149) |
| `curve_speed_mps` | 0.5 m/s | config.c:536 | 1× (behavior_planner.c:152) |
| `curve_exit_stable_frames` | 5 | config.c:532 | 2× (behavior_planner.c:122-123) |

**Impact of Removal**:
- **Code Reduction**: ~30 lines of parameter definitions and comments
- **Validation Logic**: 3 validation checks in `sd_config_validate()` (config.c:622-624)
- **Memory**: 6 × 4 bytes = 24 bytes (negligible on STM32F407)

**Parameters to Keep**:
- `line_speed_mps`: Still needed for normal tracking speed
- `heading_filter_alpha`: Used for heading_error LPF (perception.c:125)
- `white_reference[]`, `black_strength_threshold`: Core IR calibration (unrelated to line type detection)

---

## Simplified Algorithm Proposal

### Option 1: Minimal Tracking (Recommended)

**Philosophy**: Remove all line type detection, use only error signals for control.

**Algorithm**:
```c
// perception.c (simplified)
sd_status_t perception_update(perception_t *perception,
                              const ir_array_data_t *ir_data,
                              uint64_t timestamp_us,
                              perception_result_t *result) {
    // ... (black strength calculation remains unchanged)
    
    // Compute lateral_error (weighted centroid)
    if (active_count == 0) {
        result->line_valid = false;
        result->lateral_error = 0.0f;
    } else {
        result->line_valid = true;
        result->lateral_error = weighted_sum / strength_sum;
    }
    
    // Compute heading_error (time derivative for PD control)
    if (perception->initialized && dt_s > 0.0f) {
        derivative = (result->lateral_error - perception->prev_lateral_error) / dt_s;
        perception->heading_error = alpha * perception->heading_error + (1 - alpha) * derivative;
    }
    result->heading_error = perception->heading_error;
    
    // NO event detection - remove entirely
    
    return SD_OK;
}
```

**Control Flow**:
```c
// trajectory_planner.c (pseudo-code, may need modification)
float compute_speed_limit(float path_curvature, float max_speed, float k_curv) {
    // Curvature-based speed scaling (continuous)
    return max_speed / (1.0f + k_curv * fabsf(path_curvature));
}

// motion_control.c (PID/PD controller)
float compute_steering(float lateral_error, float heading_error, float kp, float kd) {
    return kp * lateral_error + kd * heading_error;
}
```

**Changes Required**:
1. **perception.c**: Remove lines 144-152 (event detection)
2. **perception.h**: Remove `road_event_t` enum (lines 18-23)
3. **perception_result_t**: Remove `event` field
4. **behavior_planner.c**: Simplify state machine to 4 states (IDLE, RUNNING, STOPPED, FAULT)
5. **config.c**: Remove 6 parameters listed above

**Benefits**:
- **Code Reduction**: ~150 lines removed (detection logic + FSM transitions + validation)
- **Reduced Complexity**: O(1) control decision vs. O(n_states × n_transitions) FSM
- **Elimination of False Positives**: No more "phantom curve detections" on straight lines
- **Smooth Control**: Continuous signals → continuous response (no state jumps)

**Risks**:
- **Overspeed on Curves**: Without proactive slowdown, robot may enter curves too fast
  - **Mitigation**: Tune PD gains for aggressive response (high kd), or implement curvature-based speed scaling

**Validation Plan**:
1. Test on straight line with intentional lateral disturbances → Verify stable tracking without false curve events
2. Test on gentle curve (R > 500mm) → Verify tracking without speed oscillation
3. Test on sharp curve (R < 300mm) → Verify no overshoot (may need curvature-based speed scaling)

---

### Option 2: Enhanced Tracking

**Philosophy**: Keep `heading_error` for PD control, but remove event classification.

**Same as Option 1**, but emphasizes that `heading_error` (time derivative of lateral_error) is valuable for:
- **D-term in PD controller**: Provides damping, reduces oscillation
- **Predictive steering**: heading_error ≈ angular deviation, helps anticipate corrections

**This is already computed** in the current algorithm (perception.c:119-140). The recommendation is to **keep this computation** but **remove the event classification** that misuses it.

---

### Option 3: Hybrid Approach (Not Recommended)

**Philosophy**: Keep curve detection for safety-critical speed limiting, but simplify the FSM.

**Algorithm**:
- Detect "high deviation + high derivative" → Set conservative speed limit (e.g., 0.7 m/s)
- Use continuous PD control for steering
- Remove APPROACH_CURVE and CURVE states (collapse to single RUNNING state)

**Why Not Recommended**:
- Still suffers from the fundamental indistinguishability problem (Problem 2)
- Adds complexity with minimal benefit (curvature-based scaling is superior)
- Requires tuning the same problematic thresholds

**Only consider this if**:
- Hardware constraints prevent implementing trajectory-based curvature estimation
- Safety regulations require explicit "slow down when uncertain" logic

---

## Impact Analysis

### Performance Impact

**Computational Savings** (per perception_update call):

| Operation Removed | CPU Cycles (ARM Cortex-M4 @ 168MHz) | Estimated Time |
|-------------------|--------------------------------------|----------------|
| Intersection check (1× int comparison) | ~5 cycles | ~30 ns |
| Curve check (2× fabs + 2× float comparison) | ~40 cycles | ~240 ns |
| Total per cycle | ~45 cycles | ~270 ns |
| **Annual savings** @ 100Hz update rate | N/A | **27 μs/s** |

**Verdict**: Negligible performance impact (0.0027% of 10ms control loop).

**Memory Savings**:

| Item | Bytes Saved |
|------|-------------|
| 6 config parameters | 24 bytes |
| `road_event_t` field in results | 4 bytes |
| FSM state variables | ~8 bytes |
| **Total** | **36 bytes** |

**Verdict**: Negligible (0.003% of 128KB RAM).

**Real Benefit**: **Code clarity and maintainability**, not performance.

---

### Control Quality Impact

**Expected Changes**:

| Scenario | Current Behavior | After Simplification |
|----------|-----------------|----------------------|
| **Straight line tracking** | May falsely trigger APPROACH_CURVE if wobbling | Smooth tracking, no state jumps |
| **Entering gentle curve** | May or may not slow down (depends on thresholds) | Speed maintained unless curvature-based scaling added |
| **Entering sharp curve** | Slows to 0.7 m/s (if detected), then 0.5 m/s | Relies on PD control + optional curvature scaling |
| **Exiting curve** | Waits for 5 stable frames before speeding up | Immediate response (no hysteresis delay) |

**Potential Concerns**:

1. **Overshoot on Sharp Curves**:
   - **Risk**: Without proactive slowdown, robot may overshoot curve entry
   - **Mitigation**: 
     - Option A: Increase PD control gains (aggressive steering)
     - Option B: Implement curvature-based speed scaling (recommended)
     - Option C: Use feedforward control based on lateral_error magnitude

2. **Aggressive Control Response**:
   - **Risk**: High derivative gain may cause steering oscillation
   - **Mitigation**: Tune `heading_filter_alpha` (currently 0.3) to increase LPF damping

**Recommendation**: Start with Option 1 (minimal tracking) and validate on test track. Add curvature-based speed scaling if overshoot occurs.

---

### Code Maintainability

**Cyclomatic Complexity Reduction**:

| Function | Current CC | After Simplification | Change |
|----------|-----------|---------------------|--------|
| `perception_update()` | 8 | 4 | -50% |
| `behavior_planner_update()` | 15 | 8 | -47% |

**Lines of Code**:

| File | Current LOC | After Simplification | Reduction |
|------|------------|---------------------|-----------|
| perception.c | 161 | ~145 | -10% |
| behavior_planner.c | 175 | ~120 | -31% |
| config.c | 676 | ~630 | -7% |
| **Total** | ~1012 | ~895 | **-12%** |

**Cognitive Load Reduction**:

- **Before**: Developer must understand 4 event types, 7 behavior states, 14 state transitions
- **After**: Developer only needs to understand lateral_error → steering, optional curvature → speed
- **Documentation**: Can remove 3 problem-specific sections from API_PITFALLS_GUIDE.md

**Future-Proofing**:

If future requirements demand complex line type detection (e.g., intersection navigation for competition expansion):
- Clean slate for redesign (no legacy assumptions)
- Can add camera-based line type recognition without conflicting with IR logic
- Modular design: perception produces signals, planner consumes signals (no implicit event semantics)

---

## Recommendation Summary

### Priority P0: Remove Invalid Intersection Detection

**Action**:
1. Delete `perception.c:145-146` (intersection detection branch)
2. Delete `config.c:334` parameter initialization
3. Delete `config.c:624-625` validation check
4. Delete `config.h:70` parameter definition

**Effort**: 15 minutes  
**Risk**: Zero (dead code, no functional dependencies)  
**Benefit**: Cleaner codebase, no confusion for future developers

---

### Priority P1: Replace Event-Driven Speed Control (Recommended)

**Action**:
1. Remove curve detection logic (`perception.c:147-149`)
2. Remove `road_event_t` enum and event field from `perception_result_t`
3. Simplify behavior FSM:
   - Remove `BEHAVIOR_STATE_APPROACH_CURVE` and `BEHAVIOR_STATE_CURVE`
   - Collapse to `BEHAVIOR_STATE_RUNNING` with continuous speed control
4. Implement curvature-based speed scaling in trajectory planner
5. Remove 6 obsolete config parameters

**Effort**: 2-3 hours (code modification + testing)  
**Risk**: Medium (requires test track validation)  
**Benefit**: 
- Eliminates false positives
- Smoother control response
- 12% code reduction
- Easier to understand and maintain

**Validation Criteria**:
- [ ] Straight line tracking stable (no speed oscillations)
- [ ] Gentle curve (R > 500mm) tracking without overshoot
- [ ] Sharp curve (R < 300mm) tracking without leaving track
- [ ] Transition from straight to curve is smooth (no jerks)

---

### Priority P2: Continuous Optimization (Optional)

If P1 validation reveals overshoot on sharp curves:

**Action**: Implement adaptive speed control
```c
// In trajectory planner or motion controller
float adaptive_speed = base_speed * (1.0f - k * fabsf(lateral_error));
float curvature_speed = max_speed / (1.0f + k_curv * fabsf(path_curvature));
speed_limit = fminf(adaptive_speed, curvature_speed);
```

**Tuning Parameters**:
- `k`: Error-based speed reduction coefficient (suggest 0.3-0.5)
- `k_curv`: Curvature-based speed reduction coefficient (suggest 1.0-2.0)

**Effort**: 1 hour  
**Risk**: Low (easily reverted)  
**Benefit**: Proactive speed control without discrete event detection

---

## Next Steps

### If Recommendation is Accepted

**Phase 1: Cleanup (P0 - Remove Intersection Detection)**

Files to modify:
1. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\src\perception.c` (lines 145-146)
2. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\inc\config.h` (line 70)
3. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\src\config.c` (lines 334, 624-625)

**Phase 2: Core Refactoring (P1 - Simplify Algorithm)**

Files to modify:
1. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\inc\perception.h` (remove event enum and field)
2. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\src\perception.c` (remove detection logic)
3. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\src\behavior_planner.c` (simplify FSM)
4. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\inc\behavior_planner.h` (update state enum)
5. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\src\config.c` (remove 6 parameters)
6. `E:\B306\2026\diansai\2026task\stm32\v1.0\modules\Sens-Decision\inc\config.h` (update config struct)

**Phase 3: Validation**

Required tests:
1. Compile verification (no build errors)
2. Unit test: perception_update with various sensor patterns
3. Integration test: Full control loop on test bench
4. Real-world test: Playground track with straight lines and curves

**Phase 4: Documentation**

Documents to update:
1. `CHANGELOG.md` - Add v1.3.0 entry
2. `API_PITFALLS_GUIDE.md` - Remove obsolete event-related pitfalls
3. `docs/SESSION_FIX_LOG_2026-07-30_PART*.md` - Create session summary
4. Create `docs/ALGORITHM_SIMPLIFICATION_2026-07-30.md` with before/after comparison

---

### If Recommendation is Rejected

**Alternative Actions**:

1. **Document the Limitations**: Add prominent warnings in code comments explaining that curve detection cannot distinguish curves from tracking errors
2. **Increase Thresholds**: Raise `curve_error_threshold` and `curve_derivative_threshold` to reduce false positives (trade-off: delayed curve detection)
3. **Add Hysteresis**: Require sustained threshold crossing (e.g., 3 consecutive frames) before triggering CURVE_ENTRY
4. **Monitor in Production**: Log all curve detection events and correlate with actual track geometry to quantify false positive rate

---

## Conclusion

The current IR tracking algorithm's line type detection logic is **fundamentally incompatible** with the physical constraints of the application:

1. **Intersection detection**: Impossible on single-line tracks (dead code)
2. **Curve detection**: Indistinguishable from tracking errors due to sensor limitations
3. **Event-driven control**: Adds complexity without providing reliable benefits

The recommended simplification (Option 1: Minimal Tracking) will:
- **Eliminate false positives** from phantom curve detections
- **Reduce code complexity** by 12% (117 lines)
- **Improve maintainability** through clearer control flow
- **Enable smooth control** via continuous signal processing

The primary risk (overshoot on sharp curves) can be mitigated through PD gain tuning or optional curvature-based speed scaling, both of which are more reliable than event-based detection.

**Final Recommendation**: Proceed with P0 (remove intersection detection) immediately, then implement P1 (simplified algorithm) with test track validation before competition deployment.

---

**Analysis Complete**  
**Document Version**: 1.0  
**Total Analysis Time**: ~45 minutes  
**Files Analyzed**: 5 source files, 3 header files (1,824 lines total)
