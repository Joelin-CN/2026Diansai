# Real-Time Vision Runtime Modes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor the Vision Layer around four task-driven formal runtime modes so the real runtime path can be tuned toward `<= 30 ms/frame` without carrying preview/debug detector overhead.

**Architecture:** Replace detector-centric runtime gating with task-centric runtime modes derived from the contest requirements. Keep `DEBUG` as a separate non-real-time path, and split high-frequency laser tracking from low-frequency or initialization-only border detection.

**Tech Stack:** Python, OpenCV `cv2`, NumPy, pytest, existing `src/vision` dataclasses/pipeline/tracker/detectors.

## Global Constraints

- Vision Layer only outputs image-space structures: `ImagePoint`, `ImageQuad`, detection results, and diagnostics.
- Do not output `ScreenPoint`, `ScreenQuad`, task paths, control errors, motor commands, or homography-converted coordinates.
- Formal runtime design must be driven by contest task modes, not by arbitrary detector combinations.
- Real-time performance target applies to formal runtime paths, not to preview/debug paths.
- `DEBUG` preview may keep heavier diagnostics and optional fallback logic, but those paths must not leak into formal runtime hot paths.
- Red/green laser control-consumable measurements must remain `found=True` and `source=measured` only.
- Kalman remains optional and is not part of this plan.
- A4 and gray-screen border detectors must not run every frame in runtime unless a later measurement proves they still fit the runtime budget.

---

## File Structure

### Modified Files

- `2023e/src/vision/types.py`
  - Add or adjust runtime mode enums to match contest task modes.

- `2023e/src/vision/pipeline.py`
  - Replace current coarse detector gating with four formal runtime modes plus debug.
  - Add low-frequency or cached border handling hooks.

- `2023e/src/vision/state_tracker.py`
  - Add support for bounded runtime reacquire behavior if needed.

- `2023e/src/vision/diagnostics.py`
  - Keep runtime diagnostics lightweight while allowing richer debug reporting.

- `2023e/scripts/test/preview_vision_video.py`
  - Keep preview-only heavy behavior explicit.
  - Distinguish runtime-like path from debug/relaxed analysis path.

- `2023e/tests/vision/test_pipeline.py`
  - Add task-mode detector gating tests.

- `2023e/tests/vision/test_state_tracker.py`
  - Add runtime reacquire/ROI behavior tests if required by implementation.

### New or Optional Files

- `2023e/src/vision/runtime_policy.py` (optional)
  - If the existing `pipeline.py` becomes too dense, move low-frequency detector scheduling and runtime policy rules here.

- `2023e/tests/vision/test_runtime_policy.py` (optional)
  - If `runtime_policy.py` is created, isolate scheduling tests here.

---

## Target Runtime Modes

Formal runtime modes derived from contest tasks:

1. `RUNTIME_SCREEN_RED`
   - Contest basic requirements (1) and (2)
   - Red laser is high-frequency
   - Gray pencil square is initialization or low-frequency support

2. `RUNTIME_A4_RED`
   - Contest basic requirements (3) and (4)
   - Red laser is high-frequency
   - A4 tape detection is initialization or low-frequency support

3. `RUNTIME_TRACK_RED_GREEN`
   - Contest advanced requirement (1)
   - Red and green lasers are high-frequency
   - No runtime A4 detector

4. `RUNTIME_A4_TRACK_RED_GREEN`
   - Contest advanced requirement (2)
   - Red and green lasers are high-frequency
   - A4 tape detection is initialization or low-frequency support

Separate non-runtime mode:

5. `DEBUG`
   - Preview/analysis only
   - May run heavier diagnostics, strict/relaxed comparison, and border overlays

---

## Task 1: Formalize Four Runtime Modes In Vision Types And Pipeline Gating

**Files:**
- Modify: `2023e/src/vision/types.py`
- Modify: `2023e/src/vision/pipeline.py`
- Test: `2023e/tests/vision/test_pipeline.py`

**Interfaces:**
- Produces: `VisionMode.RUNTIME_SCREEN_RED`
- Produces: `VisionMode.RUNTIME_A4_RED`
- Produces: `VisionMode.RUNTIME_TRACK_RED_GREEN`
- Produces: `VisionMode.RUNTIME_A4_TRACK_RED_GREEN`
- Preserves: `VisionMode.DEBUG`

- [ ] Write a failing test that `RUNTIME_SCREEN_RED` runs red laser and does not run A4 tape or green laser.

```python
def test_runtime_screen_red_runs_only_red_runtime_path(...):
    ...
```

- [ ] Run the test and verify failure comes from missing mode behavior, not syntax.

Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_pipeline.py -v`

- [ ] Add the new runtime modes in `VisionMode`.

- [ ] Replace the old `_should_run_red/_should_run_green/_should_run_tape/_should_run_screen` logic with task-mode logic matching the four contest runtime modes.

- [ ] Keep `DEBUG` behavior separate from formal runtime behavior.

- [ ] Add tests for all four runtime modes plus `DEBUG`.

Required expectations:

- `RUNTIME_SCREEN_RED`: red only; no green; no A4; no screen per-frame by default.
- `RUNTIME_A4_RED`: red + A4 support path; no green.
- `RUNTIME_TRACK_RED_GREEN`: red + green; no A4.
- `RUNTIME_A4_TRACK_RED_GREEN`: red + green + A4 support path.
- `DEBUG`: may run all enabled detectors.

- [ ] Run the pipeline tests again and confirm PASS.

## Task 2: Split High-Frequency Runtime Detection From Low-Frequency Border Support

**Files:**
- Modify: `2023e/src/vision/pipeline.py`
- Optionally create: `2023e/src/vision/runtime_policy.py`
- Test: `2023e/tests/vision/test_pipeline.py`

**Interfaces:**
- Produces: runtime policy for high-frequency detectors vs cached/low-frequency detectors.
- Produces: per-mode border refresh behavior that does not require per-frame full border detection.

- [ ] Write a failing test for `RUNTIME_A4_RED` showing lasers run every frame while A4 support can be reused across frames.

- [ ] Add a simple runtime scheduling policy with explicit concepts:

```text
high-frequency detectors:
  red laser
  green laser

