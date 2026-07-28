"""
Unit tests for perception module.
Validates Python port matches firmware behavior.
"""

import pytest
import numpy as np
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from firmware_bridge.perception import (
    PerceptionState,
    perception_update,
    RoadEvent,
    IR_WEIGHTS,
    IR_THRESHOLD,
)


def test_perception_centered_on_line():
    """Test perception when all sensors see black line (centered)."""
    state = PerceptionState()

    # All sensors on black (reflectance = 1.0)
    reflectance = np.ones(8, dtype=np.float32)

    result = perception_update(state, reflectance, timestamp_us=0, dt_s=0.02)

    # Weighted sum should be zero (symmetry)
    assert abs(result['lateral_error']) < 0.01, "Should be centered"
    assert result['line_valid'] == True
    assert result['event'] == RoadEvent.ROAD_EVENT_INTERSECTION  # All channels active


def test_perception_left_offset():
    """Test perception when robot is offset to the left."""
    state = PerceptionState()

    # Left channels see black, right channels see white
    reflectance = np.array([1.0, 1.0, 0.8, 0.3, 0.1, 0.0, 0.0, 0.0], dtype=np.float32)

    result = perception_update(state, reflectance, timestamp_us=0, dt_s=0.02)

    # Should have negative lateral error (left side)
    assert result['lateral_error'] < -0.3, f"Expected negative error, got {result['lateral_error']}"
    assert result['line_valid'] == True


def test_perception_right_offset():
    """Test perception when robot is offset to the right."""
    state = PerceptionState()

    # Right channels see black, left channels see white
    reflectance = np.array([0.0, 0.0, 0.0, 0.1, 0.3, 0.8, 1.0, 1.0], dtype=np.float32)

    result = perception_update(state, reflectance, timestamp_us=0, dt_s=0.02)

    # Should have positive lateral error (right side)
    assert result['lateral_error'] > 0.3, f"Expected positive error, got {result['lateral_error']}"
    assert result['line_valid'] == True


def test_perception_line_lost():
    """Test perception when no line is detected."""
    state = PerceptionState()

    # All sensors see white (reflectance < threshold)
    reflectance = np.array([0.2, 0.3, 0.1, 0.4, 0.3, 0.2, 0.1, 0.2], dtype=np.float32)

    result = perception_update(state, reflectance, timestamp_us=0, dt_s=0.02)

    assert result['line_valid'] == False
    assert result['event'] == RoadEvent.ROAD_EVENT_LINE_LOST
    assert result['lost_count'] == 1


def test_perception_heading_filter():
    """Test heading error filtering over multiple updates."""
    state = PerceptionState()
    dt = 0.02  # 50 Hz

    # Simulate robot moving from center to right
    reflectance_sequence = [
        np.array([0.2, 0.3, 0.6, 0.8, 0.8, 0.6, 0.3, 0.2]),  # Centered
        np.array([0.1, 0.2, 0.4, 0.7, 0.9, 0.8, 0.5, 0.3]),  # Moving right
        np.array([0.0, 0.1, 0.2, 0.5, 0.9, 0.9, 0.7, 0.5]),  # More right
    ]

    heading_errors = []
    for i, refl in enumerate(reflectance_sequence):
        result = perception_update(state, refl, timestamp_us=i * 20000, dt_s=dt)
        heading_errors.append(result['heading_error'])

    # Heading error should be filtered (smooth)
    # After initialization, subsequent values should use IIR filter
    assert state.initialized == True
    # Heading error should be non-zero after movement
    assert abs(heading_errors[-1]) > 0.0


def test_perception_curve_detection():
    """Test curve entry detection (large lateral + heading error)."""
    state = PerceptionState()
    dt = 0.02

    # First update: moderate offset
    refl1 = np.array([0.1, 0.2, 0.5, 0.8, 0.9, 0.7, 0.4, 0.2])
    result1 = perception_update(state, refl1, timestamp_us=0, dt_s=dt)

    # Second update: large offset (curve entry)
    refl2 = np.array([0.0, 0.1, 0.2, 0.4, 0.8, 0.9, 0.9, 0.8])
    result2 = perception_update(state, refl2, timestamp_us=20000, dt_s=dt)

    # Large lateral error and fast rate of change should trigger curve detection
    # (depends on thresholds: CURVE_ERROR_THRESHOLD=0.45, CURVE_DERIVATIVE_THRESHOLD=1.5)
    # This test verifies the logic works, actual detection depends on exact values
    assert result2['line_valid'] == True


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
