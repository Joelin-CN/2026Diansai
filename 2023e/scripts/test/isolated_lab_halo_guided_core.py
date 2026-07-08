"""
Isolated LAB halo-guided white-core localization prototype.

This keeps the LAB red/green A-channel halo masks as the primary candidate
source, then searches near each halo for a small white core with configurable
diameter. It is intentionally isolated from the formal Vision Layer.
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
class HaloBlob:
    center: tuple[float, float]
    blob_size: float
    response: float
    area: int
    mean_a_delta: float
    kind: str


@dataclass(frozen=True)
class Candidate:
    kind: str
    center: tuple[float, float]
    refine_status: str
    halo_center: tuple[float, float]
    halo_area: int
    halo_mean_a_delta: float
    core_center: tuple[float, float] | None
    core_area: int | None
    core_diameter: float | None
    core_mean_l: float | None
    core_max_l: int | None
    core_a_delta: int | None
    core_b_delta: int | None
    core_offset: float | None
    core_score: float | None


@dataclass(frozen=True)
class FrameAnalysis:
    l_channel: np.ndarray
    a_delta: np.ndarray
    b_delta: np.ndarray
    red_mask: np.ndarray
    green_mask: np.ndarray
    red_halos: list[HaloBlob]
    green_halos: list[HaloBlob]
    candidates: list[Candidate]


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


def analyze_frame(
    frame: np.ndarray,
    detector: cv2.SimpleBlobDetector,
    a_red_thresh: int,
    a_green_thresh: int,
    core_search_radius: int,
    core_diameter_min: float,
    core_diameter_max: float,
    core_l_floor: int,
    core_ab_neutral: int,
    core_b_neutral: int,
    distance_penalty: float,
) -> FrameAnalysis:
    lab = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
    l_channel = lab[:, :, 0]
    a_delta = lab[:, :, 1].astype(np.int16) - 128
    b_delta = lab[:, :, 2].astype(np.int16) - 128

    red_mask = np.where(a_delta > a_red_thresh, 255, 0).astype(np.uint8)
    green_mask = np.where(a_delta < -a_green_thresh, 255, 0).astype(np.uint8)
    red_halos = _detect_halos(detector, red_mask, a_delta, "red")
    green_halos = _detect_halos(detector, green_mask, a_delta, "green")
    candidates = [
        _candidate_from_halo(
            halo, l_channel, a_delta, b_delta,
            core_search_radius, core_diameter_min, core_diameter_max,
            core_l_floor, core_ab_neutral, core_b_neutral, distance_penalty,
        )
        for halo in red_halos + green_halos
    ]
    candidates.sort(key=lambda c: (c.kind, c.refine_status != "core_found", -(c.core_score or 0.0)))
    return FrameAnalysis(l_channel, a_delta, b_delta, red_mask, green_mask, red_halos, green_halos, candidates)


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


def _candidate_from_halo(
    halo: HaloBlob,
    l_channel: np.ndarray,
    a_delta: np.ndarray,
    b_delta: np.ndarray,
    search_radius: int,
    diameter_min: float,
    diameter_max: float,
    l_floor: int,
    ab_neutral: int,
    b_neutral: int,
    distance_penalty: float,
) -> Candidate:
    core = _find_core_near_halo(
        halo.center, l_channel, a_delta, b_delta,
        search_radius, diameter_min, diameter_max,
        l_floor, ab_neutral, b_neutral, distance_penalty,
    )
    if core is None:
        return Candidate(
            halo.kind, halo.center, "fallback_halo_center", halo.center,
            halo.area, halo.mean_a_delta, None, None, None, None, None,
            None, None, None, None,
        )

    center, area, diameter, mean_l, max_l, core_a, core_b, offset, score = core
    return Candidate(
        halo.kind, center, "core_found", halo.center,
        halo.area, halo.mean_a_delta, center, area, diameter,
        mean_l, max_l, core_a, core_b, offset, score,
    )


def _find_core_near_halo(
    halo_center: tuple[float, float],
    l_channel: np.ndarray,
    a_delta: np.ndarray,
    b_delta: np.ndarray,
    search_radius: int,
    diameter_min: float,
    diameter_max: float,
    l_floor: int,
    ab_neutral: int,
    b_neutral: int,
    distance_penalty: float,
) -> tuple[tuple[float, float], int, float, float, int, int, int, float, float] | None:
    cx, cy = halo_center
    x1 = max(0, int(math.floor(cx - search_radius)))
    y1 = max(0, int(math.floor(cy - search_radius)))
    x2 = min(l_channel.shape[1] - 1, int(math.ceil(cx + search_radius)))
    y2 = min(l_channel.shape[0] - 1, int(math.ceil(cy + search_radius)))
    if x2 < x1 or y2 < y1:
        return None

    l_roi = l_channel[y1:y2 + 1, x1:x2 + 1]
    a_roi = a_delta[y1:y2 + 1, x1:x2 + 1]
    b_roi = b_delta[y1:y2 + 1, x1:x2 + 1]
    yy, xx = np.mgrid[y1:y2 + 1, x1:x2 + 1]
    dist = np.hypot(xx.astype(np.float32) - cx, yy.astype(np.float32) - cy)
    core_mask = (
        (dist <= float(search_radius))
        & (l_roi >= l_floor)
        & (np.abs(a_roi) <= ab_neutral)
        & (np.abs(b_roi) <= b_neutral)
    ).astype(np.uint8)

    component_count, labels, stats, _ = cv2.connectedComponentsWithStats(core_mask, 8)
    best = None
    best_score = None
    for label in range(1, component_count):
        area = int(stats[label, cv2.CC_STAT_AREA])
        diameter = math.sqrt(4.0 * area / math.pi)
        if diameter < diameter_min or diameter > diameter_max:
            continue

        component = labels == label
        comp_l = l_roi[component]
        comp_a = a_roi[component]
        comp_b = b_roi[component]
        comp_y = yy[component]
        comp_x = xx[component]
        weights = comp_l.astype(np.float32)
        ws = float(weights.sum())
        if ws <= 0:
            continue

        center = (float((comp_x * weights).sum() / ws), float((comp_y * weights).sum() / ws))
        offset = math.hypot(center[0] - cx, center[1] - cy)
        mean_l = float(comp_l.mean())
        max_l = int(comp_l.max())
        score = mean_l - distance_penalty * offset
        candidate = (
            center, area, float(diameter), mean_l, max_l,
            int(round(float(comp_a.mean()))), int(round(float(comp_b.mean()))),
            float(offset), float(score),
        )
        if best_score is None or score > best_score:
            best = candidate
            best_score = score
    return best


CANDIDATE_COLORS = {"red": (0, 0, 255), "green": (0, 255, 0)}


def draw_candidates(frame: np.ndarray, analysis: FrameAnalysis, search_radius: int) -> np.ndarray:
    annotated = frame.copy()
    for idx, cand in enumerate(analysis.candidates):
        halo = (int(round(cand.halo_center[0])), int(round(cand.halo_center[1])))
        center = (int(round(cand.center[0])), int(round(cand.center[1])))
        color = CANDIDATE_COLORS.get(cand.kind, (200, 200, 200))
        cv2.circle(annotated, halo, search_radius, color, 1)
        cv2.circle(annotated, halo, 6, color, 2)
        if cand.refine_status == "core_found":
            _draw_cross(annotated, center, (255, 255, 255), 5)
            cv2.circle(annotated, center, 4, (255, 255, 255), 1)
            cv2.line(annotated, halo, center, color, 1)
            text = f"{idx} {cand.kind} d={cand.core_diameter:.1f} off={cand.core_offset:.1f}"
        else:
            cv2.circle(annotated, center, 3, color, -1)
            text = f"{idx} {cand.kind} fallback"
        cv2.putText(annotated, text, (center[0] + 8, center[1] - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, color, 1)
    return annotated


def draw_masks_panorama(analysis: FrameAnalysis, search_radius: int) -> np.ndarray:
    l_vis = cv2.cvtColor(analysis.l_channel, cv2.COLOR_GRAY2BGR)
    a_vis = _signed_channel_vis(analysis.a_delta)
    b_vis = _signed_channel_vis(analysis.b_delta)
    _draw_halo_contours(a_vis, analysis.red_mask, (0, 0, 255))
    _draw_halo_contours(a_vis, analysis.green_mask, (0, 255, 0))
    for cand in analysis.candidates:
        color = CANDIDATE_COLORS.get(cand.kind, (200, 200, 200))
        halo = (int(round(cand.halo_center[0])), int(round(cand.halo_center[1])))
        center = (int(round(cand.center[0])), int(round(cand.center[1])))
        cv2.circle(a_vis, halo, 5, color, 1)
        cv2.circle(l_vis, halo, search_radius, color, 1)
        if cand.refine_status == "core_found":
            _draw_cross(l_vis, center, (255, 255, 255), 4)
    _label_panel(l_vis, "L + halo-guided core")
    _label_panel(a_vis, "A delta + halo masks")
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
    return DEFAULT_OUTPUT_ROOT / f"lab_halo_guided_core_{datetime.now().strftime('%Y%m%d_%H%M%S')}"


def process_video(
    video: Path,
    start_frame: int,
    sample_count: int,
    seed: int,
    output_dir: Path,
    a_red_thresh: int,
    a_green_thresh: int,
    halo_blob_min: float,
    halo_blob_max: float,
    core_search_radius: int,
    core_diameter_min: float,
    core_diameter_max: float,
    core_l_floor: int,
    core_ab_neutral: int,
    core_b_neutral: int,
    distance_penalty: float,
) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Failed to open video: {video}")
        return 1

    detector = _build_blob_detector(halo_blob_min, halo_blob_max)
    print(f"A halos: red>{a_red_thresh}, green<-{a_green_thresh}, area=[{halo_blob_min}, {halo_blob_max}]")
    print(f"Core search: radius={core_search_radius}, diameter=[{core_diameter_min}, {core_diameter_max}], L>={core_l_floor}")

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

            analysis = analyze_frame(
                frame, detector, a_red_thresh, a_green_thresh,
                core_search_radius, core_diameter_min, core_diameter_max,
                core_l_floor, core_ab_neutral, core_b_neutral, distance_penalty,
            )
            annotated = draw_candidates(frame, analysis, core_search_radius)
            masks = draw_masks_panorama(analysis, core_search_radius)
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
            counts["red_halos"] += len(analysis.red_halos)
            counts["green_halos"] += len(analysis.green_halos)
            for ci, cand in enumerate(analysis.candidates):
                counts[f"candidate_{cand.kind}"] += 1
                counts[cand.refine_status] += 1
                rows.append(_candidate_row(frame_index, ci, cand))

        _write_summary_csv(output_dir / "summary.csv", rows)
        _write_summary_txt(
            output_dir / "summary.txt", video, start_frame, sample_count, seed,
            sampled_frames, output_dir, failed_frames, counts,
            a_red_thresh, a_green_thresh, halo_blob_min, halo_blob_max,
            core_search_radius, core_diameter_min, core_diameter_max,
            core_l_floor, core_ab_neutral, core_b_neutral, distance_penalty,
        )
        print(f"Output: {output_dir}")
        print(f"Sampled: {len(sampled_frames)}  Failed: {len(failed_frames)}")
        print(
            "Candidates red: {red}  green: {green}  core_found: {found}  fallback: {fallback}".format(
                red=counts.get("candidate_red", 0),
                green=counts.get("candidate_green", 0),
                found=counts.get("core_found", 0),
                fallback=counts.get("fallback_halo_center", 0),
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
        "refine_status": cand.refine_status,
        "u_px": cand.center[0],
        "v_px": cand.center[1],
        "halo_u_px": cand.halo_center[0],
        "halo_v_px": cand.halo_center[1],
        "halo_area": cand.halo_area,
        "halo_mean_a_delta": cand.halo_mean_a_delta,
        "core_u_px": "" if cand.core_center is None else cand.core_center[0],
        "core_v_px": "" if cand.core_center is None else cand.core_center[1],
        "core_area_px": "" if cand.core_area is None else cand.core_area,
        "core_diameter_px": "" if cand.core_diameter is None else cand.core_diameter,
        "core_mean_l": "" if cand.core_mean_l is None else cand.core_mean_l,
        "core_max_l": "" if cand.core_max_l is None else cand.core_max_l,
        "core_a_delta": "" if cand.core_a_delta is None else cand.core_a_delta,
        "core_b_delta": "" if cand.core_b_delta is None else cand.core_b_delta,
        "core_offset_px": "" if cand.core_offset is None else cand.core_offset,
        "core_score": "" if cand.core_score is None else cand.core_score,
    }


def _write_summary_csv(path: Path, rows: list[dict]) -> None:
    fieldnames = [
        "frame", "candidate_index", "kind", "refine_status", "u_px", "v_px",
        "halo_u_px", "halo_v_px", "halo_area", "halo_mean_a_delta",
        "core_u_px", "core_v_px", "core_area_px", "core_diameter_px",
        "core_mean_l", "core_max_l", "core_a_delta", "core_b_delta",
        "core_offset_px", "core_score",
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
    a_red_thresh: int,
    a_green_thresh: int,
    halo_blob_min: float,
    halo_blob_max: float,
    core_search_radius: int,
    core_diameter_min: float,
    core_diameter_max: float,
    core_l_floor: int,
    core_ab_neutral: int,
    core_b_neutral: int,
    distance_penalty: float,
) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"Video: {video}\n")
        f.write(f"Start frame: {start_frame}\n")
        f.write(f"Requested sample count: {sample_count}\n")
        f.write(f"Seed: {seed}\n")
        f.write(f"Actual sampled frames: {sampled_frames}\n")
        f.write(f"Output directory: {output_dir}\n")
        f.write(f"Failed frames: {failed_frames}\n")
        f.write("\nLAB halo-guided core parameters:\n")
        f.write(f"A red threshold: > {a_red_thresh}\n")
        f.write(f"A green threshold: < -{a_green_thresh}\n")
        f.write(f"Halo blob area: [{halo_blob_min}, {halo_blob_max}]\n")
        f.write(f"Core search radius: {core_search_radius}\n")
        f.write(f"Core diameter: [{core_diameter_min}, {core_diameter_max}]\n")
        f.write(f"Core L floor: {core_l_floor}\n")
        f.write(f"Core AB neutral max: {core_ab_neutral}\n")
        f.write(f"Core B neutral max: {core_b_neutral}\n")
        f.write(f"Distance penalty: {distance_penalty}\n")
        f.write("\nCounts:\n")
        f.write(f"Frames written: {counts.get('frames_written', 0)}\n")
        f.write(f"Red halos: {counts.get('red_halos', 0)}\n")
        f.write(f"Green halos: {counts.get('green_halos', 0)}\n")
        f.write(f"Red candidates: {counts.get('candidate_red', 0)}\n")
        f.write(f"Green candidates: {counts.get('candidate_green', 0)}\n")
        f.write(f"Core found: {counts.get('core_found', 0)}\n")
        f.write(f"Fallback halo center: {counts.get('fallback_halo_center', 0)}\n")


def _write_image(path: Path, image: np.ndarray) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Isolated LAB halo-guided white-core localization.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    parser.add_argument("--a-red-thresh", type=int, default=15)
    parser.add_argument("--a-green-thresh", type=int, default=15)
    parser.add_argument("--halo-blob-min", type=float, default=2.0)
    parser.add_argument("--halo-blob-max", type=float, default=300.0)
    parser.add_argument("--core-search-radius", type=int, default=20)
    parser.add_argument("--core-diameter-min", type=float, default=3.0)
    parser.add_argument("--core-diameter-max", type=float, default=15.0)
    parser.add_argument("--core-l-floor", type=int, default=80)
    parser.add_argument("--core-ab-neutral", type=int, default=15)
    parser.add_argument("--core-b-neutral", type=int, default=15)
    parser.add_argument("--distance-penalty", type=float, default=1.0)
    args = parser.parse_args(argv)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir()
    return process_video(
        args.video, args.start_frame, args.sample_count, args.seed, output_dir,
        args.a_red_thresh, args.a_green_thresh,
        args.halo_blob_min, args.halo_blob_max,
        args.core_search_radius, args.core_diameter_min, args.core_diameter_max,
        args.core_l_floor, args.core_ab_neutral, args.core_b_neutral,
        args.distance_penalty,
    )


if __name__ == "__main__":
    raise SystemExit(main())
