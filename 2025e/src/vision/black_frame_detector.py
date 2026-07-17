"""Fast black-frame detector for the 2025 E target."""

import cv2
import numpy as np


class BlackFrameDetection(object):
    """Detected quadrilateral, ordered TL, TR, BR, BL."""

    def __init__(self, corners, area_ratio):
        self.corners = np.asarray(corners, dtype=np.float32)
        center = self._diagonal_intersection(self.corners)
        self.center = (float(center[0]), float(center[1]))
        self.area_ratio = float(area_ratio)
        self.confidence = min(1.0, self.area_ratio / 0.20)
        self.threshold_mode = "fixed"

    @staticmethod
    def _diagonal_intersection(corners):
        """Return the intersection of TL-BR and TR-BL diagonals."""
        p1, p2, p3, p4 = corners[0], corners[2], corners[1], corners[3]
        direction_a = p2 - p1
        direction_b = p4 - p3
        denominator = (
            direction_a[0] * direction_b[1]
            - direction_a[1] * direction_b[0]
        )
        if abs(denominator) < 1e-6:
            return np.mean(corners, axis=0)
        offset = p3 - p1
        factor = (
            offset[0] * direction_b[1] - offset[1] * direction_b[0]
        ) / denominator
        return p1 + factor * direction_a

    def rectify(self, frame, output_size):
        """将黑框所在平面拉正为俯视矩形图像。

        ``output_size`` 为 ``(width, height)``。此函数只在需要查看或
        处理靶纸内部图案时调用；主瞄准链路只用 ``center``，无需每帧
        做 ``warpPerspective``，以保留树莓派的运算余量。
        """
        if frame is None or frame.size == 0:
            return None

        width, height = int(output_size[0]), int(output_size[1])
        if width <= 0 or height <= 0:
            raise ValueError("output_size must contain positive width and height")

        # corners 已经固定为 TL、TR、BR、BL 的顺序。
        destination = np.array([
            [0, 0],
            [width - 1, 0],
            [width - 1, height - 1],
            [0, height - 1],
        ], dtype=np.float32)
        transform = cv2.getPerspectiveTransform(self.corners, destination)
        return cv2.warpPerspective(frame, transform, (width, height))


