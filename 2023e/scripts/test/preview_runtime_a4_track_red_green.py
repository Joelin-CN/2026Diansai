from __future__ import annotations

import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from preview_vision_video import main as preview_video_main  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    forwarded = list(argv or [])
    forced = ["--path-mode", "runtime", "--runtime-mode", "runtime_a4_track_red_green"]
    return preview_video_main([*forced, *forwarded])


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
