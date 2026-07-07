# Task 2 Report: CLI Video Sampling And Output Evidence

## What Changed

- Added `sample_frame_indices(total_frames, start_frame, sample_count, seed)` with deterministic sorted sampling after the configured start frame.
- Added CLI support to `scripts/test/isolated_white_core_correlation.py` with `--video`, `--start-frame`, `--sample-count`, `--seed`, and `--output-dir`.
- Added video processing that samples frames, runs existing `analyze_frame`, draws candidate annotations, writes `summary.csv`, and writes `summary.txt`.
- Added Unicode-safe image writing via `_write_image` because OpenCV `cv2.imwrite` returned `False` for this workspace path on Windows.
- Added tests for deterministic sampling, short-video sampling, and Unicode output image writing.

## RED Command And Failing Output

Command:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_are_after_start_and_reproducible" "tests/scripts/test_isolated_white_core_correlation.py::test_sample_frame_indices_tolerates_short_video" -q
```

Output:

```text
ERROR: found no collectors for E:\B306\2026\����\2023e\tests\scripts\test_isolated_white_core_correlation.py::test_sample_frame_indices_are_after_start_and_reproducible

ERROR: found no collectors for E:\B306\2026\����\2023e\tests\scripts\test_isolated_white_core_correlation.py::test_sample_frame_indices_tolerates_short_video

ImportError: cannot import name 'sample_frame_indices' from 'isolated_white_core_correlation' (E:\B306\2026\����\2023e\scripts\test\isolated_white_core_correlation.py)
```

Additional focused RED after real-video evidence showed missing PNGs:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py::test_write_image_supports_unicode_paths" -q
```

Output:

```text
ImportError: cannot import name '_write_image' from 'isolated_white_core_correlation'
```

Root-cause probe:

```text
cv2.imwrite(...) returned False and the file did not exist for E:\B306\2026\电赛\2023e\outputs\white_core_correlation_20260707_223639\probe.png
```

## GREEN Unit-Test Command And Passing Output

Command:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Output:

```text
..........                                                               [100%]
10 passed in 0.16s
```

## Real-Video Command And Output

Command:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30
```

Output:

```text
Wrote isolated white-core correlation output to: E:\B306\2026\����\2023e\outputs\white_core_correlation_20260707_223806
Sampled frames: 30; failed frames: 0
```

## Output Directory Path

`E:\B306\2026\电赛\2023e\outputs\white_core_correlation_20260707_223806`

## Output File Counts

- `summary.csv`: 1
- `summary.txt`: 1
- `frame_*.png`: 30
- Failed sampled frames: 0

## Files Changed

- `E:\B306\2026\电赛\2023e\scripts\test\isolated_white_core_correlation.py`
- `E:\B306\2026\电赛\2023e\tests\scripts\test_isolated_white_core_correlation.py`
- `E:\B306\2026\电赛\.superpowers\sdd\task-2-report.md`

## Self-Review Notes

- Did not modify `src/vision`.
- Did not integrate with `VisionPipeline`.
- Preserved Task 1 `analyze_frame` and classification behavior.
- Used a fixed default seed of `7` for reproducible CLI sampling.
- Wrote generated evidence under `outputs/`.
- The first real-video run created `outputs/white_core_correlation_20260707_223639` with summaries but no PNGs because `cv2.imwrite` failed on the Unicode path. This was investigated, covered with a focused test, and fixed using `cv2.imencode(...).tofile(...)`.
- No commits were created.

## Concerns

- The PowerShell/Python command output renders the Chinese path segment as replacement characters in captured stdout, but filesystem verification confirms the Unicode output path exists and contains the expected files.
