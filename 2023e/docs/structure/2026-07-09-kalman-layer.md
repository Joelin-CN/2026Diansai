# 2026-07-09 Kalman Layer 设计

## 概述

Kalman Layer 对激光点图像坐标做平滑滤波，减少噪声抖动，预测丢失帧位置。

## 设计

```python
class LaserKalmanFilter:
    """对单个激光点的图像坐标做 Kalman 平滑

    状态向量: [u, v, du, dv]  (位置 + 速度)
    观测向量: [u, v]

    dt = 1/fps (从帧间隔估算)
    """
    def __init__(self, process_noise: float = 1e-3, measurement_noise: float = 1e-1): ...
    def predict(self) -> ImagePoint: ...     # 无观测时纯预测
    def update(self, measurement: ImagePoint) -> ImagePoint: ...  # 有观测时更新
```

## 集成位置

```
VisionFrameResult
    ↓
历史填补 (history_fill)
    ↓
LaserKalmanFilter.update()  ← 图像坐标 (ImagePoint)
    ↓
CoordinateTransformer       ← 平滑后的图像坐标
```

Kalman 在图像坐标域运行，不依赖标定结果。

## 主循环调用

```python
# 每帧对每个激光点：
if measured_image_center is not None:
    smoothed = kalman.update(measured_image_center)
else:
    smoothed = kalman.predict()  # 预测位置

screen_point = transformer.image_point_to_screen(smoothed)
```