class BlackFrameDetector(object):
    """Detect the largest black convex quadrilateral in one BGR frame.

    Camera exposure should be locked. Tune ``black_threshold`` once for the
    actual field instead of running adaptive thresholding every frame.
    """

    def __init__(self, black_threshold=90, min_area_ratio=0.015,
                 max_area_ratio=0.60, polygon_epsilon=0.03,
                 min_side_pixels=24, max_side_ratio=4.0,
                 min_parallel_cosine=0.75, max_right_angle_cosine=0.70,
                 edge_margin_pixels=4, min_inner_brightness=120):
        self.black_threshold = int(black_threshold)
        self.min_area_ratio = float(min_area_ratio)
        self.max_area_ratio = float(max_area_ratio)
        self.polygon_epsilon = float(polygon_epsilon)
        # 以下参数只做少量向量运算，用于排除细长胶带和不规则黑块。
        self.min_side_pixels = float(min_side_pixels)
        self.max_side_ratio = float(max_side_ratio)
        self.min_parallel_cosine = float(min_parallel_cosine)
        self.max_right_angle_cosine = float(max_right_angle_cosine)
        # 靶纸未完全进入画面时不输出假中心；只等待后续完整画面。
        self.edge_margin_pixels = int(edge_margin_pixels)
        # 黑框内部应为亮色靶纸，用于排除小车、纸箱等实心黑色物体。
        self.min_inner_brightness = float(min_inner_brightness)
        self.last_binary = None

    def detect(self, frame):
        if frame is None or frame.size == 0:
            return None

        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        gray = cv2.GaussianBlur(gray, (5, 5), 0)
        _, binary = cv2.threshold(
            gray, self.black_threshold, 255, cv2.THRESH_BINARY_INV
        )
        self.last_binary = binary

        # 仅检查黑色区域的外轮廓。靶纸内侧白色区域形成的是“孔洞”，
        # 不能把它的边缘当作一个独立、完整的黑框。
        result = cv2.findContours(
            binary, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_SIMPLE
        )
        contours = result[1] if len(result) == 3 else result[0]

        image_area = float(frame.shape[0] * frame.shape[1])
        min_area = image_area * self.min_area_ratio
        max_area = image_area * self.max_area_ratio
        best_corners = None
        best_area = 0.0

        for contour in contours:
            area = float(cv2.contourArea(contour))
            if area <= best_area or area < min_area or area > max_area:
                continue

            perimeter = cv2.arcLength(contour, True)
            polygon = cv2.approxPolyDP(
                contour, self.polygon_epsilon * perimeter, True
            )
            if len(polygon) != 4 or not cv2.isContourConvex(polygon):
                continue

            corners = self._order_corners(polygon.reshape(4, 2))
            if not self._is_reasonable_quad(corners):
                continue
            if not self._is_fully_visible(corners, frame.shape):
                continue
            if not self._has_bright_interior(gray, corners):
                continue

            best_area = area
            best_corners = corners

        if best_corners is None:
            return None
        return BlackFrameDetection(best_corners, best_area / image_area)

    def _is_reasonable_quad(self, corners):
        """用低成本几何条件确认候选确实接近靶纸矩形。"""
        sides = []
        directions = []
        for index in range(4):
            vector = corners[(index + 1) % 4] - corners[index]
            length = float(np.linalg.norm(vector))
            if length < self.min_side_pixels:
                return False
            sides.append(length)
            directions.append(vector / length)

        # 排除地面细胶带等细长四边形。
        if max(sides) / min(sides) > self.max_side_ratio:
            return False

        # 两组对边应近似平行；投影透视下允许约 40 度以内的偏差。
        if abs(float(np.dot(directions[0], directions[2]))) < self.min_parallel_cosine:
            return False
        if abs(float(np.dot(directions[1], directions[3]))) < self.min_parallel_cosine:
            return False

        # 邻边应大致垂直；阈值较宽，避免过度排除斜视下的靶纸。
        for index in range(4):
            if abs(float(np.dot(directions[index], directions[(index + 1) % 4]))) > \
                    self.max_right_angle_cosine:
                return False
        return True

    def _is_fully_visible(self, corners, image_shape):
        """拒绝贴到画面边缘的候选，避免半个黑框被误当成完整靶纸。"""
        height, width = image_shape[:2]
        margin = self.edge_margin_pixels
        return bool(
            np.all(corners[:, 0] >= margin)
            and np.all(corners[:, 0] < width - margin)
            and np.all(corners[:, 1] >= margin)
            and np.all(corners[:, 1] < height - margin)
        )

    def _has_bright_interior(self, gray, corners):
        """确认黑框内侧是靶纸，而非实心黑色环境物体。

        只对已通过面积和几何筛选的少数候选建立小掩膜，开销很低。
        """
        center = np.mean(corners, axis=0)
        # 向中心收缩，跳过 1.8 cm 黑色胶带本身，只采样靶纸内部。
        inner = center + 0.65 * (corners - center)
        x, y, width, height = cv2.boundingRect(np.round(inner).astype(np.int32))
        roi = gray[y:y + height, x:x + width]
        if roi.size == 0:
            return False

        mask = np.zeros(roi.shape, dtype=np.uint8)
        local_corners = np.round(inner - np.array([x, y])).astype(np.int32)
        cv2.fillConvexPoly(mask, local_corners, 255)
        return cv2.mean(roi, mask=mask)[0] >= self.min_inner_brightness

    @staticmethod
    def _order_corners(points):
        points = np.asarray(points, dtype=np.float32)
        ordered = np.zeros((4, 2), dtype=np.float32)
        point_sum = points.sum(axis=1)
        point_diff = np.diff(points, axis=1).reshape(-1)
        ordered[0] = points[np.argmin(point_sum)]   # top-left
        ordered[2] = points[np.argmax(point_sum)]   # bottom-right
        ordered[1] = points[np.argmin(point_diff)]  # top-right
        ordered[3] = points[np.argmax(point_diff)]  # bottom-left
        return ordered

    @staticmethod
    def draw(frame, detection):
        output = frame.copy()
        if detection is None:
            cv2.putText(output, "NOT FOUND", (20, 35),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
            return output

        corners = np.round(detection.corners).astype(np.int32)
        center = tuple(np.round(detection.center).astype(np.int32))
        cv2.polylines(output, [corners], True, (0, 255, 0), 3)
        cv2.drawMarker(output, center, (0, 0, 255),
                       cv2.MARKER_CROSS, 24, 2)
        return output
