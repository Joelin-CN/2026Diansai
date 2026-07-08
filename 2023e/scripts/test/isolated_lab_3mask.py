"""
Isolated LAB 3-mask laser candidate prototype.

Masks:
- white core: local maxima on LAB L channel, with neutral A/B filters
- red halo: positive LAB A channel blobs
- green halo: negative LAB A channel blobs

This script is intentionally isolated from the formal Vision Layer.
"""
import argparse
import csv
import math
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
class WhitePeak:
    center: tuple[float, float]
    l_value: int
    a_delta: int
    b_delta: int


@dataclass(frozen=True)
class HaloBlob:
    center: tuple[float, float]
    blob_size: float
    response: float
    area: int
    mean_a_delta: float
    kind: str


@dataclass(frozen=True)
class Candidate:
    center: tuple[float, float]
    l_value: int
    a_delta: int
    b_delta: int
    kind: str
    halo_kind: str
    halo_distance: float | None
    halo_center: tuple[float, float] | None
    halo_area: int | None
    halo_mean_a_delta: float | None


@dataclass(frozen=True)
class FrameAnalysis:
    l_channel: np.ndarray
    a_delta: np.ndarray
    b_delta: np.ndarray
    red_mask: np.ndarray
    green_mask: np.ndarray
    peaks: list[WhitePeak]
    red_halos: list[HaloBlob]
    green_halos: list[HaloBlob]
    candidates: list[Candidate]
    orphan_halos: list[HaloBlob]


def _build_blob_detector(min_area: float, max_area: float) -> cv2.SimpleBlobDetector:
    params = cv2.SimpleBlobDetector_Params()
    params.minThreshold = 1
    params.maxThreshold = 255
    params.minDistBetweenBlobs = 2.0
    params.filterByArea = True
    params.minArea = float(min_area)
    params.maxArea = float(max_area)
    params.filterByCircularity = False
    params.filterByConvexity = False
    params.filterByInertia = False
    params.filterByColor = True
    params.blobColor = 255
    return cv2.SimpleBlobDetector_create(params)


def analyze_frame_lab(
    frame: np.ndarray,
    detector: cv2.SimpleBlobDetector,
    l_peak_radius: int,
    l_peak_floor: int,
    l_ab_neutral: int,
    a_red_thresh: int,
    a_green_thresh: int,
    assoc_radius: float,
) -> FrameAnalysis:
    lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
    l_channel = lab[:, :, 0]
    a_delta = lab[:, :, 1].astype(np.int16) - 128
    b_delta = lab[:, :, 2].astype(np.int16) - 128

    peaks = _detect_white_peaks(
        l_channel, a_delta, b_delta,
        l_peak_radius, l_peak_floor, l_ab_neutral,
    )
    red_mask = np.where(a_delta > a_red_thresh, 255, 0).astype(np.uint8)
    green_mask = np.where(a_delta < -a_green_thresh, 255, 0).astype(np.uint8)
    red_halos = _detect_halos(detector, red_mask, a_delta, "red")
    green_halos = _detect_halos(detector, green_mask, a_delta, "green")
    candidates, orphan_halos = _associate_peaks(peaks, red_halos, green_halos, assoc_radius)
    return FrameAnalysis(
        l_channel, a_delta, b_delta, red_mask, green_mask,
        peaks, red_halos, green_halos, candidates, orphan_halos,
    )


def _detect_white_peaks(
    l_channel: np.ndarray,
    a_delta: np.ndarray,
    b_delta: np.ndarray,
    radius: int,
    l_floor: int,
    ab_neutral: int,
) -> list[WhitePeak]:
    kernel_size = max(1, radius * 2 + 1)
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (kernel_size, kernel_size))
    kernel[radius, radius] = 0
    local_neighbor_max = cv2.dilate(l_channel, kernel)
    mask = (
        (l_channel > local_neighbor_max)
        & (l_channel >= l_floor)
        & (np.abs(a_delta) <= ab_neutral)
        & (np.abs(b_delta) <= ab_neutral)
    )

    ys, xs = np.where(mask)
    raw = [
        WhitePeak((float(x), float(y)), int(l_channel[y, x]), int(a_delta[y, x]), int(b_delta[y, x]))
        for y, x in zip(ys, xs)
    ]
    return _suppress_close_peaks(raw, float(radius))


