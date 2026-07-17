"""Synthetic-image tests for black-frame detection."""

import csv
import os
import sys
import time
import unittest
from collections import deque
from typing import Optional, Tuple

import cv2
import numpy as np


TEST_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(TEST_DIR, "..", ".."))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from src.vision.black_frame_detector import BlackFrameDetector  # noqa: E402
from src.vision.target_tracker import TargetTracker  # noqa: E402


# 离线测试图片路径，只在本测试文件中使用。
# 留空时跳过真实图片测试，不影响合成图测试和 ARM 部署代码。
TEST_IMAGE_PATH = os.path.join(
    PROJECT_DIR, "docs", "pictures", "ChatGPT Image 2026年7月16日 19_16_17.png"
)
TEST_OUTPUT_PATH = os.path.abspath(
    os.path.join(PROJECT_DIR, "outputs", "black_frame_preview.png")
)

# 动态视频测试：将视频绝对路径填在这里。留空时会自动跳过该测试。
# 只属于电脑端测试，不会被 ARM 上的实际视觉程序读取。
TEST_VIDEO_PATH = os.path.join(
    PROJECT_DIR, "docs", "videos", "2025电赛e题__发挥部分两圈勉强完赛.mp4"
)
VIDEO_OUTPUT_PATH = os.path.abspath(
    os.path.join(PROJECT_DIR, "outputs", "black_frame_video_preview.avi")
)
VIDEO_CSV_PATH = os.path.abspath(
    os.path.join(PROJECT_DIR, "outputs", "black_frame_video_log.csv")
)
# 0 表示处理完整视频；调试时可改为 300 等较小值。
MAX_VIDEO_FRAMES = 0
# None 表示以视频画面中心为参考点；实际云台标定值可填为 (x, y)。
VIDEO_REFERENCE_POINT: Optional[Tuple[float, float]] = None


def make_target(outer, inner):
    image = np.full((480, 640, 3), 235, dtype=np.uint8)
    cv2.fillConvexPoly(image, np.asarray(outer, dtype=np.int32), (8, 8, 8))
    cv2.fillConvexPoly(image, np.asarray(inner, dtype=np.int32), (245, 245, 245))
    center = tuple(np.mean(np.asarray(inner), axis=0).astype(np.int32))
    cv2.circle(image, center, 35, (0, 0, 210), 3)
    cv2.circle(image, center, 5, (0, 0, 210), -1)
    return image


