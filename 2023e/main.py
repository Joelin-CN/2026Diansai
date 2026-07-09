#!/usr/bin/env python3
"""2023 E 题运动目标控制与自动追踪系统 —— 统一程序入口。

Usage:
    # 标定验证
    python main.py --system red --mode calibration
    python main.py --system green --mode calibration

    # 比赛运行（红色系统）
    python main.py --system red --mode competition --task a4
    python main.py --system red --mode competition --task screen
    python main.py --system red --mode competition --task origin

    # 比赛运行（绿色系统）
    python main.py --system green --mode competition

    # 指定设备配置
    python main.py --system red --mode competition --task a4 --device device/stage1_mock.json
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

# 确保 src 在 Python path 中
_PROJECT_ROOT = Path(__file__).resolve().parent
_SRC = _PROJECT_ROOT / "src"
if str(_SRC) not in sys.path:
    sys.path.insert(0, str(_SRC))


def _setup_logging(mode: str) -> None:
    level = logging.DEBUG if mode == "calibration" else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="2023 E 题运动目标控制与自动追踪系统",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python main.py --system red --mode calibration
  python main.py --system red --mode competition --task a4
  python main.py --system green --mode competition
        """,
    )
    parser.add_argument(
        "--system",
        required=True,
        choices=["red", "green"],
        help="系统选择: red (红色系统) | green (绿色系统)",
    )
    parser.add_argument(
        "--mode",
        required=True,
        choices=["calibration", "competition"],
        help="运行模式: calibration (标定验证) | competition (比赛运行)",
    )
    parser.add_argument(
        "--task",
        default=None,
        choices=["origin", "screen", "a4"],
        help="任务类型（仅红色系统需要）: origin | screen | a4",
    )
    parser.add_argument(
        "--device",
        default="device/stage1_mock.json",
        help="设备配置文件路径 (默认: device/stage1_mock.json)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    _setup_logging(args.mode)
    logger = logging.getLogger("main")

    # 验证参数
    if args.system == "red" and args.mode == "competition" and args.task is None:
        logger.error("Red system in competition mode requires --task (origin|screen|a4)")
        sys.exit(1)

    # 导入 + 运行
    from application.factory import SystemFactory
    from application.runtime import run

    logger.info(f"System: {args.system}, Mode: {args.mode}, Task: {args.task or 'N/A'}")
    logger.info(f"Device config: {args.device}")

    context = SystemFactory.create(
        system=args.system,
        mode=args.mode,
        task=args.task,
        device_config_path=args.device,
    )

    run(context, mode=args.mode)


if __name__ == "__main__":
    main()
