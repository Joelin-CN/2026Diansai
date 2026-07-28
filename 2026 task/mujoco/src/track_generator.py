"""
Track generator for MuJoCo simulation.
Generates square track XML and track segment data for IR sensor simulation.

Note: User coordinates are in mm with Y-forward right-hand system.
      MuJoCo uses meters with Z-up right-hand system.
      This module converts user data to MuJoCo convention.
"""

import numpy as np
from typing import List, Dict, Tuple

# User-provided waypoints in mm (x, y) - Y-forward right-hand system
# These define a rectangular track approximately 260mm x 115mm
USER_WAYPOINTS_MM = [
    (5.6944, 132.1),
    (17.0831, 132.1),
    (28.4719, 132.1),
    (39.8606, 132.1),
    (-5.6944, 132.1),
    (-17.0831, 132.1),
    (-28.4719, 132.1),
    (-39.8606, 132.1),
    (57.5, 44.5),
    (-57.5, 44.5),
    (57.5, -44.5),
    (-57.5, -44.5),
]

# Track parameters
TRACK_WIDTH = 0.025  # 25 mm black tape width
HALF_WIDTH = TRACK_WIDTH / 2.0
SCALE_FACTOR = 5.0   # Scale up the track 5x to make it suitable for the robot

# Convert to MuJoCo convention (Z-up, meters)
# User: (x, y) with Y forward (robot heading)
# MuJoCo ground plane: (X, Y) horizontal, Z up
# Mapping: user_y -> mujoco_x (forward direction), user_x -> mujoco_y (lateral)
# Apply scale factor to make track larger
WAYPOINTS_M = np.array([[y/1000.0 * SCALE_FACTOR, x/1000.0 * SCALE_FACTOR] for x, y in USER_WAYPOINTS_MM])

# Build track segments from waypoints
# The waypoints define a square track - we'll fit line segments and arcs
def generate_track_segments() -> List[Dict]:
    """
    Generate track segment list for IR distance calculation.
    Returns list of dicts with 'type', geometry params.
    """
    segments = []

    # Analyze waypoints to extract track structure
    # The 12 waypoints appear to define corners and edges of a square
    wp = WAYPOINTS_M

    # Find bounding box to determine track dimensions
    x_min, y_min = wp.min(axis=0)
    x_max, y_max = wp.max(axis=0)

    # Square track with rounded corners
    # Inner edge approximately ±0.0575 m in both X and Y
    # Based on waypoints, the track is roughly 115mm x 265mm

    # For simplicity, create a rectangular track with the waypoints as guides
    # Track centerline approximation:
    # Top edge: x ~ 0.132 m
    # Bottom edge: x ~ -0.0445 m
    # Left edge: y ~ -0.0575 m
    # Right edge: y ~ 0.0575 m

    # Simpler approach: Use the design doc's square track and scale to fit waypoints
    # Design doc uses 1.2m inner edge - we'll scale to match our waypoints

    # Actually, let's use the waypoint spread to define the track
    x_center = (x_max + x_min) / 2.0
    y_center = (y_max + y_min) / 2.0
    x_span = x_max - x_min
    y_span = y_max - y_min

    # Create rectangular track with rounded corners
    # Straight segments
    x_top = x_max - 0.01  # Offset for corner radius
    x_bottom = x_min + 0.01
    y_right = y_max - 0.01
    y_left = y_min + 0.01
    corner_radius = 0.01  # 10mm corner radius

    # Top edge (left to right)
    segments.append({
        'type': 'line',
        'p1': np.array([x_top, y_left]),
        'p2': np.array([x_top, y_right])
    })

    # Right edge (top to bottom)
    segments.append({
        'type': 'line',
        'p1': np.array([x_top, y_right]),
        'p2': np.array([x_bottom, y_right])
    })

    # Bottom edge (right to left)
    segments.append({
        'type': 'line',
        'p1': np.array([x_bottom, y_right]),
        'p2': np.array([x_bottom, y_left])
    })

    # Left edge (bottom to top)
    segments.append({
        'type': 'line',
        'p1': np.array([x_bottom, y_left]),
        'p2': np.array([x_top, y_left])
    })

    return segments


TRACK_SEGMENTS = generate_track_segments()


