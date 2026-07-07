from __future__ import annotations

import cv2
import numpy as np

from vision.types import ImagePoint, ImageQuad


def contour_to_refined_quad(contour: np.ndarray) -> np.ndarray | None:
    perimeter = cv2.arcLength(contour, True)
    for epsilon_ratio in (0.02, 0.03, 0.04, 0.06):
        approx = cv2.approxPolyDP(contour, epsilon_ratio * perimeter, True)
        if len(approx) == 4 and cv2.isContourConvex(approx):
            ordered = order_quad_points(approx.astype(np.int32))
            refined = refine_quad_by_contour_lines(contour, ordered)
            return refined if refined is not None else ordered
    rect = cv2.minAreaRect(contour)
    box = cv2.boxPoints(rect).astype(np.int32).reshape(-1, 1, 2)
    if cv2.contourArea(box) <= 0:
        return None
    ordered = order_quad_points(box)
    refined = refine_quad_by_contour_lines(contour, ordered)
    return refined if refined is not None else ordered


def quad_to_image_quad(quad: np.ndarray) -> ImageQuad:
    points = tuple(ImagePoint(float(point[0]), float(point[1])) for point in quad.reshape(4, 2))
    return ImageQuad(points)  # type: ignore[arg-type]


def image_quad_to_numpy(image_quad: ImageQuad) -> np.ndarray:
    return np.array([[[int(round(point.u_px)), int(round(point.v_px))]] for point in image_quad.points], dtype=np.int32)


def order_quad_points(quad: np.ndarray) -> np.ndarray:
    pts = quad.reshape(4, 2).astype(np.float32)
    center = pts.mean(axis=0)
    angles = np.arctan2(pts[:, 1] - center[1], pts[:, 0] - center[0])
    ordered = pts[np.argsort(angles)].astype(np.int32)
    return ordered.reshape(-1, 1, 2)


def refine_quad_by_contour_lines(contour: np.ndarray, initial_quad: np.ndarray) -> np.ndarray | None:
    contour_points = contour.reshape(-1, 2).astype(np.float32)
    quad_points = initial_quad.reshape(4, 2).astype(np.float32)
    perimeter = cv2.arcLength(contour, True)
    max_distance = max(4.0, perimeter * 0.012)
    lines = []
    for index in range(4):
        start = quad_points[index]
        end = quad_points[(index + 1) % 4]
        side_points = points_near_segment(contour_points, start, end, max_distance)
        if len(side_points) < 6:
            return None
        vx, vy, x0, y0 = cv2.fitLine(side_points.astype(np.float32), cv2.DIST_L2, 0, 0.01, 0.01).flatten()
        lines.append(line_coefficients(float(vx), float(vy), float(x0), float(y0)))

    refined_points = []
    for index in range(4):
        point = intersect_lines(lines[index - 1], lines[index])
        if point is None:
            return None
        refined_points.append(point)

    refined = np.array(refined_points, dtype=np.float32)
    if not np.all(np.isfinite(refined)):
        return None
    refined_quad = np.round(refined).astype(np.int32).reshape(-1, 1, 2)
    refined_quad = order_quad_points(refined_quad)
    if cv2.contourArea(refined_quad) <= 0 or not cv2.isContourConvex(refined_quad):
        return None
    if quad_corner_shift_too_large(initial_quad, refined_quad, max_shift=max(12.0, perimeter * 0.04)):
        return None
    return refined_quad


def points_near_segment(points: np.ndarray, start: np.ndarray, end: np.ndarray, max_distance: float) -> np.ndarray:
    segment = end - start
    length_sq = float(np.dot(segment, segment))
    if length_sq == 0:
        return np.empty((0, 2), dtype=np.float32)
    relative = points - start
    projection = (relative @ segment) / length_sq
    closest = start + np.clip(projection, 0.0, 1.0)[:, None] * segment
    distances = np.linalg.norm(points - closest, axis=1)
    mask = (projection >= -0.10) & (projection <= 1.10) & (distances <= max_distance)
    return points[mask]


def line_coefficients(vx: float, vy: float, x0: float, y0: float) -> tuple[float, float, float]:
    a = -vy
    b = vx
    c = -(a * x0 + b * y0)
    norm = (a * a + b * b) ** 0.5
    if norm == 0:
        return 0.0, 0.0, 0.0
    return a / norm, b / norm, c / norm


def intersect_lines(line_a: tuple[float, float, float], line_b: tuple[float, float, float]) -> np.ndarray | None:
    a1, b1, c1 = line_a
    a2, b2, c2 = line_b
    determinant = a1 * b2 - a2 * b1
    if abs(determinant) < 1e-6:
        return None
    x = (b1 * c2 - b2 * c1) / determinant
    y = (c1 * a2 - c2 * a1) / determinant
    return np.array([x, y], dtype=np.float32)


def quad_corner_shift_too_large(initial_quad: np.ndarray, refined_quad: np.ndarray, max_shift: float) -> bool:
    initial = order_quad_points(initial_quad).reshape(4, 2).astype(np.float32)
    refined = order_quad_points(refined_quad).reshape(4, 2).astype(np.float32)
    shifts = np.linalg.norm(initial - refined, axis=1)
    return bool(np.any(shifts > max_shift))


def quad_area(quad: np.ndarray) -> float:
    return float(abs(cv2.contourArea(quad)))


def quad_inside_quad(inner: np.ndarray, outer: np.ndarray) -> bool:
    outer_pts = outer.reshape(-1, 2).astype(np.float32)
    for point in inner.reshape(-1, 2):
        if cv2.pointPolygonTest(outer_pts, (float(point[0]), float(point[1])), False) < 0:
            return False
    return True


def quad_touches_frame_edge(quad: np.ndarray, shape: tuple[int, ...], margin: int) -> bool:
    height, width = shape[:2]
    pts = quad.reshape(-1, 2)
    return bool(
        np.any(pts[:, 0] <= margin)
        or np.any(pts[:, 1] <= margin)
        or np.any(pts[:, 0] >= width - margin)
        or np.any(pts[:, 1] >= height - margin)
    )


def quad_overlap_ratio(a: np.ndarray, b: np.ndarray) -> float:
    ax, ay, aw, ah = cv2.boundingRect(a)
    bx, by, bw, bh = cv2.boundingRect(b)
    ix1 = max(ax, bx)
    iy1 = max(ay, by)
    ix2 = min(ax + aw, bx + bw)
    iy2 = min(ay + ah, by + bh)
    if ix2 <= ix1 or iy2 <= iy1:
        return 0.0
    intersection = (ix2 - ix1) * (iy2 - iy1)
    return intersection / float(min(aw * ah, bw * bh))


def point_in_quad(point: ImagePoint, quad: np.ndarray) -> bool:
    return cv2.pointPolygonTest(quad.reshape(-1, 2).astype(np.float32), (float(point.u_px), float(point.v_px)), False) >= 0