low-frequency support detectors:
  A4 tape
  screen square
```

- [ ] Implement a minimal cached border mechanism in runtime path.

Required behavior:

- Runtime can keep the last valid A4 or screen result.
- Runtime does not re-run A4 or screen every frame by default.
- Debug preview can still choose frame-by-frame refresh.

- [ ] Keep the first implementation minimal:
  - one configurable refresh interval or
  - one explicit “reuse until stale” rule

- [ ] Add tests verifying cached reuse vs refresh trigger.

- [ ] Run targeted pipeline tests and confirm PASS.

## Task 3: Add Runtime Reacquire Policy That Avoids Full-Frame Relaxed Fallback

**Files:**
- Modify: `2023e/src/vision/state_tracker.py`
- Modify: `2023e/src/vision/pipeline.py`
- Test: `2023e/tests/vision/test_state_tracker.py`

**Interfaces:**
- Produces: bounded runtime reacquire behavior.
- Preserves: measured-only `found=True` semantics.

- [ ] Write a failing test showing runtime lost-target recovery expands ROI in bounded steps rather than jumping back to unconstrained full-frame relaxed search.

- [ ] Implement a two-stage runtime idea in tracker/pipeline:

```text
TRACKING:
  small ROI

REACQUIRE:
  expanded but bounded ROI
```

- [ ] Do not introduce preview-only relaxed full-frame search into runtime path.

- [ ] Keep existing debug preview free to do stricter analysis separately.

- [ ] Run state-tracker and relevant pipeline tests and confirm PASS.

## Task 4: Separate Runtime Diagnostics From Preview Diagnostics

**Files:**
- Modify: `2023e/src/vision/diagnostics.py`
- Modify: `2023e/src/vision/pipeline.py`
- Modify: `2023e/scripts/test/preview_vision_video.py`
- Test: `2023e/tests/vision/test_pipeline.py`

**Interfaces:**
- Produces: lightweight runtime diagnostics.
- Preserves: richer preview/debug diagnostics.

- [ ] Write a failing test or assertion update that runtime diagnostics do not require heavy debug artifacts.

- [ ] Keep runtime diagnostics focused on essentials:

```text
roi_used
candidate_count
failure_reason
tracker_state
lost_frame_count
```

- [ ] Keep preview script free to print richer details such as strict/relaxed comparison, candidate scores, and border overlays.

- [ ] Ensure preview-heavy paths remain outside formal runtime hot path.

- [ ] Run tests and confirm PASS.

## Task 5: Add Performance Measurement Hooks For Runtime Modes

**Files:**
- Modify: `2023e/src/vision/pipeline.py`
- Modify: `2023e/scripts/test/preview_vision_video.py`
- Optionally modify: `2023e/src/vision/diagnostics.py`

**Interfaces:**
- Produces: per-detector timing evidence so runtime hot spots can be measured.

- [ ] Add minimal timing capture around runtime detector stages.

Required evidence fields:

```text
red_laser_ms
green_laser_ms
a4_tape_ms
screen_square_ms
total_processing_ms
```

- [ ] Keep this instrumentation lightweight enough to stay available in runtime diagnostics.

- [ ] Update preview output to display stage timings clearly when available.

- [ ] Run a preview smoke test and inspect timing distribution across modes.

## Task 6: Rework Preview Script To Represent Debug, Not Formal Runtime

**Files:**
- Modify: `2023e/scripts/test/preview_vision_video.py`
- Modify: `2023e/scripts/test/preview_vision_detection.py`
- Test: `2023e/tests/scripts/test_preview_vision_video.py` (if needed)

**Interfaces:**
- Produces: explicit debug-mode preview behavior.
- Produces: optional runtime-like preview path for smoke checks.

- [ ] Make it explicit in the script that `strict + relaxed + border overlays` is a debug path, not the formal runtime path.

- [ ] Add a runtime-like preview option or mode selector that runs only the relevant contest runtime mode path.

- [ ] Keep A4 and screen overlays only when requested or when using debug mode.

- [ ] Run preview smoke tests in both runtime-like and debug-like modes.

## Final Verification

- [ ] Run Vision tests:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision -q
```

- [ ] Run script tests:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/scripts -q
```

- [ ] Run preview smoke test for the current debug path:

```powershell
conda run -n opencv-learning python scripts/test/preview_vision_video.py --no-window --max-frames 60
```

- [ ] Run boundary grep:

```powershell
rg "ScreenPoint|ScreenQuad|MotorCommand" src/vision
```

Expected result: no newly introduced screen-coordinate or motor-command outputs in Vision Layer.

## Recommended Execution Order

1. Formalize the four contest-driven runtime modes.
2. Split high-frequency laser work from low-frequency border support.
3. Remove runtime dependence on full-frame relaxed fallback.
4. Add stage timing so the `<= 30 ms/frame` target can be measured per mode.
5. Only then continue deeper laser/runtime optimization from evidence.
