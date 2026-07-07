from __future__ import annotations

import sys
from pathlib import Path

import cv2


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from border_preview_detectors import draw_quad  # noqa: E402
from vision.quad_utils import image_quad_to_numpy  # noqa: E402
from vision.tape_quad_detector import TapeQuadDetector  # noqa: E402


VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")


def main() -> int:
    cap = cv2.VideoCapture(str(VIDEO))
    ok, frame = cap.read()
    cap.release()
    if not ok:
        print(f"failed_to_read={VIDEO}")
        return 1

    detection = TapeQuadDetector().detect(frame)
    print(f"found={detection.found}")
    print(f"outer={None if detection.outer_quad is None else image_quad_to_numpy(detection.outer_quad).reshape(-1, 2).tolist()}")
    print(f"inner={None if detection.inner_quad is None else image_quad_to_numpy(detection.inner_quad).reshape(-1, 2).tolist()}")
    print(
        f"outer_area={(detection.contour_area_px or 0.0):.1f} inner_area={(detection.inner_area_px or 0.0):.1f} "
        f"ratio={(detection.tape_ratio or 0.0):.3f} score={detection.confidence:.3f} reason={detection.failure_reason}"
    )

    thin = frame.copy()
    thick = frame.copy()
    if detection.found and detection.outer_quad is not None and detection.inner_quad is not None:
        outer_quad = image_quad_to_numpy(detection.outer_quad)
        inner_quad = image_quad_to_numpy(detection.inner_quad)
        draw_quad(thin, outer_quad, (255, 0, 255), "outer_1px", 1)
        draw_quad(thin, inner_quad, (0, 255, 255), "inner_1px", 1)
        draw_quad(thick, outer_quad, (255, 0, 255), "outer_3px", 3)
        draw_quad(thick, inner_quad, (0, 255, 255), "inner_2px", 2)

    output_dir = PROJECT_ROOT / "outputs"
    _write_image(output_dir / "a4_border_debug_thin.jpg", thin)
    _write_image(output_dir / "a4_border_debug_thick.jpg", thick)
    return 0


def _write_image(path: Path, image) -> None:
    ok, encoded = cv2.imencode(path.suffix, image)
    if not ok:
        raise RuntimeError(f"failed_to_encode={path}")
    encoded.tofile(str(path))
    if not path.exists():
        raise RuntimeError(f"failed_to_write={path}")
    print(f"saved={path} size={path.stat().st_size}")


if __name__ == "__main__":
    raise SystemExit(main())
