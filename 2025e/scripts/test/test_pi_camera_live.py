"""树莓派实时黑框检测测试入口，适合在 VNC 桌面中查看。"""

from __future__ import print_function

import argparse
import os
import sys
import time
from collections import deque

import cv2


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
SRC_DIR = os.path.join(PROJECT_DIR, "src")
if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)

from vision.black_frame_detector import BlackFrameDetector  # noqa: E402
from vision.target_tracker import TargetTracker  # noqa: E402


class OpenCVCamera(object):
    """USB 摄像头或能够通过 V4L2 打开的摄像头。"""

    name = "OpenCV/V4L2"

    def __init__(self, index, width, height, fps):
        backend = cv2.CAP_V4L2 if hasattr(cv2, "CAP_V4L2") else 0
        self.capture = cv2.VideoCapture(index, backend)
        self.capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self.capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self.capture.set(cv2.CAP_PROP_FPS, fps)
        # 减少缓存，避免云台收到几帧以前的视觉误差。
        self.capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        if not self.capture.isOpened():
            self.capture.release()
            raise RuntimeError("OpenCV cannot open camera index %d" % index)

    def read(self):
        return self.capture.read()

    def close(self):
        self.capture.release()


class Picamera2Camera(object):
    """树莓派 CSI 摄像头，使用 Raspberry Pi OS 的 Picamera2。"""

    name = "Picamera2/libcamera"

    def __init__(self, width, height, fps):
        from picamera2 import Picamera2

        self.camera = Picamera2()
        configuration = self.camera.create_video_configuration(
            main={"size": (width, height), "format": "BGR888"},
            controls={"FrameRate": float(fps)},
            buffer_count=3,
        )
        self.camera.configure(configuration)
        self.camera.start()

    def read(self):
        frame = self.camera.capture_array("main")
        return frame is not None, frame

    def close(self):
        self.camera.stop()
        self.camera.close()


def open_camera(backend, camera_index, width, height, fps):
    """自动优先尝试 CSI 相机，失败后回退到 USB/V4L2。"""
    errors = []
    if backend in ("auto", "picamera2"):
        try:
            return Picamera2Camera(width, height, fps)
        except Exception as error:
            errors.append("Picamera2: %s" % error)
            if backend == "picamera2":
                raise RuntimeError(errors[-1])

    if backend in ("auto", "opencv"):
        try:
            return OpenCVCamera(camera_index, width, height, fps)
        except Exception as error:
            errors.append("OpenCV: %s" % error)

    raise RuntimeError("; ".join(errors) or "No camera backend selected")


def draw_status(frame, tracking, reference, live_fps, vision_fps, process_ms):
    """在 VNC 预览上绘制状态；不参与黑框检测。"""
    height = frame.shape[0]
    reference_point = (int(reference[0]), int(reference[1]))
    cv2.drawMarker(
        frame, reference_point, (255, 0, 0),
        cv2.MARKER_TILTED_CROSS, 22, 2,
    )

    status_color = (0, 255, 0) if tracking.locked else (0, 255, 255)
    cv2.putText(
        frame,
        "%s found=%d locked=%d" % (
            tracking.state, int(tracking.found), int(tracking.locked)
        ),
        (16, height - 70), cv2.FONT_HERSHEY_SIMPLEX, 0.62, status_color, 2,
    )
    cv2.putText(
        frame,
        "LIVE %.1f FPS | VISION %.1f FPS | %.1f ms" % (
            live_fps, vision_fps, process_ms
        ),
        (16, height - 42), cv2.FONT_HERSHEY_SIMPLEX, 0.58, (0, 255, 255), 2,
    )
    error_text = "error=None"
    if tracking.error is not None:
        error_text = "error=(%+.1f, %+.1f)" % tracking.error
    cv2.putText(
        frame, error_text, (16, height - 14),
        cv2.FONT_HERSHEY_SIMPLEX, 0.58, (0, 255, 255), 2,
    )