def min_dist_to_track(p: np.ndarray) -> float:
    """
    Calculate minimum distance from point p (2D, meters) to track centerline.

    Args:
        p: Point in MuJoCo ground plane coordinates [x, y]

    Returns:
        Minimum distance in meters (always positive)
    """
    min_d = np.inf

    for seg in TRACK_SEGMENTS:
        if seg['type'] == 'line':
            # Point-to-line-segment distance
            ab = seg['p2'] - seg['p1']
            ap = p - seg['p1']
            ab_len_sq = np.dot(ab, ab)

            if ab_len_sq < 1e-12:
                # Degenerate segment
                d = np.linalg.norm(ap)
            else:
                t = np.clip(np.dot(ap, ab) / ab_len_sq, 0.0, 1.0)
                closest = seg['p1'] + t * ab
                d = np.linalg.norm(p - closest)

            min_d = min(min_d, d)

        elif seg['type'] == 'arc':
            # Point-to-arc distance
            center = seg['center']
            radius = seg['radius']
            a_start = seg['a_start']
            a_end = seg['a_end']

            # Vector from center to point
            dp = p - center
            dist_to_center = np.linalg.norm(dp)

            if dist_to_center < 1e-12:
                # Point at center
                d = radius
            else:
                # Angle of point
                angle = np.arctan2(dp[1], dp[0])

                # Normalize angle to [a_start, a_start + 2π)
                angle_norm = a_start + ((angle - a_start) % (2 * np.pi))

                if angle_norm <= a_end + 1e-9:
                    # Point projects onto arc
                    d = abs(dist_to_center - radius)
                else:
                    # Point is outside arc angular range, use endpoint distance
                    ep0 = center + radius * np.array([np.cos(a_start), np.sin(a_start)])
                    ep1 = center + radius * np.array([np.cos(a_end), np.sin(a_end)])
                    d = min(np.linalg.norm(p - ep0), np.linalg.norm(p - ep1))

            min_d = min(min_d, d)

    return min_d


def generate_track_xml() -> str:
    """
    Generate MuJoCo XML for track geoms based on TRACK_SEGMENTS.
    Returns XML string to be included in track.xml.
    """
    xml_lines = []
    xml_lines.append('  <!-- Ground plane -->')
    xml_lines.append('  <geom name="floor" type="plane" size="0.3 0.3 0.1"')
    xml_lines.append('        rgba="0.95 0.95 0.95 1" friction="1.0 0.005 0.0001"/>')
    xml_lines.append('')
    xml_lines.append('  <!-- Black track segments -->')

    for i, seg in enumerate(TRACK_SEGMENTS):
        if seg['type'] == 'line':
            # Create box geom for line segment
            p1, p2 = seg['p1'], seg['p2']
            midpoint = (p1 + p2) / 2.0
            length = np.linalg.norm(p2 - p1)

            # Angle of line segment
            dx, dy = p2 - p1
            angle_rad = np.arctan2(dy, dx)
            angle_deg = np.degrees(angle_rad)

            xml_lines.append(f'  <geom name="track_seg_{i}" type="box"')
            xml_lines.append(f'        pos="{midpoint[0]:.6f} {midpoint[1]:.6f} 0.0005"')
            xml_lines.append(f'        size="{length/2:.6f} {HALF_WIDTH:.6f} 0.0005"')
            xml_lines.append(f'        euler="0 0 {angle_deg:.6f}"')
            xml_lines.append(f'        rgba="0.05 0.05 0.05 1"/>')

    return '\n'.join(xml_lines)


if __name__ == '__main__':
    # Test track generation
    print("Track segments generated:")
    for i, seg in enumerate(TRACK_SEGMENTS):
        print(f"  {i}: {seg['type']}")
        if seg['type'] == 'line':
            print(f"      p1={seg['p1']}, p2={seg['p2']}")

    print(f"\nTrack XML preview:")
    print(generate_track_xml())

    # Test distance calculation
    test_points = [
        np.array([0.132, 0.0]),  # On top edge
        np.array([0.0, 0.0]),    # Center (should be far)
        np.array([0.132, 0.06]), # Near top-right
    ]

    print(f"\nDistance tests:")
    for p in test_points:
        d = min_dist_to_track(p)
        print(f"  Point {p} -> distance {d*1000:.2f} mm")
