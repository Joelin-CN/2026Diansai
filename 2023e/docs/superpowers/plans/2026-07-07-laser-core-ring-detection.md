# Laser Core Ring Detection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stabilize red/green laser detection on A4 black tape by detecting a bright white laser core surrounded by a red or green color ring.

**Architecture:** Keep `LaserDetector` as the only formal public detector API. Add internal candidate evidence for color ring plus bright core while preserving existing pure-color blob behavior and image-space-only Vision Layer outputs. Validate with isolated synthetic tests in `tests/vision` before preview/video verification.

**Tech Stack:** Python, OpenCV `cv2`, NumPy, pytest, existing `src/vision` dataclasses.

## Global Constraints

- Vision Layer only outputs image-space structures: `ImagePoint`, `ImageQuad`, detection results, and diagnostics.
- Do not output `ScreenPoint`, `ScreenQuad`, task paths, control errors, motor commands, or homography-converted coordinates.
- Red/green system configs, trackers, and pipeline instances remain independent.
- Red/green laser control-consumable measurements must remain `found=True` and `source=measured` only.
- Kalman remains optional and is not part of this plan.
- Formal detector logic must live under `2023e/src/vision/`, not `2023e/scripts/test/`.
- Isolated tests must pass before relying on preview/video behavior.

---

## File Structure

- Modify: `2023e/tests/vision/test_laser_detector.py`
  - Add isolated synthetic tests for white-core/color-ring laser spots and diagnostics.
  - Remove or replace premature jump-rejection tests that do not match the current root cause.

- Modify: `2023e/src/vision/laser_detector.py`
  - Extend internal `_Candidate` with core/ring evidence fields.
  - Build a bright-core mask alongside the existing target color mask.
  - Associate small color-ring candidates with nearby bright cores.
  - Add required diagnostics without changing public `LaserDetector.detect(...)` signature.

---

## Task 1: Add Isolated Core/Ring Laser Tests

**Files:**
- Modify: `2023e/tests/vision/test_laser_detector.py`

**Interfaces:**
- Consumes: `LaserDetector.detect(image, roi=None, previous_detection=None) -> LaserDetection`
- Produces: failing tests for core/ring detection and diagnostics.

- [ ] Remove the tests named `test_previous_detection_keeps_near_candidate_over_brighter_far_distractor` and `test_previous_detection_rejects_large_jump_instead_of_switching_candidates` if present. They target jump behavior, not the confirmed root cause.

- [ ] Add a helper that draws a white-core/color-ring spot.

```python
def _draw_core_ring_dot(center, core_radius, ring_radius, ring_bgr, width=120, height=100, value=30):
    image = _blank(width, height, value)
    cv2.circle(image, center, ring_radius, ring_bgr, -1, lineType=cv2.LINE_AA)
    cv2.circle(image, center, core_radius, (255, 255, 255), -1, lineType=cv2.LINE_AA)
    return image
```

- [ ] Add a failing test for a red spot with a white core and red ring.

```python
def test_detects_red_ring_with_white_core_at_core_center():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _draw_core_ring_dot((55, 48), core_radius=3, ring_radius=7, ring_bgr=(0, 0, 255))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert_center_near(detection, (55, 48), tolerance=1.5)
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True
    assert detection.diagnostics["candidate_scores"][0]["bright_core_area_px"] > 0
```

- [ ] Add a failing test for a green spot with a white core and green ring.

```python
def test_detects_green_ring_with_white_core_at_core_center():
    detector = LaserDetector(LaserColor.GREEN, _green_params())
    image = _draw_core_ring_dot((70, 52), core_radius=3, ring_radius=7, ring_bgr=(0, 255, 0))

    detection = detector.detect(image)

    assert detection.found is True
    assert detection.source == DetectionSource.MEASURED
    assert_center_near(detection, (70, 52), tolerance=1.5)
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True
```

- [ ] Add a failing test where a small red ring below the configured color contour area is rescued by a valid white core.

```python
def test_small_red_ring_on_dark_background_is_rescued_by_white_core():
    params = replace(_red_params(), area_px_min=40)
    detector = LaserDetector(LaserColor.RED, params)
    image = _draw_core_ring_dot((62, 50), core_radius=3, ring_radius=4, ring_bgr=(0, 0, 255), value=5)

    detection = detector.detect(image)

    assert detection.found is True
    assert_center_near(detection, (62, 50), tolerance=1.5)
    assert detection.area_px is not None
    assert detection.area_px >= params.area_px_min
    assert detection.diagnostics["candidate_scores"][0]["color_area_px"] < params.area_px_min
    assert detection.diagnostics["candidate_scores"][0]["core_found"] is True
```

- [ ] Add a failing test where a pure white point without red/green ring is rejected.

```python
def test_pure_white_bright_point_without_color_ring_is_rejected():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (60, 50), 5, (255, 255, 255), -1, lineType=cv2.LINE_AA)

    detection = detector.detect(image)

    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.diagnostics["candidate_count"] == 0
```

- [ ] Add failing diagnostics tests.

