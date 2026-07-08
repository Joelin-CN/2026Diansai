"""Diagnostic v3: check red laser position, blob coverage, and screen context."""
import sys
from pathlib import Path
import cv2, numpy as np

PROJECT_ROOT = Path(r'E:\B306\2026\电赛\2023e').resolve()
sys.path.insert(0, str(PROJECT_ROOT / 'src'))

from vision.config import load_vision_config, HueRangeDeg
from vision.laser_detector import LaserDetector
from vision.types import LaserColor
from dataclasses import replace

VIDEO = Path(r'E:\B306\Visual\openCV\project\videos\2023e.mp4')

red_config = load_vision_config(PROJECT_ROOT / 'configs' / 'red' / 'vision.json')
relaxed_params = replace(
    red_config.red_laser,
    saturation_min=30,
    value_min=30,
    area_px_min=2,
    area_px_max=8000,
    confidence_min=0.3,
)
relaxed_det = LaserDetector(LaserColor.RED, relaxed_params)

BLUILD_DIR = PROJECT_ROOT / 'outputs' / 'red_core_diag'
BLUILD_DIR.mkdir(parents=True, exist_ok=True)

# Check a few specific frames including early ones
frames_to_check = [42, 50, 60, 80, 91, 101, 113, 116, 130, 138, 196, 261, 288, 373, 416, 446, 470, 486, 561, 590, 638, 708, 882]

def build_blob_det(v_thresh, s_max):
    params = cv2.SimpleBlobDetector_Params()
    params.minThreshold = v_thresh
    params.maxThreshold = 255
    params.minArea = 1.0
    params.maxArea = 300.0
    params.minDistBetweenBlobs = 2.0
    params.filterByArea = True
    params.filterByCircularity = False
    params.filterByConvexity = False
    params.filterByInertia = False
    params.filterByColor = True
    params.blobColor = 255
    return cv2.SimpleBlobDetector_create(params)

cap = cv2.VideoCapture(str(VIDEO))

prev_relaxed = None
for fi in frames_to_check:
    cap.set(cv2.CAP_PROP_POS_FRAMES, fi - 1)
    ok, frame = cap.read()
    if not ok:
        continue
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    bgr = frame.copy()

    det_rel = relaxed_det.detect(frame, previous_detection=prev_relaxed)
    prev_relaxed = det_rel

    # Run blob detection
    blob_det = build_blob_det(120, 100)
    mask_hv = cv2.inRange(hsv, np.array([0,0,120],dtype=np.uint8), np.array([179,100,255],dtype=np.uint8))
    mask_lv = cv2.inRange(hsv, np.array([0,0,100],dtype=np.uint8), np.array([179,100,255],dtype=np.uint8))
    kps_hv = blob_det.detect(mask_hv)
    kps_lv = blob_det.detect(mask_lv)

    # Draw blob candidates
    ann = bgr.copy()
    for kp in kps_hv:
        cx, cy = int(kp.pt[0]), int(kp.pt[1])
        cv2.circle(ann, (cx, cy), 4, (255, 160, 0), 2)  # orange for HIGH-like
    for kp in kps_lv:
        cx, cy = int(kp.pt[0]), int(kp.pt[1])
        cv2.circle(ann, (cx, cy), 4, (100, 100, 255), 1)  # light red for LOW-like

    # Mark relaxed red GT
    info_line = f"frame={fi}"
    if det_rel.found and det_rel.image_center:
        ru = int(round(det_rel.image_center.u_px))
        rv = int(round(det_rel.image_center.v_px))
        cv2.circle(ann, (ru, rv), 20, (0, 0, 255), 3)
        cv2.circle(ann, (ru, rv), 3, (255, 255, 255), -1)
        cv2.putText(ann, "RELAXED_RED", (ru + 25, rv), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,0,255), 2)

        # Check 5x5 around relaxed center
        rr = 5
        ry1 = max(0, rv - rr)
        ry2 = min(hsv.shape[0], rv + rr + 1)
        rx1 = max(0, ru - rr)
        rx2 = min(hsv.shape[1], ru + rr + 1)
        pv = hsv[ry1:ry2, rx1:rx2, 2]
        ps = hsv[ry1:ry2, rx1:rx2, 1]
        good = ps <= 100
        n_w120 = int(np.sum(good & (pv >= 120)))
        n_w100 = int(np.sum(good & (pv >= 100)))
        n_w80  = int(np.sum(good & (pv >= 80)))
        n_tot = pv.size

        # Check nearest blob
        nearest = 999.0
        nearest_kp = None
        for kp in kps_hv:
            d = np.hypot(kp.pt[0]-ru, kp.pt[1]-rv)
            if d < nearest:
                nearest = d; nearest_kp = kp
        for kp in kps_lv:
            d = np.hypot(kp.pt[0]-ru, kp.pt[1]-rv)
            if d < nearest:
                nearest = d; nearest_kp = kp

        if nearest_kp:
            nrx, nry = int(nearest_kp.pt[0]), int(nearest_kp.pt[1])
            cv2.line(ann, (ru, rv), (nrx, nry), (0, 255, 255), 1)

        info_line += (f" | rel=({ru},{rv}) V={int(hsv[rv,ru,2])} S={int(hsv[rv,ru,1])}"
                      f" | w120={n_w120}/{n_tot} w100={n_w100}/{n_tot} w80={n_w80}/{n_tot}"
                      f" | near_blob={nearest:.0f}px")
    else:
        info_line += " | rel=LOST"

    info_line += f" | H-kp={len(kps_hv)} L-kp={len(kps_lv)}"

    # Add screen context: average V in a 200x200 box at top-left (screen background likely white)
    bg_v = float(hsv[10:210, 10:210, 2].mean())
    info_line += f" | bgV={bg_v:.0f}"

    cv2.putText(ann, info_line, (10, hsv.shape[0] - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (255, 255, 255), 1)
    print(info_line)

    out_path = BLUILD_DIR / f"frame_{fi:04d}_diag.png"
    cv2.imencode('.png', ann)[1].tofile(str(out_path))

cap.release()
print(f'\nOutput saved to: {BLUILD_DIR}')