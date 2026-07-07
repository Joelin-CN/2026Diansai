# Laser Core Ring Detection Design

## Goal

Stabilize red/green laser detection when the spot falls on the A4 black tape by detecting the observed laser structure: a bright white core surrounded by a red or green colored ring.

## Confirmed Observation

- On white background, red/green spots appear larger.
- On black tape, red/green spots become smaller and weaker.
- In both cases, the spot center is close to white, while red or green color mainly appears around the center.
- The existing HSV color blob detector can lose true spots because it requires enough colored pixels in the main hue/saturation/value mask.

## Scope

This work stays inside the Vision Layer.

Vision Layer may output only image-space values:

- `ImagePoint`
- `ImageQuad`
- detection dataclasses
- diagnostics

This work must not introduce:

- `ScreenPoint`
- `ScreenQuad`
- `MotorCommand`
- Control Layer logic
- Task Layer logic
- Kalman as a hard dependency

## Directory Rules

The new behavior should be tested in isolation before being merged into the formal detector.

- Isolated tests live under `2023e/tests/vision/`.
- Formal Vision Layer code lives under `2023e/src/vision/`.
- Do not put core detection logic in `2023e/scripts/test/`.
- Preview scripts may call the formal Vision Layer but must not contain duplicate detector logic.

## Recommended Architecture

Keep `LaserDetector` as the public API. Add a small internal candidate-building path that combines color-ring evidence with bright-core evidence.

The detection flow should be:

```text
BGR image
-> HSV
-> target color ring mask
-> bright white core mask
-> connect color ring candidates to nearby bright cores
-> score combined candidates
-> output measured LaserDetection or lost LaserDetection
```

The detector should still support the existing pure color blob behavior for synthetic and simple frames.

## Candidate Semantics

Each candidate should record:

- `center`: preferred output center.
- `radius_px`: estimated candidate radius.
- `area_px`: area used for existing detection semantics.
- `brightness`: max or representative brightness.
- `circularity`: contour shape score.
- `score`: final score.
- `color_area_px`: colored ring area.
- `bright_core_area_px`: bright white core area.
- `combined_area_px`: combined color/core area.
- `core_found`: whether a bright core was associated.

Center selection should prefer the bright core centroid when a core is found. If no core is found, fall back to the existing brightness-weighted color contour center.

## Failure Semantics

Keep existing `FailureReason` values unless a new reason is clearly necessary.

- Use `TOO_SMALL` when color evidence exists but is too small and no valid core association rescues it.
- Use `LOW_BRIGHTNESS` when low-value target hue evidence exists but cannot form a valid measured candidate.
- Use `NOT_FOUND` when neither target color evidence nor useful core/color association exists.
- Use `LOW_CONFIDENCE` when candidates exist but score below `confidence_min`.
- Use `MULTIPLE_CANDIDATES` when separate candidates remain too close in score to choose safely.

## Diagnostics

Every `LaserDetection` should include enough diagnostics to compare strict and relaxed detection.

Required diagnostic keys:

- `roi_used`
- `candidate_count`
- `rejected_candidates`
- `best_center`
- `best_score`
- `second_best_score`
- `candidate_scores`
- `failure_reason`

Each entry in `candidate_scores` should include:

- `center`
- `score`
- `area_px`
- `color_area_px`
- `bright_core_area_px`
- `combined_area_px`
- `core_found`
- `brightness`

## Testing Strategy

Use synthetic images to isolate the laser structure before integrating into preview/video behavior.

Required test cases:

- Existing pure red dot still detects.
- Existing pure green dot still detects.
- Red spot with white core and red ring detects at the white core center.
- Green spot with white core and green ring detects at the white core center.
- Small red ring below the old color area threshold is rescued when a valid white core is present.
- Small green ring below the old color area threshold is rescued when a valid white core is present.
- Pure white bright point without a red/green ring is rejected.
- Lost detections include `failure_reason` in diagnostics.
- Multiple candidate diagnostics include `best_score`, `second_best_score`, `best_center`, and `candidate_scores`.

## Non-Goals

- Do not tune Kalman filters in this step.
- Do not add motor/control behavior.
- Do not convert image coordinates to screen coordinates.
- Do not make A4 quad filtering a hard dependency of `LaserDetector`.
- Do not replace the current detector with a preview-only script implementation.
