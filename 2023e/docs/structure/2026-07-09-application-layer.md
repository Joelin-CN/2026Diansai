# 2026-07-09 Application Layer 设计

## 概述

Application Layer 包含 SystemFactory（初始化工厂）和 runtime（统一主循环）。

## SystemFactory

```python
class SystemFactory:
    @classmethod
    def create(
        cls,
        system: str,         # "red" | "green"
        mode: str,           # "calibration" | "competition"
        task: str | None,    # "origin" | "screen" | "a4" (仅红色)
        device_config_path: str = "device/stage1_mock.json",
    ) -> RuntimeContext: ...
```

### 初始化流程

1. 加载设备配置 → 创建 CameraPort + MotorPort
2. 加载 vision config → 创建 VisionPipeline
3. 加载标定 → 创建 CoordinateTransformer + VisualServoController
4. 创建 Kalman 滤波器（红系统 1 个，绿系统 2 个）
5. 创建 Task（绿系统 GreenTrackRedTask，红系统延迟初始化）
6. 组装 RuntimeContext

## 统一主循环

```python
def run(context: RuntimeContext, mode: str) -> None:
    listener = start_keyboard_listener()  # Ctrl+Q 退出
    logger = RuntimeLogger(mode)

    while not should_exit:
        frame = context.camera.read()
        if frame is None:
            break

        # 1. Vision
        vision_result = context.vision.process(frame, context.vision_mode)

        # 2. History fill
        fill_history(vision_result, context.last_valid)

        # 3. Kalman smooth
        smoothed = apply_kalman(context.kalman, context.last_valid)

        # 4. Coordinate transform
        screen_points = transform_points(context.transformer, smoothed)

        # 5. Task init (lazy, when quad detected)
        init_task_if_needed(context, vision_result)

        # 6. Task target
        target = context.task.next_target(screen_points.current)

        # 7. Control command
        command = context.controller.update(target.point, screen_points.current)

        # 8. Send command
        context.motor.send(command)

        # 9. Log
        logger.record(frame_id, screen_points, target, command)
```

## calibration vs competition

| | calibration | competition |
|---|---|---|
| Task | 无（仅反向投影显示） | PathFollowerTask / GreenTrackRedTask |
| Motor | MockMotorPort（仅CSV） | MockMotorPort → SerialMotorPort |
| GUI | OpenCV窗口覆盖坐标 | 无GUI（Stage2无头） |
| 目标 | 验证 H_img_to_screen 精度 | 完成题目任务 |

## 入口调用

```bash
# 标定验证
python main.py --system red --mode calibration

# 比赛
python main.py --system red --mode competition --task a4
```
