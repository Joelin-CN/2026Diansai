# White Core Reacquire Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add diagnostic-first white-core reacquire analysis for bounded task regions so we can evaluate whether white laser cores plus local red/green color correlation improve black-tape tracking stability.

**Architecture:** Keep the formal runtime detector path unchanged in phase 1. Add a small internal white-core candidate analysis helper to `LaserDetector`, expose it through diagnostics, and add debug preview summary fields so black-tape reacquire behavior can be inspected without full-frame relaxed fallback.

**Tech Stack:** Python 3.12, OpenCV (`cv2`), NumPy, pytest, existing `2023e/src/vision` dataclasses and diagnostics.

## Global Constraints

- Vision Layer outputs remain limited to `ImagePoint`, `ImageQuad`, detection dataclasses, and diagnostics.
- Do not introduce `ScreenPoint`, `ScreenQuad`, `MotorCommand`, Control Layer logic, or Kalman as a hard dependency.
- Formal runtime must not return to unconstrained full-frame relaxed fallback.
- Phase 1 is diagnostic-first and must not replace formal runtime detector selection logic.
- White-core reacquire analysis must operate only inside the provided bounded ROI.
- Use TDD for production code changes: write a failing test, verify it fails, implement minimal code, verify it passes.
- Do not commit changes unless the user explicitly requests a commit.

---

## File Structure

- Modify `2023e/src/vision/laser_detector.py`
  - Add a private `_WhiteCoreCandidate` dataclass.
  - Add private white-core extraction and local color-correlation helpers.
  - Add an optional diagnostic-only white-core analysis call inside `LaserDetector.detect` when a previous detection or ROI is available.
  - Keep the public `LaserDetector.detect(...) -> LaserDetection` API unchanged.
- Modify `2023e/scripts/test/preview_vision_video.py`
  - Extend debug/runtime summaries with compact white-core diagnostic fields when present.
  - Do not add duplicate detector logic to the script.
- Modify `2023e/tests/vision/test_laser_detector.py`
  - Add synthetic tests for red/green local color correlation and pure-white rejection diagnostics.
- Modify `2023e/tests/vision/test_pipeline.py`
  - Add or update one diagnostics propagation assertion if needed after `LaserDetector` diagnostics grow.

---

### Task 1: White-Core Local Color Correlation Tests

**Files:**
- Modify: `2023e/tests/vision/test_laser_detector.py`
- Modify: `2023e/src/vision/laser_detector.py`

**Interfaces:**
- Consumes: `LaserDetector.detect(image, roi=None, previous_detection=None) -> LaserDetection`
- Produces diagnostics keys inside `LaserDetection.diagnostics`:
  - `white_core_candidate_count: int`
  - `white_core_candidates: list[dict[str, Any]]`
  - each candidate includes `center`, `core_area_px`, `brightness`, `distance_to_previous`, `local_red_score`, `local_green_score`, `rank_score`

- [ ] **Step 1: Add failing red local-correlation test**

Append this test to `2023e/tests/vision/test_laser_detector.py`:

```python
def test_white_core_diagnostics_score_red_support_above_green():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank(value=5)
    cv2.circle(image, (60, 50), 6, (0, 0, 180), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (60, 50), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.RED,
        ImagePoint(59, 50),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert abs(best["center"][0] - 60) <= 2.0
    assert abs(best["center"][1] - 50) <= 2.0
    assert best["local_red_score"] > best["local_green_score"]
    assert best["local_red_score"] > 0.0
```

- [ ] **Step 2: Run the red test and verify it fails for missing diagnostics**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py::test_white_core_diagnostics_score_red_support_above_green" -q
```

Expected: FAIL with `KeyError: 'white_core_candidates'` or equivalent missing diagnostic key.

- [ ] **Step 3: Add minimal white-core diagnostics implementation**

In `2023e/src/vision/laser_detector.py`, add this dataclass after `_Candidate`:

```python
@dataclass(frozen=True)
class _WhiteCoreCandidate:
    center: ImagePoint
    core_area_px: float
    brightness: float
    distance_to_previous: float | None
    local_red_score: float
    local_green_score: float
    rank_score: float
