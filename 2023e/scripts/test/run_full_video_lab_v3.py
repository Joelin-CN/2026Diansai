"""
Full-video processing with LAB halo-guided core v3.
Processes every frame, outputs annotated images + full annotated MP4 video.
"""
import sys
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from isolated_lab_halo_guided_core import (
    analyze_frame,
    draw_candidates,
    draw_masks_panorama,
    _build_blob_detector,
    _write_image,
)

DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"


def main() -> int:
    video_path = DEFAULT_VIDEO
    output_dir = DEFAULT_OUTPUT_ROOT / f"lab_halo_v3_full_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    output_dir.mkdir(parents=True, exist_ok=True)

    cap = cv2.VideoCapture(str(video_path))
    if not cap.isOpened():
        print(f"Cannot open: {video_path}")
        return 1

    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"Video: {total_frames} frames, {fps:.1f} fps, {width}x{height}")

    # --- parameters (same as v3 defaults) ---
    a_red_thresh = 15
    a_green_thresh = 8
    halo_blob_min = 2.0
    halo_blob_max = 500.0
    core_dilate_kernel = 5
    core_l_percentile = 90
    core_area_min = 1.0
    core_area_max = 200.0
    core_diameter_min = 1.0
    core_diameter_max = 15.0

    detector = _build_blob_detector(halo_blob_min, halo_blob_max)

    # Video writer for annotated frames
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    # annotated video: 2 panels side by side (original + annotated, each width x height)
    video_out_path = output_dir / "full_annotated.mp4"
    video_writer = cv2.VideoWriter(str(video_out_path), fourcc, fps, (width * 2, height))

    # Video writer for 3-panel masks (L + A + B)
    masks_out_path = output_dir / "full_masks.mp4"
    masks_writer = cv2.VideoWriter(str(masks_out_path), fourcc, fps, (width * 3, height))

    frames_processed = 0
    frames_with_halo = 0
    total_red = 0
    total_green = 0
    total_core = 0

    try:
        for fid in range(1, total_frames + 1):
            ok, frame = cap.read()
            if not ok:
                break

            analysis = analyze_frame(
                frame, detector,
                a_red_thresh, a_green_thresh,
                core_dilate_kernel, core_l_percentile,
                core_area_min, core_area_max,
                core_diameter_min, core_diameter_max,
            )

            has_halo = len(analysis.red_halos) + len(analysis.green_halos) > 0
            if has_halo:
                frames_with_halo += 1
            total_red += len(analysis.red_halos)
            total_green += len(analysis.green_halos)
            total_core += sum(1 for c in analysis.candidates if c.core.found)

            annotated = draw_candidates(frame, analysis, search_radius=10)
            masks = draw_masks_panorama(analysis)

            # Build 2-panel: original | annotated
            side_by_side = np.hstack([frame, annotated])
            if side_by_side.shape[1] != width * 2:
                side_by_side = cv2.resize(side_by_side, (width * 2, height))
            video_writer.write(side_by_side)

            # Build 3-panel masks
            if masks.shape[1] != width * 3:
                masks = cv2.resize(masks, (width * 3, height))
            masks_writer.write(masks)

            frames_processed += 1
            if frames_processed % 100 == 0 or frames_processed == 1:
                percent = frames_processed / total_frames * 100
                print(f"  frame {frames_processed}/{total_frames} ({percent:.0f}%)  "
                      f"halo_frames={frames_with_halo}  red={total_red}  green={total_green}  core={total_core}")

        video_writer.release()
        masks_writer.release()

        print(f"\nOutput: {output_dir}")
        print(f"Frames processed: {frames_processed}")
        print(f"Frames with at least 1 halo: {frames_with_halo}/{frames_processed} "
              f"({frames_with_halo/max(frames_processed,1)*100:.0f}%)")
        total_candidates = total_red + total_green
        print(f"Total red candidates: {total_red}  green: {total_green}")
        print(f"Core found: {total_core}/{total_candidates} "
              f"({total_core/max(total_candidates,1)*100:.0f}%)")
        print(f"\nAnnotated video: {video_out_path}")
        print(f"Masks video:     {masks_out_path}")

        return 0
    finally:
        cap.release()
        try:
            video_writer.release()
        except Exception:
            pass
        try:
            masks_writer.release()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
