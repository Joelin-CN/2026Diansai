# Vision Border Migration And Laser Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move accepted A4/screen border preview logic into the formal Vision Layer, then improve red/green laser tracking stability using diagnostics, ROI, and continuity scoring.

**Architecture:** Implement accepted border detectors as formal `src/vision` modules first, keeping all outputs in image coordinates. Then stabilize laser detection without Kalman as a hard dependency by improving diagnostics, candidate scoring, ROI use, and preview/runtime parameter handling.

**Tech Stack:** Python, OpenCV `cv2`, NumPy, pytest, existing `src/vision` dataclasses and pipeline.

## Global Constraints

- Vision Layer only outputs image-space structures: `ImagePoint`, `ImageQuad`, detection results, and diagnostics.
- Do not output `ScreenPoint`, `ScreenQuad`, task paths, control errors, motor commands, or homography-converted coordinates.
- A4 tape detection is `found=True` only when both `outer_quad` and `inner_quad` exist.
- Screen pencil square and A4 tape must use separate detection logic.
- Red/green system configs, trackers, and pipeline instances remain independent.
- Red/green laser control-consumable measurements must remain `found=True` and `source=measured` only.
- Kalman remains optional and is not part of this plan.

---

## File Structure

### New Files

- `src/vision/quad_utils.py`
  - Shared image-space quad helpers: contour-to-quad, line refinement, quad ordering, area, point containment, edge checks.

- `src/vision/tape_quad_detector.py`
  - Formal A4 black tape detector. Detects `outer_quad` and `inner_quad` using black-mask contour hierarchy and line-refined quadrilateral fitting.

- `src/vision/screen_detector.py`
  - Formal screen pencil square detector. Detects thin gray screen square separately from A4 tape.

- `tests/vision/test_tape_quad_detector.py`
  - Synthetic A4 black tape ring tests for outer/inner quads, missing inner contour, and full-frame rejection.

- `tests/vision/test_screen_detector.py`
  - Synthetic/controlled screen square tests and negative tests against A4-like black tape.

### Modified Files

- `src/vision/types.py`
  - Extend `TapeQuadDetection` to include `outer_quad`, `inner_quad`, `inner_area_px`, `tape_ratio`, and diagnostics.
  - Optionally extend `ScreenDetection` with diagnostics and clearer image-space fields.

- `src/vision/pipeline.py`
  - Instantiate and run `TapeQuadDetector` in `RUNTIME_RED_A4_TAPE` and `DEBUG` for red profile.
  - Instantiate and run `ScreenDetector` only in `DEBUG` or explicit calibration/self-check mode.
  - Keep green profile laser-only unless explicitly in debug.

- `src/vision/diagnostics.py`
  - Add detector diagnostics for `a4_tape` and `screen_square`.

- `scripts/test/border_preview_detectors.py`
  - Remove or convert to thin wrapper around formal `src/vision` detectors after migration.

- `scripts/test/preview_vision_detection.py`
  - Call formal `TapeQuadDetector` and `ScreenDetector` instead of script-level duplicate logic.

- `scripts/test/preview_vision_video.py`
  - Call formal detectors, draw `a4_outer`, `a4_inner`, and `screen_square` overlays.
  - Keep `--border-interval` default `1` for frame-by-frame A4 tracking.

- `src/vision/laser_detector.py`
  - Later phase: improve laser diagnostics and candidate scoring for stability.

- `src/vision/state_tracker.py`
  - Later phase: expose configurable ROI radius / max jump inputs if needed.

- `tests/vision/test_laser_detector.py`
  - Later phase: add stability tests for temporal candidate selection, jump rejection, and ROI use.

- `tests/vision/test_pipeline.py`
  - Add tests for tape/screen detector aggregation and ensure red/green independence remains intact.

---

## Phase 1: Formalize Border Detectors

### Task 1: Add Quad Utility Module

**Files:**
- Create: `src/vision/quad_utils.py`
- Test: `tests/vision/test_tape_quad_detector.py`

**Interfaces:**
- Produces: `order_quad_points(points: np.ndarray) -> np.ndarray`
- Produces: `contour_to_refined_quad(contour: np.ndarray) -> np.ndarray | None`
- Produces: `quad_to_image_quad(quad: np.ndarray) -> ImageQuad`
- Produces: `quad_area(quad: np.ndarray) -> float`
- Produces: `quad_inside_quad(inner: np.ndarray, outer: np.ndarray) -> bool`

- [ ] Write a failing synthetic test that imports `contour_to_refined_quad` and checks a skewed quadrilateral contour is recovered within 4 px.

```python
def test_contour_to_refined_quad_recovers_skewed_quad():
    contour = np.array([[[80, 50]], [[250, 60]], [[235, 190]], [[65, 180]]], dtype=np.int32)
    quad = contour_to_refined_quad(contour)
    assert quad is not None
    assert max_corner_distance(quad, contour.reshape(4, 2)) <= 4.0
```

