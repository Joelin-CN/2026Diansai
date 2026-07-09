"""
Isolated LAB halo-guided white-core localization — v3.
Changes from v2:
- White core: L-channel bright pixels restricted to the HALO BLOB REGION (dilated 5px),
  NOT the full 20px search circle. This ensures the core center is near the color region.
- a_green_thresh: 10 -> 8
- core_diameter_min: 2.0 -> 1.0
- core_diameter_max: 20.0 -> 15.0 (tighter max)
- core_l_percentile: 95 -> 90 (slightly more pixels in the restricted region)
- When red+green halos overlap, same white core center is ok (explicitly allowed).
"""
import argparse
import csv
import math
import random
from collections import Counter
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any

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
    mask: np.ndarray      # binary mask of THIS halo's connected component
    contour_points: Any   # contour of this halo


@dataclass(frozen=True)
class CoreResult:
    found: bool
    center: tuple[float, float]
    area_px: float | None
    diameter_px: float | None
    offset_px: float | None
    mean_l: float | None
    max_l: float | None


@dataclass(frozen=True)
class Candidate:
    kind: str
    center: tuple[float, float]
    halo_center: tuple[float, float]
    halo_area: int
    halo_mean_a_delta: float
    core: CoreResult


@dataclass(frozen=True)
class FrameAnalysis:
    lab: np.ndarray
    l_channel: np.ndarray
    a_delta: np.ndarray
    b_delta: np.ndarray
    red_mask: np.ndarray
    green_mask: np.ndarray
    red_halos: list[HaloBlob]
    green_halos: list[HaloBlob]
    candidates: list[Candidate]


# ─── blob detector ───

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


# ─── halo extraction (updated: returns mask + contour per halo) ───

def _detect_halos(
    detector: cv2.SimpleBlobDetector,
    mask: np.ndarray,
    a_delta: np.ndarray,
    kind: str,
) -> list[HaloBlob]:
    """Detect halos with per-halo binary mask (connected component)."""
    # Find all connected components in the A-threshold mask
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    halos: list[HaloBlob] = []

    for cnt in contours:
        area_px = float(cv2.contourArea(cnt))
        if area_px < 2.0 or area_px > 500.0:
            continue

        # Build per-halo binary mask
        halo_mask = np.zeros(mask.shape, dtype=np.uint8)
        cv2.drawContours(halo_mask, [cnt], -1, 255, -1)

        # A-weighted centroid within this contour
        ys, xs = np.where(halo_mask > 0)
        if len(xs) == 0:
            continue
        weights = np.abs(a_delta[ys, xs]).astype(np.float32)
        ws = float(weights.sum())
        if ws > 0:
            cx = float((xs * weights).sum() / ws)
            cy = float((ys * weights).sum() / ws)
        else:
            cx = float(xs.mean())
            cy = float(ys.mean())
        center = (cx, cy)

        mean_a = float(a_delta[ys, xs].mean())
        area = int(len(xs))
        # Estimate blob size from area
        blob_size = math.sqrt(4.0 * area / math.pi)

        halos.append(HaloBlob(
            center=center,
            blob_size=blob_size,
            response=abs(mean_a),
            area=area,
            mean_a_delta=mean_a,
            kind=kind,
            mask=halo_mask,
            contour_points=cnt,
        ))

    halos.sort(key=lambda h: abs(h.mean_a_delta), reverse=True)
    return halos


# ─── white core v3: halo-blob-restricted L-bright pixel search ───

