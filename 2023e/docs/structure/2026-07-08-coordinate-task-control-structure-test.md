# 2026-07-08 Coordinate / Task / Control Layer 结构与测试记录

Document Version: v1.0

## 1. 文档目的

本文记录 2023 年电赛 E 题项目中 `Coordinate Layer`、`Task Layer` 和 `Control Layer` 的当前结构设计、模块边界、与 `Vision Layer` 的连接关系，以及本阶段使用假数据完成的单元测试结果。

当前阶段目标不是完成真实摄像头闭环，也不是驱动步进电机，而是先确认三层核心数学逻辑可以独立工作：

```text
Vision Layer 输出图像观测
        |
        v
Coordinate Layer 转换为屏幕坐标
        |
        v
Task Layer 生成目标点或路径
        |
        v
Control Layer 计算电机步数命令
```

本文只讨论前三个已测试的中间层：

- `Coordinate Layer`
- `Task Layer`
- `Control Layer`

本文不讨论：

- `Vision Layer` 内部检测算法。
- `Application Layer` 运行主循环。
- 摄像头读取、串口发送和步进电机驱动。
- 真实图片识别效果。
- 实物云台方向、速度和稳定性。

## 2. 已确认边界

三层边界必须和 `Vision Layer` 保持清晰分离。

当前边界定义如下：

```text
Vision Layer:
  输出 ImagePoint / ImageQuad / LaserDetection / TapeQuadDetection

Coordinate Layer:
  将 ImagePoint / ImageQuad 转为 ScreenPoint / ScreenQuad

Task Layer:
  使用 ScreenPoint / ScreenQuad 生成目标点或路径

Control Layer:
  使用 target ScreenPoint 和 current ScreenPoint 计算 MotorCommand
```

关键约束：

- `Task Layer` 不直接使用像素坐标。
- `Control Layer` 不直接使用像素坐标。
- `Control Layer` 不关心目标来自红光路径、A4 黑框还是绿光追红光。
- `Coordinate Layer` 只做坐标变换，不生成任务路径。
- `Task Layer` 只生成目标点，不计算电机步数。
- `Control Layer` 只计算命令，不读取图像、不检测激光点。

因此，上层正式运行时的数据类型转换关系为：

```text
ImagePoint(u_px, v_px)
  -> ScreenPoint(x_mm, y_mm)

ImageQuad(points: ImagePoint x 4)
  -> ScreenQuad(points: ScreenPoint x 4)
```

## 3. 当前新增源码结构

当前新增源码目录：

```text
2023e/src/
  coordinate/
    __init__.py
    types.py
    transformer.py
    quad.py

  task/
    __init__.py
    types.py
    path.py
    red_tasks.py
    green_tasks.py

  control/
    __init__.py
    types.py
    visual_servo.py
```

## 4. Coordinate Layer 结构

### 4.1 职责

`Coordinate Layer` 负责把视觉层的图像空间结果转换到屏幕平面坐标系。

当前主坐标系统一为：

```text
ScreenPoint(x_mm, y_mm)
```

单位为 `mm`。

### 4.2 当前类型

定义位置：

```text
src/coordinate/types.py
```

当前类型：

```text
ScreenPoint:
- x_mm
- y_mm

ScreenQuad:
- points: tuple[ScreenPoint, ScreenPoint, ScreenPoint, ScreenPoint]
```

`ScreenQuad` 明确要求只能包含 4 个点。

### 4.3 坐标变换

定义位置：

```text
src/coordinate/transformer.py
```

当前入口：

```text
CoordinateTransformer
```

当前接口：

```text
image_point_to_screen(ImagePoint) -> ScreenPoint
image_quad_to_screen(ImageQuad) -> ScreenQuad
```

实现方式：

```text
[x, y, w]^T = H_img_to_screen * [u, v, 1]^T

x_mm = x / w
y_mm = y / w
```

其中 `H_img_to_screen` 来自现场标定阶段，是图像坐标到屏幕坐标的单应性矩阵。

