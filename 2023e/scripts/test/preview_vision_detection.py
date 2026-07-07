from __future__ import annotations

import argparse
import sys
from dataclasses import replace
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from vision.config import HueRangeDeg, load_vision_config  # noqa: E402
from vision.laser_detector import LaserDetector  # noqa: E402
from vision.types import ImagePoint, LaserColor, LaserDetection  # noqa: E402
from border_preview_detectors import draw_quad  # noqa: E402
from vision.quad_utils import image_quad_to_numpy  # noqa: E402
from vision.screen_detector import ScreenDetector  # noqa: E402
from vision.tape_quad_detector import TapeQuadDetector  # noqa: E402


DEFAULT_IMAGE = Path(r"E:\B306\Visual\openCV\project\Images\2023e.jpg")


def main() -> int:
    parser = argparse.ArgumentParser(description="Preview red/green laser and border detection on one image.")
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE, help="Input image path.")
    parser.add_argument("--save", type=Path, default=None, help="Optional annotated image output path.")
    parser.add_argument("--strict-only", action="store_true", help="Only use configs/*/vision.json thresholds.")
    parser.add_argument("--no-window", action="store_true", help="Do not call cv2.imshow; useful for smoke checks.")
    args = parser.parse_args()

    image = cv2.imread(str(args.image))
    if image is None:
        print(f"Failed to read image: {args.image}")
        return 1

    annotated = image.copy()
    red_detection, green_detection = detect_lasers(image, relaxed=False)
    relaxed_red = relaxed_green = None
    if not args.strict_only and not red_detection.found:
        relaxed_red, _ = detect_lasers(image, relaxed=True)
    if not args.strict_only and not green_detection.found:
        _, relaxed_green = detect_lasers(image, relaxed=True)
    a4_tape = TapeQuadDetector().detect(image)
    screen_square = ScreenDetector().detect(image, exclude_quad=a4_tape.outer_quad if a4_tape.found else None)

    draw_laser_detection(annotated, red_detection, (0, 0, 255), "red")
    draw_laser_detection(annotated, green_detection, (0, 255, 0), "green")
    if relaxed_red is not None:
        draw_laser_detection(annotated, relaxed_red, (0, 128, 255), "red relaxed")
    if relaxed_green is not None:
        draw_laser_detection(annotated, relaxed_green, (0, 255, 255), "green relaxed")
    if screen_square.found and screen_square.image_corners is not None:
        screen_quad = np.array([[[int(point.u_px), int(point.v_px)]] for point in screen_square.image_corners], dtype=np.int32)
        draw_quad(annotated, screen_quad, (255, 255, 0), "screen_square", 2)
    if a4_tape.found and a4_tape.outer_quad is not None and a4_tape.inner_quad is not None:
        draw_quad(annotated, image_quad_to_numpy(a4_tape.outer_quad), (255, 0, 255), "a4_outer", 3)
        draw_quad(annotated, image_quad_to_numpy(a4_tape.inner_quad), (0, 255, 255), "a4_inner", 2)

    print_detection("red", red_detection)
    print_detection("green", green_detection)
    if relaxed_red is not None:
        print_detection("red_relaxed", relaxed_red)
    if relaxed_green is not None:
        print_detection("green_relaxed", relaxed_green)
    print(
        f"screen_square_found={screen_square.found} area={screen_square.diagnostics.get('area', 0.0):.1f} "
        f"score={screen_square.confidence:.2f} reason={screen_square.failure_reason}"
    )
    print(
        f"a4_tape_found={a4_tape.found} outer_area={(a4_tape.contour_area_px or 0.0):.1f} "
        f"inner_area={(a4_tape.inner_area_px or 0.0):.1f} tape_ratio={(a4_tape.tape_ratio or 0.0):.2f} "
        f"score={a4_tape.confidence:.2f} reason={a4_tape.failure_reason}"
    )

    if args.save is not None:
        args.save.parent.mkdir(parents=True, exist_ok=True)
        cv2.imwrite(str(args.save), annotated)
        print(f"saved={args.save}")

    if not args.no_window:
        cv2.imshow("2023E vision preview", annotated)
        print("Press any key in the image window to close.")
        cv2.waitKey(0)
        cv2.destroyAllWindows()

    return 0


def detect_lasers(image: np.ndarray, relaxed: bool) -> tuple[LaserDetection, LaserDetection]:
    red_config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    green_config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")
    red_params = red_config.red_laser
    green_params = green_config.green_laser
    if relaxed:
        red_params = replace(red_params, saturation_min=30, value_min=30, area_px_min=2, area_px_max=8000, confidence_min=0.3)
        green_params = replace(
            green_params,
            hue_ranges_deg=[HueRangeDeg(35, 95)],
            saturation_min=30,
            value_min=30,
            area_px_min=2,
            area_px_max=8000,
            confidence_min=0.3,
        )
    red_detector = LaserDetector(LaserColor.RED, red_params)
    green_detector = LaserDetector(LaserColor.GREEN, green_params)
    return red_detector.detect(image), green_detector.detect(image)


def detect_likely_border(image: np.ndarray) -> np.ndarray | None:
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    edges = cv2.Canny(blurred, 50, 150)
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 5))
    edges = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, kernel)
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None

    image_area = image.shape[0] * image.shape[1]
    candidates: list[tuple[float, np.ndarray]] = []
    for contour in contours:
        area = cv2.contourArea(contour)
        if area < image_area * 0.03:
            continue
        perimeter = cv2.arcLength(contour, True)
        approx = cv2.approxPolyDP(contour, 0.03 * perimeter, True)
        if len(approx) == 4 and cv2.isContourConvex(approx):
            candidates.append((area, approx))

    if candidates:
        return max(candidates, key=lambda item: item[0])[1]

    largest = max(contours, key=cv2.contourArea)
    rect = cv2.minAreaRect(largest)
    return cv2.boxPoints(rect).astype(np.int32).reshape(-1, 1, 2)


def draw_laser_detection(image: np.ndarray, detection: LaserDetection, color: tuple[int, int, int], label: str) -> None:
    if not detection.found or detection.image_center is None:
        cv2.putText(
            image,
            f"{label}: {detection.failure_reason.value if detection.failure_reason else 'lost'}",
            (20, 40 if label == "red" else 75),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            color,
            2,
        )
        return

    center = to_int_point(detection.image_center)
    radius = max(6, int(round(detection.image_radius_px or 6)))
    cv2.circle(image, center, radius + 8, color, 2)
    cv2.drawMarker(image, center, color, cv2.MARKER_CROSS, 20, 2)
    cv2.putText(
        image,
        f"{label} ({center[0]}, {center[1]}) conf={detection.confidence:.2f}",
        (center[0] + 10, center[1] - 10),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        color,
        2,
    )


def print_detection(label: str, detection: LaserDetection) -> None:
    if detection.found and detection.image_center is not None:
        print(
            f"{label}_found=True center=({detection.image_center.u_px:.1f}, {detection.image_center.v_px:.1f}) "
            f"radius={detection.image_radius_px:.1f} area={detection.area_px:.1f} "
            f"brightness={detection.brightness:.1f} confidence={detection.confidence:.2f}"
        )
    else:
        reason = detection.failure_reason.value if detection.failure_reason else "unknown"
        print(f"{label}_found=False reason={reason}")


def to_int_point(point: ImagePoint) -> tuple[int, int]:
    return int(round(point.u_px)), int(round(point.v_px))


if __name__ == "__main__":
    raise SystemExit(main())