def _find_core_in_halo(
    frame_lab: np.ndarray,
    halo: HaloBlob,
    dilate_kernel_size: int,
    l_percentile: int,
    area_min: float,
    area_max: float,
    diameter_min: float,
    diameter_max: float,
) -> CoreResult:
    """Find white core INSIDE the halo blob region (dilated to include edge white pixels).
    
    Strategy:
    1. Dilate the halo mask by dilate_kernel_size px to include pixels just outside
       the A-channel color blob (where the white core typically sits)
    2. Within this restricted region, find L-channel bright pixels (top l_percentile)
    3. Find connected components in [area_min, area_max] & [diameter_min, diameter_max]
    4. Pick the largest component by area
    5. L-brightness-weighted centroid
    """
    h, w = frame_lab.shape[:2]
    
    # Step 1: dilate the halo mask to capture edge/core pixels
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (dilate_kernel_size, dilate_kernel_size))
    search_mask = cv2.dilate(halo.mask, kernel, iterations=1)

    # Step 2: find bright L pixels within the dilated halo region
    l_channel = frame_lab[:, :, 0].astype(np.float32)
    bright_mask = np.zeros(search_mask.shape, dtype=np.uint8)
    region = search_mask > 0
    if not np.any(region):
        return CoreResult(False, halo.center, None, None, None, None, None)

    l_region = l_channel[region]
    if l_region.size == 0:
        return CoreResult(False, halo.center, None, None, None, None, None)

    thresh = float(np.percentile(l_region, l_percentile))
    bright_mask[region] = (l_region >= thresh).astype(np.uint8)

    # Step 3&4: connected components, filter by area/diameter, pick largest
    component_count, labels, stats, _ = cv2.connectedComponentsWithStats(bright_mask, 8)
    best_label = -1
    best_area = -1.0
    for li in range(1, component_count):
        area = float(stats[li, cv2.CC_STAT_AREA])
        diameter = math.sqrt(4.0 * area / math.pi)
        if area < area_min or area > area_max:
            continue
        if diameter < diameter_min or diameter > diameter_max:
            continue
        if area > best_area:
            best_area = area
            best_label = li

    if best_label < 0:
        return CoreResult(False, halo.center, None, None, None, None, None)

    # Step 5: L-brightness-weighted centroid
    component = labels == best_label
    comp_y, comp_x = np.where(component)
    if len(comp_x) == 0:
        return CoreResult(False, halo.center, None, None, None, None, None)

    weights = l_channel[comp_y, comp_x].astype(np.float32)
    ws = float(weights.sum())
    if ws <= 0:
        return CoreResult(False, halo.center, None, None, None, None, None)

    center_x = float((comp_x * weights).sum() / ws)
    center_y = float((comp_y * weights).sum() / ws)
    center = (center_x, center_y)

    offset = float(math.hypot(center_x - halo.center[0], center_y - halo.center[1]))
    comp_l = l_channel[comp_y, comp_x]
    mean_l = float(comp_l.mean())
    max_l = float(comp_l.max())
    diameter = math.sqrt(4.0 * best_area / math.pi)

    return CoreResult(True, center, float(best_area), float(diameter), offset, mean_l, max_l)


# ─── main analysis ───

def analyze_frame(
    frame_bgr: np.ndarray,
    detector: cv2.SimpleBlobDetector,
    a_red_thresh: int,
    a_green_thresh: int,
    core_dilate_kernel: int,
    core_l_percentile: int,
    core_area_min: float,
    core_area_max: float,
    core_diameter_min: float,
    core_diameter_max: float,
) -> FrameAnalysis:
    lab = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2LAB)
    l_channel = lab[:, :, 0]
    a_delta = lab[:, :, 1].astype(np.int16) - 128
    b_delta = lab[:, :, 2].astype(np.int16) - 128

    red_mask = np.where(a_delta > a_red_thresh, 255, 0).astype(np.uint8)
    green_mask = np.where(a_delta < -a_green_thresh, 255, 0).astype(np.uint8)
    red_halos = _detect_halos(detector, red_mask, a_delta, "red")
    green_halos = _detect_halos(detector, green_mask, a_delta, "green")

    candidates: list[Candidate] = []
    for halo in red_halos + green_halos:
        core = _find_core_in_halo(
            lab, halo,
            dilate_kernel_size=core_dilate_kernel,
            l_percentile=core_l_percentile,
            area_min=core_area_min,
            area_max=core_area_max,
            diameter_min=core_diameter_min,
            diameter_max=core_diameter_max,
        )
        candidates.append(Candidate(
            kind=halo.kind,
            center=core.center if core.found else halo.center,
            halo_center=halo.center,
            halo_area=halo.area,
            halo_mean_a_delta=halo.mean_a_delta,
            core=core,
        ))
    candidates.sort(key=lambda c: (c.kind, not c.core.found))
    return FrameAnalysis(lab, l_channel, a_delta, b_delta, red_mask, green_mask, red_halos, green_halos, candidates)