```

Add default diagnostic fields in `LaserDetector.detect` immediately after `"failure_reason": None,`:

```python
            "white_core_candidate_count": 0,
            "white_core_candidates": [],
```

After `mask, low_value_mask, bright_core_mask, hsv = self._build_mask(crop)`, add:

```python
        white_core_candidates = self._analyze_white_core_candidates(
            bright_core_mask,
            hsv,
            offset_x,
            offset_y,
            previous_detection,
        )
        diagnostics["white_core_candidate_count"] = len(white_core_candidates)
        diagnostics["white_core_candidates"] = self._white_core_candidate_diagnostics(white_core_candidates)
```

Add these private methods before `_extract_candidates`:

```python
    def _analyze_white_core_candidates(
        self,
        bright_core_mask: np.ndarray,
        hsv: np.ndarray,
        offset_x: int,
        offset_y: int,
        previous_detection: LaserDetection | None,
    ) -> list[_WhiteCoreCandidate]:
        contours, _ = cv2.findContours(bright_core_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        candidates: list[_WhiteCoreCandidate] = []
        value_channel = hsv[:, :, 2]
        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area < 2.0 or area > 300.0:
                continue
            contour_mask = np.zeros(bright_core_mask.shape, dtype=np.uint8)
            cv2.drawContours(contour_mask, [contour], -1, 255, -1)
            center = self._brightness_weighted_center(contour_mask, value_channel, offset_x, offset_y)
            brightness_values = value_channel[contour_mask > 0]
            brightness = float(brightness_values.max() if brightness_values.size else 0.0)
            local_red_score = self._local_color_score(hsv, center, offset_x, offset_y, LaserColor.RED)
            local_green_score = self._local_color_score(hsv, center, offset_x, offset_y, LaserColor.GREEN)
            distance = None
            temporal_score = 0.5
            if previous_detection and previous_detection.found and previous_detection.image_center:
                distance = math.hypot(center.u_px - previous_detection.image_center.u_px, center.v_px - previous_detection.image_center.v_px)
                temporal_score = max(0.0, 1.0 - distance / 100.0)
            color_score = local_red_score if self.color == LaserColor.RED else local_green_score
            brightness_score = min(1.0, brightness / 255.0)
            area_score = min(1.0, area / 30.0)
            rank_score = 0.35 * color_score + 0.30 * temporal_score + 0.20 * brightness_score + 0.15 * area_score
            candidates.append(
                _WhiteCoreCandidate(
                    center=center,
                    core_area_px=area,
                    brightness=brightness,
                    distance_to_previous=distance,
                    local_red_score=local_red_score,
                    local_green_score=local_green_score,
                    rank_score=rank_score,
                )
            )
        candidates.sort(key=lambda candidate: candidate.rank_score, reverse=True)
        return candidates

    def _local_color_score(
        self,
        hsv: np.ndarray,
        center: ImagePoint,
        offset_x: int,
        offset_y: int,
        color: LaserColor,
    ) -> float:
        radius = 9
        center_x = int(round(center.u_px - offset_x))
        center_y = int(round(center.v_px - offset_y))
        x1 = max(0, center_x - radius)
        y1 = max(0, center_y - radius)
        x2 = min(hsv.shape[1], center_x + radius + 1)
        y2 = min(hsv.shape[0], center_y + radius + 1)
        if x1 >= x2 or y1 >= y2:
            return 0.0

        patch = hsv[y1:y2, x1:x2]
        hue = patch[:, :, 0]
        saturation = patch[:, :, 1].astype(np.float32) / 255.0
        value = patch[:, :, 2].astype(np.float32) / 255.0
        if color == LaserColor.RED:
            hue_mask = (hue <= 10) | (hue >= 170)
        else:
            hue_mask = (hue >= 45) & (hue <= 85)

        yy, xx = np.indices(hue.shape)
        local_center_x = center_x - x1
        local_center_y = center_y - y1
        distance = np.hypot(xx - local_center_x, yy - local_center_y)
        spatial_weight = np.clip(1.0 - distance / float(radius + 1), 0.0, 1.0)
        weighted_support = hue_mask.astype(np.float32) * saturation * value * spatial_weight
        return float(min(1.0, weighted_support.sum() / 6.0))

    def _white_core_candidate_diagnostics(self, candidates: list[_WhiteCoreCandidate]) -> list[dict[str, Any]]:
        return [
            {
                "center": (candidate.center.u_px, candidate.center.v_px),
                "core_area_px": candidate.core_area_px,
                "brightness": candidate.brightness,
                "distance_to_previous": candidate.distance_to_previous,
                "local_red_score": candidate.local_red_score,
                "local_green_score": candidate.local_green_score,
                "rank_score": candidate.rank_score,
            }
            for candidate in candidates[:10]
        ]
```

- [ ] **Step 4: Run the red test and verify it passes**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py::test_white_core_diagnostics_score_red_support_above_green" -q
```

Expected: PASS.

---

### Task 2: Green Correlation And Pure-White Guard Tests

**Files:**
- Modify: `2023e/tests/vision/test_laser_detector.py`
- Modify: `2023e/src/vision/laser_detector.py`

**Interfaces:**
- Consumes diagnostics from Task 1.
- Produces stable behavior where local color scoring separates green support from red support and pure-white reflections remain low color-score candidates.

- [ ] **Step 1: Add failing green and pure-white tests**

Append these tests to `2023e/tests/vision/test_laser_detector.py`:

```python
def test_white_core_diagnostics_score_green_support_above_red():
    detector = LaserDetector(LaserColor.GREEN, _green_params())
    image = _blank(value=5)
    cv2.circle(image, (65, 45), 6, (0, 180, 0), -1, lineType=cv2.LINE_AA)
    cv2.circle(image, (65, 45), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.GREEN,
        ImagePoint(66, 45),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert abs(best["center"][0] - 65) <= 2.0
    assert abs(best["center"][1] - 45) <= 2.0
    assert best["local_green_score"] > best["local_red_score"]
    assert best["local_green_score"] > 0.0


def test_white_core_diagnostics_keep_pure_white_color_scores_low():
    detector = LaserDetector(LaserColor.RED, _red_params())
    image = _blank(value=5)
    cv2.circle(image, (60, 50), 4, (245, 245, 245), -1, lineType=cv2.LINE_AA)
    previous = LaserDetection.measured(
        LaserColor.RED,
        ImagePoint(60, 50),
        image_radius_px=5,
        area_px=40,
        brightness=255,
        confidence=0.9,
    )

    detection = detector.detect(image, previous_detection=previous)

    candidates = detection.diagnostics["white_core_candidates"]
    assert detection.diagnostics["white_core_candidate_count"] >= 1
    best = candidates[0]
    assert best["local_red_score"] < 0.05
    assert best["local_green_score"] < 0.05
```

- [ ] **Step 2: Run the new tests and verify expected behavior**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py::test_white_core_diagnostics_score_green_support_above_red" "tests/vision/test_laser_detector.py::test_white_core_diagnostics_keep_pure_white_color_scores_low" -q
```

Expected before Task 1 implementation: FAIL for missing diagnostics. Expected after Task 1 implementation: PASS. If either fails because scoring is too weak or too strong, adjust only `_local_color_score` normalization in `laser_detector.py` and rerun.

- [ ] **Step 3: Run all laser detector tests**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py" -q
```

Expected: all tests pass.

---

### Task 3: Preview Debug Summary For White-Core Evidence

**Files:**
- Modify: `2023e/scripts/test/preview_vision_video.py`
- Test manually with: `scripts/test/preview_vision_video.py --path-mode debug --no-window --max-frames 120`

**Interfaces:**
- Consumes `LaserDetection.diagnostics["white_core_candidate_count"]` and `LaserDetection.diagnostics["white_core_candidates"]` from Tasks 1-2.
- Produces compact summary tokens such as `red_wc=53:top=(692.6,346.2):r=0.71:g=0.00`.

- [ ] **Step 1: Add summary helper**

In `2023e/scripts/test/preview_vision_video.py`, add this function after `format_detection`:

```python
def format_white_core_summary(label: str, detection: LaserDetection) -> str:
    diagnostics = detection.diagnostics or {}
    count = diagnostics.get("white_core_candidate_count")
    candidates = diagnostics.get("white_core_candidates") or []
    if count is None:
        return f"{label}_wc=na"
    if not candidates:
        return f"{label}_wc={count}:top=none"
    top = candidates[0]
    center = top.get("center") or (0.0, 0.0)
    return (
        f"{label}_wc={count}:top=({center[0]:.1f},{center[1]:.1f})"
        f":r={top.get('local_red_score', 0.0):.2f}"
        f":g={top.get('local_green_score', 0.0):.2f}"
    )
```

- [ ] **Step 2: Include white-core summaries in debug output**

In `build_summary(...)`, after these lines:

```python
    parts.append(format_detection("red", strict_red))
    parts.append(format_detection("green", strict_green))
```

add:

```python
    parts.append(format_white_core_summary("red", strict_red))
    parts.append(format_white_core_summary("green", strict_green))
```

After this block:

```python
    if relaxed_red is not None:
        parts.append(format_detection("red_relaxed", relaxed_red))
    if relaxed_green is not None:
        parts.append(format_detection("green_relaxed", relaxed_green))
```

add:

```python
    if relaxed_red is not None:
        parts.append(format_white_core_summary("red_relaxed", relaxed_red))
    if relaxed_green is not None:
        parts.append(format_white_core_summary("green_relaxed", relaxed_green))
```

- [ ] **Step 3: Run script smoke check**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/preview_vision_video.py" --path-mode debug --no-window --max-frames 20
```

Expected: command exits with code 0 and printed summaries include `red_wc=` and `green_wc=` tokens.

---

### Task 4: Bounded Reacquire Evidence Collection Command

**Files:**
- No production file changes required.
- Use existing `2023e/scripts/test/preview_vision_video.py` and `2023e/src/vision/laser_detector.py` diagnostics.

**Interfaces:**
- Consumes white-core diagnostics from Tasks 1-3.
- Produces console evidence for frames around black-tape reacquire behavior.

- [ ] **Step 1: Run 120-frame debug preview**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/preview_vision_video.py" --path-mode debug --no-window --max-frames 120
```

Expected: command exits with code 0 and summaries include white-core counts and top red/green local scores.

- [ ] **Step 2: Inspect black-tape frames manually from console output**

Look at frames where strict red or green reports `lost:too_small`, `lost:low_brightness`, or `lost:low_confidence`.

Record these facts in the session summary:

```text
frame range inspected: 41..120
red white-core count pattern: <observed range>
green white-core count pattern: <observed range>
top red local score pattern: <observed pattern>
top green local score pattern: <observed pattern>
whether true-looking candidates remain near previous track: <yes/no/unclear>
```

Do not edit code in this step.

---

### Task 5: Final Verification

**Files:**
- Verify all changed code and tests.

**Interfaces:**
- Confirms phase-1 diagnostic path works without changing runtime mode semantics.

- [ ] **Step 1: Run focused laser tests**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision/test_laser_detector.py" -q
```

Expected: all tests pass.

- [ ] **Step 2: Run vision test suite**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/vision" -q
```

Expected: all vision tests pass.

- [ ] **Step 3: Run debug preview smoke test**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/preview_vision_video.py" --path-mode debug --no-window --max-frames 120
```

Expected: command exits with code 0 and printed summaries include white-core diagnostics.

- [ ] **Step 4: Confirm runtime path remains bounded**

Inspect `2023e/src/vision/pipeline.py` and confirm `_run_laser(...)` still calls:

```python
roi = self.tracker.suggest_roi(color)
previous = self.tracker.last_detection_for(color)
detection = detector.detect(frame.image, roi=roi, previous_detection=previous)
self.tracker.update(color, detection)
```

Expected: no full-frame relaxed fallback is added to formal runtime.

---

## Self-Review Notes

- Spec coverage: The plan covers diagnostic-first implementation, bounded ROI behavior, candidate semantics, local color correlation, diagnostics, synthetic tests, and debug-video evidence collection.
- Placeholder scan: No `TBD`, `TODO`, or open-ended implementation placeholders remain.
- Type consistency: The plan uses only existing public APIs plus new private `_WhiteCoreCandidate` and diagnostics keys defined in Task 1.
- Constraint check: The plan does not add screen-space outputs, motor/control behavior, Kalman, or full-frame runtime fallback.