def _suppress_close_peaks(peaks: list[WhitePeak], min_dist_px: float) -> list[WhitePeak]:
    kept: list[WhitePeak] = []
    if not peaks:
        return kept

    max_x = int(max(p.center[0] for p in peaks)) + 1
    max_y = int(max(p.center[1] for p in peaks)) + 1
    suppressed = np.zeros((max_y, max_x), dtype=np.uint8)
    radius = max(1, int(round(min_dist_px)))

    for peak in sorted(peaks, key=lambda p: p.l_value, reverse=True):
        x, y = int(round(peak.center[0])), int(round(peak.center[1]))
        if suppressed[y, x]:
            continue
        kept.append(peak)
        cv2.circle(suppressed, (x, y), radius, 255, -1)
    kept.sort(key=lambda p: p.l_value, reverse=True)
    return kept


def _detect_halos(
    detector: cv2.SimpleBlobDetector,
    mask: np.ndarray,
    a_delta: np.ndarray,
    kind: str,
) -> list[HaloBlob]:
    halos: list[HaloBlob] = []
    for kp in detector.detect(mask):
        cx, cy = kp.pt[0], kp.pt[1]
        r = max(2, int(math.ceil(kp.size / 2.0)))
        x1, y1 = max(0, int(cx) - r), max(0, int(cy) - r)
        x2 = min(mask.shape[1] - 1, int(cx) + r)
        y2 = min(mask.shape[0] - 1, int(cy) + r)
        patch = mask[y1:y2 + 1, x1:x2 + 1] > 0
        if np.any(patch):
            yy, xx = np.mgrid[y1:y2 + 1, x1:x2 + 1]
            ys, xs = yy[patch], xx[patch]
            area = int(len(xs))
            weights = np.abs(a_delta[ys, xs]).astype(np.float32)
            ws = float(weights.sum())
            center = (
                (float((xs * weights).sum() / ws), float((ys * weights).sum() / ws))
                if ws > 0
                else (cx, cy)
            )
            mean_a_delta = float(a_delta[ys, xs].mean())
        else:
            px = max(0, min(mask.shape[1] - 1, int(round(cx))))
            py = max(0, min(mask.shape[0] - 1, int(round(cy))))
            area = 0
            center = (cx, cy)
            mean_a_delta = float(a_delta[py, px])
        halos.append(HaloBlob(center, kp.size, kp.response, area, mean_a_delta, kind))
    halos.sort(key=lambda h: abs(h.mean_a_delta), reverse=True)
    return halos


def _associate_peaks(
    peaks: list[WhitePeak],
    red_halos: list[HaloBlob],
    green_halos: list[HaloBlob],
    assoc_radius: float,
) -> tuple[list[Candidate], list[HaloBlob]]:
    halos = red_halos + green_halos
    used_halo_ids: set[int] = set()
    candidates: list[Candidate] = []

    for peak in peaks:
        nearest: HaloBlob | None = None
        nearest_dist: float | None = None
        for halo in halos:
            dist = _distance(peak.center, halo.center)
            if dist <= assoc_radius and (nearest_dist is None or dist < nearest_dist):
                nearest = halo
                nearest_dist = dist
        if nearest is not None and nearest_dist is not None:
            used_halo_ids.add(id(nearest))
            kind = nearest.kind
            candidates.append(Candidate(
                peak.center, peak.l_value, peak.a_delta, peak.b_delta,
                kind, nearest.kind, nearest_dist, nearest.center,
                nearest.area, nearest.mean_a_delta,
            ))
        else:
            candidates.append(Candidate(
                peak.center, peak.l_value, peak.a_delta, peak.b_delta,
                "unknown", "none", None, None, None, None,
            ))

    orphan_halos = [halo for halo in halos if id(halo) not in used_halo_ids]
    candidates.sort(key=lambda c: (c.kind == "unknown", -c.l_value))
    return candidates, orphan_halos