本实现使用 `numpy` 完成单应性计算，没有依赖 `cv2.perspectiveTransform`。这样前三层假数据测试不需要 OpenCV 环境。

### 4.4 黑胶带中心线

定义位置：

```text
src/coordinate/quad.py
```

当前接口：

```text
midpoint(ScreenPoint, ScreenPoint) -> ScreenPoint
tape_centerline_quad(outer: ScreenQuad, inner: ScreenQuad) -> ScreenQuad
```

设计原因：

`Vision Layer` 中 `TapeQuadDetection` 已经输出：

```text
outer_quad
inner_quad
```

红色激光沿 A4 黑胶带运动时，不应沿外边缘，也不应沿内边缘，而应沿黑胶带中心线运动。

因此中心线四边形按对应角点取中点：

```text
center[i] = (outer[i] + inner[i]) / 2
```

生成结果：

```text
ScreenQuad(center points)
```

供 `Task Layer` 生成路径点。

## 5. Task Layer 结构

### 5.1 职责

`Task Layer` 负责回答：

```text
下一步应该让激光点去屏幕上的哪个位置？
```

它只输出目标点，不计算控制误差，也不发送电机命令。

### 5.2 当前类型

定义位置：

```text
src/task/types.py
```

当前类型：

```text
TaskTarget:
- point: ScreenPoint
- done: bool
```

当前 `done` 字段保留给后续一次性任务、终点任务或计时任务使用。当前路径循环任务中保持 `False`。

### 5.3 四边形路径插值

定义位置：

```text
src/task/path.py
```

当前接口：

```text
interpolate_quad_path(quad: ScreenQuad, points_per_edge: int) -> list[ScreenPoint]
```

作用：

将四边形的四个角点插值成连续路径点。

例如：

```text
A -> B -> C -> D -> A
```

如果 `points_per_edge = 40`，则每条边生成 40 个路径点，总共生成：

```text
4 * 40 = 160 points
```

红光沿黑框移动时，控制层不直接追四个角，而是依次追这些小目标点。

### 5.4 红色路径跟随任务

定义位置：

```text
src/task/red_tasks.py
```

当前类：

```text
PathFollowerTask
```

当前接口：

```text
next_target(current: ScreenPoint) -> TaskTarget
```

行为：

1. 读取当前路径索引 `index` 对应的目标点。
2. 计算当前红点与目标点距离。
3. 如果距离小于等于 `arrive_dist_mm`，切换到下一个路径点。
4. 返回当前应追踪的目标点。

该任务适合：

- 红点沿屏幕正方形边框运动。
- 红点沿 A4 黑胶带中心线运动。

### 5.5 绿色追红任务

定义位置：

```text
src/task/green_tasks.py
```

当前类：

```text
GreenTrackRedTask
```

当前接口：

```text
next_target(detected_red_point: ScreenPoint) -> TaskTarget
```

行为：

绿色系统只把自身摄像头检测到的红点作为目标：

```text
target = detected_red_point
```

该设计符合题目约束：

```text
绿色系统不得读取红色系统的路径、状态或指令。
```

## 6. Control Layer 结构

### 6.1 职责

`Control Layer` 负责把屏幕平面上的误差转换为电机命令。

输入：

```text
target: ScreenPoint
current: ScreenPoint
```

输出：

```text
MotorCommand
```

它不关心：

- 目标点来自红光路径还是绿光追踪。
- 视觉层如何识别激光点。
- 电机命令最终如何通过串口发送。

### 6.2 当前类型

定义位置：

```text
src/control/types.py
```

当前类型：

```text
MotorCommand:
- yaw_delta_steps
- pitch_delta_steps
- enable
```

当前命令语义：

```text
yaw_delta_steps: 本帧水平轴相对运动步数
pitch_delta_steps: 本帧俯仰轴相对运动步数
enable: 是否允许执行
```

### 6.3 视觉伺服控制器

定义位置：

```text
src/control/visual_servo.py
```

当前类：

```text
VisualServoController
```

当前配置：

```text
ServoConfig:
- yaw_kp_steps_per_mm
- pitch_kp_steps_per_mm
- deadband_mm
- max_yaw_delta_steps
- max_pitch_delta_steps
```