- [ ] Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_tape_quad_detector.py::test_contour_to_refined_quad_recovers_skewed_quad -v`
- [ ] Expected: FAIL with import error for `vision.quad_utils`.
- [ ] Implement `src/vision/quad_utils.py` by moving the accepted helper logic from `scripts/test/border_preview_detectors.py`.
- [ ] Run the same test and confirm PASS.

### Task 2: Add Formal A4 Tape Quad Detector

**Files:**
- Create: `src/vision/tape_quad_detector.py`
- Modify: `src/vision/types.py`
- Test: `tests/vision/test_tape_quad_detector.py`

**Interfaces:**
- Produces: `TapeQuadDetector.detect(image: np.ndarray, roi: ImageRoi | None = None) -> TapeQuadDetection`
- Produces: `TapeQuadDetection.outer_quad: ImageQuad | None`
- Produces: `TapeQuadDetection.inner_quad: ImageQuad | None`

- [ ] Write a failing test for synthetic A4 black tape ring.

```python
def test_tape_quad_detector_finds_outer_and_inner_quads():
    image = make_a4_tape_ring()
    detection = TapeQuadDetector().detect(image)
    assert detection.found is True
    assert detection.outer_quad is not None
    assert detection.inner_quad is not None
    assert detection.tape_ratio is not None
    assert 0.08 <= detection.tape_ratio <= 0.65
    assert detection.source == DetectionSource.MEASURED
```

- [ ] Write a failing test that a solid black rectangle without an inner contour returns `found=False` and `failure_reason=FailureReason.NOT_FOUND` or a new `FailureReason.INNER_CONTOUR_NOT_FOUND` if added.
- [ ] Implement `TapeQuadDetector` using accepted preview algorithm: gray, adaptive black mask, morphology close, `RETR_TREE`, outer/inner pairing, line-refined quads.
- [ ] Convert numpy quads to `ImageQuad` for formal output.
- [ ] Keep `image_corners` and `image_corners_ordered` as outer quad points for backward compatibility.
- [ ] Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_tape_quad_detector.py -v`
- [ ] Expected: PASS.

### Task 3: Add Formal Screen Detector

**Files:**
- Create: `src/vision/screen_detector.py`
- Modify: `src/vision/types.py` if diagnostics need extension
- Test: `tests/vision/test_screen_detector.py`

**Interfaces:**
- Produces: `ScreenDetector.detect(image: np.ndarray, exclude_quad: ImageQuad | None = None) -> ScreenDetection`

- [ ] Write a failing test for a synthetic gray thin square on a light background.

```python
def test_screen_detector_finds_gray_thin_square():
    image = make_gray_pencil_square()
    detection = ScreenDetector().detect(image)
    assert detection.found is True
    assert detection.image_corners is not None
    assert detection.source == DetectionSource.MEASURED
```

- [ ] Write a negative test that a black A4 ring alone is not accepted as `screen_square` when passed as `exclude_quad`.
- [ ] Implement `ScreenDetector` using CLAHE/Canny/contour quad scoring from accepted preview logic.
- [ ] Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_screen_detector.py -v`
- [ ] Expected: PASS.

### Task 4: Integrate Border Detectors Into VisionPipeline

**Files:**
- Modify: `src/vision/pipeline.py`
- Modify: `src/vision/diagnostics.py`
- Test: `tests/vision/test_pipeline.py`

**Interfaces:**
- `VisionFrameResult.tape_quad` receives formal A4 detector result in `RUNTIME_RED_A4_TAPE` and `DEBUG` for red profile.
- `VisionFrameResult.screen_detection` receives formal screen detector result only in `DEBUG` or explicit calibration/self-check mode.

- [ ] Write failing test: red pipeline in `RUNTIME_RED_A4_TAPE` returns `tape_quad.found=True` for synthetic ring image.
- [ ] Write failing test: red pipeline in `RUNTIME_RED_CENTER` does not run tape detector.
- [ ] Write failing test: green pipeline remains independent and does not run A4 tape in `RUNTIME_GREEN_TRACKING`.
- [ ] Implement pipeline detector instantiation and mode gating.
- [ ] Add diagnostics keys: `a4_tape`, `screen_square` with candidate/area/failure metadata.
- [ ] Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_pipeline.py -v`
- [ ] Expected: PASS.

### Task 5: Update Preview Scripts To Use Formal Detectors

**Files:**
- Modify: `scripts/test/border_preview_detectors.py`
- Modify: `scripts/test/preview_vision_detection.py`
- Modify: `scripts/test/preview_vision_video.py`
- Modify: `scripts/test/debug_a4_border_overlay.py`
- Test: `tests/scripts/test_border_preview_detectors.py`

**Interfaces:**
- Preview scripts call formal `TapeQuadDetector` / `ScreenDetector`.
- `scripts/test/border_preview_detectors.py` becomes a wrapper or is reduced to drawing helpers.

