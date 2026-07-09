# 2026-07-09 Device Layer 设计

## 概述

Device Layer 抽象相机和电机接口，支持 mock（离线验证）和真实硬件（现场运行）两种后端。

## 接口定义

### CameraPort

```python
class CameraPort(ABC):
    def open(self) -> None: ...
    def read(self) -> CameraFrame: ...
    def close(self) -> None: ...
```

### MotorPort

```python
class MotorPort(ABC):
    def send(self, command: MotorCommand) -> None: ...
    def open(self) -> None: ...
    def close(self) -> None: ...
```

## 实现

| 实现 | 用途 | 阶段 |
|------|------|------|
| VideoFileCameraPort | 读取录制视频文件 | Stage 1 |
| MockMotorPort | 记录 motor_command 到 CSV | Stage 1 |
| OpenCVCameraPort | 真实 USB 相机 | Stage 2 |
| SerialMotorPort | 通过串口控制舵机 | Stage 2 |

## Stage 1 设备配置

```json
{
  "camera": {
    "type": "video_file",
    "video_path": "test_data/red_laser_video.mp4"
  },
  "motor": {
    "type": "mock",
    "log_dir": "outputs/mock_test"
  }
}
```

## MockMotorPort 输出格式

每帧一行 CSV：
```csv
timestamp_ms,frame_id,yaw_delta_steps,pitch_delta_steps,enable
0,0,5,-3,True
33,1,4,-2,True
67,2,0,0,False
```