# ─── drawing ───

CANDIDATE_COLORS = {"red": (0, 0, 255), "green": (0, 255, 0)}
WHITE = (255, 255, 255)


def draw_candidates(frame_bgr: np.ndarray, analysis: FrameAnalysis, search_radius: int) -> np.ndarray:
    """Annotated frame with halo circles (A-channel detections) and core crosses."""
    annotated = frame_bgr.copy()
    for idx, cand in enumerate(analysis.candidates):
        halo_uv = (int(round(cand.halo_center[0])), int(round(cand.halo_center[1])))
        center_uv = (int(round(cand.center[0])), int(round(cand.center[1])))
        color = CANDIDATE_COLORS.get(cand.kind, (200, 200, 200))

        # Draw halo: small circle at A-weighted center + 10px search indicator
        cv2.circle(annotated, halo_uv, 10, color, 1)
        cv2.circle(annotated, halo_uv, 5, color, 2)

        if cand.core.found:
            # White core found: white cross + line from halo to core
            _draw_cross(annotated, center_uv, WHITE, 4)
            cv2.circle(annotated, center_uv, 3, WHITE, 1)
            cv2.line(annotated, halo_uv, center_uv, color, 1)
            text = (
                f"#{idx} {cand.kind} CORE dia={cand.core.diameter_px:.1f} "
                f"off={cand.core.offset_px:.1f}"
            )
        else:
            # Fallback: filled dot at halo A-weighted center
            cv2.circle(annotated, center_uv, 3, color, -1)
            text = f"#{idx} {cand.kind} fallback (halo A-centroid)"

        cv2.putText(annotated, text, (center_uv[0] + 8, center_uv[1] - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.33, color, 1)
    return annotated


def draw_masks_panorama(analysis: FrameAnalysis) -> np.ndarray:
    """Three-panel: L with cores, A with halos, B diagnostic."""
    l_vis = cv2.cvtColor(analysis.l_channel, cv2.COLOR_GRAY2BGR)
    a_vis = _signed_channel_vis(analysis.a_delta)
    b_vis = _signed_channel_vis(analysis.b_delta)
    _draw_halo_contours(a_vis, analysis.red_mask, (0, 0, 255))
    _draw_halo_contours(a_vis, analysis.green_mask, (0, 255, 0))
    for cand in analysis.candidates:
        color = CANDIDATE_COLORS.get(cand.kind, (200, 200, 200))
        halo_uv = (int(round(cand.halo_center[0])), int(round(cand.halo_center[1])))
        center_uv = (int(round(cand.center[0])), int(round(cand.center[1])))
        cv2.circle(a_vis, halo_uv, 5, color, 1)
        cv2.circle(l_vis, halo_uv, 10, color, 1)
        if cand.core.found:
            _draw_cross(l_vis, center_uv, WHITE, 4)
        else:
            cv2.circle(l_vis, center_uv, 3, color, -1)
    _label_panel(l_vis, "L channel + halo-restricted core")
    _label_panel(a_vis, "A delta + halo contours (red A>15, green A<-8)")
    _label_panel(b_vis, "B delta (diagnostic)")
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
    cv2.rectangle(image, (0, 0), (400, 24), (0, 0, 0), -1)
    cv2.putText(image, text, (6, 17), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 255), 1)


