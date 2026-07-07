# Vision Layer Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first-stage Vision Layer minimum testable loop for the 2023E OpenCV project.

**Architecture:** `VisionPipeline` receives `CameraFrame`, enables configured laser detectors, asks `VisionStateTracker` for ROI, updates tracker state with measured/lost results, and returns image-space detections plus diagnostics. Red and green systems share detector code but use independent config, tracker, and pipeline instances.

**Tech Stack:** Python, OpenCV `cv2`, NumPy, pytest, stdlib `dataclasses`, `enum`, `json`, `pathlib`, `time`.

## Global Constraints

- Vision Layer only outputs `ImagePoint`, `ImageQuad`, detection results, and diagnostics.
- Do not output `ScreenPoint`, `ScreenQuad`, or `MotorCommand`.
- Do not implement Coordinate Layer Homography conversion.
- Do not implement Control Layer, Task Layer, or stepper motor drivers.
- `found=True` only represents a real measured observation from the current frame with `source=measured`.
- Predicted points must not be reported as `found=True` measurements.
- Kalman is not a first-stage dependency.
- Red and green systems must use independent config objects, tracker state, and pipeline instances.
- Red hue supports `[0, 10]` and `[170, 180]`; green hue supports `[45, 85]`.
- OpenCV hue `180` in config maps to max OpenCV hue `179`.

---

### Task 1: Types And Config

**Files:**
- Create: `src/vision/__init__.py`
- Create: `src/vision/types.py`
- Create: `src/vision/config.py`
- Test: `tests/vision/test_config.py`

**Interfaces:**
- Consumes: `configs/red/vision.json`, `configs/green/vision.json`
- Produces: `load_vision_config(path) -> VisionRuntimeConfig`

- [ ] Write failing config and type tests.
- [ ] Run `pytest tests/vision/test_config.py -v` and confirm import failure.
- [ ] Implement enums, dataclasses, and JSON config loader.
- [ ] Run `pytest tests/vision/test_config.py -v` and confirm pass.

### Task 2: Laser Detector Basics

**Files:**
- Create: `src/vision/laser_detector.py`
- Test: `tests/vision/test_laser_detector.py`

**Interfaces:**
- Consumes: `LaserDetectionParams`, `LaserColor`, `ImageRoi`, `LaserDetection`
- Produces: `LaserDetector.detect(image, roi=None, previous_detection=None) -> LaserDetection`

- [ ] Write failing synthetic red/green dot tests.
- [ ] Include red hue low-side and high-side HSV tests.
- [ ] Run targeted tests and confirm failure.
- [ ] Implement HSV multi-range mask, contour extraction, and measured detection output.
- [ ] Run targeted tests and confirm pass.

### Task 3: ROI And Failure Reasons

**Files:**
- Modify: `src/vision/laser_detector.py`
- Test: `tests/vision/test_laser_detector.py`

**Interfaces:**
- Consumes: `ImageRoi`, `FailureReason`
- Produces: full-frame output coordinates after ROI crop

- [ ] Write failing tests for ROI coordinate preservation and explicit failure reasons.
- [ ] Run targeted tests and confirm failure.
- [ ] Implement ROI clipping, offset restoration, and failure reason precedence.
- [ ] Run targeted tests and confirm pass.

### Task 4: Multi-Candidate Scoring

**Files:**
- Modify: `src/vision/laser_detector.py`
- Test: `tests/vision/test_laser_detector.py`

**Interfaces:**
- Consumes: optional `previous_detection`
- Produces: temporally stable candidate selection or `multiple_candidates`

- [ ] Write failing tests for temporal candidate selection and ambiguous candidates.
- [ ] Run targeted tests and confirm failure.
- [ ] Implement candidate scoring with brightness, shape, area, and temporal terms.
- [ ] Run targeted tests and confirm pass.

### Task 5: State Tracker

**Files:**
- Create: `src/vision/state_tracker.py`
- Test: `tests/vision/test_state_tracker.py`

**Interfaces:**
- Produces: `VisionStateTracker.suggest_roi`, `update`, `state_for`, `lost_count_for`

- [ ] Write failing tests for `UNINITIALIZED`, `TRACKING`, `TEMP_LOST`, `REACQUIRING`, and `LOST`.
- [ ] Run targeted tests and confirm failure.
- [ ] Implement per-color state, lost counts, last measured detection, and ROI expansion.
- [ ] Run targeted tests and confirm pass.

### Task 6: Pipeline And Diagnostics

**Files:**
- Create: `src/vision/diagnostics.py`
- Create: `src/vision/pipeline.py`
- Test: `tests/vision/test_pipeline.py`

**Interfaces:**
- Consumes: `CameraFrame`, `VisionRuntimeConfig`, `VisionStateTracker`, `LaserDetector`
- Produces: `VisionPipeline.process(frame, mode=VisionMode.DEBUG) -> VisionFrameResult`

- [ ] Write failing tests for red and green profile aggregation.
- [ ] Run targeted tests and confirm failure.
- [ ] Implement profile detector selection, tracker updates, result aggregation, and diagnostics.
- [ ] Run targeted tests and confirm pass.

### Task 7: Full Verification

**Files:**
- All files above.

- [ ] Run `$env:PYTHONPATH="src"; pytest tests/vision -v` from `2023e`.
- [ ] Inspect changes and confirm no coordinate/control/motor layer leakage.
- [ ] Fix only issues related to first-stage Vision Layer.
