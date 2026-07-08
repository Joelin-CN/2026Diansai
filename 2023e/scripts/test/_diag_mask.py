"""Diagnostic: show white mask and blob connectivity around red laser position."""
import sys
from pathlib import Path
import cv2, numpy as np

PROJECT_ROOT = Path(r'E:\B306\2026\电赛\2023e').resolve()
sys.path.insert(0, str(PROJECT_ROOT / 'src'))

from vision.config import load_vision_config
from vision.laser_detector import LaserDetector
from vision.types import LaserColor
from dataclasses import replace

VIDEO = Path(r'E:\B306\Visual\openCV\project\videos\2023e.mp4')
red_config = load_vision_config(PROJECT_ROOT / 'configs' / 'red' / 'vision.json')
relaxed_params = replace(
    red_config.red_laser,
    saturation_min=30, value_min=30,
    area_px_min=2, area_px_max=8000, confidence_min=0.3,
)
relaxed_det = LaserDetector(LaserColor.RED, relaxed_params)
OUT_DIR = PROJECT_ROOT / 'outputs' / 'red_core_mask_diag'
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Frames where relaxed was found (from previous diag)
frames = [50, 416, 470, 486, 590]
cap = cv2.VideoCapture(str(VIDEO))

for fi in frames:
    cap.set(cv2.CAP_PROP_POS_FRAMES, fi - 1)
    ok, frame = cap.read()
    if not ok:
        continue
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    det = relaxed_det.detect(frame)
    if not det.found or not det.image_center:
        continue

    ru = int(round(det.image_center.u_px))
    rv = int(round(det.image_center.v_px))

    # White mask at various thresholds
    mask_v120_s100 = cv2.inRange(hsv, np.array([0,0,120], dtype=np.uint8), np.array([179,100,255], dtype=np.uint8))
    mask_v100_s100 = cv2.inRange(hsv, np.array([0,0,100], dtype=np.uint8), np.array([179,100,255], dtype=np.uint8))
    mask_v80_s100 = cv2.inRange(hsv, np.array([0,0,80], dtype=np.uint8), np.array([179,100,255], dtype=np.uint8))

    # Find connected components in v120 mask
    n_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(mask_v120_s100, connectivity=8)

    # Which component does the red laser belong to?
    laser_label = labels[rv, ru]
    laser_area = stats[laser_label, cv2.CC_STAT_AREA] if laser_label < n_labels else 0
    laser_cx = centroids[laser_label][0] if laser_label < n_labels else 0
    laser_cy = centroids[laser_label][1] if laser_label < n_labels else 0

    # Create overlay: left = frame+marker, right = mask
    h, w = frame.shape[:2]
    combined = np.zeros((h, w * 3, 3), dtype=np.uint8)

    # Panel 1: original + relaxed GT
    p1 = frame.copy()
    cv2.circle(p1, (ru, rv), 20, (0, 0, 255), 3)
    cv2.circle(p1, (ru, rv), 4, (255, 255, 255), -1)
    cv2.putText(p1, f"frame={fi} relaxed=({ru},{rv})", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255), 2)
    combined[0:h, 0:w] = p1

    # Panel 2: mask V>=120 S<=100 with laser marker
    p2 = cv2.cvtColor(mask_v120_s100, cv2.COLOR_GRAY2BGR)
    cv2.circle(p2, (ru, rv), 6, (0, 0, 255), 2)
    # Highlight the giant component containing laser
    p2[labels == laser_label] = (255, 160, 0)  # orange tint
    cv2.circle(p2, (ru, rv), 6, (0, 0, 255), -1)
    cv2.putText(p2, f"Mask V>=120 S<=100 | laser_comp={laser_area}px centroid=({laser_cx:.0f},{laser_cy:.0f})",
                (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,255), 1)
    # Show all components with different colors
    for lbl in range(1, n_labels):
        if lbl == laser_label:
            continue
        p2[labels == lbl] = (0, 255, 255)  # cyan for other components
    combined[0:h, w:2*w] = p2

    # Panel 3: mask threshold comparison (V100, V80)
    p3 = np.zeros((h, w, 3), dtype=np.uint8)
    p3[:, :, 0] = mask_v100_s100  # Blue channel = V>=100
    p3[:, :, 1] = mask_v120_s100  # Green = V>=120
    p3[:, :, 2] = mask_v80_s100   # Red = V>=80
    cv2.circle(p3, (ru, rv), 6, (255, 255, 255), 2)
    cv2.putText(p3, "RGB=V80/V120/V100 masks", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,255), 1)
    # Count white pixels at each threshold at laser neighborhood (5x5)
    rr = 2
    ny1 = max(0, rv-rr); ny2 = min(h, rv+rr+1)
    nx1 = max(0, ru-rr); nx2 = min(w, ru+rr+1)
    cv2.rectangle(p3, (nx1, ny1), (nx2, ny2), (0, 255, 255), 1)
    combined[0:h, 2*w:3*w] = p3

    out_path = OUT_DIR / f"frame_{fi:04d}_mask.png"
    cv2.imencode('.png', combined)[1].tofile(str(out_path))

    hsv_center = hsv[rv, ru]
    print(f"frame={fi} center=({ru},{rv}) HSV=({int(hsv_center[0])},{int(hsv_center[1])},{int(hsv_center[2])}) "
          f"laser_comp_id={laser_label} area={laser_area}px centroid_offset=({laser_cx-ru:.0f},{laser_cy-rv:.0f})")
    # List all large components
    big_comps = [(l, stats[l, cv2.CC_STAT_AREA], centroids[l]) for l in range(1, n_labels) if stats[l, cv2.CC_STAT_AREA] > 100]
    print(f"  Large components (area > 100): {big_comps}")

cap.release()
print(f"\nOutput: {OUT_DIR}")