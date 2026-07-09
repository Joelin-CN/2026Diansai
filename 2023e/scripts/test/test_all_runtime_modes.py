"""
Test all 4 runtime modes with full video visualization.
Creates annotated videos for each mode.
"""
from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path

import cv2
import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SRC_DIR = PROJECT_ROOT / "src"
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from preview_vision_video import _camera_frame, _draw_runtime_result  # noqa: E402
from vision.config import load_vision_config  # noqa: E402
from vision.pipeline import VisionPipeline  # noqa: E402
from vision.types import VisionMode  # noqa: E402


DEFAULT_VIDEO = Path(r"E:\B306\Visual\openCV\project\videos\2023e.mp4")
DEFAULT_OUTPUT_ROOT = PROJECT_ROOT / "outputs"


def process_mode(
    video_path: Path,
    profile: str,
    mode: VisionMode,
    output_dir: Path,
    max_frames: int = 300,
) -> dict:
    """Process video with a single runtime mode."""
    print(f"\n{'='*60}")
    print(f"Processing mode: {mode.value}")
    print(f"{'='*60}")
    
    cap = cv2.VideoCapture(str(video_path))
    if not cap.isOpened():
        print(f"[ERROR] Failed to open video: {video_path}")
        return {"success": False, "error": "cannot_open_video"}

    config_path = PROJECT_ROOT / "configs" / profile / "vision.json"
    config = load_vision_config(config_path)
    
    # Get video properties
    fps = cap.get(cv2.CAP_PROP_FPS)
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    # Limit frames for testing
    frames_to_process = min(total_frames, max_frames)
    
    print(f"Video: {width}x{height} @ {fps:.1f}fps")
    print(f"Processing {frames_to_process}/{total_frames} frames")
    
    # Create output video
    output_video_path = output_dir / f"{mode.value}_annotated.mp4"
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(str(output_video_path), fourcc, fps, (width, height))
    
    # Initialize pipeline
    pipeline = VisionPipeline(profile, config, frame_size=(width, height))
    
    # Statistics
    stats = {
        "mode": mode.value,
        "total_frames": frames_to_process,
        "red_found_count": 0,
        "green_found_count": 0,
        "tape_found_count": 0,
        "screen_found_count": 0,
        "avg_processing_time_ms": 0.0,
        "avg_red_stage_ms": 0.0,
        "avg_green_stage_ms": 0.0,
        "avg_a4_stage_ms": 0.0,
        "avg_screen_stage_ms": 0.0,
    }
    
    processing_times = []
    red_stage_times = []
    green_stage_times = []
    a4_stage_times = []
    screen_stage_times = []
    
    try:
        for frame_id in range(1, frames_to_process + 1):
            ret, frame = cap.read()
            if not ret:
                break
            
            # Process frame
            result = pipeline.process(_camera_frame(frame, frame_id), mode)
            
            # Draw annotations
            annotated = frame.copy()
            _draw_runtime_result(annotated, result)
            
            # Add mode label
            cv2.putText(
                annotated,
                f"Mode: {mode.value}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (255, 255, 255),
                2,
            )
            cv2.putText(
                annotated,
                f"Frame: {frame_id}/{frames_to_process}",
                (10, 60),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.6,
                (255, 255, 255),
                1,
            )
            
            # Write frame
            out.write(annotated)
            
            # Update statistics
            if result.red_laser and result.red_laser.found:
                stats["red_found_count"] += 1
            if result.green_laser and result.green_laser.found:
                stats["green_found_count"] += 1
            if result.tape_quad and result.tape_quad.found:
                stats["tape_found_count"] += 1
            if result.screen_detection and result.screen_detection.found:
                stats["screen_found_count"] += 1
            
            processing_times.append(result.diagnostics.processing_time_ms)
            red_stage_times.append(result.diagnostics.stage_time_ms.get("red_laser", 0.0))
            green_stage_times.append(result.diagnostics.stage_time_ms.get("green_laser", 0.0))
            a4_stage_times.append(result.diagnostics.stage_time_ms.get("a4_tape", 0.0))
            screen_stage_times.append(result.diagnostics.stage_time_ms.get("screen_square", 0.0))
            
            # Progress
            if frame_id % 50 == 0 or frame_id == frames_to_process:
                print(f"  Progress: {frame_id}/{frames_to_process} ({frame_id*100//frames_to_process}%)")
        
        # Calculate averages
        stats["avg_processing_time_ms"] = np.mean(processing_times) if processing_times else 0.0
        stats["avg_red_stage_ms"] = np.mean(red_stage_times) if red_stage_times else 0.0
        stats["avg_green_stage_ms"] = np.mean(green_stage_times) if green_stage_times else 0.0
        stats["avg_a4_stage_ms"] = np.mean(a4_stage_times) if a4_stage_times else 0.0
        stats["avg_screen_stage_ms"] = np.mean(screen_stage_times) if screen_stage_times else 0.0
        
        stats["success"] = True
        stats["output_video"] = str(output_video_path)
        
        print(f"\n[OK] Mode {mode.value} completed")
        print(f"   Output: {output_video_path}")
        print(f"   Red found: {stats['red_found_count']}/{frames_to_process} ({stats['red_found_count']*100//frames_to_process}%)")
        print(f"   Green found: {stats['green_found_count']}/{frames_to_process} ({stats['green_found_count']*100//frames_to_process if frames_to_process > 0 else 0}%)")
        print(f"   Tape found: {stats['tape_found_count']}/{frames_to_process} ({stats['tape_found_count']*100//frames_to_process if frames_to_process > 0 else 0}%)")
        print(f"   Screen found: {stats['screen_found_count']}/{frames_to_process} ({stats['screen_found_count']*100//frames_to_process if frames_to_process > 0 else 0}%)")
        print(f"   Avg processing: {stats['avg_processing_time_ms']:.2f}ms")
        
        return stats
        
    finally:
        cap.release()
        out.release()


