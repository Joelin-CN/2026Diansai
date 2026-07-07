import argparse
import csv
import random
from collections import Counter
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"


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
    mask = cv2.inRange(hsv, np.array([0, 0, 180], dtype=np.uint8), np.array([179, 80, 255], dtype=np.uint8))
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    candidates: list[WhiteCoreCandidate] = []
    value_channel = hsv[:, :, 2].astype(np.float32)

    for contour in contours:
        area = float(cv2.contourArea(contour))
        if area < 2.0 or area > 300.0:
            continue

        contour_mask = np.zeros(mask.shape, dtype=np.uint8)
        cv2.drawContours(contour_mask, [contour], -1, 255, -1)
        ys, xs = np.nonzero(contour_mask)
        if len(xs) == 0:
            continue

        weights = value_channel[ys, xs]
        weight_sum = float(weights.sum())
        if weight_sum <= 0.0:
            continue

        center_x = float((xs * weights).sum() / weight_sum)
        center_y = float((ys * weights).sum() / weight_sum)
        brightness = float(weights.mean())
        red_score, green_score = _local_color_scores(hsv, center_x, center_y, radius=9)
        label = classify_scores(red_score, green_score)
        candidates.append(WhiteCoreCandidate((center_x, center_y), area, brightness, red_score, green_score, label))

    return sorted(candidates, key=lambda candidate: max(candidate.red_score, candidate.green_score), reverse=True)


def sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]:
    available = list(range(start_frame + 1, total_frames + 1))
    if len(available) <= sample_count:
        return available
    return sorted(random.Random(seed).sample(available, sample_count))


def draw_candidates(frame: np.ndarray, candidates: list[WhiteCoreCandidate]) -> np.ndarray:
    annotated = frame.copy()
    colors = {
        "red": (0, 0, 255),
        "green": (0, 255, 0),
        "uncertain": (0, 255, 255),
        "white/no-color": (160, 160, 160),
    }

    for index, candidate in enumerate(candidates):
        center = (int(round(candidate.center[0])), int(round(candidate.center[1])))
        color = colors.get(candidate.label, colors["white/no-color"])
        cv2.circle(annotated, center, 8, color, 2)
        text = f"{index} {candidate.label} r={candidate.red_score:.2f} g={candidate.green_score:.2f}"
        cv2.putText(annotated, text, (center[0] + 10, center[1] - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.4, color, 1)

    return annotated


def default_output_dir() -> Path:
    return DEFAULT_OUTPUT_ROOT / f"white_core_correlation_{datetime.now().strftime('%Y%m%d_%H%M%S')}"


def process_video(video: Path, start_frame: int, sample_count: int, seed: int, output_dir: Path) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Failed to open video: {video}")
        return 1

    try:
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        sampled_frames = sample_frame_indices(total_frames, start_frame, sample_count, seed)
        output_dir.mkdir(parents=True, exist_ok=True)

        rows: list[dict[str, object]] = []
        failed_frames: list[int] = []
        label_counts: Counter[str] = Counter()

        for frame_index in sampled_frames:
            cap.set(cv2.CAP_PROP_POS_FRAMES, frame_index - 1)
            ok, frame = cap.read()
            if not ok:
                failed_frames.append(frame_index)
                continue

            candidates = analyze_frame(frame)
            annotated = draw_candidates(frame, candidates)
            if not _write_image(output_dir / f"frame_{frame_index:04d}.png", annotated):
                failed_frames.append(frame_index)
                continue

            for candidate_index, candidate in enumerate(candidates):
                label_counts[candidate.label] += 1
                rows.append(
                    {
                        "frame": frame_index,
                        "candidate_index": candidate_index,
                        "u_px": candidate.center[0],
                        "v_px": candidate.center[1],
                        "core_area_px": candidate.core_area_px,
                        "brightness": candidate.brightness,
                        "red_score": candidate.red_score,
                        "green_score": candidate.green_score,
                        "label": candidate.label,
                        "score_gap": abs(candidate.red_score - candidate.green_score),
                    }
                )

        _write_summary_csv(output_dir / "summary.csv", rows)
        _write_summary_txt(
            output_dir / "summary.txt",
            video,
            start_frame,
            sample_count,
            seed,
            sampled_frames,
            output_dir,
            failed_frames,
            label_counts,
        )
        print(f"Wrote isolated white-core correlation output to: {output_dir}")
        print(f"Sampled frames: {len(sampled_frames)}; failed frames: {len(failed_frames)}")
        return 0
    finally:
        cap.release()


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


def _write_image(path: Path, image: np.ndarray) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


def _write_summary_txt(
    path: Path,
    video: Path,
    start_frame: int,
    sample_count: int,
    seed: int,
    sampled_frames: list[int],
    output_dir: Path,
    failed_frames: list[int],
    label_counts: Counter[str],
) -> None:
    with path.open("w", encoding="utf-8") as file:
        file.write(f"Video: {video}\n")
        file.write(f"Start frame: {start_frame}\n")
        file.write(f"Requested sample count: {sample_count}\n")
        file.write(f"Seed: {seed}\n")
        file.write(f"Actual sampled frames: {sampled_frames}\n")
        file.write(f"Output directory: {output_dir}\n")
        file.write(f"Failed frames: {failed_frames}\n")
        file.write("Label counts:\n")
        for label in ["red", "green", "uncertain", "white/no-color"]:
            file.write(f"{label}: {label_counts[label]}\n")


def classify_scores(red_score: float, green_score: float) -> str:
    if red_score >= 0.20 and red_score - green_score >= 0.15:
        return "red"
    if green_score >= 0.20 and green_score - red_score >= 0.15:
        return "green"
    if red_score < 0.05 and green_score < 0.05:
        return "white/no-color"
    return "uncertain"


def _local_color_scores(hsv: np.ndarray, center_x: float, center_y: float, radius: int) -> tuple[float, float]:
    height, width = hsv.shape[:2]
    min_x = max(0, int(np.floor(center_x - radius)))
    max_x = min(width - 1, int(np.ceil(center_x + radius)))
    min_y = max(0, int(np.floor(center_y - radius)))
    max_y = min(height - 1, int(np.ceil(center_y + radius)))

    patch = hsv[min_y : max_y + 1, min_x : max_x + 1]
    yy, xx = np.mgrid[min_y : max_y + 1, min_x : max_x + 1]
    distances = np.sqrt((xx.astype(np.float32) - center_x) ** 2 + (yy.astype(np.float32) - center_y) ** 2)
    spatial_weight = np.clip(1.0 - distances / float(radius), 0.0, 1.0)

    hue = patch[:, :, 0]
    saturation = patch[:, :, 1].astype(np.float32) / 255.0
    value = patch[:, :, 2].astype(np.float32) / 255.0
    weighted_color = saturation * value * spatial_weight
    red_support = (hue <= 10) | (hue >= 170)
    green_support = (hue >= 45) & (hue <= 85)

    red_score = float(min(1.0, weighted_color[red_support].sum() / 6.0))
    green_score = float(min(1.0, weighted_color[green_support].sum() / 6.0))
    return red_score, green_score


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Sample video frames for isolated white-core correlation evidence.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=30)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    args = parser.parse_args(argv)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir()
    return process_video(args.video, args.start_frame, args.sample_count, args.seed, output_dir)


if __name__ == "__main__":
    raise SystemExit(main())
