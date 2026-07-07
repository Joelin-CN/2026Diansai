from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np


@dataclass(frozen=True)
class QuadPreview:
    found: bool
    quad: np.ndarray | None
    area: float
    score: float
    failure_reason: str | None


@dataclass(frozen=True)
class A4TapeBorderPreview:
    found: bool
    outer_quad: np.ndarray | None
    inner_quad: np.ndarray | None
    outer_area: float
    inner_area: float
    tape_ratio: float
    score: float
    failure_reason: str | None


def detect_a4_tape_border(frame: np.ndarray) -> A4TapeBorderPreview:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    mask = cv2.adaptiveThreshold(
        blurred,
        255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY_INV,
        41,
        8,
    )
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (7, 7))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)
    contours, hierarchy = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
    if hierarchy is None or not contours:
        return _a4_lost("outer_contour_not_found")

    frame_area = frame.shape[0] * frame.shape[1]
    candidates: list[tuple[float, np.ndarray, np.ndarray, float, float, float]] = []
    hierarchy_rows = hierarchy[0]
    for outer_index, outer in enumerate(contours):
        outer_area = float(cv2.contourArea(outer))
        if outer_area < frame_area * 0.005 or outer_area > frame_area * 0.45:
            continue
        outer_quad = _quad_from_contour(outer)
        if outer_quad is None or _touches_frame_edge(outer_quad, frame.shape, margin=4):
            continue

        child_index = hierarchy_rows[outer_index][2]
        while child_index != -1:
            inner = contours[child_index]
            inner_area = float(cv2.contourArea(inner))
            child_index = hierarchy_rows[child_index][0]
            if inner_area < outer_area * 0.25 or inner_area > outer_area * 0.9:
                continue
            inner_quad = _quad_from_contour(inner)
            if inner_quad is None:
                continue
            if not _quad_inside_quad(inner_quad, outer_quad):
                continue
            tape_ratio = (outer_area - inner_area) / outer_area
            if tape_ratio < 0.08 or tape_ratio > 0.65:
                continue
            score = _a4_score(outer_quad, inner_quad, outer_area, inner_area, frame.shape)
            candidates.append((score, outer_quad, inner_quad, outer_area, inner_area, tape_ratio))

    if not candidates:
        return _a4_lost("inner_contour_not_found")

    score, outer_quad, inner_quad, outer_area, inner_area, tape_ratio = max(candidates, key=lambda item: item[0])
    return A4TapeBorderPreview(True, outer_quad, inner_quad, outer_area, inner_area, tape_ratio, score, None)


def detect_screen_pencil_square(frame: np.ndarray, exclude_quad: np.ndarray | None = None) -> QuadPreview:
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    enhanced = clahe.apply(gray)
    edges = cv2.Canny(cv2.GaussianBlur(enhanced, (5, 5), 0), 40, 120)
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (3, 3))
    edges = cv2.morphologyEx(edges, cv2.MORPH_CLOSE, kernel, iterations=1)
    contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return QuadPreview(False, None, 0.0, 0.0, "not_found")

    frame_area = frame.shape[0] * frame.shape[1]
    candidates: list[tuple[float, np.ndarray, float]] = []
    for contour in contours:
        area = float(cv2.contourArea(contour))
        if area < frame_area * 0.03 or area > frame_area * 0.6:
            continue
        quad = _quad_from_contour(contour)
        if quad is None or _touches_frame_edge(quad, frame.shape, margin=8):
            continue
        if exclude_quad is not None and _quad_overlap_ratio(quad, exclude_quad) > 0.85:
            continue
        score = _screen_score(quad, area, frame.shape)
        candidates.append((score, quad, area))

    if not candidates:
        return QuadPreview(False, None, 0.0, 0.0, "not_found")

    score, quad, area = max(candidates, key=lambda item: item[0])
    return QuadPreview(True, quad, area, score, None)


