"""在树莓派上实时验证相机内参和矩形靶面 PnP 距离。

不做整帧去畸变，也不控制电机或激光；只显示尺寸已知的黑色矩形
中心在相机坐标系中的 X/Y/Z 和距离。
"""

from __future__ import print_function

import argparse
import os
import sys
import time

import cv2
import numpy as np


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
SRC_DIR = os.path.join(PROJECT_DIR, "src")
if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)

from coordinate.laser_aim_solver import LaserAimSolver  # noqa: E402
from vision.black_frame_detector import BlackFrameDetector  # noqa: E402


def fourcc_text(value):
    value = int(value)
    return "".join(chr((value >> (8 * index)) & 0xFF) for index in range(4))


def parse_args():
    parser = argparse.ArgumentParser(
        description="Live PnP validation with a known rectangular frame"
    )
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--display-width", type=int, default=960)
    parser.add_argument("--black-threshold", type=int, default=90)
    parser.add_argument(
        "--calibration",
        default="src/calibration/usb_camera_1280x720_v2.npz",
    )
    parser.add_argument(
        "--target-width-mm", type=float, default=297.0,
        help="physical width between the four detected outer corners",
    )
    parser.add_argument(
        "--target-height-mm", type=float, default=210.0,
        help="physical height between the four detected outer corners",
    )
    return parser.parse_args()


def run(args):
    if args.target_width_mm <= 0 or args.target_height_mm <= 0:
        raise ValueError("target dimensions must be positive")

    calibration_path = os.path.abspath(os.path.expanduser(args.calibration))
    detector = BlackFrameDetector(black_threshold=args.black_threshold)
    solver = LaserAimSolver.from_npz(
        calibration_path,
        target_width_m=args.target_width_mm / 1000.0,
        target_height_m=args.target_height_mm / 1000.0,
        max_yaw_step_deg=90.0,
        max_pitch_step_deg=90.0,
        deadband_deg=0.0,
    )

    camera = cv2.VideoCapture(args.camera, cv2.CAP_V4L2)
    camera.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    camera.set(cv2.CAP_PROP_FPS, args.fps)
    camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if not camera.isOpened():
        camera.release()
        raise RuntimeError("Cannot open camera index %d" % args.camera)

    actual_width = int(camera.get(cv2.CAP_PROP_FRAME_WIDTH))
    actual_height = int(camera.get(cv2.CAP_PROP_FRAME_HEIGHT))
    actual_fps = camera.get(cv2.CAP_PROP_FPS)
    actual_fourcc = fourcc_text(camera.get(cv2.CAP_PROP_FOURCC))
    print("Calibration: %s" % calibration_path)
    print("Target outer size: %.1f x %.1f mm" % (
        args.target_width_mm, args.target_height_mm
    ))
    print("Actual camera mode: %dx%d @ %.1f FPS, %s" % (
        actual_width, actual_height, actual_fps, actual_fourcc
    ))
    print("No full-frame remap; solvePnP uses the distortion coefficients.")
    print("Press q or Esc to exit.")

    window = "2025E target pose validation"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)
    display_height = int(args.display_width * actual_height / float(actual_width))
    cv2.resizeWindow(window, args.display_width, display_height)

    smooth_target = None
    previous_time = None
    live_fps = 0.0
    try:
        while True:
            ok, frame = camera.read()
            if not ok or frame is None:
                continue

            now = time.perf_counter()
            if previous_time is not None:
                instant_fps = 1.0 / max(now - previous_time, 1e-6)
                live_fps = instant_fps if live_fps == 0.0 else (
                    0.9 * live_fps + 0.1 * instant_fps
                )
            previous_time = now

            detection = detector.detect(frame)
            preview = BlackFrameDetector.draw(frame, detection)
            command = None
            if detection is not None:
                command = solver.solve_from_corners(detection.corners)

            if command is None:
                smooth_target = None
                cv2.putText(
                    preview, "PNP: NOT AVAILABLE", (16, 68),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 0, 255), 2,
                )
            else:
                target = np.asarray(command.target_camera, dtype=np.float64)
                smooth_target = target if smooth_target is None else (
                    0.8 * smooth_target + 0.2 * target
                )
                distance = float(np.linalg.norm(smooth_target))
                cv2.putText(
                    preview, "X=%+.3fm  Y=%+.3fm  Z=%.3fm" % tuple(smooth_target),
                    (16, 68), cv2.FONT_HERSHEY_SIMPLEX,
                    0.65, (0, 255, 255), 2,
                )
                cv2.putText(
                    preview, "distance=%.3fm  yaw=%+.2f  pitch=%+.2f" % (
                        distance, command.yaw_deg, command.pitch_deg
                    ),
                    (16, 98), cv2.FONT_HERSHEY_SIMPLEX,
                    0.65, (0, 255, 255), 2,
                )

            cv2.putText(
                preview, "LIVE %.1f FPS" % live_fps,
                (16, frame.shape[0] - 18), cv2.FONT_HERSHEY_SIMPLEX,
                0.60, (255, 255, 0), 2,
            )
            cv2.imshow(window, preview)
            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break
    finally:
        camera.release()
        cv2.destroyAllWindows()


def main():
    try:
        run(parse_args())
    except KeyboardInterrupt:
        return 0
    except Exception as error:
        print("ERROR: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
