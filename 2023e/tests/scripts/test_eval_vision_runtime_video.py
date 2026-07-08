import sys
from pathlib import Path

import cv2
import numpy as np


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = PROJECT_ROOT / "scripts" / "test"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from eval_vision_runtime_video import process_video  # noqa: E402
from vision.types import VisionMode  # noqa: E402


def test_process_video_writes_runtime_eval_artifacts(tmp_path):
    video = tmp_path / "synthetic.mp4"
    output_dir = tmp_path / "eval_output"
    _write_synthetic_video(video)

    exit_code = process_video(
        video=video,
        profile="green",
        mode=VisionMode.RUNTIME_TRACK_RED_GREEN,
        start_frame=0,
        sample_count=2,
        seed=7,
        output_dir=output_dir,
    )

    assert exit_code == 0
    assert (output_dir / "frame_0001_orig.png").exists()
    assert (output_dir / "frame_0001_annot.png").exists()
    assert (output_dir / "frame_0002_orig.png").exists()
    assert (output_dir / "frame_0002_annot.png").exists()
    assert (output_dir / "summary.csv").exists()
    assert (output_dir / "summary.txt").exists()


def _write_synthetic_video(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(
        str(path),
        cv2.VideoWriter_fourcc(*"mp4v"),
        5.0,
        (120, 100),
    )
    if not writer.isOpened():
        raise RuntimeError(f"Failed to create synthetic video: {path}")
    try:
        frame1 = np.full((100, 120, 3), 30, dtype=np.uint8)
        cv2.circle(frame1, (30, 40), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
        cv2.circle(frame1, (85, 70), 5, (0, 255, 0), -1, lineType=cv2.LINE_AA)
        frame2 = np.full((100, 120, 3), 30, dtype=np.uint8)
        cv2.circle(frame2, (35, 42), 5, (0, 0, 255), -1, lineType=cv2.LINE_AA)
        cv2.circle(frame2, (82, 68), 5, (0, 255, 0), -1, lineType=cv2.LINE_AA)
        writer.write(frame1)
        writer.write(frame2)
    finally:
        writer.release()
