# 2026-07-09 Calibration Layer 设计

## 概述

Calibration Layer 定义标定产物的最小必要字段，提供 JSON 加载与保存能力。红绿系统各自独立标定。

## CalibrationResult

```python
@dataclass
class CalibrationResult:
    H_img_to_screen: np.ndarray  # 3x3 homography matrix (ImagePoint → ScreenPoint)
    servo_config: ServoConfig    # 控制增益参数
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| H_img_to_screen | np.ndarray (3,3) float32 | 图像坐标到屏幕坐标的单应性矩阵 |
| servo_config | ServoConfig | yaw/pitch 增益和限幅参数 |

## 文件格式

```json
{
  "H_img_to_screen": [
    [h00, h01, h02],
    [h10, h11, h12],
    [h20, h21, h22]
  ],
  "servo_config": {
    "yaw_kp_steps_per_mm": 1.0,
    "pitch_kp_steps_per_mm": 1.0,
    "deadband_mm": 3.0,
    "max_yaw_delta_steps": 80,
    "max_pitch_delta_steps": 80
  }
}
```

## 加载器 API

```python
from calibration.loader import load_calibration, save_calibration

# 加载
calib = load_calibration("calibration/red_calibration.json")
# 使用
transformer = CoordinateTransformer(calib.H_img_to_screen)
controller = VisualServoController(calib.servo_config)

# 保存
save_calibration(calib, "calibration/red_calibration.json")
```

## 文件位置

```
calibration/
  red_calibration.json     # 红色系统标定
  green_calibration.json   # 绿色系统标定
```
