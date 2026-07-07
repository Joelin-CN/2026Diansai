# Task 1 Report: Core White-Core Correlation Helpers

## What Changed

- Added `tests/scripts/test_isolated_white_core_correlation.py` with required TDD coverage for score classification and synthetic red-ring, green-ring, and white/no-color frame analysis.
- Added `scripts/test/isolated_white_core_correlation.py` as a pure script module defining `WhiteCoreCandidate`, `analyze_frame`, and `classify_scores`.
- The script uses HSV white-core masking, 3x3 elliptical open/close morphology, contour filtering, brightness-weighted centers, local red/green scoring in radius 9, and the exact classification thresholds from the brief.
- No `src/vision` files were modified and no VisionPipeline integration was added.

## RED Evidence

Command:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Output:

```text
=================================== ERRORS ====================================
___ ERROR collecting tests/scripts/test_isolated_white_core_correlation.py ____
ImportError while importing test module 'E:\B306\2026\����\2023e\tests\scripts\test_isolated_white_core_correlation.py'.
Hint: make sure your test modules/packages have valid Python names.
Traceback:
E:\Softwares\Anaconda\envs\opencv-learning\Lib\importlib\__init__.py:90: in import_module
    return _bootstrap._gcd_import(name[level:], package, level)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
tests\scripts\test_isolated_white_core_correlation.py:13: in <module>
    from isolated_white_core_correlation import analyze_frame, classify_scores  # noqa: E402
    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
E   ModuleNotFoundError: No module named 'isolated_white_core_correlation'
=========================== short test summary info ===========================
ERROR tests/scripts/test_isolated_white_core_correlation.py
!!!!!!!!!!!!!!!!!!! Interrupted: 1 error during collection !!!!!!!!!!!!!!!!!!!!
1 error in 0.32s
```

## GREEN Evidence

Command:

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" -m pytest "tests/scripts/test_isolated_white_core_correlation.py" -q
```

Output:

```text
.......                                                                  [100%]
7 passed in 0.11s
```

## Files Changed

- `scripts/test/isolated_white_core_correlation.py`
- `tests/scripts/test_isolated_white_core_correlation.py`
- `E:\B306\2026\电赛\.superpowers\sdd\task-1-report.md`

## Self-Review Notes

- Verified the module imports only `dataclasses`, `cv2`, and `numpy`.
- Verified `WhiteCoreCandidate` is a frozen dataclass with the required fields.
- Verified `classify_scores` implements the exact required threshold order and labels.
- Verified `analyze_frame` uses HSV conversion, white mask bounds `[0, 0, 180]` to `[179, 80, 255]`, 3x3 elliptical open/close morphology, external contours, contour area rejection, brightness-weighted centers, radius-9 local color scoring, `/ 6.0` score normalization, cap at `1.0`, and candidate sorting by strongest color score.
- Verified no `src/vision` files were modified and no pipeline integration was added.
- No commit was created.

## Concerns

- The RED output path includes mojibake for the Chinese directory segment due to terminal encoding, but the failure reason is the required missing-module error.
