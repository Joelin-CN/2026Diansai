# Task 2: CLI Video Sampling And Output Evidence

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

- Modify: `E:\B306\2026\电赛\2023e\scripts\test\isolated_white_core_correlation.py`
- Modify: `E:\B306\2026\电赛\2023e\tests\scripts\test_isolated_white_core_correlation.py`

## Interfaces

- Consume `analyze_frame(frame: np.ndarray) -> list[WhiteCoreCandidate]` from Task 1.
- Produce function `sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]`.
- Produce CLI command that writes annotated images, `summary.csv`, and `summary.txt`.

## Required Tests

Append tests to `tests/scripts/test_isolated_white_core_correlation.py` that import `sample_frame_indices` and verify:

1. `sample_frame_indices(total_frames=100, start_frame=41, sample_count=10, seed=7)` returns the same list on repeated calls.
2. The returned list length is `10`.
3. All returned frame indices are `> 41` and `<= 100`.
4. The returned list is sorted.
5. `sample_frame_indices(total_frames=45, start_frame=41, sample_count=30, seed=7)` returns exactly `[42, 43, 44, 45]`.

## Required RED Command

Run before implementing `sample_frame_indices`:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_are_after_start_and_reproducible" "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_tolerates_short_video" -q
```

Expected RED failure: missing `sample_frame_indices` import or equivalent missing function failure.

## Required Implementation

Update `scripts/test/isolated_white_core_correlation.py` while preserving Task 1 behavior.

Add imports:

- `argparse`
- `csv`
- `random`
- `Counter` from `collections`
- `datetime` from `datetime`
- `Path` from `pathlib`

Add constants:

- `PROJECT_ROOT = Path(__file__).resolve().parents[2]`
- `DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")`
- `DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"`

Add function `sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]`:

- Available frames are `range(start_frame + 1, total_frames + 1)`.
- If available count is `<= sample_count`, return all available frames.
- Otherwise use `random.Random(seed).sample(available, sample_count)` and return the sorted result.

Add function `draw_candidates(frame: np.ndarray, candidates: list[WhiteCoreCandidate]) -> np.ndarray`:

- Copy the frame.
- Draw a circle and text for each candidate.
- Use BGR colors: red `(0, 0, 255)`, green `(0, 255, 0)`, uncertain `(0, 255, 255)`, white/no-color `(160, 160, 160)`.
- Text format should include candidate index, label, `r=<score>` and `g=<score>`.

Add function `default_output_dir() -> Path`:

- Return `outputs/white_core_correlation_YYYYMMDD_HHMMSS` using current local time.

Add function `process_video(video: Path, start_frame: int, sample_count: int, seed: int, output_dir: Path) -> int`:

- Open the video with `cv2.VideoCapture`.
- If it cannot open, print `Failed to open video: <path>` and return `1`.
- Read total frames from `cv2.CAP_PROP_FRAME_COUNT`.
- Use `sample_frame_indices`.
- Create `output_dir` with parents.
- For each sampled frame, seek with `cap.set(cv2.CAP_PROP_POS_FRAMES, frame_index - 1)`, read frame, analyze, draw, and save `frame_<frame_index:04d>.png`.
- Accumulate CSV rows with fields: `frame`, `candidate_index`, `u_px`, `v_px`, `core_area_px`, `brightness`, `red_score`, `green_score`, `label`, `score_gap`.
- Track failed frames and label counts.
- Write `summary.csv` and `summary.txt`.
- Print output directory and sampled/failed counts.
- Return `0` on successful processing.

Add helper `_write_summary_csv(path: Path, rows: list[dict[str, object]]) -> None`:

- Write UTF-8 CSV with the exact field order listed above.

Add helper `_write_summary_txt(...) -> None`:

- Include video path, start frame, requested sample count, seed, actual sampled frames, output directory, failed frames, and label counts for `red`, `green`, `uncertain`, and `white/no-color`.

Add `main(argv: list[str] | None = None) -> int`:

- Supports `--video`, `--start-frame`, `--sample-count`, `--seed`, and `--output-dir`.
- Defaults match global constraints.
- Calls `process_video`.

Add:

```python
if __name__ == "__main__":
    raise SystemExit(main())
```

## Required GREEN Commands

Run after implementation:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Expected result: all tests pass.

Then run the script against the real video:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30
```

Expected result: command exits with code `0`, prints `Wrote isolated white-core correlation output to: ...`, and writes an output folder.

Verify the output folder contains:

- `summary.csv`
- `summary.txt`
- 30 `frame_*.png` files if all sampled frames read successfully.

## Report

Write report to `E:\B306\2026\电赛\.superpowers\sdd\task-2-report.md`.

Include:

- What changed.
- RED command and failing output.
- GREEN unit-test command and passing output.
- Real-video command and output.
- Output directory path.
- Output file counts.
- Files changed.
- Self-review notes.
- Concerns, if any.
