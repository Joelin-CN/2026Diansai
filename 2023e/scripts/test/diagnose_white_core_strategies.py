"""
Diagnose white-core detection across multiple color spaces / strategies.

For every halo found by LAB A-channel, we test multiple "white core" definitions
on the same halo-local ROI and report which ones succeed and what area / diameter
they produce.
"""
from __future__ import annotations

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


# ── shared halo detection (kept identical to isolated_lab_halo_guided_core) ──


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


@dataclass(frozen=True)
class Halo:
    center_uv: tuple[float, float]
    blob_size: float
    response: float
    area_px: int
    mean_a: float
    kind: str  # "red" | "green"


def _find_halos(
    frame_bgr: np.ndarray,
    detector: cv2.SimpleBlobDetector,
    a_red_thresh: int,
    a_green_thresh: int,
) -> list[Halo]:
    lab = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2LAB)
    a_delta = lab[:, :, 1].astype(np.int16) - 128
    red_mask = np.where(a_delta > a_red_thresh, 255, 0).astype(np.uint8)
    green_mask = np.where(a_delta < -a_green_thresh, 255, 0).astype(np.uint8)

    halos: list[Halo] = []
    for mask, kind in [(red_mask, "red"), (green_mask, "green")]:
        for kp in detector.detect(mask):
            cx, cy = kp.pt[0], kp.pt[1]
            r = max(2, int(math.ceil(kp.size / 2.0)))
            x1, y1 = max(0, int(cx) - r), max(0, int(cy) - r)
            x2 = min(mask.shape[1] - 1, int(cx) + r)
            y2 = min(mask.shape[0] - 1, int(cy) + r)
            patch = mask[y1 : y2 + 1, x1 : x2 + 1] > 0
            if np.any(patch):
                yy, xx = np.mgrid[y1 : y2 + 1, x1 : x2 + 1]
                ys, xs = yy[patch], xx[patch]
                area = int(len(xs))
                weights = np.abs(a_delta[ys, xs]).astype(np.float32)
                ws = float(weights.sum())
                center = (
                    (float((xs * weights).sum() / ws), float((ys * weights).sum() / ws))
                    if ws > 0
                    else (cx, cy)
                )
                mean_a = float(a_delta[ys, xs].mean())
            else:
                px = max(0, min(mask.shape[1] - 1, int(round(cx))))
                py = max(0, min(mask.shape[0] - 1, int(round(cy))))
                area = 0
                center = (cx, cy)
                mean_a = float(a_delta[py, px])
            halos.append(Halo(center, kp.size, kp.response, area, mean_a, kind))
    halos.sort(key=lambda h: abs(h.mean_a), reverse=True)
    return halos


# ── white-core strategies ──


def core_lab_neutral(
    frame_bgr: np.ndarray, halo: Halo,
    search_radius: int, diameter_min: float, diameter_max: float,
    l_floor: int, ab_neutral: int, b_neutral: int, dist_penalty: float,
) -> dict[str, Any] | None:
    """Original LAB 3-channel neutral mask (AB close to 0)."""
    cx, cy = halo.center_uv
    lab = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2LAB)
    h, w = lab.shape[:2]
    x1, y1 = max(0, int(cx) - search_radius), max(0, int(cy) - search_radius)
    x2, y2 = min(w, int(cx) + search_radius + 1), min(h, int(cy) + search_radius + 1)
    if x2 <= x1 or y2 <= y1:
        return None

    l_roi = lab[y1:y2, x1:x2, 0]
    a_roi = lab[y1:y2, x1:x2, 1].astype(np.int16) - 128
    b_roi = lab[y1:y2, x1:x2, 2].astype(np.int16) - 128
    yy, xx = np.mgrid[y1:y2, x1:x2]
    dist = np.hypot(xx.astype(np.float32) - cx, yy.astype(np.float32) - cy)

    core_mask = (
        (dist <= float(search_radius))
        & (l_roi >= l_floor)
        & (np.abs(a_roi) <= ab_neutral)
        & (np.abs(b_roi) <= b_neutral)
    ).astype(np.uint8)

    return _extract_best_blob(core_mask, yy, xx, diameter_min, diameter_max, halo.center_uv, dist_penalty,
                              "lab_neutral",
                              extra_stats={"l_mean": float(l_roi[core_mask > 0].mean()) if np.any(core_mask) else None})