def _distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


CANDIDATE_COLORS = {
    "red": (0, 0, 255),
    "green": (0, 255, 0),
    "unknown": (255, 255, 255),
}


def draw_candidates(frame: np.ndarray, analysis: FrameAnalysis) -> np.ndarray:
    annotated = frame.copy()
    for idx, cand in enumerate(analysis.candidates):
        center = (int(round(cand.center[0])), int(round(cand.center[1])))
        color = CANDIDATE_COLORS.get(cand.kind, (200, 200, 200))
        _draw_cross(annotated, center, (255, 255, 255), 5)
        cv2.circle(annotated, center, 7, color, 2)
        text = f"{idx} {cand.kind} L={cand.l_value} A={cand.a_delta}"
        if cand.halo_distance is not None:
            text += f" d={cand.halo_distance:.1f}"
            halo_center = (
                int(round(cand.halo_center[0])),
                int(round(cand.halo_center[1])),
            )
            cv2.line(annotated, center, halo_center, color, 1)
        cv2.putText(annotated, text, (center[0] + 10, center[1] - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, color, 1)

    for halo in analysis.orphan_halos:
        center = (int(round(halo.center[0])), int(round(halo.center[1])))
        color = CANDIDATE_COLORS.get(halo.kind, (128, 128, 128))
        cv2.circle(annotated, center, 4, color, 1)
        cv2.putText(annotated, f"orphan {halo.kind}", (center[0] + 6, center[1] + 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.3, color, 1)
    return annotated


def draw_masks_panorama(analysis: FrameAnalysis) -> np.ndarray:
    l_vis = cv2.cvtColor(analysis.l_channel, cv2.COLOR_GRAY2BGR)
    a_vis = _signed_channel_vis(analysis.a_delta)
    b_vis = _signed_channel_vis(analysis.b_delta)

    for peak in analysis.peaks:
        center = (int(round(peak.center[0])), int(round(peak.center[1])))
        _draw_cross(l_vis, center, (255, 255, 255), 4)

    _draw_halo_contours(a_vis, analysis.red_mask, (0, 0, 255))
    _draw_halo_contours(a_vis, analysis.green_mask, (0, 255, 0))
    for halo in analysis.red_halos + analysis.green_halos:
        center = (int(round(halo.center[0])), int(round(halo.center[1])))
        cv2.circle(a_vis, center, 5, CANDIDATE_COLORS[halo.kind], 1)

    _label_panel(l_vis, "L channel + white peaks")
    _label_panel(a_vis, "A delta + red/green halos")
    _label_panel(b_vis, "B delta diagnostic")
    return np.hstack([l_vis, a_vis, b_vis])


def _signed_channel_vis(channel: np.ndarray) -> np.ndarray:
    clipped = np.clip(channel, -64, 64).astype(np.float32)
    normalized = ((clipped + 64.0) * (255.0 / 128.0)).astype(np.uint8)
    return cv2.applyColorMap(normalized, cv2.COLORMAP_TURBO)


def _draw_halo_contours(image: np.ndarray, mask: np.ndarray, color: tuple[int, int, int]) -> None:
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    cv2.drawContours(image, contours, -1, color, 1)


def _draw_cross(image: np.ndarray, center: tuple[int, int], color: tuple[int, int, int], radius: int) -> None:
    x, y = center
    cv2.line(image, (x - radius, y), (x + radius, y), color, 1)
    cv2.line(image, (x, y - radius), (x, y + radius), color, 1)


def _label_panel(image: np.ndarray, text: str) -> None:
    cv2.rectangle(image, (0, 0), (300, 24), (0, 0, 0), -1)
    cv2.putText(image, text, (6, 17), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)


def sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]:
    available = list(range(start_frame + 1, total_frames + 1))
    if len(available) <= sample_count:
        return available
    return sorted(random.Random(seed).sample(available, sample_count))


def default_output_dir() -> Path:
    return DEFAULT_OUTPUT_ROOT / f"lab_3mask_{datetime.now().strftime('%Y%m%d_%H%M%S')}"


def process_video(
    video: Path,
    start_frame: int,
    sample_count: int,
    seed: int,
    output_dir: Path,
    l_peak_radius: int,
    l_peak_floor: int,
    l_ab_neutral: int,
    a_red_thresh: int,
    a_green_thresh: int,
    halo_blob_min: float,
    halo_blob_max: float,
    assoc_radius: float,
) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Failed to open video: {video}")
        return 1

    detector = _build_blob_detector(halo_blob_min, halo_blob_max)
    print(f"LAB white peaks: L>={l_peak_floor}, |A|/|B|<={l_ab_neutral}, radius={l_peak_radius}")
    print(f"A halos: red>{a_red_thresh}, green<-{a_green_thresh}, area=[{halo_blob_min}, {halo_blob_max}]")
    print(f"Association radius: {assoc_radius} px")

    try:
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        sampled_frames = sample_frame_indices(total_frames, start_frame, sample_count, seed)
        output_dir.mkdir(parents=True, exist_ok=True)

        rows: list[dict] = []
        failed_frames: list[int] = []
        counts: Counter[str] = Counter()

        for frame_index in sampled_frames:
            cap.set(cv2.CAP_PROP_POS_FRAMES, frame_index - 1)
            ok, frame = cap.read()
            if not ok:
                failed_frames.append(frame_index)
                continue

            analysis = analyze_frame_lab(
                frame, detector,
                l_peak_radius, l_peak_floor, l_ab_neutral,
                a_red_thresh, a_green_thresh, assoc_radius,
            )
            annotated = draw_candidates(frame, analysis)
            masks = draw_masks_panorama(analysis)

            if not _write_image(output_dir / f"frame_{frame_index:04d}_orig.png", frame):
                failed_frames.append(frame_index)
                continue
            if not _write_image(output_dir / f"frame_{frame_index:04d}_annot.png", annotated):
                failed_frames.append(frame_index)
                continue
            if not _write_image(output_dir / f"frame_{frame_index:04d}_masks.png", masks):
                failed_frames.append(frame_index)
                continue

            counts["frames_written"] += 1
            counts["white_peaks"] += len(analysis.peaks)
            counts["red_halos"] += len(analysis.red_halos)
            counts["green_halos"] += len(analysis.green_halos)
            counts["orphan_halos"] += len(analysis.orphan_halos)
            for ci, cand in enumerate(analysis.candidates):
                counts[f"candidate_{cand.kind}"] += 1
                rows.append(_candidate_row(frame_index, ci, cand))

        _write_summary_csv(output_dir / "summary.csv", rows)
        _write_summary_txt(
            output_dir / "summary.txt",
            video, start_frame, sample_count, seed, sampled_frames,
            output_dir, failed_frames, counts,
            l_peak_radius, l_peak_floor, l_ab_neutral,
            a_red_thresh, a_green_thresh,
            halo_blob_min, halo_blob_max, assoc_radius,
        )

        print(f"Output: {output_dir}")
        print(f"Sampled: {len(sampled_frames)}  Failed: {len(failed_frames)}")
        print(
            "Candidates red: {red}  green: {green}  unknown: {unknown}  orphan halos: {orphan}".format(
                red=counts.get("candidate_red", 0),
                green=counts.get("candidate_green", 0),
                unknown=counts.get("candidate_unknown", 0),
                orphan=counts.get("orphan_halos", 0),
            )
        )
        return 0
    finally:
        cap.release()


def _candidate_row(frame_index: int, candidate_index: int, cand: Candidate) -> dict:
    return {
        "frame": frame_index,
        "candidate_index": candidate_index,
        "kind": cand.kind,
        "u_px": cand.center[0],
        "v_px": cand.center[1],
        "l_value": cand.l_value,
        "a_delta": cand.a_delta,
        "b_delta": cand.b_delta,
        "halo_kind": cand.halo_kind,
        "halo_distance_px": "" if cand.halo_distance is None else cand.halo_distance,
        "halo_u_px": "" if cand.halo_center is None else cand.halo_center[0],
        "halo_v_px": "" if cand.halo_center is None else cand.halo_center[1],
        "halo_area": "" if cand.halo_area is None else cand.halo_area,
        "halo_mean_a_delta": "" if cand.halo_mean_a_delta is None else cand.halo_mean_a_delta,
    }


def _write_summary_csv(path: Path, rows: list[dict]) -> None:
    fieldnames = [
        "frame", "candidate_index", "kind", "u_px", "v_px",
        "l_value", "a_delta", "b_delta", "halo_kind", "halo_distance_px",
        "halo_u_px", "halo_v_px", "halo_area", "halo_mean_a_delta",
    ]
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def _write_summary_txt(
    path: Path,
    video: Path,
    start_frame: int,
    sample_count: int,
    seed: int,
    sampled_frames: list[int],
    output_dir: Path,
    failed_frames: list[int],
    counts: Counter[str],
    l_peak_radius: int,
    l_peak_floor: int,
    l_ab_neutral: int,
    a_red_thresh: int,
    a_green_thresh: int,
    halo_blob_min: float,
    halo_blob_max: float,
    assoc_radius: float,
) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"Video: {video}\n")
        f.write(f"Start frame: {start_frame}\n")
        f.write(f"Requested sample count: {sample_count}\n")
        f.write(f"Seed: {seed}\n")
        f.write(f"Actual sampled frames: {sampled_frames}\n")
        f.write(f"Output directory: {output_dir}\n")
        f.write(f"Failed frames: {failed_frames}\n")
        f.write("\nLAB 3-mask parameters:\n")
        f.write(f"L peak radius: {l_peak_radius}\n")
        f.write(f"L peak floor: {l_peak_floor}\n")
        f.write(f"L AB neutral max: {l_ab_neutral}\n")
        f.write(f"A red threshold: > {a_red_thresh}\n")
        f.write(f"A green threshold: < -{a_green_thresh}\n")
        f.write(f"Halo blob area: [{halo_blob_min}, {halo_blob_max}]\n")
        f.write(f"Association radius: {assoc_radius}\n")
        f.write("\nCounts:\n")
        f.write(f"Frames written: {counts.get('frames_written', 0)}\n")
        f.write(f"White peaks: {counts.get('white_peaks', 0)}\n")
        f.write(f"Red halos: {counts.get('red_halos', 0)}\n")
        f.write(f"Green halos: {counts.get('green_halos', 0)}\n")
        f.write(f"Red candidates: {counts.get('candidate_red', 0)}\n")
        f.write(f"Green candidates: {counts.get('candidate_green', 0)}\n")
        f.write(f"Unknown white cores: {counts.get('candidate_unknown', 0)}\n")
        f.write(f"Orphan halos: {counts.get('orphan_halos', 0)}\n")


def _write_image(path: Path, image: np.ndarray) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Isolated LAB 3-mask laser candidate prototype.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    parser.add_argument("--l-peak-radius", type=int, default=5)
    parser.add_argument("--l-peak-floor", type=int, default=80)
    parser.add_argument("--l-ab-neutral", type=int, default=10)
    parser.add_argument("--a-red-thresh", type=int, default=15)
    parser.add_argument("--a-green-thresh", type=int, default=15)
    parser.add_argument("--halo-blob-min", type=float, default=2.0)
    parser.add_argument("--halo-blob-max", type=float, default=300.0)
    parser.add_argument("--assoc-radius", type=float, default=25.0)
    args = parser.parse_args(argv)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir()
    return process_video(
        args.video, args.start_frame, args.sample_count, args.seed, output_dir,
        args.l_peak_radius, args.l_peak_floor, args.l_ab_neutral,
        args.a_red_thresh, args.a_green_thresh,
        args.halo_blob_min, args.halo_blob_max, args.assoc_radius,
    )


if __name__ == "__main__":
    raise SystemExit(main())