def run(args):
    camera = open_camera(
        args.backend, args.camera, args.width, args.height, args.fps
    )
    detector = BlackFrameDetector(black_threshold=args.black_threshold)
    tracker = None
    reference = None
    process_times = deque(maxlen=30)
    live_fps = 0.0
    previous_frame_time = None
    video_writer = None
    record_start_time = None
    record_path = None
    recorded_frame_count = 0

    if os.name != "nt" and not (
            os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        camera.close()
        raise RuntimeError(
            "No graphical desktop was found. Run this command in a VNC desktop terminal."
        )

    print("Camera backend: %s" % camera.name)
    print("Requested mode: %dx%d @ %d FPS" % (
        args.width, args.height, args.fps
    ))
    print("Undistortion: disabled (raw camera frames)")
    print("Press q or Esc to exit; press r to reset target tracking.")

    window_name = "2025E black-frame live"
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    display_height = int(args.display_width * args.height / float(args.width))
    cv2.resizeWindow(window_name, args.display_width, display_height)

    try:
        while True:
            ok, frame = camera.read()
            if not ok or frame is None:
                continue
            if frame.ndim == 3 and frame.shape[2] == 4:
                frame = cv2.cvtColor(frame, cv2.COLOR_BGRA2BGR)

            current_frame_time = time.perf_counter()
            if previous_frame_time is not None:
                instant_live_fps = 1.0 / max(
                    current_frame_time - previous_frame_time, 1e-6
                )
                live_fps = (
                    instant_live_fps if live_fps == 0.0
                    else 0.9 * live_fps + 0.1 * instant_live_fps
                )
            previous_frame_time = current_frame_time

            if tracker is None:
                frame_height, frame_width = frame.shape[:2]
                reference = (
                    args.reference_x if args.reference_x is not None
                    else frame_width / 2.0,
                    args.reference_y if args.reference_y is not None
                    else frame_height / 2.0,
                )
                tracker = TargetTracker(
                    reference[0], reference[1],
                    lock_frames=args.lock_frames,
                    lost_frames=args.lost_frames,
                    smooth_alpha=args.smooth_alpha,
                    max_center_jump=args.max_center_jump,
                )
                if args.record:
                    record_path = os.path.abspath(os.path.expanduser(args.record))
                    record_dir = os.path.dirname(record_path)
                    if not os.path.isdir(record_dir):
                        os.makedirs(record_dir)
                    video_writer = cv2.VideoWriter(
                        record_path,
                        cv2.VideoWriter_fourcc(*"MJPG"),
                        float(args.record_fps),
                        (frame_width, frame_height),
                    )
                    if not video_writer.isOpened():
                        raise RuntimeError(
                            "Cannot create recording: %s; use an .avi filename"
                            % record_path
                        )
                    record_start_time = time.perf_counter()
                    print("Recording to: %s (%.1f FPS timeline)" % (
                        record_path, args.record_fps
                    ))

            # 正式测试链路：原始相机画面 -> 黑框检测 -> 目标跟踪。
            process_start = time.perf_counter()
            detection = detector.detect(frame)
            tracking = tracker.update(detection)
            process_ms = (time.perf_counter() - process_start) * 1000.0
            process_times.append(process_ms)
            average_process_ms = sum(process_times) / len(process_times)
            vision_fps = (
                1000.0 / average_process_ms if average_process_ms > 0 else 0.0
            )

            preview = detector.draw(frame, detection)
            draw_status(
                preview, tracking, reference, live_fps, vision_fps, process_ms
            )
            recording_finished = False
            if video_writer is not None:
                record_elapsed = time.perf_counter() - record_start_time
                cv2.circle(preview, (22, 24), 8, (0, 0, 255), -1)
                cv2.putText(
                    preview, "REC %.1fs @ %.0fFPS" % (
                        record_elapsed, args.record_fps
                    ), (38, 31),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.65, (0, 0, 255), 2,
                )
                # 录像时间轴独立于视觉循环。若树莓派实时循环低于录制帧率，
                # 重复最近预览帧来保持真实秒数，避免 AVI 被播放器“快进”。
                target_frame_count = int(record_elapsed * args.record_fps)
                while recorded_frame_count < target_frame_count:
                    video_writer.write(preview)
                    recorded_frame_count += 1
                recording_finished = (
                    args.record_seconds > 0
                    and record_elapsed >= args.record_seconds
                )
            cv2.imshow(window_name, preview)
            if args.show_mask and detector.last_binary is not None:
                cv2.imshow("2025E binary mask", detector.last_binary)

            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break
            if key == ord("r"):
                tracker.reset()
            if recording_finished:
                print("Recording time reached; stopping.")
                break
    finally:
        if video_writer is not None:
            video_writer.release()
            print("Recording saved to: %s" % record_path)
        camera.close()
        cv2.destroyAllWindows()


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run 2025E black-frame detection on Raspberry Pi"
    )
    parser.add_argument(
        "--backend", choices=("auto", "picamera2", "opencv"), default="auto",
        help="auto tries CSI Picamera2 first, then USB OpenCV/V4L2",
    )
    parser.add_argument("--camera", type=int, default=0, help="USB camera index")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--display-width", type=int, default=960)
    parser.add_argument("--reference-x", type=float)
    parser.add_argument("--reference-y", type=float)
    parser.add_argument("--black-threshold", type=int, default=90)
    parser.add_argument("--lock-frames", type=int, default=3)
    parser.add_argument("--lost-frames", type=int, default=3)
    parser.add_argument("--smooth-alpha", type=float, default=0.35)
    parser.add_argument("--max-center-jump", type=float, default=90.0)
    parser.add_argument(
        "--record", help="save the annotated preview; use an .avi filename",
    )
    parser.add_argument(
        "--record-seconds", type=float, default=0.0,
        help="automatically stop after this many seconds; 0 records until q",
    )
    parser.add_argument(
        "--record-fps", type=float, default=10.0,
        help="output video FPS; independent of live loop to preserve real time",
    )
    parser.add_argument(
        "--show-mask", action="store_true",
        help="also show the threshold mask; disabled by default to reduce VNC load",
    )
    return parser.parse_args()


def main():
    try:
        args = parse_args()
        if args.record_fps <= 0:
            raise ValueError("--record-fps must be positive")
        run(args)
    except KeyboardInterrupt:
        return 0
    except Exception as error:
        print("ERROR: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