def core_l_peak(
    frame_bgr: np.ndarray, halo: Halo,
    search_radius: int, diameter_min: float, diameter_max: float,
    l_percentile: int, dist_penalty: float,
) -> dict[str, Any] | None:
    """L-channel top-percentile bright pixels in halo ROI, no A/B constraint."""
    cx, cy = halo.center_uv
    lab = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2LAB)
    h, w = lab.shape[:2]
    x1, y1 = max(0, int(cx) - search_radius), max(0, int(cy) - search_radius)
    x2, y2 = min(w, int(cx) + search_radius + 1), min(h, int(cy) + search_radius + 1)
    if x2 <= x1 or y2 <= y1:
        return None

    l_roi = lab[y1:y2, x1:x2, 0].astype(np.float32)
    yy, xx = np.mgrid[y1:y2, x1:x2]
    dist = np.hypot(xx.astype(np.float32) - cx, yy.astype(np.float32) - cy)
    in_roi = dist <= float(search_radius)
    if not np.any(in_roi):
        return None

    l_values = l_roi[in_roi]
    thresh = np.percentile(l_values, l_percentile)
    core_mask = np.zeros_like(l_roi, dtype=np.uint8)
    core_mask[in_roi] = (l_roi[in_roi] >= thresh).astype(np.uint8)

    return _extract_best_blob(core_mask, yy, xx, diameter_min, diameter_max, halo.center_uv, dist_penalty,
                              "l_peak",
                              extra_stats={"percentile": l_percentile, "l_threshold": float(thresh)})


def core_hsv_bright(
    frame_bgr: np.ndarray, halo: Halo,
    search_radius: int, diameter_min: float, diameter_max: float,
    v_floor: int, s_ceiling: int, dist_penalty: float,
) -> dict[str, Any] | None:
    """HSV V>=floor and S<=ceiling (like current formal detector)."""
    cx, cy = halo.center_uv
    hsv = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2HSV)
    h, w = hsv.shape[:2]
    x1, y1 = max(0, int(cx) - search_radius), max(0, int(cy) - search_radius)
    x2, y2 = min(w, int(cx) + search_radius + 1), min(h, int(cy) + search_radius + 1)
    if x2 <= x1 or y2 <= y1:
        return None

    s_roi = hsv[y1:y2, x1:x2, 1]
    v_roi = hsv[y1:y2, x1:x2, 2]
    yy, xx = np.mgrid[y1:y2, x1:x2]
    dist = np.hypot(xx.astype(np.float32) - cx, yy.astype(np.float32) - cy)

    core_mask = (
        (dist <= float(search_radius))
        & (v_roi >= v_floor)
        & (s_roi <= s_ceiling)
    ).astype(np.uint8)

    return _extract_best_blob(core_mask, yy, xx, diameter_min, diameter_max, halo.center_uv, dist_penalty,
                              "hsv_bright",
                              extra_stats={"v_max": int(v_roi[core_mask > 0].max()) if np.any(core_mask) else None})


def core_gray_thresh(
    frame_bgr: np.ndarray, halo: Halo,
    search_radius: int, diameter_min: float, diameter_max: float,
    gray_floor: int, dist_penalty: float,
) -> dict[str, Any] | None:
    """Simple grayscale threshold."""
    cx, cy = halo.center_uv
    gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)
    h, w = gray.shape[:2]
    x1, y1 = max(0, int(cx) - search_radius), max(0, int(cy) - search_radius)
    x2, y2 = min(w, int(cx) + search_radius + 1), min(h, int(cy) + search_radius + 1)
    if x2 <= x1 or y2 <= y1:
        return None

    g_roi = gray[y1:y2, x1:x2]
    yy, xx = np.mgrid[y1:y2, x1:x2]
    dist = np.hypot(xx.astype(np.float32) - cx, yy.astype(np.float32) - cy)

    core_mask = ((dist <= float(search_radius)) & (g_roi >= gray_floor)).astype(np.uint8)

    return _extract_best_blob(core_mask, yy, xx, diameter_min, diameter_max, halo.center_uv, dist_penalty,
                              "gray_thresh",
                              extra_stats={"gray_max": int(g_roi[core_mask > 0].max()) if np.any(core_mask) else None})


