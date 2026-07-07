# Two Border Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add script-level preview detection for the screen pencil square and A4 black tape inner/outer contours.

**Architecture:** Keep this as script-level validation first. Create a focused `scripts/test/border_preview_detectors.py` module with image-space preview result dataclasses and detector functions, then update `preview_vision_video.py` and `preview_vision_detection.py` to draw separate overlays.

**Tech Stack:** Python, OpenCV `cv2`, NumPy, pytest for smoke/regression checks where practical.

## Global Constraints

- Do not output `ScreenPoint`, `ScreenQuad`, paths, control errors, or motor commands.
- Keep all border outputs as image-space quads and diagnostics.
- A4 tape is found only when both `outer_quad` and `inner_quad` exist.
- Screen pencil square and A4 tape must be detected by separate logic.
- Red/green laser tracking instability is out of scope for this plan.

---

### Task 1: Border Preview Data And A4 Tape Detector

**Files:**
- Create: `scripts/test/border_preview_detectors.py`

**Interfaces:**
- Produces: `detect_a4_tape_border(frame) -> A4TapeBorderPreview`
- Produces: `A4TapeBorderPreview(found, outer_quad, inner_quad, outer_area, inner_area, tape_ratio, score, failure_reason)`

- [ ] Write dataclasses for preview quad and A4 tape result.
- [ ] Implement black-mask based contour hierarchy using `cv2.RETR_TREE`.
- [ ] Pair outer contour with inner child contour.
- [ ] Require both outer and inner quadrilaterals for found=true.
- [ ] Add diagnostics fields for score, area, ratio, and failure reason.

### Task 2: Screen Pencil Square Detector

**Files:**
- Modify: `scripts/test/border_preview_detectors.py`

**Interfaces:**
- Produces: `detect_screen_pencil_square(frame, exclude_quad=None) -> QuadPreview`
- Produces: `QuadPreview(found, quad, area, score, failure_reason)`

- [ ] Implement edge/line based screen square candidate detection.
- [ ] Reject full-frame candidates.
- [ ] Prefer larger, centered, near-square candidates.
- [ ] Keep this separate from A4 black-mask logic.

### Task 3: Preview Script Integration

**Files:**
- Modify: `scripts/test/preview_vision_detection.py`
- Modify: `scripts/test/preview_vision_video.py`

**Interfaces:**
- Consumes: `detect_a4_tape_border`, `detect_screen_pencil_square`, draw helpers.
- Produces: separate cyan screen square, magenta A4 outer quad, yellow A4 inner quad overlays.

- [ ] Replace `detect_likely_border` usage with separate detector calls.
- [ ] Keep `detect_likely_border` only as optional fallback/debug if needed.
- [ ] Update console summaries to include `screen_square` and `a4_tape` separately.
- [ ] Cache `cached_screen_square` and `cached_a4_tape` separately in video preview.

### Task 4: Verification

**Files:**
- All above.

- [ ] Run `conda run -n opencv-learning python scripts/test/preview_vision_video.py --no-window --max-frames 5`.
- [ ] Run `conda run -n opencv-learning python scripts/test/preview_vision_detection.py --no-window --save outputs/vision_preview_2023e.jpg`.
- [ ] Run `$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision -q`.
- [ ] Inspect console output and confirm A4 reports found only when both inner and outer contours exist.