当前控制方式为 P 控制：

```text
error_x = target.x_mm - current.x_mm
error_y = target.y_mm - current.y_mm

yaw_steps = error_x * yaw_kp_steps_per_mm
pitch_steps = error_y * pitch_kp_steps_per_mm
```

控制层还包含两个基础保护：

```text
deadband:
  小误差不动，减少抖动

clamp:
  单帧步数限幅，防止命令过大
```

如果实际云台方向相反，优先通过参数调整：

```text
yaw_kp_steps_per_mm = -yaw_kp_steps_per_mm
pitch_kp_steps_per_mm = -pitch_kp_steps_per_mm
```

而不是修改任务层或坐标层。

## 7. 与 Vision Layer 的连接方式

### 7.1 红色 A4 任务连接链路

红色系统沿 A4 黑胶带中心线运动时，推荐链路：

```text
VisionPipeline(mode=RUNTIME_A4_RED)
  -> result.red_laser.image_center
  -> result.tape_quad.outer_quad
  -> result.tape_quad.inner_quad

CoordinateTransformer
  -> red_laser_screen_point
  -> outer_screen_quad
  -> inner_screen_quad

tape_centerline_quad
  -> centerline_screen_quad

interpolate_quad_path
  -> path: list[ScreenPoint]

PathFollowerTask
  -> target_screen_point

VisualServoController
  -> MotorCommand
```

### 7.2 绿色追红任务连接链路

绿色系统追踪红点时，推荐链路：

```text
VisionPipeline(mode=RUNTIME_TRACK_RED_GREEN)
  -> result.red_laser.image_center
  -> result.green_laser.image_center

CoordinateTransformer
  -> red_laser_screen_point
  -> green_laser_screen_point

GreenTrackRedTask
  -> target_screen_point = red_laser_screen_point

VisualServoController
  -> MotorCommand
```

注意：

```text
绿色系统只能使用自己视觉检测到的 red_laser_screen_point，
不能读取红色系统内部 path、task index 或 motor command。
```

## 8. 当前测试目录结构

当前新增测试目录：

```text
2023e/tests/
  coordinate/
    test_coordinate_transformer.py

  task/
    test_tasks.py

  control/
    test_visual_servo.py
```

这些测试使用假数据，不读取真实图片、不打开摄像头、不连接电机。

## 9. 假数据测试内容

### 9.1 Coordinate Layer 测试

测试文件：

```text
tests/coordinate/test_coordinate_transformer.py
```

测试目标：

1. 假单应性矩阵是否能正确把 `ImagePoint` 转为 `ScreenPoint`。
2. 单位矩阵是否能把 `ImageQuad` 四个点逐一转成 `ScreenQuad`。
3. 黑胶带外框与内框是否能正确计算中心线四边形。
4. `ScreenQuad` 是否拒绝非 4 点输入。

假数据示例：

```text
H =
[2, 0, 10]
[0, 3, 20]
[0, 0,  1]

ImagePoint(5, 7)
  -> ScreenPoint(20, 41)
```

中心线测试示例：

```text
outer:
  (0,0), (120,0), (120,80), (0,80)

inner:
  (20,10), (100,10), (100,70), (20,70)

center:
  (10,5), (110,5), (110,75), (10,75)
```

### 9.2 Task Layer 测试

测试文件：

```text
tests/task/test_tasks.py
```

测试目标：

1. 四边形路径插值是否生成预期数量和顺序的路径点。
2. `PathFollowerTask` 是否在当前点到达目标点后切换到下一个路径点。
3. `PathFollowerTask` 到达最后一个目标后是否回到第一个目标。
4. `GreenTrackRedTask` 是否直接把检测到的红点作为目标点。

假路径示例：

```text
quad:
  (0,0), (100,0), (100,100), (0,100)

points_per_edge = 2

path:
  (0,0)
  (50,0)
  (100,0)
  (100,50)
  (100,100)
  (50,100)
  (0,100)
  (0,50)
```

### 9.3 Control Layer 测试

