# Isolated White-Core Correlation Test Design

## Goal

Validate the white-laser-core idea in isolation before changing formal Vision Layer behavior.

The experiment should answer whether pixels near a bright white laser core contain enough local red or green color evidence to distinguish red and green laser spots on sampled video frames.

## Scope

This phase is a pure script experiment.

Allowed changes:

- Add one script under `scripts/test/`.
- Write generated evidence under a new `outputs/` subdirectory.

Not allowed in this phase:

- Do not modify `src/vision` behavior.
- Do not integrate with `VisionPipeline`.
- Do not replace `LaserDetector` logic.
- Do not add motor/control/screen-space behavior.

## Script

Add:

```text
scripts/test/isolated_white_core_correlation.py
```

Default input video:

```text
E:\B306\Visual\openCV\project\videos\2023e.mp4
```

The script should support command-line options for:

- `--video`
- `--start-frame`, default `41`
- `--sample-count`, default `30`
- `--seed`, default fixed value for reproducible random sampling
- `--output-dir`, optional; default creates a timestamped directory under `outputs/`

## Sampling

The script should randomly sample 30 frames after frame 41, using a fixed random seed.

Sampling should be reproducible for the same video, frame range, count, and seed.

The script should tolerate videos with fewer available frames after the start frame by sampling as many as are available and reporting the actual count.

## Algorithm

For each sampled frame:

1. Convert BGR frame to HSV.
2. Build a white-core mask using bright, low-saturation pixels.
3. Extract small white-core contours.
4. Reject obvious non-laser bright regions by area.
5. Compute a brightness-weighted center for each core.
6. Compute local `red_score` and `green_score` in a small patch around the core.
7. Classify each candidate as `red`, `green`, `uncertain`, or `white/no-color`.
8. Draw annotated candidates on the frame.

The first version should keep the scoring simple and explainable:

- red hue support: hue near 0 or 179
- green hue support: hue in the green range
- support weighted by saturation, value, and distance to the core center

## Initial Classification Rules

- `red` if `red_score >= 0.20` and `red_score - green_score >= 0.15`
- `green` if `green_score >= 0.20` and `green_score - red_score >= 0.15`
- `white/no-color` if `red_score < 0.05` and `green_score < 0.05`
- otherwise `uncertain`

These thresholds are diagnostic defaults, not formal runtime parameters.

## Output

Create a directory like:

```text
outputs/white_core_correlation_YYYYMMDD_HHMMSS/
```

For each sampled frame, write an annotated image:

```text
frame_0042.png
frame_0067.png
...
```

Candidate annotation colors:

- red label: red circle
- green label: green circle
- uncertain label: yellow circle
- white/no-color label: gray circle

Each candidate should be labeled with candidate index plus `r=<score>` and `g=<score>`.

Write `summary.csv` with at least:

- `frame`
- `candidate_index`
- `u_px`
- `v_px`
- `core_area_px`
- `brightness`
- `red_score`
- `green_score`
- `label`
- `score_gap`

Write `summary.txt` with:

- input video path
- start frame
- sample count requested
- actual sampled frames
- output directory
- label counts
- any frames that failed to read

## Success Criteria

- The script runs from PowerShell with the project Python environment.
- It produces annotated images and summary files under `outputs/`.
- It does not modify formal Vision Layer behavior.
- The output is sufficient for manual visual inspection of whether white-core local color correlation separates red and green lasers.

## Verification Command

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 30
```
