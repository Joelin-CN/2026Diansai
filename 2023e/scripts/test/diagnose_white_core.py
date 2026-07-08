"""
White core detection diagnostic script v2.

Changes from v1:
- Adds SimpleBlobDetector layer alongside findContours
- Adds zoomed crop around ground truth laser positions (5x pixel view)
- No morphological operations on mask
"""
import argparse
import random
import shutil
from pathlib import Path

import cv2
import numpy as np

import sys
PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from vision.config import load_vision_config
from vision.laser_detector import LaserDetector
from vision.types import LaserColor


DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "outputs" / "white_core_diagnosis"


def _build_blob_detector() -> cv2.SimpleBlobDetector:
    params = cv2.SimpleBlobDetector_Params()
    params.minThreshold = 180
    params.maxThreshold = 255
    params.filterByArea = True
    params.minArea = 1.0
    params.maxArea = 300.0
    params.filterByCircularity = False
    params.filterByConvexity = False
    params.filterByInertia = False
    params.filterByColor = True
    params.blobColor = 255  # white blobs on black background
    return cv2.SimpleBlobDetector_create(params)


def _detect_blobs(mask: np.ndarray, detector: cv2.SimpleBlobDetector) -> list[cv2.KeyPoint]:
    # SimpleBlobDetector expects a black background with white blobs
    keypoints = detector.detect(mask)
    return keypoints


def _pixel_zoom(frame: np.ndarray, center: tuple[int, int], size: int = 10, scale: int = 5) -> np.ndarray:
    """Extract a size*scale x size*scale zoomed region around center using nearest-neighbour resize."""
    h, w = frame.shape[:2]
    x, y = int(center[0]), int(center[1])
    x1 = max(0, x - size)
    y1 = max(0, y - size)
    x2 = min(w, x + size + 1)
    y2 = min(h, y + size + 1)
    crop = frame[y1:y2, x1:x2].copy()
    crop_h, crop_w = crop.shape[:2]
    zoomed = cv2.resize(crop, (crop_w * scale, crop_h * scale), interpolation=cv2.INTER_NEAREST)
    return zoomed