测试文件：

```text
tests/control/test_visual_servo.py
```

测试目标：

1. 控制器是否能把屏幕误差转换为步进电机步数。
2. 小误差是否会进入死区，不输出抖动命令。
3. 大误差是否会被限幅，避免单帧命令过大。

假数据示例：

```text
target = ScreenPoint(60, 30)
current = ScreenPoint(50, 40)

error_x = 10
error_y = -10

yaw_kp = 2
pitch_kp = -3

MotorCommand:
  yaw_delta_steps = 20
  pitch_delta_steps = 30
```

## 10. 测试运行命令

当前测试命令：

```powershell
$env:PYTHONPATH='C:\Users\35336\Desktop\desk\2026diansai\2026diansai\new gitteam\2026Diansai\2023e\src'
$env:PYTHONDONTWRITEBYTECODE='1'
python -m pytest tests\coordinate tests\task tests\control -q
```

说明：

- `PYTHONPATH` 用于让测试能直接导入 `src` 下的包。
- `PYTHONDONTWRITEBYTECODE=1` 用于避免测试过程生成新的 `__pycache__` 文件。
- 当前测试只覆盖 `coordinate/task/control` 三层。

## 11. 当前测试结果

当前测试输出：

```text
...........                                                              [100%]
11 passed in 0.22s
```

含义：

```text
一共执行 11 个假数据单元测试。
11 个测试全部通过。
```

当前可确认：

- 坐标转换基本数学逻辑通过。
- 黑胶带中心线计算逻辑通过。
- 四边形路径插值逻辑通过。
- 红色路径跟随任务切点逻辑通过。
- 绿色追红任务目标生成逻辑通过。
- P 控制器步数计算、死区和限幅逻辑通过。

## 12. 当前未测试内容

当前测试仍然没有覆盖以下内容：

- 真实图片中的红点、绿点和 A4 黑框检测效果。
- `VisionPipeline` 与 `Coordinate Layer` 的真实联调。
- 标定文件中的真实 `H_img_to_screen` 是否正确。
- 摄像头实时输入。
- Application 主循环。
- 串口或 GPIO 设备接口。
- 步进电机方向是否和控制命令一致。
- 云台实际运动是否稳定。
- 红点是否能真实沿黑胶带中心线运动。
- 绿点是否能真实追踪红点并满足 3 cm 要求。

因此当前结论只能表述为：

```text
Coordinate / Task / Control 三层的基础数学逻辑通过假数据测试。
完整系统仍需后续真实图片测试和实物闭环测试。
```

## 13. 后续建议

下一步建议按以下顺序推进：

1. 新增 `Application Layer` 的离线联调入口。
2. 用真实图片跑 `VisionPipeline`。
3. 将 `VisionFrameResult` 接入 `CoordinateTransformer`。
4. 用 `tape_quad.outer_quad` 和 `tape_quad.inner_quad` 生成黑胶带中心线。
5. 生成红光 A4 路径。
6. 输出一帧对应的 `MotorCommand`，先打印不发给电机。
7. 确认方向无误后再接设备接口。

推荐下一阶段的测试链路：

```text
real image
  -> VisionPipeline
  -> CoordinateTransformer
  -> Task
  -> VisualServoController
  -> print MotorCommand
```

当离线图片链路稳定后，再进入：

```text
camera frame
  -> realtime loop
  -> motor command port
  -> physical feedback
```

## 14. 当前结构结论

当前 v1.0 结论如下：

```text
Coordinate Layer:
  已具备 ImagePoint/ImageQuad 到 ScreenPoint/ScreenQuad 的基础变换能力
  已具备 A4 黑胶带中心线计算能力

Task Layer:
  已具备四边形路径插值能力
  已具备红光路径跟随任务
  已具备绿光追红目标任务

Control Layer:
  已具备 P 控制、死区和单帧限幅能力
  已输出统一 MotorCommand
```

这三层目前已经可以作为 `Vision Layer` 和后续 `Application Layer` 之间的中间基础。下一步重点应从假数据测试转向真实图片离线联调。
