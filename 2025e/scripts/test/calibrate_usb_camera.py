"""USB 摄像头内参标定工具（在树莓派 VNC 桌面中运行）。

默认使用 calibration/chessboard_10x7.svg：10 列 x 7 行方格，
即 OpenCV 的 9 x 6 个内角点。屏幕标定只求内参，方格单位设为 1。
按 c 保存当前有效棋盘格，按 s 使用已采集图片计算内参，按 q 退出。
"""

from __future__ import print_function

import argparse
import glob
import os
import sys
import time

import cv2
import numpy as np


def parse_board_size(value):
    """把命令行的 9x6 转为 OpenCV 所需的 (9, 6)。"""
    try:
        columns, rows = value.lower().split("x")
        board_size = (int(columns), int(rows))
    except (AttributeError, ValueError):
        raise argparse.ArgumentTypeError("board size must look like 9x6")
    if board_size[0] < 2 or board_size[1] < 2:
        raise argparse.ArgumentTypeError("board size must be at least 2x2")
    return board_size


def find_corners(frame, board_size, detection_scale, robust_sb):
    """定位并亚像素化棋盘格内角点。

    先在缩小图上运行普通检测以保证 VNC 流畅；只在用户明确要求时
    才启用耗时较高的 SB 检测器。
    """
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    if detection_scale < 1.0:
        search_gray = cv2.resize(
            gray, None, fx=detection_scale, fy=detection_scale,
            interpolation=cv2.INTER_AREA,
        )
    else:
        search_gray = gray

    found, corners = cv2.findChessboardCorners(
        search_gray,
        board_size,
        cv2.CALIB_CB_ADAPTIVE_THRESH | cv2.CALIB_CB_NORMALIZE_IMAGE,
    )
    if found and detection_scale < 1.0:
        corners /= detection_scale

    # SB 更稳但在树莓派上明显更慢，作为人工启用的回退方案。
    if not found and robust_sb and hasattr(cv2, "findChessboardCornersSB"):
        sb_flags = cv2.CALIB_CB_NORMALIZE_IMAGE
        sb_flags |= getattr(cv2, "CALIB_CB_EXHAUSTIVE", 0)
        sb_flags |= getattr(cv2, "CALIB_CB_ACCURACY", 0)
        found, corners = cv2.findChessboardCornersSB(
            gray, board_size, sb_flags
        )
    if not found:
        return False, None

    criteria = (
        cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_MAX_ITER,
        30,
        0.001,
    )
    corners = cv2.cornerSubPix(
        gray, corners, (11, 11), (-1, -1), criteria
    )
    return True, corners


def make_object_points(board_size, square_mm):
    """生成棋盘格平面上的三维点，单位为毫米。"""
    points = np.zeros((board_size[0] * board_size[1], 3), np.float32)
    points[:, :2] = np.mgrid[0:board_size[0], 0:board_size[1]].T.reshape(-1, 2)
    points *= square_mm
    return points


def view_descriptor(corners, frame_shape, board_size):
    """用四个外侧内角点描述视角，避免自动模式反复保存同一姿态。"""
    points = corners.reshape(-1, 2)
    width = float(frame_shape[1])
    height = float(frame_shape[0])
    indices = (
        0,
        board_size[0] - 1,
        board_size[0] * board_size[1] - 1,
        board_size[0] * (board_size[1] - 1),
    )
    descriptor = points[list(indices)].astype(np.float32)
    descriptor[:, 0] /= width
    descriptor[:, 1] /= height
    return descriptor.reshape(-1)


def is_new_view(descriptor, saved_descriptors, minimum_change):
    """四个角的平均归一化位移达到阈值才视作新视角。"""
    if not saved_descriptors:
        return True
    for previous in saved_descriptors:
        point_distance = np.linalg.norm(
            descriptor.reshape(4, 2) - previous.reshape(4, 2), axis=1
        )
        if float(np.mean(point_distance)) < minimum_change:
            return False
    return True