```python
def test_multiple_candidate_diagnostics_include_scores_and_best_center():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank()
    cv2.circle(image, (35, 45), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (95, 80), 5, (0, 0, 220), -1, lineType=cv2.LINE_AA)

    detection = detector.detect(image)

    assert detection.diagnostics["candidate_count"] == 2
    assert detection.diagnostics["best_center"] is not None
    assert detection.diagnostics["best_score"] is not None
    assert detection.diagnostics["second_best_score"] is not None
    assert len(detection.diagnostics["candidate_scores"]) == 2
    assert detection.diagnostics["candidate_scores"][0]["score"] >= detection.diagnostics["candidate_scores"][1]["score"]


def test_lost_detection_includes_failure_reason_in_diagnostics():
    detector = LaserDetector(LaserColor.RED, _red_params())

    detection = detector.detect(_blank())

    assert detection.found is False
    assert detection.failure_reason == FailureReason.NOT_FOUND
    assert detection.diagnostics["failure_reason"] == FailureReason.NOT_FOUND.value
```

- [ ] Run the new tests and confirm they fail for missing behavior, not syntax errors.

Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_laser_detector.py -v`

Expected: FAIL on the newly added core/ring and diagnostics assertions.

## Task 2: Add Core/Ring Candidate Evidence To LaserDetector

**Files:**
- Modify: `2023e/src/vision/laser_detector.py`
- Test: `2023e/tests/vision/test_laser_detector.py`

**Interfaces:**
- Consumes: existing `LaserDetectionParams` thresholds.
- Produces: `LaserDetection.diagnostics` with `best_center`, `candidate_scores`, and core/ring fields.

- [ ] Extend `_Candidate` with these fields.

```python
color_area_px: float
bright_core_area_px: float
combined_area_px: float
core_found: bool
```

- [ ] Change `_build_mask(...)` to return `mask`, `low_value_mask`, `bright_core_mask`, and `hsv`.

Implementation rule:

```text
bright_core_mask = V >= max(params.value_min, 180) and S <= 80
```

- [ ] In `_extract_candidates(...)`, keep contour extraction based on target color mask so a pure white point alone is not accepted.

- [ ] For each color contour, build a local search mask by dilating the contour mask with a `7x7` elliptical kernel. Intersect that local mask with `bright_core_mask` to find associated bright cores.

- [ ] Allow a contour with `area < params.area_px_min` only when it has an associated bright core. Use `combined_area_px = max(area, area + bright_core_area_px)` for area threshold semantics.

- [ ] Choose candidate center from the bright core centroid when `core_found=True`; otherwise use the existing brightness-weighted color contour center.

- [ ] Set `_Candidate.area_px` to `combined_area_px` and `_Candidate.color_area_px` to the raw color contour area.

- [ ] Update `_score_candidate(...)` to include a bright core score while keeping existing pure-color dots valid.

Scoring shape:

```python
core_score = 1.0 if core_found else 0.5
return 0.38 * brightness_score + 0.22 * circularity + 0.15 * area_score + 0.15 * temporal_score + 0.10 * core_score
```

- [ ] Add a helper to serialize candidate diagnostics.

```python
def _candidate_diagnostics(self, candidates: list[_Candidate]) -> list[dict[str, Any]]:
    return [
        {
            "center": (candidate.center.u_px, candidate.center.v_px),
            "score": candidate.score,
            "area_px": candidate.area_px,
            "color_area_px": candidate.color_area_px,
            "bright_core_area_px": candidate.bright_core_area_px,
            "combined_area_px": candidate.combined_area_px,
            "core_found": candidate.core_found,
            "brightness": candidate.brightness,
        }
        for candidate in candidates
    ]
```

- [ ] Ensure every return path sets `diagnostics["failure_reason"]` to the enum `.value` string for lost detections and `None` for measured detections.

- [ ] Ensure measured detections set:

```python
diagnostics["best_center"] = (best.center.u_px, best.center.v_px)
diagnostics["candidate_scores"] = self._candidate_diagnostics(candidates)
```

- [ ] Run targeted laser tests.

Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_laser_detector.py -v`

Expected: PASS.

## Task 3: Verify Integration Boundaries And Preview Smoke

**Files:**
- No production file changes expected unless tests reveal a boundary regression.
- Test: `2023e/tests/vision/test_laser_detector.py`, `2023e/tests/vision`, `2023e/tests/scripts`

**Interfaces:**
- Consumes: final `LaserDetector` behavior from Task 2.
- Produces: verified Vision Layer laser behavior with no Control/Screen coordinate leakage.

- [ ] Run all Vision tests.

Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision -q`

Expected: PASS.

- [ ] Run script tests.

Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/scripts -q`

Expected: PASS.

- [ ] Run video smoke test.

Run: `conda run -n opencv-learning python scripts/test/preview_vision_video.py --no-window --max-frames 30`

Expected: command exits `0`; A4 remains detected; red/green diagnostics in summaries remain readable.

- [ ] Run boundary grep.

Run: `rg "ScreenPoint|ScreenQuad|MotorCommand" src/vision`

Expected: no matches.

## Self-Review Notes

- The plan implements only core/ring laser stability and diagnostics.
- A4 quad filtering and Kalman ROI tuning are explicitly excluded.
- The public `LaserDetector.detect(...)` signature remains unchanged.
- Tests are isolated in `tests/vision` before preview smoke verification.