# ─── main pipeline ───

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
    core_dilate_kernel: int,
    core_l_percentile: int,
    core_area_min: float,
    core_area_max: float,
    core_diameter_min: float,
    core_diameter_max: float,
) -> int:
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        print(f"Cannot open: {video}")
        return 1

    # BlobDetector is only used for mask generation (red_mask, green_mask by A threshold)
    # Actual halo extraction now uses findContours on the mask directly
    detector = _build_blob_detector(halo_blob_min, halo_blob_max)
    print(f"A halos: red>{a_red_thresh}, green<-{a_green_thresh}")
    print(f"Core: dilate={core_dilate_kernel}px, L={core_l_percentile}%, "
          f"area=[{core_area_min},{core_area_max}], dia=[{core_diameter_min},{core_diameter_max}]")

    try:
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        sampled_frames = sample_frame_indices(total_frames, start_frame, sample_count, seed)
        output_dir.mkdir(parents=True, exist_ok=True)

        rows: list[dict] = []
        failed_frames: list[int] = []
        counts: Counter[str] = Counter()
        for fid in sampled_frames:
            cap.set(cv2.CAP_PROP_POS_FRAMES, fid - 1)
            ok, frame = cap.read()
            if not ok:
                failed_frames.append(fid)
                continue

            analysis = analyze_frame(
                frame, detector,
                a_red_thresh, a_green_thresh,
                core_dilate_kernel, core_l_percentile,
                core_area_min, core_area_max,
                core_diameter_min, core_diameter_max,
            )
            counts["frames_written"] += 1
            counts["red_halos"] += len(analysis.red_halos)
            counts["green_halos"] += len(analysis.green_halos)

            annotated = draw_candidates(frame, analysis, 10)
            masks = draw_masks_panorama(analysis)

            if not _write_image(output_dir / f"frame_{fid:04d}_orig.png", frame):
                failed_frames.append(fid)
                continue
            if not _write_image(output_dir / f"frame_{fid:04d}_annot.png", annotated):
                failed_frames.append(fid)
                continue
            if not _write_image(output_dir / f"frame_{fid:04d}_masks.png", masks):
                failed_frames.append(fid)
                continue

            for ci, cand in enumerate(analysis.candidates):
                counts[f"candidate_{cand.kind}"] += 1
                counts["core_found" if cand.core.found else "core_fallback"] += 1
                rows.append(_candidate_row(fid, ci, cand))

        _write_summary_csv(output_dir / "summary.csv", rows)
        _write_summary_txt(
            output_dir / "summary.txt", video, start_frame, sample_count, seed,
            sampled_frames, output_dir, failed_frames, counts,
            a_red_thresh, a_green_thresh,
            core_dilate_kernel, core_l_percentile,
            core_area_min, core_area_max,
            core_diameter_min, core_diameter_max,
        )
        total_cand = counts.get("candidate_red", 0) + counts.get("candidate_green", 0)
        found = counts.get("core_found", 0)
        fallback = counts.get("core_fallback", 0)
        print(f"Output: {output_dir}")
        print(f"Sampled: {len(sampled_frames)}  Failed: {len(failed_frames)}")
        print(f"Red: {counts.get('candidate_red',0)}  Green: {counts.get('candidate_green',0)}  "
              f"CoreFound: {found}/{total_cand} ({found/max(total_cand,1)*100:.0f}%)  "
              f"Fallback: {fallback}/{total_cand} ({fallback/max(total_cand,1)*100:.0f}%)")
        return 0
    finally:
        cap.release()


def _candidate_row(frame_index: int, candidate_index: int, cand: Candidate) -> dict[str, Any]:
    c = cand.core
    return {
        "frame": frame_index,
        "candidate_index": candidate_index,
        "kind": cand.kind,
        "core_found": c.found,
        "u_px": f"{cand.center[0]:.2f}",
        "v_px": f"{cand.center[1]:.2f}",
        "halo_u_px": f"{cand.halo_center[0]:.2f}",
        "halo_v_px": f"{cand.halo_center[1]:.2f}",
        "halo_area": cand.halo_area,
        "halo_mean_a_delta": f"{cand.halo_mean_a_delta:.1f}",
        "core_area_px": f"{c.area_px:.1f}" if c.area_px is not None else "",
        "core_diameter_px": f"{c.diameter_px:.1f}" if c.diameter_px is not None else "",
        "core_mean_l": f"{c.mean_l:.1f}" if c.mean_l is not None else "",
        "core_max_l": f"{c.max_l:.0f}" if c.max_l is not None else "",
        "core_offset_px": f"{c.offset_px:.1f}" if c.offset_px is not None else "",
    }