def diagnose_frame(
    frame: np.ndarray,
    frame_index: int,
    red_detector: LaserDetector,
    green_detector: LaserDetector,
    blob_detector: cv2.SimpleBlobDetector,
) -> dict[str, np.ndarray]:
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    red_detection = red_detector.detect(frame)
    green_detection = green_detector.detect(frame)

    # --- Layer 1: Original + ground truth ---
    original_vis = frame.copy()
    red_gt_center = None; green_gt_center = None
    if red_detection.found and red_detection.image_center:
        red_gt_center = (int(red_detection.image_center.u_px), int(red_detection.image_center.v_px))
        cv2.circle(original_vis, red_gt_center, 15, (0, 0, 255), 3)
        cv2.putText(original_vis, "RED (strict)", (red_gt_center[0] + 20, red_gt_center[1]),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
    if green_detection.found and green_detection.image_center:
        green_gt_center = (int(green_detection.image_center.u_px), int(green_detection.image_center.v_px))
        cv2.circle(original_vis, green_gt_center, 15, (0, 255, 0), 3)
        cv2.putText(original_vis, "GREEN (strict)", (green_gt_center[0] + 20, green_gt_center[1]),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
    cv2.putText(original_vis, f"Frame {frame_index}: Ground Truth", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)

    # --- Layer 2: White mask (no morph) ---
    white_mask = cv2.inRange(
        hsv,
        np.array([0, 0, 180], dtype=np.uint8),
        np.array([179, 80, 255], dtype=np.uint8),
    )
    white_mask_vis = cv2.cvtColor(white_mask, cv2.COLOR_GRAY2BGR)
    cv2.putText(white_mask_vis, f"Frame {frame_index}: White Core Mask (V>=180, S<=80, NO MORPH)",
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    cv2.putText(white_mask_vis, f"White pixels: {cv2.countNonZero(white_mask)}",
                (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

    # --- Layer 3: Contour-based (old) ---
    contours, _ = cv2.findContours(white_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contours_vis = frame.copy()
    for contour in contours:
        area = cv2.contourArea(contour)
        color = (0, 255, 0) if 1.0 <= area <= 300.0 else (0, 0, 255)
        cv2.drawContours(contours_vis, [contour], -1, color, 1)
        M = cv2.moments(contour)
        if M["m00"] > 0:
            cx, cy = int(M["m10"] / M["m00"]), int(M["m01"] / M["m00"])
            cv2.circle(contours_vis, (cx, cy), 3, color, -1)
    if red_gt_center:
        cv2.circle(contours_vis, red_gt_center, 12, (0, 0, 255), 2)
    if green_gt_center:
        cv2.circle(contours_vis, green_gt_center, 12, (0, 255, 0), 2)
    cv2.putText(contours_vis, f"Frame {frame_index}: findContours ({len(contours)} contours)",
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

    # --- Layer 4: Blob-based (new) ---
    keypoints = _detect_blobs(white_mask, blob_detector)
    blob_vis = frame.copy()
    for kp in keypoints:
        cx, cy = int(kp.pt[0]), int(kp.pt[1])
        diam = int(kp.size)
        cv2.circle(blob_vis, (cx, cy), max(3, diam // 2), (255, 0, 255), 2)
        cv2.circle(blob_vis, (cx, cy), 2, (255, 255, 255), -1)
        cv2.putText(blob_vis, f"r={kp.response:.0f}", (cx + 8, cy - 6),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, (255, 0, 255), 1)
    if red_gt_center:
        cv2.circle(blob_vis, red_gt_center, 12, (0, 0, 255), 2)
    if green_gt_center:
        cv2.circle(blob_vis, green_gt_center, 12, (0, 255, 0), 2)
    cv2.putText(blob_vis, f"Frame {frame_index}: SimpleBlobDetector ({len(keypoints)} blobs, minArea=1)",
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

    # --- Layer 5: Pixel zoom ---
    zoom_rows = []
    zoom_labels = []
    if red_gt_center is not None:
        z = _pixel_zoom(frame, red_gt_center, size=8, scale=6)
        cv2.rectangle(z, (0, 0), (z.shape[1] - 1, z.shape[0] - 1), (0, 0, 255), 3)
        zoom_rows.append(z)
        zoom_labels.append("RED")
        # Also zoom the mask
        zm = _pixel_zoom(white_mask, red_gt_center, size=8, scale=6)
        zm_bgr = cv2.cvtColor(zm, cv2.COLOR_GRAY2BGR)
        cv2.rectangle(zm_bgr, (0, 0), (zm_bgr.shape[1] - 1, zm_bgr.shape[0] - 1), (0, 0, 255), 3)
        zoom_rows.append(zm_bgr)
        zoom_labels.append("RED-mask")
    if green_gt_center is not None:
        z = _pixel_zoom(frame, green_gt_center, size=8, scale=6)
        cv2.rectangle(z, (0, 0), (z.shape[1] - 1, z.shape[0] - 1), (0, 255, 0), 3)
        zoom_rows.append(z)
        zoom_labels.append("GREEN")
        zm = _pixel_zoom(white_mask, green_gt_center, size=8, scale=6)
        zm_bgr = cv2.cvtColor(zm, cv2.COLOR_GRAY2BGR)
        cv2.rectangle(zm_bgr, (0, 0), (zm_bgr.shape[1] - 1, zm_bgr.shape[0] - 1), (0, 255, 0), 3)
        zoom_rows.append(zm_bgr)
        zoom_labels.append("GREEN-mask")

    if zoom_rows:
        max_zoom_h = max(z.shape[0] for z in zoom_rows)
        total_zoom_w = sum(z.shape[1] for z in zoom_rows)
        zoom_vis = np.zeros((max_zoom_h, total_zoom_w, 3), dtype=np.uint8)
        x_offset = 0
        for i, z in enumerate(zoom_rows):
            zoom_vis[0:z.shape[0], x_offset:x_offset + z.shape[1]] = z
            label = zoom_labels[i] if i < len(zoom_labels) else ""
            cv2.putText(zoom_vis, label, (x_offset + 4, z.shape[0] - 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
            x_offset += z.shape[1]
    else:
        zoom_vis = np.zeros((60, 200, 3), dtype=np.uint8)
        cv2.putText(zoom_vis, "No GT found", (10, 35),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

    return {
        'original': original_vis,
        'white_mask': white_mask_vis,
        'contours': contours_vis,
        'blob': blob_vis,
        'pixel_zoom': zoom_vis,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Diagnose white core detection v2 — blob + zoom")
    parser.add_argument("--video", type=Path, default=DEFAULT_VIDEO)
    parser.add_argument("--frames", type=str, default=None)
    parser.add_argument("--start-frame", type=int, default=41)
    parser.add_argument("--sample-count", type=int, default=20)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    args = parser.parse_args(argv)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    # Clear previous output to avoid mixing old/new layers
    for prev_file in args.output_dir.glob("frame_*"):
        prev_file.unlink()
    print(f"Cleared previous output in {args.output_dir}")

    cap_probe = cv2.VideoCapture(str(args.video))
    if not cap_probe.isOpened():
        print(f"Failed to open video: {args.video}")
        return 1
    total_frames = int(cap_probe.get(cv2.CAP_PROP_FRAME_COUNT))
    cap_probe.release()

    if args.frames is not None:
        frame_indices = [int(x.strip()) for x in args.frames.split(",")]
    else:
        available = list(range(args.start_frame + 1, total_frames + 1))
        k = min(args.sample_count, len(available))
        frame_indices = sorted(random.Random(args.seed).sample(available, k))
        print(f"Random sample (seed={args.seed}): {frame_indices}")

    red_config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    green_config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    red_detector = LaserDetector(LaserColor.RED, red_config.red_laser)
    green_detector = LaserDetector(LaserColor.GREEN, green_config.green_laser)
    blob_detector = _build_blob_detector()

    cap = cv2.VideoCapture(str(args.video))
    if not cap.isOpened():
        return 1

    try:
        for frame_idx in frame_indices:
            cap.set(cv2.CAP_PROP_POS_FRAMES, frame_idx - 1)
            ok, frame = cap.read()
            if not ok:
                print(f"Failed to read frame {frame_idx}")
                continue

            layers = diagnose_frame(frame, frame_idx, red_detector, green_detector, blob_detector)

            for layer_name, layer_img in layers.items():
                output_path = args.output_dir / f"frame_{frame_idx:04d}_{layer_name}.png"
                success = _write_image(output_path, layer_img)
                if not success:
                    print(f"Failed to write {output_path}")

            # Combined view: 3x2 grid
            h, w = frame.shape[:2]
            # Determine grid: original | mask | zoom
            #              contours | blob  | (pad/zoom)
            pad_w = w
            combined = np.zeros((h * 2, w * 3, 3), dtype=np.uint8)
            combined[0:h, 0:w] = layers['original']
            combined[0:h, w:2*w] = _fit_to(layers['white_mask'], h, w)
            combined[0:h, 2*w:3*w] = _fit_to(layers['pixel_zoom'], h, w)
            combined[h:2*h, 0:w] = layers['contours']
            combined[h:2*h, w:2*w] = layers['blob']
            # bottom-right pad: blank
            combined[h:2*h, 2*w:3*w] = np.zeros((h, w, 3), dtype=np.uint8)

            combined_path = args.output_dir / f"frame_{frame_idx:04d}_combined.png"
            if _write_image(combined_path, combined):
                print(f"Wrote {combined_path}")

        print(f"\nDiagnosis complete. Output in: {args.output_dir}")
        return 0
    finally:
        cap.release()


def _pixel_zoom_standalone(frame, red_det, green_det, white_mask):
    """Fallback zoom if no gt, used for grid padding."""
    zoom_rows = []
    if red_det.found and red_det.image_center:
        zoom_rows.append(_pixel_zoom(frame, (int(red_det.image_center.u_px), int(red_det.image_center.v_px))))
    if green_det.found and green_det.image_center:
        zoom_rows.append(_pixel_zoom(frame, (int(green_det.image_center.u_px), int(green_det.image_center.v_px))))
    if not zoom_rows:
        z = np.zeros((60, 100, 3), dtype=np.uint8)
        cv2.putText(z, "No GT", (10, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        return z
    result = zoom_rows[0]
    for z in zoom_rows[1:]:
        result = np.hstack([result, z])
    return result


def _fit_to(img: np.ndarray, target_h: int, target_w: int) -> np.ndarray:
    """Pad or crop image to fit target dimensions."""
    h, w = img.shape[:2]
    if img.ndim == 2:
        img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGR)
    if h == target_h and w == target_w:
        return img
    if h <= target_h and w <= target_w:
        result = np.zeros((target_h, target_w, 3), dtype=np.uint8)
        result[0:h, 0:w] = img
        return result
    # crop
    return img[0:target_h, 0:target_w]


def _write_image(path: Path, image: np.ndarray) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        return False
    encoded.tofile(str(path))
    return path.exists()


if __name__ == "__main__":
    raise SystemExit(main())
