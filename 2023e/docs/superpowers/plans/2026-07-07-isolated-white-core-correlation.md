# Isolated White-Core Correlation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a pure script experiment that samples 30 video frames after frame 41, classifies white laser core candidates by local red/green color correlation, and writes annotated evidence under `outputs/`.

**Architecture:** Keep this experiment outside formal Vision Layer behavior. Put the algorithm and CLI in one focused script under `scripts/test/`, with small pure functions that can be unit tested without opening a video. The script reads frames, extracts white-core candidates, computes local red/green scores, classifies candidates, draws annotations, and writes `summary.csv` plus `summary.txt`.

**Tech Stack:** Python 3.12, OpenCV (`cv2`), NumPy, pytest, standard-library `argparse`, `csv`, `datetime`, `random`, and `pathlib`.

## Global Constraints

- Pure script experiment only.
- Do not modify `src/vision` behavior.
- Do not integrate with `VisionPipeline`.
- Do not replace `LaserDetector` logic.
- Do not add motor/control/screen-space behavior.
- Default video path is `E:\B306\Visual\openCV\project\videos\2023e.mp4`.
- Default `--start-frame` is `41`.
- Default `--sample-count` is `30`.
- Use a fixed default random seed for reproducible sampling.
- Write generated evidence under a new `outputs/` subdirectory.
- Use TDD for production code changes: write a failing test, verify it fails, implement minimal code, verify it passes.
- Do not commit changes unless the user explicitly requests a commit.

---

## File Structure

- Create `scripts/test/isolated_white_core_correlation.py`
  - Owns the isolated algorithm, CLI, video sampling, drawing, and output writing.
  - Does not import `vision.laser_detector` or `vision.pipeline`.
- Create `tests/scripts/test_isolated_white_core_correlation.py`
  - Owns unit tests for pure algorithm helpers using synthetic images.
  - Does not require the real video file.
- Write generated output at runtime under `outputs/white_core_correlation_YYYYMMDD_HHMMSS/` or a user-provided `--output-dir`.

---

### Task 1: Core White-Core Correlation Helpers

**Files:**
- Create: `scripts/test/isolated_white_core_correlation.py`
- Create: `tests/scripts/test_isolated_white_core_correlation.py`

**Interfaces:**
- Produces dataclass `WhiteCoreCandidate(center: tuple[float, float], core_area_px: float, brightness: float, red_score: float, green_score: float, label: str)`.
- Produces function `analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]`.
- Produces function `classify_scores(red_score: float, green_score: float) -> str`.

- [ ] **Step 1: Add failing tests for synthetic red, green, and pure-white cases**

Create `tests/scripts/test_isolated_white_core_correlation.py`:

```python
import sys
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = PROJECT_ROOT / "scripts" / "test"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from isolated_white_core_correlation import analyze_frame, classify_scores  # noqa: E402


def _blank(width=120, height=100, value=5):
    return np.full((height, width, 3), value, dtype=np.uint8)


def test_classify_scores_uses_gap_and_minimum_score():
    assert classify_scores(0.40, 0.05) == "red"
    assert classify_scores(0.05, 0.40) == "green"
    assert classify_scores(0.02, 0.03) == "white/no-color"
    assert classify_scores(0.22, 0.18) == "uncertain"


def test_analyze_frame_labels_red_core_from_local_red_support():
    frame = _blank()
    cv2.circle(frame, (60, 50), 6, (0, 0, 180), -1, lineType=cv2.LINE_AA)
    cv2.circle(frame, (60, 50), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)

    candidates = analyze_frame(frame)

    assert candidates
    best = candidates[0]
    assert abs(best.center[0] - 60) <= 2.0
    assert abs(best.center[1] - 50) <= 2.0
    assert best.red_score > best.green_score
    assert best.label == "red"


def test_analyze_frame_labels_green_core_from_local_green_support():
    frame = _blank()
    cv2.circle(frame, (65, 45), 6, (0, 180, 0), -1, lineType=cv2.LINE_AA)
    cv2.circle(frame, (65, 45), 3, (245, 245, 245), -1, lineType=cv2.LINE_AA)

    candidates = analyze_frame(frame)

    assert candidates
    best = candidates[0]
    assert abs(best.center[0] - 65) <= 2.0
    assert abs(best.center[1] - 45) <= 2.0
    assert best.green_score > best.red_score
    assert best.label == "green"


def test_analyze_frame_keeps_pure_white_scores_low():
    frame = _blank()
    cv2.circle(frame, (60, 50), 4, (245, 245, 245), -1, lineType=cv2.LINE_AA)

    candidates = analyze_frame(frame)
    assert candidates
    best = candidates[0]
    assert best.red_score < 0.05
    assert best.green_score < 0.05
    assert best.label == "white/no-color"
```

- [ ] **Step 2: Run tests to verify they fail because the script does not exist**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected: FAIL with `ModuleNotFoundError: No module named 'isolated_white_core_correlation'`.

- [ ] **Step 3: Add minimal algorithm implementation**

Create `scripts/test/isolated_white_core_correlation.py`:

