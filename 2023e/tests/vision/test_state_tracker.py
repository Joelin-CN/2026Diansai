from vision.state_tracker import VisionStateTracker
from vision.types import FailureReason, ImagePoint, ImageRoi, LaserColor, LaserDetection, TrackerState


def _measured(color=LaserColor.RED, u=50, v=40):
    return LaserDetection.measured(
        color,
        ImagePoint(u, v),
        image_radius_px=5,
        area_px=80,
        brightness=255,
        confidence=0.9,
    )


def _lost(color=LaserColor.RED):
    return LaserDetection.lost(color, FailureReason.NOT_FOUND)


def test_tracker_starts_uninitialized_and_uses_screen_roi():
    screen_roi = ImageRoi(10, 20, 100, 80)
    tracker = VisionStateTracker(max_missing_frames=5, frame_size=(160, 120), screen_roi=screen_roi)

    assert tracker.state_for(LaserColor.RED) == TrackerState.UNINITIALIZED
    assert tracker.lost_count_for(LaserColor.RED) == 0
    assert tracker.suggest_roi(LaserColor.RED) == screen_roi


def test_tracker_transitions_tracking_temp_lost_reacquiring_lost_and_back():
    tracker = VisionStateTracker(max_missing_frames=3, frame_size=(160, 120), screen_roi=None)

    tracker.update(LaserColor.RED, _measured())
    assert tracker.state_for(LaserColor.RED) == TrackerState.TRACKING
    assert tracker.lost_count_for(LaserColor.RED) == 0

    tracking_roi = tracker.suggest_roi(LaserColor.RED)
    assert tracking_roi is not None
    assert tracking_roi.x_px <= 50 <= tracking_roi.x_px + tracking_roi.width_px
    assert tracking_roi.y_px <= 40 <= tracking_roi.y_px + tracking_roi.height_px

    tracker.update(LaserColor.RED, _lost())
    assert tracker.state_for(LaserColor.RED) == TrackerState.TEMP_LOST
    assert tracker.lost_count_for(LaserColor.RED) == 1

    temp_lost_roi = tracker.suggest_roi(LaserColor.RED)
    assert temp_lost_roi is not None
    assert temp_lost_roi.width_px > tracking_roi.width_px
    assert temp_lost_roi.height_px > tracking_roi.height_px

    tracker.update(LaserColor.RED, _lost())
    assert tracker.state_for(LaserColor.RED) == TrackerState.REACQUIRING
    assert tracker.lost_count_for(LaserColor.RED) == 2

    tracker.update(LaserColor.RED, _lost())
    tracker.update(LaserColor.RED, _lost())
    assert tracker.state_for(LaserColor.RED) == TrackerState.LOST
    assert tracker.lost_count_for(LaserColor.RED) == 4

    lost_roi = tracker.suggest_roi(LaserColor.RED)
    assert lost_roi == temp_lost_roi
    assert lost_roi != ImageRoi(0, 0, 160, 120)

    tracker.update(LaserColor.RED, _measured(u=70, v=60))
    assert tracker.state_for(LaserColor.RED) == TrackerState.TRACKING
    assert tracker.lost_count_for(LaserColor.RED) == 0


def test_tracker_lost_state_returns_screen_roi_when_available():
    screen_roi = ImageRoi(10, 20, 100, 80)
    tracker = VisionStateTracker(max_missing_frames=2, frame_size=(160, 120), screen_roi=screen_roi)

    tracker.update(LaserColor.RED, _measured())
    tracker.update(LaserColor.RED, _lost())
    tracker.update(LaserColor.RED, _lost())
    tracker.update(LaserColor.RED, _lost())

    assert tracker.state_for(LaserColor.RED) == TrackerState.LOST
    assert tracker.suggest_roi(LaserColor.RED) == screen_roi


def test_tracker_keeps_red_and_green_state_independent():
    tracker = VisionStateTracker(max_missing_frames=5, frame_size=(160, 120), screen_roi=None)

    tracker.update(LaserColor.RED, _measured(LaserColor.RED, 40, 50))
    tracker.update(LaserColor.GREEN, _lost(LaserColor.GREEN))

    assert tracker.state_for(LaserColor.RED) == TrackerState.TRACKING
    assert tracker.state_for(LaserColor.GREEN) == TrackerState.UNINITIALIZED
    assert tracker.lost_count_for(LaserColor.RED) == 0
    assert tracker.lost_count_for(LaserColor.GREEN) == 0


def test_tracker_does_not_fabricate_detection_when_lost():
    tracker = VisionStateTracker(max_missing_frames=5, frame_size=(160, 120), screen_roi=None)
    missing = _lost()

    tracker.update(LaserColor.RED, missing)

    assert missing.found is False
    assert missing.image_center is None
    assert tracker.state_for(LaserColor.RED) == TrackerState.UNINITIALIZED
