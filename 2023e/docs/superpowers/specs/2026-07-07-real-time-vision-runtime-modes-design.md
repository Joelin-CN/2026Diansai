# Real-Time Vision Runtime Modes Design

## Goal

Refactor the Vision Layer formal runtime path around contest-task runtime modes so the hot path can converge toward `<= 30 ms/frame` without carrying preview/debug-only detector overhead.

## Confirmed Constraints

- Vision Layer only outputs image-space structures:
  - `ImagePoint`
  - `ImageQuad`
  - detection dataclasses
  - diagnostics
- Vision Layer must not output:
  - `ScreenPoint`
  - `ScreenQuad`
  - `MotorCommand`
  - Control Layer objects
- Formal runtime behavior must be driven by contest tasks, not arbitrary detector combinations.
- The `<= 30 ms/frame` target applies to formal runtime paths only.
- `DEBUG` and preview paths may remain heavier, but that behavior must stay outside the formal runtime hot path.
- Runtime laser outputs must preserve measured-only semantics: `found=True` implies a measured detection.
- Runtime must not use strict-failure to unconstrained full-frame relaxed fallback.
- Kalman remains out of scope for this step.

## Formal Runtime Modes

The formal runtime path is organized into four task-driven modes plus one non-runtime debug mode.

### `RUNTIME_SCREEN_RED`

Used for the red-system screen-origin and gray-square tasks.

- High-frequency: `red_laser`
- Low-frequency support: `screen_square`
- No runtime `green_laser`
- No runtime `a4_tape`

### `RUNTIME_A4_RED`

Used for the red-system A4 black-tape tasks.

- High-frequency: `red_laser`
- Low-frequency support: `a4_tape`
- No runtime `green_laser`

### `RUNTIME_TRACK_RED_GREEN`

Used for the green-system red/green tracking task.

- High-frequency: `red_laser`
- High-frequency: `green_laser`
- No runtime `a4_tape`
- No runtime `screen_square`

### `RUNTIME_A4_TRACK_RED_GREEN`

Used for the combined A4 red task plus green tracking task.

- High-frequency: `red_laser`
- High-frequency: `green_laser`
- Low-frequency support: `a4_tape`

### `DEBUG`

Used only for preview, diagnostics, and detector analysis.

- May run strict/relaxed comparisons
- May refresh border detectors every frame
- May include richer overlay/debug artifacts
- Is explicitly not part of the `<= 30 ms/frame` target

## Runtime Scheduling Model

Formal runtime separates high-frequency laser work from low-frequency support detection.

### High-Frequency Runtime Work

Run every frame when required by the mode:

- `red_laser`
- `green_laser`
- tracker update and ROI selection

### Low-Frequency Support Work

Run only on initialization or refresh frames:

- `a4_tape`
- `screen_square`

Runtime keeps the last valid support detection and reuses it between refreshes. The first implementation should stay minimal by using one explicit refresh interval rule per support detector class.

If a support refresh fails, runtime may continue exposing the last valid cached support result until it becomes stale according to the configured refresh rule. The first step does not require advanced staleness heuristics.

## Pipeline Responsibilities

`pipeline.py` remains the main runtime-policy location for the first implementation.

The pipeline is responsible for:

- selecting which high-frequency detectors run for the current mode
- selecting which support detector, if any, belongs to the mode
- deciding whether the support detector refreshes this frame or reuses cache
- keeping `DEBUG` behavior separate from formal runtime behavior
- reporting lightweight runtime timings and diagnostics

An additional `runtime_policy.py` file is not required unless the pipeline becomes too dense after the first implementation.

## Bounded Reacquire Policy

Formal runtime laser tracking uses bounded ROI expansion rather than preview-style unconstrained fallback.

### Tracker States

The tracker keeps the existing state family:

- `UNINITIALIZED`
- `TRACKING`
- `TEMP_LOST`
- `REACQUIRING`
- `LOST`

### ROI Policy

- `UNINITIALIZED`: use `screen_roi` if available, otherwise use the configured startup search region.
- `TRACKING`: use a small local ROI around the last measured detection.
- `TEMP_LOST`: expand the last tracking ROI by the first bounded step.
- `REACQUIRING`: expand the last tracking ROI by the second bounded step.
- `LOST`:
  - if `screen_roi` is available, return to `screen_roi`
  - if `screen_roi` is not available, keep using the last expanded bounded ROI and stop growing it

This rule prevents the formal runtime path from falling back to unconstrained full-frame search when the tracker exceeds `max_missing_frames`.

## Runtime vs Debug Detection Behavior

Formal runtime and debug preview intentionally diverge.

### Formal Runtime

- Uses the strict formal detector path only
- Uses tracker-provided bounded ROIs
- Avoids debug-only strict/relaxed comparison work
- Avoids unconstrained full-frame relaxed fallback

### Debug / Preview

- May compare strict and relaxed results
- May use heavier diagnostics and overlays
- May run support detectors every frame
- May keep global analysis behavior for investigation

The preview script should make this distinction explicit so runtime-like smoke checks do not accidentally inherit debug hot-path overhead.

## Diagnostics

Formal runtime diagnostics should stay lightweight while still giving enough evidence to optimize runtime.

Required runtime-facing fields include:

- `roi_used`
- `candidate_count`
- `failure_reason`
- `tracker_state`
- `lost_frame_count`
- `red_laser_ms`
- `green_laser_ms`
- `a4_tape_ms`
- `screen_square_ms`
- `total_processing_ms`

Debug preview may continue to expose richer per-candidate artifacts such as:

- candidate score breakdowns
- strict/relaxed comparison output
- border overlay metadata

Those richer artifacts are allowed, but they must not be required in the formal runtime hot path.

## Testing Strategy

This refactor should be delivered test-first.

Required coverage:

- mode-gating tests for all four formal runtime modes plus `DEBUG`
- runtime support-detector cache/refresh tests
- bounded reacquire ROI behavior tests
- runtime diagnostics/timing presence tests
- preview-path tests that keep debug behavior explicit and separate from runtime-like behavior

The first verification focus is correctness of mode gating and bounded runtime behavior. Performance measurement comes next through recorded stage timings rather than assumptions.

## Non-Goals

- Do not introduce screen-coordinate outputs.
- Do not introduce motor or control-layer outputs.
- Do not make A4 or screen detection per-frame mandatory in runtime.
- Do not make relaxed full-frame search part of the formal runtime recovery path.
- Do not add Kalman as a required dependency.