def collect_observations(image_paths, board_size, square_mm):
    """从照片中提取所有成功的棋盘格观测。"""
    object_template = make_object_points(board_size, square_mm)
    object_points = []
    image_points = []
    image_size = None
    usable_paths = []

    for path in image_paths:
        image = cv2.imread(path)
        if image is None:
            print("Skipping unreadable image: %s" % path)
            continue
        size = (image.shape[1], image.shape[0])
        if image_size is None:
            image_size = size
        elif size != image_size:
            print("Skipping different resolution: %s" % path)
            continue

        # 离线计算只处理有限张图片，可直接用原分辨率提高精度。
        found, corners = find_corners(image, board_size, 1.0, True)
        if found:
            object_points.append(object_template.copy())
            image_points.append(corners)
            usable_paths.append(path)
        else:
            print("No chessboard found: %s" % path)

    return object_points, image_points, image_size, usable_paths


def calibrate(image_dir, output_path, board_size, square_mm):
    """读取采集照片，计算内参，并保存为 OpenCV/NumPy 可直接读取的 npz。"""
    image_paths = sorted(glob.glob(os.path.join(image_dir, "*.png")))
    if len(image_paths) < 10:
        raise RuntimeError("Need at least 10 captured images, got %d" % len(image_paths))

    object_points, image_points, image_size, usable_paths = collect_observations(
        image_paths, board_size, square_mm
    )
    if len(usable_paths) < 10:
        raise RuntimeError(
            "Only %d usable chessboard images; collect at least 10" % len(usable_paths)
        )

    rms, camera_matrix, distortion, rotations, translations = cv2.calibrateCamera(
        object_points, image_points, image_size, None, None
    )
    squared_error = 0.0
    total_points = 0
    for index, object_point in enumerate(object_points):
        projected, _ = cv2.projectPoints(
            object_point, rotations[index], translations[index], camera_matrix, distortion
        )
        squared_error += cv2.norm(image_points[index], projected, cv2.NORM_L2SQR)
        total_points += len(projected)
    mean_error = (squared_error / total_points) ** 0.5

    output_dir = os.path.dirname(output_path)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    np.savez(
        output_path,
        camera_matrix=camera_matrix,
        distortion_coefficients=distortion,
        image_size=np.array(image_size, dtype=np.int32),
        board_size=np.array(board_size, dtype=np.int32),
        square_mm=np.array([square_mm], dtype=np.float32),
        rms=np.array([rms], dtype=np.float64),
        mean_reprojection_error=np.array([mean_error], dtype=np.float64),
    )

    print("\nCalibration saved to: %s" % output_path)
    print("Usable images: %d / %d" % (len(usable_paths), len(image_paths)))
    print("Image size: %dx%d" % image_size)
    print("RMS reprojection error: %.4f pixels" % rms)
    print("Mean reprojection error: %.4f pixels" % mean_error)
    print("Camera matrix:\n%s" % camera_matrix)
    print("Distortion coefficients:\n%s" % distortion.ravel())
    if mean_error > 0.5:
        print("WARNING: error is high; recapture more varied, sharp chessboard views.")