class BlackFrameDetectorTest(unittest.TestCase):
    def setUp(self):
        self.detector = BlackFrameDetector()

    def test_detects_front_facing_frame(self):
        image = make_target(
            [(170, 60), (470, 60), (470, 420), (170, 420)],
            [(192, 82), (448, 82), (448, 398), (192, 398)],
        )
        detection = self.detector.detect(image)
        assert detection is not None
        self.assertLess(np.linalg.norm(np.asarray(detection.center) - [320, 240]), 5)
        self.assertGreater(detection.confidence, 0.6)

    def test_detects_perspective_frame(self):
        image = make_target(
            [(205, 70), (455, 105), (500, 410), (145, 390)],
            [(222, 94), (437, 124), (474, 384), (170, 367)],
        )
        detection = self.detector.detect(image)
        assert detection is not None
        # For a perspective quadrilateral, the target center is the diagonal
        # intersection, not the arithmetic mean of four corners.
        expected_center = np.array([332.82, 217.32])
        self.assertLess(
            np.linalg.norm(np.asarray(detection.center) - expected_center), 12
        )

    def test_rectifies_detected_target_plane(self):
        image = make_target(
            [(205, 70), (455, 105), (500, 410), (145, 390)],
            [(222, 94), (437, 124), (474, 384), (170, 367)],
        )
        detection = self.detector.detect(image)
        assert detection is not None
        rectified = detection.rectify(image, (320, 240))
        assert rectified is not None

        self.assertEqual(rectified.shape, (240, 320, 3))
        # 拉正后中心仍在亮色靶纸内部，左上角保留黑色边框。
        self.assertGreater(int(rectified[120, 160].mean()), 100)
        self.assertLess(int(rectified[2, 2].mean()), 80)

    def test_returns_none_on_blank_image(self):
        image = np.full((480, 640, 3), 235, dtype=np.uint8)
        self.assertIsNone(self.detector.detect(image))

    def test_rejects_thin_black_rectangle(self):
        image = np.full((480, 640, 3), 235, dtype=np.uint8)
        cv2.rectangle(image, (40, 200), (600, 240), (0, 0, 0), -1)
        self.assertIsNone(self.detector.detect(image))

    def test_configured_real_image(self):
        if not TEST_IMAGE_PATH:
            self.skipTest("TEST_IMAGE_PATH is empty")

        image = cv2.imdecode(
            np.fromfile(TEST_IMAGE_PATH, dtype=np.uint8), cv2.IMREAD_COLOR
        )
        self.assertIsNotNone(image, TEST_IMAGE_PATH)
        assert image is not None
        detection = self.detector.detect(image)
        self.assertIsNotNone(detection, "black frame was not found")
        assert detection is not None
        self.assertGreater(detection.confidence, 0.2)

        output = self.detector.draw(image, detection)
        output_dir = os.path.dirname(TEST_OUTPUT_PATH)
        if not os.path.isdir(output_dir):
            os.makedirs(output_dir)
        ok, encoded = cv2.imencode(".png", output)
        self.assertTrue(ok, "cannot encode preview image")
        encoded.tofile(TEST_OUTPUT_PATH)
        print("\nPreview saved to: %s" % TEST_OUTPUT_PATH)

    def test_configured_dynamic_video(self):
        """逐帧运行当前检测器，输出标注视频和便于分析的 CSV 日志。"""
        if not TEST_VIDEO_PATH:
            self.skipTest("TEST_VIDEO_PATH is empty")

        capture = cv2.VideoCapture(TEST_VIDEO_PATH)
        self.assertTrue(capture.isOpened(), TEST_VIDEO_PATH)
        output_dir = os.path.dirname(VIDEO_OUTPUT_PATH)
        if not os.path.isdir(output_dir):
            os.makedirs(output_dir)

        fps = capture.get(cv2.CAP_PROP_FPS)
        if fps <= 1.0:
            fps = 30.0
        width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.assertGreater(width, 0, "video width is invalid")
        self.assertGreater(height, 0, "video height is invalid")

        if VIDEO_REFERENCE_POINT is None:
            reference_x, reference_y = width / 2.0, height / 2.0
        else:
            reference_x, reference_y = VIDEO_REFERENCE_POINT
        tracker = TargetTracker(reference_x, reference_y)

        # MJPG + AVI 在 Windows OpenCV 环境兼容性较好，便于直接在 VS Code 外打开。
        writer = cv2.VideoWriter(
            VIDEO_OUTPUT_PATH,
            # 采用类静态方法，兼容 Pylance 的 OpenCV 类型标注。
            cv2.VideoWriter.fourcc("M", "J", "P", "G"),
            fps,
            (width, height),
        )
        self.assertTrue(writer.isOpened(), "cannot create output video")

        frame_index = 0
        found_count = 0
        locked_count = 0
        total_process_ms = 0.0
        # 使用最近 30 帧的视觉耗时计算 FPS，避免单帧波动导致数字跳动。
        recent_process_ms = deque(maxlen=30)
        try:
            with open(VIDEO_CSV_PATH, "w", newline="") as csv_file:
                log_writer = csv.writer(csv_file)
                log_writer.writerow([
                    "frame", "found", "state", "locked", "raw_x", "raw_y",
                    "smooth_x", "smooth_y", "error_x", "error_y", "process_ms",
                    "vision_fps",
                ])

                while True:
                    ok, frame = capture.read()
                    if not ok or frame is None:
                        break
                    if MAX_VIDEO_FRAMES and frame_index >= MAX_VIDEO_FRAMES:
                        break

                    start_time = time.perf_counter()
                    detection = self.detector.detect(frame)
                    result = tracker.update(detection)
                    process_ms = (time.perf_counter() - start_time) * 1000.0
                    total_process_ms += process_ms
                    recent_process_ms.append(process_ms)
                    average_recent_ms = sum(recent_process_ms) / len(recent_process_ms)
                    vision_fps = 1000.0 / average_recent_ms if average_recent_ms > 0 else 0.0

                    if result.found:
                        found_count += 1
                    if result.locked:
                        locked_count += 1

                    # 绿色四边形和红色叉号来自 detector.draw；蓝叉为参考瞄准点。
                    output = self.detector.draw(frame, detection)
                    cv2.drawMarker(
                        output,
                        (int(reference_x), int(reference_y)),
                        (255, 0, 0),
                        cv2.MARKER_TILTED_CROSS,
                        20,
                        2,
                    )
                    status_text = "{} found={} locked={} {:.1f}ms {:.1f}FPS".format(
                        result.state, int(result.found), int(result.locked),
                        process_ms, vision_fps
                    )
                    cv2.putText(output, status_text, (15, height - 42),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 2)
                    if result.error is not None:
                        error_text = "error=({:+.1f}, {:+.1f})".format(
                            result.error[0], result.error[1]
                        )
                        cv2.putText(output, error_text, (15, height - 16),
                                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 2)
                    writer.write(output)

                    raw_x = result.raw_center[0] if result.raw_center else ""
                    raw_y = result.raw_center[1] if result.raw_center else ""
                    smooth_x = result.center[0] if result.center else ""
                    smooth_y = result.center[1] if result.center else ""
                    error_x = result.error[0] if result.error else ""
                    error_y = result.error[1] if result.error else ""
                    log_writer.writerow([
                        frame_index, int(result.found), result.state,
                        int(result.locked), raw_x, raw_y, smooth_x, smooth_y,
                        error_x, error_y, "{:.3f}".format(process_ms),
                        "{:.2f}".format(vision_fps),
                    ])
                    frame_index += 1
        finally:
            capture.release()
            writer.release()

        self.assertGreater(frame_index, 0, "video contains no readable frames")
        print("\nVideo preview saved to: %s" % VIDEO_OUTPUT_PATH)
        print("Video log saved to: %s" % VIDEO_CSV_PATH)
        print(
            "Processed {} frames; found {:.1%}; locked {:.1%}; "
            "average {:.2f} ms/frame ({:.1f} vision FPS)".format(
                frame_index,
                found_count / float(frame_index),
                locked_count / float(frame_index),
                total_process_ms / frame_index,
                1000.0 / (total_process_ms / frame_index),
            )
        )

    def test_rejects_frame_touching_image_edge(self):
        image = np.full((480, 640, 3), 235, dtype=np.uint8)
        outer = np.array([(-35, 70), (390, 70), (390, 410), (-35, 410)])
        inner = np.array([(-12, 93), (367, 93), (367, 387), (-12, 387)])
        cv2.fillConvexPoly(image, outer, (8, 8, 8))
        cv2.fillConvexPoly(image, inner, (245, 245, 245))
        # 靶框左侧确实超出画面，不能根据残缺轮廓猜测中心。
        self.assertIsNone(self.detector.detect(image))

    def test_rejects_solid_black_quadrilateral(self):
        image = np.full((480, 640, 3), 235, dtype=np.uint8)
        car_like_shape = np.array([(200, 100), (460, 130), (430, 380), (180, 350)])
        cv2.fillConvexPoly(image, car_like_shape, (10, 10, 10))
        self.assertIsNone(self.detector.detect(image))


if __name__ == "__main__":
    unittest.main()