def _write_summary_csv(path: Path, rows: list[dict]) -> None:
    fieldnames = [
        "frame", "candidate_index", "kind", "core_found", "u_px", "v_px",
        "halo_u_px", "halo_v_px", "halo_area", "halo_mean_a_delta",
        "core_area_px", "core_diameter_px", "core_mean_l", "core_max_l", "core_offset_px",
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
    core_dilate_kernel: int,
    core_l_percentile: int,
    core_area_min: float,
    core_area_max: float,
    core_diameter_min: float,
    core_diameter_max: float,
) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write(f"Video: {video}\n")
        f.write(f"Start frame: {start_frame}\n")
        f.write(f"Requested sample count: {sample_count}\n")
        f.write(f"Seed: {seed}\n")
        f.write(f"Actual sampled frames: {sampled_frames}\n")
        f.write(f"Output directory: {output_dir}\n")
        f.write(f"Failed frames: {failed_frames}\n")
        f.write("\nLAB halo-guided core v3 parameters:\n")
        f.write(f"A red threshold: > {a_red_thresh}\n")
        f.write(f"A green threshold: < -{a_green_thresh}\n")
        f.write(f"Core dilate kernel: {core_dilate_kernel}px\n")
        f.write(f"Core L percentile: {core_l_percentile}%\n")
        f.write(f"Core area: [{core_area_min}, {core_area_max}]\n")
        f.write(f"Core diameter: [{core_diameter_min}, {core_diameter_max}]\n")
        f.write("\nCounts:\n")
        total_cand = counts.get("candidate_red", 0) + counts.get("candidate_green", 0)
        f.write(f"Frames written: {counts.get('frames_written', 0)}\n")
        r = counts.get("red_halos", 0);
        g = counts.get("green_halos", 0)
        cr = counts.get("candidate_red", 0);
        cg = counts.get("candidate_green", 0)
        cf = counts.get("core_found", 0);
        cb = counts.get("core_fallback", 0)
        f.write(f"Red halos/cand: {r}/{cr}  Green halos/cand: {g}/{cg}\n")
        f.write(f"Core found: {cf}/{total_cand}  Fallback: {cb}/{total_cand}\n")


def _write_image(path: Path, image: np.ndarray) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="LAB halo-guided white-core localization v3.")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=30)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    # halo
    parser.add_argument("--a-red-thresh", type=int, default=15)
    parser.add_argument("--a-green-thresh", type=int, default=8)
    parser.add_argument("--halo-blob-min", type=float, default=2.0)
    parser.add_argument("--halo-blob-max", type=float, default=500.0)
    # core
    parser.add_argument("--core-dilate-kernel", type=int, default=5)
    parser.add_argument("--core-l-percentile", type=int, default=90)
    parser.add_argument("--core-area-min", type=float, default=1.0)
    parser.add_argument("--core-area-max", type=float, default=200.0)
    parser.add_argument("--core-diameter-min", type=float, default=1.0)
    parser.add_argument("--core-diameter-max", type=float, default=15.0)
    args = parser.parse_args(argv)

    output_dir = args.output_dir if args.output_dir is not None else default_output_dir()
    return process_video(
        args.video, args.start_frame, args.sample_count, args.seed, output_dir,
        args.a_red_thresh, args.a_green_thresh,
        args.halo_blob_min, args.halo_blob_max,
        args.core_dilate_kernel, args.core_l_percentile,
        args.core_area_min, args.core_area_max,
        args.core_diameter_min, args.core_diameter_max,
    )


if __name__ == "__main__":
    raise SystemExit(main())
