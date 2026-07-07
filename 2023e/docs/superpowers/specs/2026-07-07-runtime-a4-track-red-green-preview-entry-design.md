# Runtime A4 Track Red Green Preview Entry Design

## Goal

Add a dedicated test script entry for the fourth formal runtime mode so the user can inspect `RUNTIME_A4_TRACK_RED_GREEN` directly without repeating long command-line mode flags.

## Scope

This work adds a thin script entry under `2023e/scripts/test/`.

The new script:

- exists only as a convenience preview/test entry
- reuses the formal runtime preview path already implemented in `preview_vision_video.py`
- does not duplicate runtime pipeline, drawing, or summary logic

## Directory Rule

The script must live in:

`2023e/scripts/test/`

This keeps the new file aligned with the existing repository rule that helper/test entry points live under `scripts/`, while runtime logic remains in `src/vision`.

## Recommended Shape

Create one thin script, for example:

`2023e/scripts/test/preview_runtime_a4_track_red_green.py`

Its responsibilities are:

- parse a minimal subset of preview arguments
- force `--path-mode runtime`
- force `--runtime-mode runtime_a4_track_red_green`
- delegate execution to the existing video preview entry

Its responsibilities do not include:

- creating a second runtime pipeline implementation
- re-implementing drawing helpers
- re-implementing border cache logic
- re-implementing runtime summaries

## Interface

The script should support the same practical arguments the user is likely to need for inspection:

- `--video`
- `--max-frames`
- `--no-window`

Optional passthrough support for other existing preview arguments is acceptable if it stays thin.

## Behavior

Running the dedicated script should behave the same as invoking:

```powershell
conda run -n opencv-learning python scripts/test/preview_vision_video.py --path-mode runtime --runtime-mode runtime_a4_track_red_green
```

That means the preview should show:

- runtime path execution only
- red laser overlay when detected
- green laser overlay when detected
- A4 outer/inner overlays when detected or cached
- runtime timing summary fields

## Non-Goals

- Do not add new runtime modes.
- Do not move logic out of `preview_vision_video.py` unless required for a very small reuse point.
- Do not duplicate existing runtime preview behavior in a second script body.
- Do not add control-layer or screen-coordinate outputs.

## Testing Strategy

Keep verification lightweight:

- script should start successfully with `--no-window`
- script should drive the runtime mode `runtime_a4_track_red_green`
- existing vision/script tests should continue to pass

The first success criterion is convenience and behavioral equivalence, not new runtime functionality.