def draw_quad(image: np.ndarray, quad: np.ndarray, color: tuple[int, int, int], label: str, thickness: int) -> None:
    cv2.polylines(image, [quad], isClosed=True, color=color, thickness=thickness)
    cv2.putText(image, label, tuple(quad[0][0]), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)


def _a4_lost(reason: str) -> A4TapeBorderPreview:
    return A4TapeBorderPreview(False, None, None, 0.0, 0.0, 0.0, 0.0, reason)


def _quad_from_contour(contour: np.ndarray) -> np.ndarray | None:
    perimeter = cv2.arcLength(contour, True)
    for epsilon_ratio in (0.02, 0.03, 0.04, 0.06):
        approx = cv2.approxPolyDP(contour, epsilon_ratio * perimeter, True)
        if len(approx) == 4 and cv2.isContourConvex(approx):
            ordered = _order_quad(approx.astype(np.int32))
            refined = _refine_quad_by_contour_lines(contour, ordered)
            return refined if refined is not None else ordered
    rect = cv2.minAreaRect(contour)
    box = cv2.boxPoints(rect).astype(np.int32).reshape(-1, 1, 2)
    if cv2.contourArea(box) <= 0:
        return None
    ordered = _order_quad(box)
    refined = _refine_quad_by_contour_lines(contour, ordered)
    return refined if refined is not None else ordered


def _refine_quad_by_contour_lines(contour: np.ndarray, initial_quad: np.ndarray) -> np.ndarray | None:
    contour_points = contour.reshape(-1, 2).astype(np.float32)
    quad_points = initial_quad.reshape(4, 2).astype(np.float32)
    perimeter = cv2.arcLength(contour, True)
    max_distance = max(4.0, perimeter * 0.012)
    lines = []
    for index in range(4):
        start = quad_points[index]
        end = quad_points[(index + 1) % 4]
        side_points = _points_near_segment(contour_points, start, end, max_distance)
        if len(side_points) < 6:
            return None
        vx, vy, x0, y0 = cv2.fitLine(side_points.astype(np.float32), cv2.DIST_L2, 0, 0.01, 0.01).flatten()
        lines.append(_line_coefficients(float(vx), float(vy), float(x0), float(y0)))

    refined_points = []
    for index in range(4):
        point = _intersect_lines(lines[index - 1], lines[index])
        if point is None:
            return None
        refined_points.append(point)

    refined = np.array(refined_points, dtype=np.float32)
    if not np.all(np.isfinite(refined)):
        return None
    refined_quad = np.round(refined).astype(np.int32).reshape(-1, 1, 2)
    refined_quad = _order_quad(refined_quad)
    if cv2.contourArea(refined_quad) <= 0 or not cv2.isContourConvex(refined_quad):
        return None
    if _quad_corner_shift_too_large(initial_quad, refined_quad, max_shift=max(12.0, perimeter * 0.04)):
        return None
    return refined_quad


def _points_near_segment(points: np.ndarray, start: np.ndarray, end: np.ndarray, max_distance: float) -> np.ndarray:
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


def _line_coefficients(vx: float, vy: float, x0: float, y0: float) -> tuple[float, float, float]:
    # Direction form from cv2.fitLine -> normalized implicit line ax + by + c = 0.
    a = -vy
    b = vx
    c = -(a * x0 + b * y0)
    norm = (a * a + b * b) ** 0.5
    if norm == 0:
        return 0.0, 0.0, 0.0
    return a / norm, b / norm, c / norm


def _intersect_lines(line_a: tuple[float, float, float], line_b: tuple[float, float, float]) -> np.ndarray | None:
    a1, b1, c1 = line_a
    a2, b2, c2 = line_b
    determinant = a1 * b2 - a2 * b1
    if abs(determinant) < 1e-6:
        return None
    x = (b1 * c2 - b2 * c1) / determinant
    y = (c1 * a2 - c2 * a1) / determinant
    return np.array([x, y], dtype=np.float32)