def main():
    """Run all 4 runtime modes."""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_root = DEFAULT_OUTPUT_ROOT / f"runtime_modes_test_{timestamp}"
    output_root.mkdir(parents=True, exist_ok=True)
    
    print("="*60)
    print("Testing All Runtime Modes")
    print("="*60)
    print(f"Video: {DEFAULT_VIDEO}")
    print(f"Output: {output_root}")
    
    modes = [
        VisionMode.RUNTIME_SCREEN_RED,
        VisionMode.RUNTIME_A4_RED,
        VisionMode.RUNTIME_TRACK_RED_GREEN,
        VisionMode.RUNTIME_A4_TRACK_RED_GREEN,
    ]
    
    all_stats = []
    
    for mode in modes:
        stats = process_mode(
            video_path=DEFAULT_VIDEO,
            profile="green",
            mode=mode,
            output_dir=output_root,
            max_frames=300,  # Process first 300 frames (~10 seconds @ 30fps)
        )
        all_stats.append(stats)
    
    # Write summary
    summary_path = output_root / "summary.txt"
    with summary_path.open("w", encoding="utf-8") as f:
        f.write("Runtime Modes Test Summary\n")
        f.write("="*60 + "\n\n")
        f.write(f"Timestamp: {timestamp}\n")
        f.write(f"Video: {DEFAULT_VIDEO}\n")
        f.write(f"Output: {output_root}\n\n")
        
        for stats in all_stats:
            f.write(f"\nMode: {stats['mode']}\n")
            f.write("-"*40 + "\n")
            if stats.get("success"):
                f.write(f"Total frames: {stats['total_frames']}\n")
                f.write(f"Red found: {stats['red_found_count']} ({stats['red_found_count']*100//stats['total_frames'] if stats['total_frames'] > 0 else 0}%)\n")
                f.write(f"Green found: {stats['green_found_count']} ({stats['green_found_count']*100//stats['total_frames'] if stats['total_frames'] > 0 else 0}%)\n")
                f.write(f"Tape found: {stats['tape_found_count']} ({stats['tape_found_count']*100//stats['total_frames'] if stats['total_frames'] > 0 else 0}%)\n")
                f.write(f"Screen found: {stats['screen_found_count']} ({stats['screen_found_count']*100//stats['total_frames'] if stats['total_frames'] > 0 else 0}%)\n")
                f.write(f"Avg processing time: {stats['avg_processing_time_ms']:.2f}ms\n")
                f.write(f"  - Red stage: {stats['avg_red_stage_ms']:.2f}ms\n")
                f.write(f"  - Green stage: {stats['avg_green_stage_ms']:.2f}ms\n")
                f.write(f"  - A4 stage: {stats['avg_a4_stage_ms']:.2f}ms\n")
                f.write(f"  - Screen stage: {stats['avg_screen_stage_ms']:.2f}ms\n")
                f.write(f"Output video: {stats['output_video']}\n")
            else:
                f.write(f"[FAILED]: {stats.get('error', 'unknown')}\n")
    
    print(f"\n{'='*60}")
    print("All modes completed!")
    print(f"Summary: {summary_path}")
    print(f"{'='*60}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