def run(args):
    if os.name != "nt" and not (
            os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY")):
        raise RuntimeError("Run this command in a VNC desktop terminal.")

    camera = cv2.VideoCapture(args.camera, cv2.CAP_V4L2)
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    camera.set(cv2.CAP_PROP_FPS, args.fps)
    camera.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)
    if not camera.isOpened():
        camera.release()
        raise RuntimeError("Cannot open USB camera index %d" % args.camera)

    image_dir = os.path.abspath(os.path.expanduser(args.image_dir))
    output_path = os.path.abspath(os.path.expanduser(args.output))
    if not os.path.isdir(image_dir):
        os.makedirs(image_dir)

    print("Camera mode requested: %dx%d @ %d FPS" % (
        args.width, args.height, args.fps
    ))
    print("Board: %dx%d inner corners, square: %.2f mm" % (
        args.board_size[0], args.board_size[1], args.square_mm
    ))
    print("Move the board across the whole image, tilt it, then press c.")
    print("Collect 15-25 sharp images; s=solve, q/Esc=quit.")

    window_name = "2025E camera calibration"
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(window_name, args.display_width,
                     int(args.display_width * args.height / float(args.width)))
    captured_count = len(glob.glob(os.path.join(image_dir, "*.png")))
    last_capture_time = 0.0
    saved_descriptors = []

    try:
        while True:
            ok, frame = camera.read()
            if not ok or frame is None:
                continue
            found, corners = find_corners(
                frame, args.board_size, args.detection_scale, args.robust_sb
            )
            preview = frame.copy()
            if found:
                cv2.drawChessboardCorners(preview, args.board_size, corners, found)

            color = (0, 255, 0) if found else (0, 0, 255)
            state = "CHESSBOARD FOUND" if found else "CHESSBOARD NOT FOUND"
            cv2.putText(preview, state, (16, 32), cv2.FONT_HERSHEY_SIMPLEX,
                        0.75, color, 2)
            cv2.putText(preview, "looking for %dx%d INNER corners" % args.board_size,
                        (16, 62), cv2.FONT_HERSHEY_SIMPLEX, 0.60, (0, 255, 255), 2)
            cv2.putText(preview, "saved=%d  c=capture  s=solve  q=quit" % captured_count,
                        (16, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.60, (0, 255, 255), 2)
            if args.auto_capture:
                cv2.putText(
                    preview,
                    "AUTO: move camera slowly; target=%d" % args.target_images,
                    (16, 118), cv2.FONT_HERSHEY_SIMPLEX, 0.60, (255, 255, 0), 2,
                )
            cv2.imshow(window_name, preview)

            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break
            capture_requested = key == ord("c")
            descriptor = None
            if found:
                descriptor = view_descriptor(corners, frame.shape, args.board_size)
            if args.auto_capture and found:
                capture_requested = (
                    time.perf_counter() - last_capture_time >= args.capture_interval
                    and is_new_view(descriptor, saved_descriptors, args.min_view_change)
                )
            if capture_requested:
                now = time.perf_counter()
                if not found:
                    print("Not saved: chessboard not found.")
                elif not args.auto_capture and now - last_capture_time < 0.5:
                    print("Wait a moment before the next capture.")
                else:
                    filename = "chessboard_%03d.png" % (captured_count + 1)
                    path = os.path.join(image_dir, filename)
                    cv2.imwrite(path, frame)
                    captured_count += 1
                    last_capture_time = now
                    if descriptor is not None:
                        saved_descriptors.append(descriptor)
                    print("Saved %s" % path)
                    if args.auto_capture and captured_count >= args.target_images:
                        calibrate(
                            image_dir, output_path, args.board_size, args.square_mm
                        )
                        print("Automatic calibration completed.")
                        break
            if key == ord("s"):
                try:
                    calibrate(image_dir, output_path, args.board_size, args.square_mm)
                except RuntimeError as error:
                    print("CALIBRATION FAILED: %s" % error)
    finally:
        camera.release()
        cv2.destroyAllWindows()


def parse_args():
    parser = argparse.ArgumentParser(description="Calibrate a USB camera with chessboard images")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--display-width", type=int, default=960)
    parser.add_argument("--board-size", type=parse_board_size, default=(9, 6),
                        help="number of INNER corners, for example 9x6")
    parser.add_argument("--square-mm", type=float, default=1.0,
                        help="square size; 1 is sufficient for intrinsics only")
    parser.add_argument(
        "--image-dir",
        default="outputs/screen_9x6_images_1280x720",
        help="capture folder dedicated to the generated 10x7 screen board",
    )
    parser.add_argument(
        "--output",
        default="src/calibration/usb_camera_screen_1280x720.npz",
    )
    parser.add_argument(
        "--auto-capture", action="store_true",
        help="capture sufficiently different views and solve automatically",
    )
    parser.add_argument("--target-images", type=int, default=20)
    parser.add_argument("--capture-interval", type=float, default=0.8)
    parser.add_argument(
        "--min-view-change", type=float, default=0.025,
        help="minimum normalized four-corner movement between saved views",
    )
    parser.add_argument(
        "--detection-scale", type=float, default=0.5,
        help="preview detection scale; 0.5 is fast enough for the screen board",
    )
    parser.add_argument(
        "--robust-sb", action="store_true",
        help="use slow SB fallback only when normal detection cannot find the board",
    )
    return parser.parse_args()


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