```python
from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np


@dataclass(frozen=True)
class WhiteCoreCandidate:
    center: tuple[float, float]
    core_area_px: float
    brightness: float
    red_score: float
    green_score: float
    label: str


def analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]:
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    bright_core_mask = _build_white_core_mask(hsv)
    contours, _ = cv2.findContours(bright_core_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    value_channel = hsv[:, :, 2]
    candidates: list[WhiteCoreCandidate] = []
    for contour in contours:
        area = float(cv2.contourArea(contour))
        if area < 2.0 or area > 300.0:
            continue
        contour_mask = np.zeros(bright_core_mask.shape, dtype=np.uint8)
        cv2.drawContours(contour_mask, [contour], -1, 255, -1)
        center = _brightness_weighted_center(contour_mask, value_channel)
        brightness_values = value_channel[contour_mask > 0]
        brightness = float(brightness_values.max() if brightness_values.size else 0.0)
        red_score = _local_color_score(hsv, center, "red")
        green_score = _local_color_score(hsv, center, "green")
        candidates.append(
            WhiteCoreCandidate(
                center=center,
                core_area_px=area,
                brightness=brightness,
                red_score=red_score,
                green_score=green_score,
                label=classify_scores(red_score, green_score),
            )
        )
    candidates.sort(key=lambda candidate: max(candidate.red_score, candidate.green_score), reverse=True)
    return candidates


def classify_scores(red_score: float, green_score: float) -> str:
    if red_score >= 0.20 and red_score - green_score >= 0.15:
        return "red"
    if green_score >= 0.20 and green_score - red_score >= 0.15:
        return "green"
    if red_score < 0.05 and green_score < 0.05:
        return "white/no-color"
    return "uncertain"


def _build_white_core_mask(hsv: np.ndarray) -> np.ndarray:
    lower = np.array([0, 0, 180], dtype=np.uint8)
    upper = np.array([179, 80, 255], dtype=np.uint8)
    mask = cv2.inRange(hsv, lower, upper)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    return mask


def _brightness_weighted_center(mask: np.ndarray, value_channel: np.ndarray) -> tuple[float, float]:
    weights = value_channel.astype(np.float32) * (mask > 0)
    total = float(weights.sum())
    if total > 0.0:
        ys, xs = np.indices(weights.shape)
        return float((xs * weights).sum() / total), float((ys * weights).sum() / total)
    moments = cv2.moments(mask)
    if moments["m00"]:
        return float(moments["m10"] / moments["m00"]), float(moments["m01"] / moments["m00"])
    return 0.0, 0.0


def _local_color_score(hsv: np.ndarray, center: tuple[float, float], color: str) -> float:
    radius = 9
    center_x = int(round(center[0]))
    center_y = int(round(center[1]))
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
    if color == "red":
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
```

- [ ] **Step 4: Run tests to verify they pass**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected: all tests pass.

---

### Task 2: CLI Video Sampling And Output Evidence

**Files:**
- Modify: `scripts/test/isolated_white_core_correlation.py`
- Modify: `tests/scripts/test_isolated_white_core_correlation.py`

**Interfaces:**
- Consumes `analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]` from Task 1.
- Produces function `sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]`.
- Produces CLI command that writes annotated images, `summary.csv`, and `summary.txt`.

- [ ] **Step 1: Add failing test for reproducible frame sampling**

Append to `tests/scripts/test_isolated_white_core_correlation.py`:

```python
from isolated_white_core_correlation import sample_frame_indices  # noqa: E402


def test_sample_frame_indices_are_after_start_and_reproducible():
    first = sample_frame_indices(total_frames=100, start_frame=41, sample_count=10, seed=7)
    second = sample_frame_indices(total_frames=100, start_frame=41, sample_count=10, seed=7)

    assert first == second
    assert len(first) == 10
    assert min(first) > 41
    assert max(first) <= 100
    assert first == sorted(first)


def test_sample_frame_indices_tolerates_short_video():
    indices = sample_frame_indices(total_frames=45, start_frame=41, sample_count=30, seed=7)

    assert indices == [42, 43, 44, 45]
```

- [ ] **Step 2: Run new sampling tests to verify they fail**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_are_after_start_and_reproducible" "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_tolerates_short_video" -q
```

Expected: FAIL with `ImportError` or `AttributeError` for missing `sample_frame_indices`.

- [ ] **Step 3: Add sampling, drawing, output writing, and CLI**

Update `scripts/test/isolated_white_core_correlation.py` by keeping Task 1 functions and adding these imports and functions:

```python
import argparse
import csv
import random
from collections import Counter
from datetime import datetime
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"


def sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]:
    available = list(range(start_frame + 1, total_frames + 1))
    if len(available) <= sample_count:
        return available
    rng = random.Random(seed)
    return sorted(rng.sample(available, sample_count))