- [ ] Update script tests to import formal detectors where possible.
- [ ] Update drawing functions to consume formal `ImageQuad` outputs.
- [ ] Keep video overlays: magenta `a4_outer`, yellow `a4_inner`, cyan `screen_square`.
- [ ] Keep `--border-interval` default `1`.
- [ ] Run: `conda run -n opencv-learning python scripts/test/preview_vision_video.py --no-window --max-frames 5`
- [ ] Expected: A4 logs show `a4_tape=True` with outer/inner/tape ratio.
- [ ] Run: `conda run -n opencv-learning python scripts/test/debug_a4_border_overlay.py`
- [ ] Expected: debug images are written with real file sizes.

---

## Phase 2: Stabilize Red/Green Laser Tracking

### Task 6: Add Laser Detection Diagnostics And Parameter Surface

**Files:**
- Modify: `src/vision/laser_detector.py`
- Modify: `src/vision/config.py` if adding optional defaults
- Test: `tests/vision/test_laser_detector.py`

**Interfaces:**
- Existing `LaserDetection.diagnostics` gains:
  - `best_center`
  - `best_score`
  - `second_best_score`
  - `candidate_scores`
  - `roi_used`
  - `failure_reason`

- [ ] Write failing test that multiple candidate diagnostics include best and second scores.
- [ ] Write failing test that a lost detection includes `failure_reason` inside diagnostics.
- [ ] Implement diagnostics without changing measured/lost semantics.
- [ ] Run: `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_laser_detector.py -v`
- [ ] Expected: PASS.

### Task 7: Improve Candidate Continuity Scoring

**Files:**
- Modify: `src/vision/laser_detector.py`
- Test: `tests/vision/test_laser_detector.py`

**Interfaces:**
- Optional detector defaults:
  - `temporal_weight`
  - `max_jump_px`
  - `jump_reject_px`
  - `multi_candidate_margin`

- [ ] Write failing test where a brighter far distractor exists but previous detection should keep the near candidate.
- [ ] Write failing test where all candidates exceed `jump_reject_px` and detection returns lost/multiple candidates instead of jumping.
- [ ] Increase temporal scoring weight and add jump penalty/rejection in `_score_candidate` or candidate filtering.
- [ ] Preserve rule: no predicted-only `found=True` output.
- [ ] Run targeted laser tests.

### Task 8: Use A4 Inner Quad As Optional Laser Constraint In Preview

**Files:**
- Modify: `scripts/test/preview_vision_video.py`
- Modify: `scripts/test/preview_vision_detection.py`
- Possibly add helper in `src/vision/quad_utils.py`: `point_in_quad(point, quad)`

**Interfaces:**
- Preview laser detection can receive A4 inner/outer quads and prefer candidates inside/near A4 target area.
- This remains preview behavior unless later promoted into formal task-specific pipeline mode.

- [ ] Add point-in-quad helper tests.
- [ ] Filter or down-rank laser candidates outside the A4 outer quad in preview mode, without changing formal `LaserDetector` API if avoidable.
- [ ] If formal API change is needed, add optional `accepted_region_quad` / `rejected_region_quad` with default `None`.
- [ ] Run video smoke test and inspect candidate jumps.

### Task 9: Preview Parameter Cleanup

**Files:**
- Modify: `scripts/test/preview_vision_video.py`
- Modify: `scripts/test/preview_vision_detection.py`
- Optional: create `configs/common/vision_preview.json` only if hardcoded relaxed params become unmanageable.

**Interfaces:**
- Keep strict detector output visible.
- Keep relaxed detector output visible, but make relaxed parameters explicit and shared by image/video scripts.

- [ ] Extract relaxed red/green params into one helper function or small preview config loader.
- [ ] Ensure relaxed red/green use independent previous detections.
- [ ] Keep console summaries clear: strict vs relaxed.
- [ ] Run preview smoke tests.

---

## Final Verification

- [ ] Run full Vision tests:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest "tests/vision" -q
```

- [ ] Run script tests:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest "tests/scripts" -q
```

- [ ] Run video smoke test:

```powershell
conda run -n opencv-learning python "scripts/test/preview_vision_video.py" --no-window --max-frames 30
```

- [ ] Run debug overlay export:

```powershell
conda run -n opencv-learning python "scripts/test/debug_a4_border_overlay.py"
```

- [ ] Boundary grep:

```powershell
rg "ScreenPoint|ScreenQuad|MotorCommand|homography|motor" src/vision
```

Expected result: no newly introduced control/motor/screen-coordinate output in Vision Layer.

## Recommended Execution Order

1. Complete Phase 1 and visually re-check A4 outer/inner in video.
2. Only after Phase 1 is stable, begin Phase 2 laser tracking stabilization.
3. If laser tracking changes make A4 border overlays worse, revert only Phase 2 changes and keep Phase 1.