def _quad_corner_shift_too_large(initial_quad: np.ndarray, refined_quad: np.ndarray, max_shift: float) -> bool:
    initial = _order_quad(initial_quad).reshape(4, 2).astype(np.float32)
    refined = _order_quad(refined_quad).reshape(4, 2).astype(np.float32)
    shifts = np.linalg.norm(initial - refined, axis=1)
    return bool(np.any(shifts > max_shift))


def _order_quad(quad: np.ndarray) -> np.ndarray:
    pts = quad.reshape(4, 2).astype(np.float32)
    center = pts.mean(axis=0)
    angles = np.arctan2(pts[:, 1] - center[1], pts[:, 0] - center[0])
    ordered = pts[np.argsort(angles)].astype(np.int32)
    return ordered.reshape(-1, 1, 2)


def _touches_frame_edge(quad: np.ndarray, shape: tuple[int, ...], margin: int) -> bool:
    height, width = shape[:2]
    pts = quad.reshape(-1, 2)
    return bool(
        np.any(pts[:, 0] <= margin)
        or np.any(pts[:, 1] <= margin)
        or np.any(pts[:, 0] >= width - margin)
        or np.any(pts[:, 1] >= height - margin)
    )


def _quad_inside_quad(inner: np.ndarray, outer: np.ndarray) -> bool:
    outer_pts = outer.reshape(-1, 2).astype(np.float32)
    for point in inner.reshape(-1, 2):
        if cv2.pointPolygonTest(outer_pts, (float(point[0]), float(point[1])), False) < 0:
            return False
    return True


def _quad_overlap_ratio(a: np.ndarray, b: np.ndarray) -> float:
    rect_a = cv2.boundingRect(a)
    rect_b = cv2.boundingRect(b)
    ax, ay, aw, ah = rect_a
    bx, by, bw, bh = rect_b
    ix1 = max(ax, bx)
    iy1 = max(ay, by)
    ix2 = min(ax + aw, bx + bw)
    iy2 = min(ay + ah, by + bh)
    if ix2 <= ix1 or iy2 <= iy1:
        return 0.0
    intersection = (ix2 - ix1) * (iy2 - iy1)
    return intersection / float(min(aw * ah, bw * bh))


def _a4_score(
    outer_quad: np.ndarray,
    inner_quad: np.ndarray,
    outer_area: float,
    inner_area: float,
    shape: tuple[int, ...],
) -> float:
    tape_ratio = (outer_area - inner_area) / outer_area
    ratio_score = 1.0 - min(1.0, abs(tape_ratio - 0.3) / 0.3)
    center_score = _center_score(outer_quad, shape)
    nesting_score = min(1.0, inner_area / outer_area / 0.75)
    return 0.45 * ratio_score + 0.30 * center_score + 0.25 * nesting_score


def _screen_score(quad: np.ndarray, area: float, shape: tuple[int, ...]) -> float:
    frame_area = shape[0] * shape[1]
    area_score = 1.0 - min(1.0, abs((area / frame_area) - 0.25) / 0.25)
    center_score = _center_score(quad, shape)
    square_score = _square_like_score(quad)
    return 0.35 * area_score + 0.35 * center_score + 0.30 * square_score


def _center_score(quad: np.ndarray, shape: tuple[int, ...]) -> float:
    height, width = shape[:2]
    center = quad.reshape(-1, 2).mean(axis=0)
    distance = np.linalg.norm(center - np.array([width / 2.0, height / 2.0]))
    max_distance = np.linalg.norm(np.array([width / 2.0, height / 2.0]))
    return float(max(0.0, 1.0 - distance / max_distance))


def _square_like_score(quad: np.ndarray) -> float:
    pts = quad.reshape(4, 2).astype(np.float32)
    lengths = [float(np.linalg.norm(pts[(i + 1) % 4] - pts[i])) for i in range(4)]
    longest = max(lengths)
    shortest = min(lengths)
    if longest == 0:
        return 0.0
    return shortest / longest
