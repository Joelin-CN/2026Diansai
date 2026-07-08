from __future__ import annotations

import argparse
import csv
import random
import sys
from datetime import datetime
from pathlib import Path

import cv2


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from preview_vision_video import _camera_frame, _draw_runtime_result  # noqa: E402
from vision.config import load_vision_config  # noqa: E402
from vision.pipeline import VisionPipeline  # noqa: E402
from vision.types import VisionFrameResult, VisionMode  # noqa: E402


DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"


def sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]:
    available = list(range(start_frame + 1, total_frames + 1))
    if len(available) <= sample_count:
        return available
    return sorted(random.Random(seed).sample(available, sample_count))


def default_output_dir() -> Path:
    return DEFAULT_OUTPUT_ROOT / f"vision_runtime_eval_{datetime.now().strftime('%Y%m%d_%H%M%S')}"


def process_video(
    video: Path,
    profile: str,
    mode: VisionMode,
    start_frame: int,
    sample_count: int,
    seed: int,
    output_dir: Path,
) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Failed to open video: {video}")
        return 1

    config_path = PROJECT_ROOT / "configs" / profile / "vision.json"
    config = load_vision_config(config_path)
    pipeline: VisionPipeline | None = None

    try:
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        sampled_frames = sample_frame_indices(total_frames, start_frame, sample_count, seed)
        output_dir.mkdir(parents=True, exist_ok=True)

        rows: list[dict] = []
        failed_frames: list[int] = []
        for frame_index in sampled_frames:
            cap.set(cv2.CAP_PROP_POS_FRAMES, frame_index - 1)
            ok, frame = cap.read()
            if not ok:
                failed_frames.append(frame_index)
                continue

            if pipeline is None:
                pipeline = VisionPipeline(profile, config, frame_size=(frame.shape[1], frame.shape[0]))

            result = pipeline.process(_camera_frame(frame, frame_index), mode)
            annotated = frame.copy()
            _draw_runtime_result(annotated, result)

            if not _write_image(output_dir / f"frame_{frame_index:04d}_orig.png", frame):
                failed_frames.append(frame_index)
                continue
            if not _write_image(output_dir / f"frame_{frame_index:04d}_annot.png", annotated):
                failed_frames.append(frame_index)
                continue

            rows.append(_frame_row(frame_index, result))

        _write_summary_csv(output_dir / "summary.csv", rows)
        _write_summary_txt(
            output_dir / "summary.txt",
            video=video,
            profile=profile,
            mode=mode,
            start_frame=start_frame,
            sample_count=sample_count,
            seed=seed,
            sampled_frames=sampled_frames,
            output_dir=output_dir,
            failed_frames=failed_frames,
        )
        print(f"Output: {output_dir}")
        print(f"Sampled: {len(sampled_frames)}  Failed: {len(failed_frames)}")
        return 0
    finally:
        cap.release()


def _frame_row(frame_index: int, result: VisionFrameResult) -> dict:
    red = result.red_laser
    green = result.green_laser
    return {
        "frame": frame_index,
        "red_found": False if red is None else red.found,
        "red_u_px": "" if red is None or red.image_center is None else red.image_center.u_px,
        "red_v_px": "" if red is None or red.image_center is None else red.image_center.v_px,
        "red_confidence": "" if red is None else red.confidence,
        "red_reason": "" if red is None or red.failure_reason is None else red.failure_reason.value,
        "green_found": False if green is None else green.found,
        "green_u_px": "" if green is None or green.image_center is None else green.image_center.u_px,
        "green_v_px": "" if green is None or green.image_center is None else green.image_center.v_px,
        "green_confidence": "" if green is None else green.confidence,
        "green_reason": "" if green is None or green.failure_reason is None else green.failure_reason.value,
        "red_candidate_count": result.diagnostics.candidate_count.get("red_laser", 0),
        "green_candidate_count": result.diagnostics.candidate_count.get("green_laser", 0),
        "processing_time_ms": result.diagnostics.processing_time_ms,
        "red_stage_ms": result.diagnostics.stage_time_ms.get("red_laser", 0.0),
        "green_stage_ms": result.diagnostics.stage_time_ms.get("green_laser", 0.0),
        "a4_stage_ms": result.diagnostics.stage_time_ms.get("a4_tape", 0.0),
        "screen_stage_ms": result.diagnostics.stage_time_ms.get("screen_square", 0.0),
    }


def _write_summary_csv(path: Path, rows: list[dict]) -> None:
    fieldnames = [
        "frame",
        "red_found", "red_u_px", "red_v_px", "red_confidence", "red_reason",
        "green_found", "green_u_px", "green_v_px", "green_confidence", "green_reason",
        "red_candidate_count", "green_candidate_count",
        "processing_time_ms", "red_stage_ms", "green_stage_ms", "a4_stage_ms", "screen_stage_ms",
    ]
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def _write_summary_txt(
    path: Path,
    video: Path,
    profile: str,
    mode: VisionMode,
    start_frame: int,
    sample_count: int,
    seed: int,
    sampled_frames: list[int],
    output_dir: Path,
    failed_frames: list[int],
) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"Video: {video}\n")
        f.write(f"Profile: {profile}\n")
        f.write(f"Mode: {mode.value}\n")
        f.write(f"Start frame: {start_frame}\n")
        f.write(f"Requested sample count: {sample_count}\n")
        f.write(f"Seed: {seed}\n")
        f.write(f"Actual sampled frames: {sampled_frames}\n")
        f.write(f"Output directory: {output_dir}\n")
        f.write(f"Failed frames: {failed_frames}\n")


def _write_image(path: Path, image) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Evaluate formal VisionPipeline runtime path on sampled video frames.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--profile", choices=("red", "green"), default="green")
    parser.add_argument(
        "--runtime-mode",
        choices=tuple(mode.value for mode in VisionMode if mode not in {VisionMode.CALIBRATION, VisionMode.DEBUG}),
        default=VisionMode.RUNTIME_TRACK_RED_GREEN.value,
    )
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=20)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    args = parser.parse_args(argv)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir()
    return process_video(
        video=args.video,
        profile=args.profile,
        mode=VisionMode(args.runtime_mode),
        start_frame=args.start_frame,
        sample_count=args.sample_count,
        seed=args.seed,
        output_dir=output_dir,
    )


if __name__ == "__main__":
    raise SystemExit(main())
