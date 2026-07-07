# Task 1: Core White-Core Correlation Helpers

Plan file: `E:\B306\2026\电赛\2023e\docs\superpowers\plans\2026-07-07-isolated-white-core-correlation.md`

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

## Files

- Create: `E:\B306\2026\电赛\2023e\scripts\test\isolated_white_core_correlation.py`
- Create: `E:\B306\2026\电赛\2023e\tests\scripts\test_isolated_white_core_correlation.py`

## Interfaces

- Produce dataclass `WhiteCoreCandidate(center: tuple[float, float], core_area_px: float, brightness: float, red_score: float, green_score: float, label: str)`.
- Produce function `analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]`.
- Produce function `classify_scores(red_score: float, green_score: float) -> str`.

## Required Tests

Create `tests/scripts/test_isolated_white_core_correlation.py` with tests that:

1. Import `analyze_frame` and `classify_scores` from `scripts/test/isolated_white_core_correlation.py` by adding `scripts/test` to `sys.path`.
2. Verify `classify_scores(0.40, 0.05) == "red"`.
3. Verify `classify_scores(0.05, 0.40) == "green"`.
4. Verify `classify_scores(0.02, 0.03) == "white/no-color"`.
5. Verify `classify_scores(0.22, 0.18) == "uncertain"`.
6. Build a synthetic dark image with a red BGR ring `(0, 0, 180)` and white core `(245, 245, 245)` around `(60, 50)`, call `analyze_frame`, and assert the best candidate is near `(60, 50)`, has `red_score > green_score`, and label `red`.
7. Build a synthetic dark image with a green BGR ring `(0, 180, 0)` and white core `(245, 245, 245)` around `(65, 45)`, call `analyze_frame`, and assert the best candidate is near `(65, 45)`, has `green_score > red_score`, and label `green`.
8. Build a synthetic dark image with only a pure white core `(245, 245, 245)`, call `analyze_frame`, and assert the best candidate has both scores `< 0.05` and label `white/no-color`.

## Required RED Command

Run before creating the implementation script:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected RED failure: `ModuleNotFoundError: No module named 'isolated_white_core_correlation'` or equivalent missing module failure.

## Required Implementation

Create `scripts/test/isolated_white_core_correlation.py` as a pure script module that:

- Imports only `dataclasses`, `cv2`, and `numpy` for Task 1.
- Defines frozen dataclass `WhiteCoreCandidate` with the fields listed above.
- Defines `analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]`.
- Converts BGR to HSV.
- Builds a white-core mask using HSV lower `[0, 0, 180]` and upper `[179, 80, 255]`, followed by 3x3 elliptical open and close morphology.
- Extracts contours with `cv2.RETR_EXTERNAL` and `cv2.CHAIN_APPROX_SIMPLE`.
- Rejects contours with `area < 2.0` or `area > 300.0`.
- Computes brightness-weighted centers from the HSV value channel.
- Computes local red and green scores in radius `9` around the center.
- Red hue support is `(hue <= 10) | (hue >= 170)`.
- Green hue support is `(hue >= 45) & (hue <= 85)`.
- Score is saturation * value * spatial weight, normalized by `/ 6.0` and capped at `1.0`.
- Sorts candidates by `max(red_score, green_score)` descending.
- Defines `classify_scores` with these exact rules:
  - `red` if `red_score >= 0.20` and `red_score - green_score >= 0.15`
  - `green` if `green_score >= 0.20` and `green_score - red_score >= 0.15`
  - `white/no-color` if `red_score < 0.05` and `green_score < 0.05`
  - otherwise `uncertain`

## Required GREEN Command

Run after implementation:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected GREEN result: all tests pass.

## Report

Write report to `E:\B306\2026\电赛\.superpowers\sdd\task-1-report.md`.

Include:

- What changed.
- RED command and failing output.
- GREEN command and passing output.
- Files changed.
- Self-review notes.
- Concerns, if any.