def _extract_best_blob(
    core_mask: np.ndarray, yy: np.ndarray, xx: np.ndarray,
    diameter_min: float, diameter_max: float,
    halo_center: tuple[float, float],
    dist_penalty: float,
    strategy: str,
    extra_stats: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    """Given a binary core_mask, find connected components in [diameter_min,diameter_max],
    score by area (用户要求用面积筛选), then brightest weighted center."""
    component_count, labels, stats, _ = cv2.connectedComponentsWithStats(core_mask, 8)
    best: dict[str, Any] | None = None
    best_area = -1.0
    for label_idx in range(1, component_count):
        area = float(stats[label_idx, cv2.CC_STAT_AREA])
        diameter = math.sqrt(4.0 * area / math.pi)
        if diameter < diameter_min or diameter > diameter_max:
            continue

        component = (labels == label_idx)
        comp_yy = yy[component]
        comp_xx = xx[component]
        # brightness-weighted center using core_mask value channel (always 1 here,
        # so effectively geometric centroid of the blob)
        ws = float(area)
        if ws <= 0:
            continue
        center = (float(comp_xx.sum() / ws), float(comp_yy.sum() / ws))
        offset = float(math.hypot(center[0] - halo_center[0], center[1] - halo_center[1]))

        if area > best_area:
            best_area = area
            best = {
                "strategy": strategy,
                "center": center,
                "area_px": area,
                "diameter_px": float(diameter),
                "offset_px": offset,
                **(extra_stats or {}),
            }
    return best


# ── strategies registry ──

STRATEGIES: list[tuple[str, Any]] = [
    ("lab_neutral_orig", lambda bgr, halo: core_lab_neutral(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        l_floor=80, ab_neutral=15, b_neutral=15, dist_penalty=1.0,
    )),
    ("l_peak_85", lambda bgr, halo: core_l_peak(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        l_percentile=85, dist_penalty=1.0,
    )),
    ("l_peak_90", lambda bgr, halo: core_l_peak(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        l_percentile=90, dist_penalty=1.0,
    )),
    ("l_peak_95", lambda bgr, halo: core_l_peak(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        l_percentile=95, dist_penalty=1.0,
    )),
    ("hsv_bright_180", lambda bgr, halo: core_hsv_bright(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        v_floor=180, s_ceiling=80, dist_penalty=1.0,
    )),
    ("hsv_bright_140", lambda bgr, halo: core_hsv_bright(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        v_floor=140, s_ceiling=80, dist_penalty=1.0,
    )),
    ("gray_thresh_180", lambda bgr, halo: core_gray_thresh(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        gray_floor=180, dist_penalty=1.0,
    )),
    ("gray_thresh_140", lambda bgr, halo: core_gray_thresh(
        bgr, halo, search_radius=20, diameter_min=2.0, diameter_max=20.0,
        gray_floor=140, dist_penalty=1.0,
    )),
]


# ── main ──

def sample_frame_indices(total_frames: int, start_frame: int, sample_count: int, seed: int) -> list[int]:
    available = list(range(start_frame + 1, total_frames + 1))
    if len(available) <= sample_count:
        return available
    return sorted(random.Random(seed).sample(available, sample_count))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=40)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--output-dir", type=Path, default=None)
    parser.add_argument("--a-red-thresh", type=int, default=15)
    parser.add_argument("--a-green-thresh", type=int, default=10)
    parser.add_argument("--halo-blob-min", type=float, default=2.0)
    parser.add_argument("--halo-blob-max", type=float, default=300.0)
    args = parser.parse_args(argv)

    output_dir = args.output_dir or (DEFAULT_OUTPUT_ROOT / f"diag_white_core_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
    output_dir.mkdir(parents=True, exist_ok=True)

    cap = cv2.VideoCapture(str(args.video))
    if not cap.isOpened():
        print(f"Cannot open: {args.video}")
        return 1

    detector = _build_blob_detector(args.halo_blob_min, args.halo_blob_max)

    try:
        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        sampled = sample_frame_indices(total_frames, args.start_frame, args.sample_count, args.seed)

        rows: list[dict] = []
        strategy_hits: Counter[str] = Counter()
        total_halos = 0
        failed_frames: list[int] = []

        for fid in sampled:
            cap.set(cv2.CAP_PROP_POS_FRAMES, fid - 1)
            ok, frame = cap.read()
            if not ok:
                failed_frames.append(fid)
                continue

            halos = _find_halos(frame, detector, args.a_red_thresh, args.a_green_thresh)
            total_halos += len(halos)

            for hi, halo in enumerate(halos):
                for strategy_name, strategy_fn in STRATEGIES:
                    result = strategy_fn(frame, halo)
                    if result is not None:
                        strategy_hits[strategy_name] += 1
                    rows.append({
                        "frame": fid,
                        "halo_index": hi,
                        "kind": halo.kind,
                        "halo_u": f"{halo.center_uv[0]:.1f}",
                        "halo_v": f"{halo.center_uv[1]:.1f}",
                        "halo_area": halo.area_px,
                        "halo_mean_a": f"{halo.mean_a:.1f}",
                        "strategy": strategy_name,
                        "hit": result is not None,
                        "core_u": f"{result['center'][0]:.1f}" if result else "",
                        "core_v": f"{result['center'][1]:.1f}" if result else "",
                        "core_area": result["area_px"] if result else "",
                        "core_diameter": result["diameter_px"] if result else "",
                        "core_offset": result["offset_px"] if result else "",
                    })

        # ── 打印总结 ──
        print(f"\nTotal halos evaluated: {total_halos}")
        print(f"Frames sampled: {len(sampled)}  Failed: {len(failed_frames)}")
        print(f"\n{'Strategy':<24} {'Hits':>6} {'Hit%':>7} {'AvgArea':>8} {'AvgDia':>8} {'AvgOffset':>9}")
        print("-" * 66)

        # 计算每种策略的命中率和平均面积
        for strategy_name, _ in STRATEGIES:
            hits = strategy_hits.get(strategy_name, 0)
            pct = hits / max(total_halos, 1) * 100
            strategy_rows = [r for r in rows if r["strategy"] == strategy_name and r["hit"]]
            areas = [float(r["core_area"]) for r in strategy_rows if r["core_area"]]
            diams = [float(r["core_diameter"]) for r in strategy_rows if r["core_diameter"]]
            offsets = [float(r["core_offset"]) for r in strategy_rows if r["core_offset"]]
            avg_area = sum(areas) / len(areas) if areas else 0
            avg_dia = sum(diams) / len(diams) if diams else 0
            avg_off = sum(offsets) / len(offsets) if offsets else 0
            print(f"{strategy_name:<24} {hits:>6} {pct:>6.1f}% {avg_area:>8.1f} {avg_dia:>8.1f} {avg_off:>9.1f}")

        # 写 CSV
        fieldnames = ["frame", "halo_index", "kind", "halo_u", "halo_v", "halo_area", "halo_mean_a",
                      "strategy", "hit", "core_u", "core_v", "core_area", "core_diameter", "core_offset"]
        csv_path = output_dir / "white_core_diagnosis.csv"
        with csv_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

        txt_path = output_dir / "white_core_diagnosis.txt"
        with txt_path.open("w", encoding="utf-8") as f:
            f.write(f"Video: {args.video}\n")
            f.write(f"Total halos: {total_halos}\n")
            f.write(f"Frames sampled: {len(sampled)}\n")
            f.write(f"Parameters: a_red>{args.a_red_thresh}  a_green<-{args.a_green_thresh}\n")
            f.write("\n--- Strategy summary ---\n")
            for strategy_name, _ in STRATEGIES:
                hits = strategy_hits.get(strategy_name, 0)
                pct = hits / max(total_halos, 1) * 100
                f.write(f"  {strategy_name}: {hits}/{total_halos} = {pct:.1f}%\n")

        print(f"\nOutput: {output_dir}")
        return 0
    finally:
        cap.release()


if __name__ == "__main__":
    raise SystemExit(main())
