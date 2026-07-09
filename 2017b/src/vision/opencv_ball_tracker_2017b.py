"""OpenCV ball tracker for the 2017B ball plate project.

This script is the PC/Raspberry Pi replacement for the OpenMV version.

Main features:
  - USB/V4L2 camera input, video file input, or single image input.
  - LAB or HSV color threshold tracking.
  - Noise reduction with blur and morphology.
  - Largest circular target selection.
  - Exponential smoothing for stable ball center coordinates.
  - Optional serial output using the same packet style as the OpenMV script.

Example:
  python opencv_ball_tracker_2017b.py --camera 0 --display
  python opencv_ball_tracker_2017b.py --camera 0 --serial COM5
  python opencv_ball_tracker_2017b.py --camera 0 --serial /dev/ttyUSB0 --no-display
  python opencv_ball_tracker_2017b.py --image test.jpg --display
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Sequence, Tuple

import cv2
import numpy as np


Point = Tuple[int, int]
ColorRange = Tuple[Tuple[int, int, int], Tuple[int, int, int]]


@dataclass
class TrackerConfig:
    color_space: str
    lower: Tuple[int, int, int]
    upper: Tuple[int, int, int]
    min_area: float
    max_area: float
    min_circularity: float
    blur_size: int
    morph_size: int
    smooth_alpha: float
    lost_report_period: int


@dataclass
class Detection:
    ok: bool
    center: Point = (-1, -1)
    raw_center: Point = (-1, -1)
    radius: float = 0.0
    area: float = 0.0
    circularity: float = 0.0
    bbox: Tuple[int, int, int, int] = (0, 0, 0, 0)


class SerialWriter:
    def __init__(self, port: Optional[str], baudrate: int) -> None:
        self.port = port
        self.serial = None
        if not port:
            return

        try:
            import serial  # type: ignore
        except ImportError as exc:
            raise RuntimeError(
                "pyserial is not installed. Install it with: pip install pyserial"
            ) from exc

        self.serial = serial.Serial(port, baudrate=baudrate, timeout=0)

    def write_packet(self, x: int, y: int, ok: bool) -> str:
        if ok:
            packet = "[x=%03d y=%03d ok=1 *]" % (x, y)
        else:
            packet = "[x=-01 y=-01 ok=0 *]"

        if self.serial is not None:
            self.serial.write(packet.encode("ascii"))
        return packet

    def close(self) -> None:
        if self.serial is not None:
            self.serial.close()


class BallTracker:
    def __init__(self, config: TrackerConfig) -> None:
        self.config = config
        self.smooth_center: Optional[Tuple[float, float]] = None

    def detect(self, frame: np.ndarray) -> Tuple[Detection, np.ndarray]:
        mask = self._make_mask(frame)
        contour = self._select_contour(mask)

        if contour is None:
            self.smooth_center = None
            return Detection(ok=False), mask

        moments = cv2.moments(contour)
        if moments["m00"] == 0:
            self.smooth_center = None
            return Detection(ok=False), mask

        raw_x = int(moments["m10"] / moments["m00"])
        raw_y = int(moments["m01"] / moments["m00"])
        center = self._smooth(raw_x, raw_y, frame.shape[1], frame.shape[0])

        area = float(cv2.contourArea(contour))
        perimeter = float(cv2.arcLength(contour, True))
        circularity = contour_circularity(area, perimeter)
        (_, _), radius = cv2.minEnclosingCircle(contour)
        x, y, w, h = cv2.boundingRect(contour)

        return (
            Detection(
                ok=True,
                center=center,
                raw_center=(raw_x, raw_y),
                radius=float(radius),
                area=area,
                circularity=circularity,
                bbox=(x, y, w, h),
            ),
            mask,
        )

    def _make_mask(self, frame: np.ndarray) -> np.ndarray:
        cfg = self.config

        if cfg.blur_size >= 3:
            k = cfg.blur_size if cfg.blur_size % 2 == 1 else cfg.blur_size + 1
            frame = cv2.GaussianBlur(frame, (k, k), 0)

        if cfg.color_space == "lab":
            converted = cv2.cvtColor(frame, cv2.COLOR_BGR2LAB)
        elif cfg.color_space == "hsv":
            converted = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        else:
            raise ValueError("Unsupported color space: %s" % cfg.color_space)

        lower = np.array(cfg.lower, dtype=np.uint8)
        upper = np.array(cfg.upper, dtype=np.uint8)
        mask = cv2.inRange(converted, lower, upper)

        if cfg.morph_size >= 3:
            k = cfg.morph_size
            kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k))
            mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
            mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

        return mask

    def _select_contour(self, mask: np.ndarray) -> Optional[np.ndarray]:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        best_contour = None
        best_score = -1.0

        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area < self.config.min_area or area > self.config.max_area:
                continue

            perimeter = float(cv2.arcLength(contour, True))
            circularity = contour_circularity(area, perimeter)
            if circularity < self.config.min_circularity:
                continue

            score = area * (0.35 + 0.65 * circularity)
            if score > best_score:
                best_score = score
                best_contour = contour

        return best_contour

    def _smooth(self, raw_x: int, raw_y: int, width: int, height: int) -> Point:
        alpha = self.config.smooth_alpha

        if self.smooth_center is None:
            sx = float(raw_x)
            sy = float(raw_y)
        else:
            old_x, old_y = self.smooth_center
            sx = (1.0 - alpha) * old_x + alpha * raw_x
            sy = (1.0 - alpha) * old_y + alpha * raw_y

        self.smooth_center = (sx, sy)
        return (clamp_int(round(sx), 0, width - 1), clamp_int(round(sy), 0, height - 1))


def contour_circularity(area: float, perimeter: float) -> float:
    if perimeter <= 0:
        return 0.0
    return float(4.0 * np.pi * area / (perimeter * perimeter))


def clamp_int(value: float, low: int, high: int) -> int:
    return max(low, min(high, int(value)))


def parse_triplet(value: str) -> Tuple[int, int, int]:
    parts = value.split(",")
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("Use three comma-separated numbers, e.g. 230,122,107")
    try:
        numbers = tuple(int(part.strip()) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Threshold values must be integers") from exc
    if any(number < 0 or number > 255 for number in numbers):
        raise argparse.ArgumentTypeError("Threshold values must be in 0..255")
    return numbers  # type: ignore[return-value]


def parse_roi(value: Optional[str]) -> Optional[Tuple[int, int, int, int]]:
    if not value:
        return None
    parts = value.split(",")
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("ROI must be x,y,w,h")
    x, y, w, h = (int(part.strip()) for part in parts)
    if w <= 0 or h <= 0:
        raise argparse.ArgumentTypeError("ROI width and height must be positive")
    return (x, y, w, h)


def crop_roi(frame: np.ndarray, roi: Optional[Tuple[int, int, int, int]]) -> Tuple[np.ndarray, Point]:
    if roi is None:
        return frame, (0, 0)

    x, y, w, h = roi
    frame_h, frame_w = frame.shape[:2]
    x = clamp_int(x, 0, frame_w - 1)
    y = clamp_int(y, 0, frame_h - 1)
    w = max(1, min(w, frame_w - x))
    h = max(1, min(h, frame_h - y))
    return frame[y : y + h, x : x + w], (x, y)


def offset_detection(detection: Detection, offset: Point) -> Detection:
    if not detection.ok:
        return detection

    ox, oy = offset
    x, y = detection.center
    rx, ry = detection.raw_center
    bx, by, bw, bh = detection.bbox
    detection.center = (x + ox, y + oy)
    detection.raw_center = (rx + ox, ry + oy)
    detection.bbox = (bx + ox, by + oy, bw, bh)
    return detection


def draw_overlay(frame: np.ndarray, detection: Detection, fps: float, packet: str) -> None:
    if detection.ok:
        x, y, w, h = detection.bbox
        cx, cy = detection.center
        raw_x, raw_y = detection.raw_center
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 0, 255), 2)
        cv2.drawMarker(frame, (cx, cy), (0, 255, 0), cv2.MARKER_CROSS, 16, 2)
        cv2.circle(frame, (raw_x, raw_y), max(2, int(detection.radius)), (255, 128, 0), 1)
        line = "ball x=%d y=%d area=%.0f circ=%.2f" % (
            cx,
            cy,
            detection.area,
            detection.circularity,
        )
        color = (0, 255, 0)
    else:
        line = "ball lost"
        color = (0, 0, 255)

    cv2.putText(frame, line, (10, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.65, color, 2)
    cv2.putText(frame, "fps=%.1f %s" % (fps, packet), (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 2)


def open_capture(args: argparse.Namespace) -> cv2.VideoCapture:
    source: object
    if args.video:
        source = args.video
    else:
        source = args.camera

    capture = cv2.VideoCapture(source)
    if not capture.isOpened():
        raise RuntimeError("Cannot open camera/video source: %s" % source)

    if not args.video:
        capture.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        capture.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        if args.exposure is not None:
            capture.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
            capture.set(cv2.CAP_PROP_EXPOSURE, args.exposure)
        if args.gain is not None:
            capture.set(cv2.CAP_PROP_GAIN, args.gain)

    return capture


def process_image(path: Path, tracker: BallTracker, args: argparse.Namespace) -> int:
    frame = cv2.imread(str(path))
    if frame is None:
        print("Cannot read image: %s" % path, file=sys.stderr)
        return 2

    roi_frame, offset = crop_roi(frame, args.roi)
    detection, mask = tracker.detect(roi_frame)
    detection = offset_detection(detection, offset)

    packet = "[x=%03d y=%03d ok=1 *]" % detection.center if detection.ok else "[x=-01 y=-01 ok=0 *]"
    print(packet)

    if args.display:
        draw_overlay(frame, detection, 0.0, packet)
        cv2.imshow("2017B tracker", frame)
        if args.show_mask:
            cv2.imshow("mask", mask)
        cv2.waitKey(0)

    return 0


def run_stream(args: argparse.Namespace, tracker: BallTracker, serial_writer: SerialWriter) -> int:
    capture = open_capture(args)
    lost_count = 0
    last_time = time.perf_counter()
    fps = 0.0

    try:
        while True:
            ok, frame = capture.read()
            if not ok or frame is None:
                print("Frame read failed", file=sys.stderr)
                return 2

            if args.flip:
                frame = cv2.flip(frame, args.flip)

            roi_frame, offset = crop_roi(frame, args.roi)
            detection, mask = tracker.detect(roi_frame)
            detection = offset_detection(detection, offset)

            now = time.perf_counter()
            dt = now - last_time
            last_time = now
            if dt > 0:
                current_fps = 1.0 / dt
                fps = current_fps if fps <= 0 else (0.85 * fps + 0.15 * current_fps)

            packet = ""
            if detection.ok:
                lost_count = 0
                packet = serial_writer.write_packet(detection.center[0], detection.center[1], True)
                if args.print_packets:
                    print("%s fps=%.1f area=%.0f circ=%.2f" % (packet, fps, detection.area, detection.circularity))
            else:
                lost_count += 1
                if lost_count == 1 or lost_count % args.lost_report_period == 0:
                    packet = serial_writer.write_packet(-1, -1, False)
                    if args.print_packets:
                        print("%s fps=%.1f" % (packet, fps))

            if args.display:
                draw_overlay(frame, detection, fps, packet)
                cv2.imshow("2017B tracker", frame)
                if args.show_mask:
                    cv2.imshow("mask", mask)
                key = cv2.waitKey(1) & 0xFF
                if key == 27 or key == ord("q"):
                    break

    finally:
        capture.release()
        serial_writer.close()
        if args.display:
            cv2.destroyAllWindows()

    return 0


def build_config(args: argparse.Namespace) -> TrackerConfig:
    lower, upper = default_threshold(args.color_space)
    if args.lower is not None:
        lower = args.lower
    if args.upper is not None:
        upper = args.upper

    return TrackerConfig(
        color_space=args.color_space,
        lower=lower,
        upper=upper,
        min_area=args.min_area,
        max_area=args.max_area,
        min_circularity=args.min_circularity,
        blur_size=args.blur_size,
        morph_size=args.morph_size,
        smooth_alpha=args.smooth_alpha,
        lost_report_period=args.lost_report_period,
    )


def default_threshold(color_space: str) -> ColorRange:
    if color_space == "lab":
        # OpenMV LAB (L 90..100, A -6..2, B -21..6) mapped to OpenCV LAB
        # (L 0..255, A/B shifted by +128).
        return (230, 122, 107), (255, 130, 134)
    if color_space == "hsv":
        # Bright, low-saturation target: a practical starting point for a
        # white ball under normal indoor lighting.
        return (0, 0, 160), (179, 80, 255)
    raise ValueError("Unsupported color space: %s" % color_space)


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="2017B OpenCV ball tracker")

    source = parser.add_mutually_exclusive_group()
    source.add_argument("--camera", type=int, default=0, help="camera index, usually 0")
    source.add_argument("--video", help="video file path")
    source.add_argument("--image", type=Path, help="single image path for threshold testing")

    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--roi", type=parse_roi, default=None, help="optional crop area: x,y,w,h")
    parser.add_argument("--flip", type=int, choices=(-1, 0, 1), default=None, help="OpenCV flip code")
    parser.add_argument("--exposure", type=float, default=None, help="manual camera exposure if supported")
    parser.add_argument("--gain", type=float, default=None, help="manual camera gain if supported")

    parser.add_argument("--color-space", choices=("lab", "hsv"), default="lab")
    parser.add_argument(
        "--lower",
        type=parse_triplet,
        default=None,
        help="lower threshold. Example: 230,122,107 for LAB or 0,0,160 for HSV",
    )
    parser.add_argument("--upper", type=parse_triplet, default=None, help="upper threshold")
    parser.add_argument("--min-area", type=float, default=80.0)
    parser.add_argument("--max-area", type=float, default=20000.0)
    parser.add_argument("--min-circularity", type=float, default=0.45)
    parser.add_argument("--blur-size", type=int, default=5)
    parser.add_argument("--morph-size", type=int, default=5)
    parser.add_argument("--smooth-alpha", type=float, default=0.45)
    parser.add_argument("--lost-report-period", type=int, default=5)

    parser.add_argument("--serial", default=None, help="serial port, e.g. COM5 or /dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--display", action="store_true", help="show debug window")
    parser.add_argument("--no-display", dest="display", action="store_false")
    parser.add_argument("--show-mask", action="store_true")
    parser.add_argument("--print-packets", action="store_true")
    parser.set_defaults(display=True)

    args = parser.parse_args(argv)

    args.smooth_alpha = min(1.0, max(0.0, args.smooth_alpha))
    args.lost_report_period = max(1, args.lost_report_period)
    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    tracker = BallTracker(build_config(args))

    if args.image:
        return process_image(args.image, tracker, args)

    serial_writer = SerialWriter(args.serial, args.baudrate)
    return run_stream(args, tracker, serial_writer)


if __name__ == "__main__":
    raise SystemExit(main())
