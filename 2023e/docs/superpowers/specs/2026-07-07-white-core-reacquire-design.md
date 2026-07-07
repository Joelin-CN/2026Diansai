# White Core Reacquire Design

## Goal

Evaluate whether black-tape laser instability can be reduced by switching the reacquire analysis path from color-first detection to white-core-first candidate analysis inside a bounded task region.

This step is diagnostic-first. It does not immediately replace the formal runtime laser path.

## Problem Statement

Current evidence shows two related issues on the black tape region:

- `red` strict detection frequently fails as `too_small`, `low_brightness`, or `low_confidence`.
- When a relaxed debug search is used for evidence, many bright-core-heavy candidates appear, and several candidates have very weak red support but still rank near the top.

The working hypothesis is:

- the bright white laser core may be more stable than the colored ring on black tape
- the difficult part is not always finding a bright point, but deciding which bright point is the real laser and what color it belongs to

## Scope

This work stays inside the Vision Layer.

Allowed outputs remain:

- `ImagePoint`
- `ImageQuad`
- detection dataclasses
- diagnostics

This work must not introduce:

- `ScreenPoint`
- `ScreenQuad`
- `MotorCommand`
- Control Layer logic
- Kalman as a hard dependency

## Runtime Boundary

Formal runtime keeps the current bounded search architecture:

- `TRACKING` uses a local tracker ROI
- `REACQUIRING` may use a bounded task region such as `screen_roi` or A4 support ROI
- formal runtime must not return to unconstrained full-frame relaxed fallback

The first implementation step is limited to debug evidence collection and isolated tests. It does not yet replace the formal runtime detector selection logic.

## Proposed Direction

Add an experimental white-core-first reacquire analysis path that operates only inside a bounded ROI.

The analysis flow is:

```text
bounded reacquire ROI
-> white core candidate extraction
-> candidate pruning
-> local red/green color correlation around each core
-> candidate diagnostics and ranking evidence
```

This path is intended to answer three questions before any runtime integration:

1. Is the white-core candidate count still manageable inside bounded reacquire ROIs?
2. Does the true laser core remain near the top candidates across black-tape frames?
3. Is local color evidence around the core sufficient to distinguish red and green lasers from white reflections?

## Candidate Semantics

Each experimental white-core candidate should record at least:

- `center`
- `core_area_px`
- `brightness`
- `distance_to_previous`
- `local_red_score`
- `local_green_score`
- `rank_score`

Optional diagnostics may also include:

- local patch radius
- color-support pixel counts
- whether the candidate lies near the bounded ROI edge

## White Core Extraction

White core extraction should favor small, bright, low-saturation points that resemble the observed laser center.

The extractor should:

- operate only inside the provided bounded ROI
- reject obviously large bright regions early
- keep small bright cores that may still correspond to real lasers on black tape
- produce a deduplicated candidate list suitable for downstream ranking

The goal is not to identify the final laser directly, only to produce plausible core candidates.

## Local Color Correlation

For each white core candidate, compute color evidence in a small neighborhood around the core center.

The first version should use simple, explainable metrics rather than a learned classifier. Candidate color evidence may combine:

- target hue hits in the local patch
- saturation-weighted support
- distance-weighted support favoring pixels near the core

This yields two separate values:

- `local_red_score`
- `local_green_score`

These scores are intended to measure whether a bright core is surrounded by color evidence consistent with a red or green laser spot, rather than whether a large color blob exists.

## Ranking Intent

The first phase does not need to decide final runtime behavior, but rankings should still be diagnosable.

Candidate ranking should be explainable from a small set of factors:

- white-core brightness and size plausibility
- distance to previous detection when available
- local target-color support
- separation between top candidates

The design goal is not to maximize sophistication. The goal is to make failures interpretable.

## Integration Plan For Phase 1

Phase 1 integrates only into the debug evidence path.

Recommended placement:

- keep `LaserDetector` public API unchanged
- add an internal experimental helper for white-core candidate analysis
- surface richer diagnostics through the existing debug/preview path

Phase 1 does not yet change:

- formal runtime detector selection
- tracker state machine semantics
- support detector refresh behavior

## Diagnostics Requirements

The debug path should expose enough evidence to compare current color-first behavior with white-core-first reacquire behavior.

At minimum, diagnostics should report:

- bounded ROI used for analysis
- white-core candidate count
- top candidate list with center, brightness, area, and distance to previous
- per-candidate `local_red_score`
- per-candidate `local_green_score`
- any pruning reasons applied before ranking

## Testing Strategy

Phase 1 should use both synthetic tests and debug-video evidence.

Required synthetic cases:

- white core with red local support produces `local_red_score > local_green_score`
- white core with green local support produces `local_green_score > local_red_score`
- pure white bright point without meaningful color support does not look like a strong red or green candidate
- multiple nearby white cores produce distinguishable diagnostics rather than opaque success/failure behavior

Required debug evidence goals:

- inspect bounded reacquire ROI candidate counts on black-tape frames
- verify whether top candidates remain near the expected track
- compare red/green local scores against known reflections and noise

## Success Criteria

This direction is considered promising only if evidence shows all of the following:

- bounded reacquire ROIs do not routinely explode into unmanageable white-core candidate counts
- the true laser core appears reliably in the top candidate set
- local color correlation provides usable separation between real red/green lasers and white reflections
- the approach can be kept inside the current bounded runtime architecture without restoring full-frame relaxed fallback

## Non-Goals

- Do not replace the formal runtime laser path in this phase.
- Do not introduce full-frame reacquire logic.
- Do not add Control Layer behavior.
- Do not convert detections into screen-space outputs.
- Do not tune Kalman or add it as a dependency.
