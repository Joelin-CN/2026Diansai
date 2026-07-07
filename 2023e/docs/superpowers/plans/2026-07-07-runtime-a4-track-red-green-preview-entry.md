# Runtime A4 Track Red Green Preview Entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a dedicated test-script entry that always runs the fourth formal runtime mode preview path for quick inspection.

**Architecture:** Keep the new script as a thin wrapper under `scripts/test/` and reuse the existing runtime preview path in `preview_vision_video.py`. Do not duplicate pipeline, overlay, cache, or summary logic.

**Tech Stack:** Python, argparse, existing `scripts/test/preview_vision_video.py`, OpenCV `cv2` runtime preview flow.

## Global Constraints

- The new file must live under `2023e/scripts/test/`.
- The new file must remain a helper/test entry only; core runtime logic stays in `2023e/src/vision/`.
- The new file must force `--path-mode runtime` and `--runtime-mode runtime_a4_track_red_green`.
- The new file must not duplicate runtime pipeline, drawing, cache, or summary logic.
- The new file may expose only the practical preview arguments needed for inspection.
- Do not add new runtime modes.
- Do not add `ScreenPoint`, `ScreenQuad`, or `MotorCommand` outputs.

---

## File Structure

### Modified Files

- `2023e/scripts/test/preview_vision_video.py`
  - Add one small reusable entry function that accepts argv so wrapper scripts can delegate without reimplementing behavior.

### New Files

- `2023e/scripts/test/preview_runtime_a4_track_red_green.py`
  - Thin dedicated wrapper for `runtime_a4_track_red_green`.

---

### Task 1: Expose A Reusable Preview Entry And Add The Dedicated Mode-4 Wrapper

**Files:**
- Modify: `2023e/scripts/test/preview_vision_video.py`
- Create: `2023e/scripts/test/preview_runtime_a4_track_red_green.py`

**Interfaces:**
- Produces: `main(argv: list[str] | None = None) -> int` in `preview_vision_video.py`
- Produces: standalone wrapper script `preview_runtime_a4_track_red_green.py`

- [ ] **Step 1: Write the failing smoke command expectation**

Target command:

```powershell
conda run -n opencv-learning python scripts/test/preview_runtime_a4_track_red_green.py --no-window --max-frames 2
```

Expected initial failure:

```text
python: can't open file 'scripts/test/preview_runtime_a4_track_red_green.py'
```

- [ ] **Step 2: Run the missing-script command to verify failure**

Run:

```powershell
conda run -n opencv-learning python scripts/test/preview_runtime_a4_track_red_green.py --no-window --max-frames 2
```

Expected: FAIL because the wrapper file does not exist yet.

- [ ] **Step 3: Refactor `preview_vision_video.py` minimally so wrappers can delegate argv**

Update the entry shape from:

```python
def main() -> int:
    parser = argparse.ArgumentParser(...)
    ...
    args = parser.parse_args()
```

to:

```python
def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(...)
    ...
    args = parser.parse_args(argv)
```

Keep all existing behavior unchanged when `argv` is `None`.

- [ ] **Step 4: Add the dedicated thin wrapper script**

Create `2023e/scripts/test/preview_runtime_a4_track_red_green.py` with minimal logic like:

```python
from __future__ import annotations

import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from preview_vision_video import main as preview_video_main  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    forwarded = list(argv or [])
    forced = ["--path-mode", "runtime", "--runtime-mode", "runtime_a4_track_red_green"]
    return preview_video_main([*forced, *forwarded])


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

- [ ] **Step 5: Run the dedicated wrapper smoke command to verify it passes**

Run:

```powershell
conda run -n opencv-learning python scripts/test/preview_runtime_a4_track_red_green.py --no-window --max-frames 2
```

Expected: PASS with stdout containing `path=runtime` and `mode=runtime_a4_track_red_green`.

- [ ] **Step 6: Run the existing script tests to verify no regression**

Run:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/scripts -q
```

Expected: PASS.

- [ ] **Step 7: Run the targeted vision/script sanity suite**

Run:

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision/test_pipeline.py tests/scripts/test_border_preview_detectors.py -q
```

Expected: PASS.
