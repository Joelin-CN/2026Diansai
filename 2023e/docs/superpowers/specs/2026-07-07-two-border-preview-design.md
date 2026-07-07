# Two Border Preview Detector Design

## Goal

Add script-level preview detection for two distinct border targets before migrating stable logic into the formal Vision Layer:

- `screen_square`: the gray, thin pencil square drawn at the screen center.
- `a4_tape`: the A4 black electrical tape target, including both outer and inner tape contours.

This design only covers border problem 1. Red/green laser tracking instability remains out of scope until this border work is accepted.

## Scope

First implement and tune in `scripts/test` so video overlays can be inspected quickly. After visual acceptance, migrate the stable logic into `src/vision/screen_detector.py` and `src/vision/tape_quad_detector.py`.

Do not output `ScreenPoint`, `ScreenQuad`, paths, control errors, or motor commands. All output remains image-space quads and diagnostics.

## Current Problem

`preview_vision_video.py` currently uses one generic `detect_likely_border()` function. That function runs Canny, closes edges, finds external contours, approximates quadrilaterals, and selects the largest candidate. It does not distinguish screen pencil square from A4 tape. This is why it can select the large screen/video rectangle instead of the A4 target.

## A4 Black Tape Detector

The A4 target is a black rectangular ring. The detector must treat it as a ring with two boundaries:

- `outer_quad`: outside edge of the black tape.
- `inner_quad`: inside edge of the black tape, i.e. the boundary of the white paper region.

Detection flow:

```text
frame
-> grayscale
-> black mask
-> morphology close to connect tape edges
-> findContours(mask, RETR_TREE)
-> pair outer contour with an inner child contour
-> approximate both contours as quadrilaterals
-> score and validate the pair
```

Acceptance for `a4_tape.found=true`:

- Both `outer_quad` and `inner_quad` exist.
- Both are convex four-point image-space quads.
- `inner_quad` is contained inside `outer_quad`.
- `outer_area > inner_area`.
- The pair is not close to the whole frame boundary.
- Tape ratio `(outer_area - inner_area) / outer_area` is within a broad plausible range.

If only one contour is found, the result is not found and must report a failure reason such as `inner_contour_not_found` or `outer_contour_not_found`.

Visual overlay:

- A4 outer quad: magenta thick line.
- A4 inner quad: yellow thin line.

The later out-of-border judgment should use `inner_quad` first:

```text
inside inner_quad                       -> inside target area
between inner_quad and outer_quad        -> on tape boundary
outside outer_quad                       -> outside A4 target
```

## Screen Pencil Square Detector

The screen square is a thin gray pencil line at the screen center. It is a different target from the A4 tape and must not share the A4 black-mask logic.

Detection flow:

```text
frame
-> grayscale
-> optional contrast enhancement
-> Canny or Sobel edges
-> HoughLinesP or contour line grouping
-> group long thin line segments into a near-square quadrilateral
-> score and validate the candidate
```

Acceptance for `screen_square.found=true`:

- A four-sided image-space quad exists.
- The quad is approximately square under perspective tolerance.
- The lines are thin and low-contrast relative to A4 tape.
- The area is larger than the A4 tape candidate when both are present.
- The candidate is not the full frame boundary.

Visual overlay:

- Screen square: cyan line.

## Video Preview Integration

Replace the single `cached_border` state with separate states:

```text
cached_screen_square
cached_a4_tape
```

The video preview should print per-frame diagnostics:

```text
screen_square=True/False score=... area=...
a4_tape=True/False outer_area=... inner_area=... tape_ratio=... score=...
```

The old generic `detect_likely_border()` can remain as a debug fallback, but it must not be labeled as either screen square or A4 tape.

## Testing And Verification

Script-level verification:

- Run `preview_vision_video.py --no-window --max-frames 5` and confirm it opens the video and reports separate screen/A4 diagnostics.
- Run the windowed script and visually inspect the overlays.
- Run existing `tests/vision` regression tests.

Visual acceptance:

- The A4 black tape target shows both magenta outer quad and yellow inner quad.
- The screen pencil square is shown separately in cyan.
- The whole video frame is not mislabeled as A4 tape.
- A4 is not marked found unless both inner and outer quads are present.
