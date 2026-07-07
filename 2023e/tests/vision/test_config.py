from pathlib import Path

from vision.config import load_vision_config
from vision.types import DetectionSource, FailureReason, LaserColor, LaserDetection


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def test_loads_red_vision_config_with_two_red_hue_ranges():
    config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")

    assert config.red_laser is not None
    assert config.red_laser.enabled is True
    assert [(r.min, r.max) for r in config.red_laser.hue_ranges_deg] == [(0, 10), (170, 180)]
    assert config.red_laser.saturation_min == 80
    assert config.red_laser.value_min == 120
    assert config.red_laser.area_px_min == 3
    assert config.red_laser.area_px_max == 2000
    assert config.red_laser.confidence_min == 0.6
    assert config.green_laser is None
    assert config.lost_target_policy.max_missing_frames == 5


def test_loads_green_vision_config_with_independent_red_and_green_params():
    config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")

    assert config.red_laser is not None
    assert config.green_laser is not None
    assert [(r.min, r.max) for r in config.red_laser.hue_ranges_deg] == [(0, 10), (170, 180)]
    assert [(r.min, r.max) for r in config.green_laser.hue_ranges_deg] == [(45, 85)]
    assert config.green_laser.enabled is True
    assert config.lost_target_policy.max_missing_frames == 5


def test_red_and_green_config_instances_do_not_share_mutable_params():
    red_config = load_vision_config(PROJECT_ROOT / "configs" / "red" / "vision.json")
    green_config = load_vision_config(PROJECT_ROOT / "configs" / "green" / "vision.json")

    assert red_config.red_laser is not green_config.red_laser
    assert red_config.red_laser.hue_ranges_deg is not green_config.red_laser.hue_ranges_deg
    assert red_config.lost_target_policy is not green_config.lost_target_policy


def test_lost_detection_has_no_measured_point():
    detection = LaserDetection.lost(LaserColor.RED, FailureReason.NOT_FOUND)

    assert detection.found is False
    assert detection.source == DetectionSource.LOST
    assert detection.image_center is None
    assert detection.failure_reason == FailureReason.NOT_FOUND