def draw_candidates(frame: np.ndarray, candidates: list[WhiteCoreCandidate]) -> np.ndarray:
    annotated = frame.copy()
    colors = {
        "red": (0, 0, 255),
        "green": (0, 255, 0),
        "uncertain": (0, 255, 255),
        "white/no-color": (160, 160, 160),
    }
    for index, candidate in enumerate(candidates):
        x = int(round(candidate.center[0]))
        y = int(round(candidate.center[1]))
        color = colors.get(candidate.label, (255, 255, 255))
        cv2.circle(annotated, (x, y), 10, color, 2)
        text = f"#{index} {candidate.label} r={candidate.red_score:.2f} g={candidate.green_score:.2f}"
        cv2.putText(annotated, text, (x + 12, y - 8), cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 1, cv2.LINE_AA)
    return annotated


def default_output_dir() -> Path:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return DEFAULT_OUTPUT_ROOT / f"white_core_correlation_{timestamp}"


def process_video(video: Path, start_frame: int, sample_count: int, seed: int, output_dir: Path) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Failed to open video: {video}")
        return 1

    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT) or 0)
    frame_indices = sample_frame_indices(total_frames, start_frame, sample_count, seed)
    output_dir.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, object]] = []
    failed_frames: list[int] = []
    label_counts: Counter[str] = Counter()
    for frame_index in frame_indices:
        cap.set(cv2.CAP_PROP_POS_FRAMES, frame_index - 1)
        ok, frame = cap.read()
        if not ok:
            failed_frames.append(frame_index)
            continue
        candidates = analyze_frame(frame)
        annotated = draw_candidates(frame, candidates)
        cv2.imwrite(str(output_dir / f"frame_{frame_index:04d}.png"), annotated)
        for candidate_index, candidate in enumerate(candidates):
            label_counts[candidate.label] += 1
            rows.append(
                {
                    "frame": frame_index,
                    "candidate_index": candidate_index,
                    "u_px": f"{candidate.center[0]:.3f}",
                    "v_px": f"{candidate.center[1]:.3f}",
                    "core_area_px": f"{candidate.core_area_px:.3f}",
                    "brightness": f"{candidate.brightness:.3f}",
                    "red_score": f"{candidate.red_score:.6f}",
                    "green_score": f"{candidate.green_score:.6f}",
                    "label": candidate.label,
                    "score_gap": f"{abs(candidate.red_score - candidate.green_score):.6f}",
                }
            )

    cap.release()
    _write_summary_csv(output_dir / "summary.csv", rows)
    _write_summary_txt(
        output_dir / "summary.txt",
        video,
        start_frame,
        sample_count,
        seed,
        frame_indices,
        failed_frames,
        label_counts,
        output_dir,
    )
    print(f"Wrote isolated white-core correlation output to: {output_dir}")
    print(f"Sampled frames: {len(frame_indices)} requested={sample_count} failed={len(failed_frames)}")
    return 0


def _write_summary_csv(path: Path, rows: list[dict[str, object]]) -> None:
    fieldnames = [
        "frame",
        "candidate_index",
        "u_px",
        "v_px",
        "core_area_px",
        "brightness",
        "red_score",
        "green_score",
        "label",
        "score_gap",
    ]
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def _write_summary_txt(
    path: Path,
    video: Path,
    start_frame: int,
    sample_count: int,
    seed: int,
    frame_indices: list[int],
    failed_frames: list[int],
    label_counts: Counter[str],
    output_dir: Path,
) -> None:
    lines = [
        f"video={video}",
        f"start_frame={start_frame}",
        f"sample_count_requested={sample_count}",
        f"seed={seed}",
        f"actual_sampled_frames={','.join(str(frame) for frame in frame_indices)}",
        f"output_dir={output_dir}",
        f"failed_frames={','.join(str(frame) for frame in failed_frames)}",
    ]
    for label in ("red", "green", "uncertain", "white/no-color"):
        lines.append(f"label_count_{label}={label_counts.get(label, 0)}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Isolated white-core red/green correlation experiment.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=30)
    parser.add_argument("--seed", type=int, default=20260707)
    parser.add_argument("--output-dir", type=Path, default=None)
    args = parser.parse_args(argv)
    output_dir = args.output_dir or default_output_dir()
    return process_video(args.video, args.start_frame, args.sample_count, args.seed, output_dir)


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 4: Run sampling tests and full script tests**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected: all tests pass.

- [ ] **Step 5: Run the script against the real video**

Run:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30
```

Expected: command exits with code 0 and prints `Wrote isolated white-core correlation output to: outputs\white_core_correlation_...`.

- [ ] **Step 6: Verify output files exist**

Run a directory listing on the printed output directory.

Expected:

- 30 `frame_*.png` files when the video has at least 30 frames after frame 41 and all sampled frames read successfully.
- `summary.csv`
- `summary.txt`

---

## Self-Review Notes

- Spec coverage: The plan creates the pure script, samples after frame 41, uses 30 reproducible random frames, writes annotated images plus CSV/TXT summaries under `outputs/`, and avoids `src/vision` changes.
- Placeholder scan: No `TBD`, `TODO`, or open-ended implementation placeholders remain.
- Type consistency: `WhiteCoreCandidate`, `analyze_frame`, `classify_scores`, and `sample_frame_indices` names are consistent across tests and implementation steps.
- Scope check: The work is one isolated script experiment and does not require decomposition.
