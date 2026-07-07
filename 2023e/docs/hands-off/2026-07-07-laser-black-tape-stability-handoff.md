# 2026-07-07 激光点黑胶带稳定跟踪交接文档

## 1. 当前项目位置

当前项目目录：

```text
2023e/
```

当前阶段目标已经从“先把 Vision Layer runtime 架构改对”推进到“继续调 `red_laser` / `green_laser`，让它们在黑色胶带区域上也能稳定跟踪”。

## 2. 当前已完成内容

### 2.1 Vision Layer 边界已收敛

当前已确认并已经按此实现：

```text
Vision Layer 只输出：
- ImagePoint
- ImageQuad
- detection dataclass
- diagnostics

Vision Layer 不输出：
- ScreenPoint
- ScreenQuad
- MotorCommand
- Control Layer 对象
```

### 2.2 formal runtime modes 已改为 4 个题目驱动模式

当前正式运行模式是：

```text
RUNTIME_SCREEN_RED
RUNTIME_A4_RED
RUNTIME_TRACK_RED_GREEN
RUNTIME_A4_TRACK_RED_GREEN
```

另保留：

```text
DEBUG
```

其中 `DEBUG` 明确不是 `<= 30 ms/frame` 的正式 runtime 路径。

### 2.3 runtime / debug 已解耦

当前已经完成：

- formal runtime path 与 preview/debug path 分离
- formal runtime 不再按 detector 任意组合组织
- formal runtime 已拆成高频 laser 路径和低频 support detector 路径
- A4 support 已做缓存复用，避免每帧进入热路径
- runtime bounded reacquire 已落地，不再在 formal runtime 中回到 preview 风格全图 relaxed fallback

### 2.4 激光 detector 已完成一轮 bright-core + color-ring 改造

当前 `src/vision/laser_detector.py` 已具备：

- bright white core mask
- color ring mask
- core/ring 关联候选
- 候选打分
- diagnostics 输出

当前候选 diagnostics 已包括：

```text
roi_used
candidate_count
rejected_candidates
best_center
best_score
second_best_score
candidate_scores
failure_reason
```

当前候选项内已包括：

```text
center
score
area_px
color_area_px
bright_core_area_px
combined_area_px
core_found
brightness
```

### 2.5 相关测试当前状态

本轮已验证：

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/vision -q
```

结果：

```text
45 passed in 0.19s
```

以及：

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest tests/scripts -q
```

结果：

```text
1 passed in 0.10s
```

## 3. 当前已知现象

### 3.1 当前最重要的问题不是 runtime 架构，而是黑胶带区域上的激光稳定性

目前 runtime 架构已经做过一轮正确性收敛。

接下来更关键的问题是：

```text
当红点/绿点进入黑色胶带区域时，
red_laser / green_laser 是否还能稳定、连续、低误检地跟踪。
```

### 3.2 已知视觉现象

先前证据和当前 detector 设计都指向同一观察：

- 在白底区域，红/绿点更大、更明显
- 在黑胶带区域，红/绿点通常会更小、更弱
- 光斑中心往往偏白，颜色更像分布在外围 ring 上
- 纯 HSV blob 方案在黑胶带区域容易因为彩色像素面积不够而退化

这也是前一轮引入 bright-core + color-ring 的直接原因。

### 3.3 当前还不能直接宣称“黑胶带上已经稳定”

虽然 detector 已做过一轮结构改造，runtime 也已解耦，但接下来仍需要继续系统调试：

- `red_laser` 是否在黑胶带上仍频繁出现 `too_small` / `low_brightness`
- `green_laser` 是否在黑胶带上更容易跳点或落到错误候选
- 两个颜色是否会在同一区域内触发 `multiple_candidates`
- 当前 temporal scoring 是否足够稳定
- 当前 core/ring 关联规则是否过松或过紧

## 4. 当前代码状态的关键点

### 4.1 LaserDetector 当前打分结构

当前 `src/vision/laser_detector.py` 候选分数主要由以下项组成：

```text
brightness_score
circularity
area_score
temporal_score
core_score
```

当前权重：

```text
0.38 * brightness_score
0.22 * circularity
0.15 * area_score
0.15 * temporal_score
0.10 * core_score
```

这意味着：

- brightness 目前仍是最高权重项
- temporal 只是辅助项，不是主导项
- core_found 目前只给 0.10 权重

如果黑胶带区域上的真实光斑颜色更弱、面积更小，而背景噪声更复杂，这套权重是否足够，还需要证据驱动判断。

### 4.2 当前 runtime laser 路径

formal runtime 当前做的是：

```text
tracker.suggest_roi(color)
  -> detector.detect(image, roi=..., previous_detection=...)
  -> tracker.update(color, detection)
```

这说明下一阶段不应该再把“全图 relaxed fallback”塞回 formal runtime。

下一阶段如果要定位问题，应优先在 `DEBUG` 路径中看清：

- strict 为什么失败
- relaxed 看到了什么候选
- 候选分数差距是多少
- 为什么最终选错或丢失

### 4.3 当前 preview/debug 路径已经适合做证据采集

当前 `scripts/test/preview_vision_video.py` 已明确区分：

- `--path-mode debug`
- `--path-mode runtime`

并且 `debug` 路径中仍保留：

- strict / relaxed 分析
- A4 overlay
- richer summary

这正适合作为下一阶段调激光稳定性的证据采集入口。

## 5. 下一阶段建议调试目标

建议把下一阶段目标定义为：

```text
让 red_laser / green_laser 在黑色胶带区域上也能尽量连续、稳定、低误检地输出 measured detection，
并用 diagnostics 证明是哪些条件导致它们成功或失败。
```

具体要回答的问题：

1. 黑胶带区域内的真实点，当前更常因什么失败：
   - `too_small`
   - `low_brightness`
   - `low_confidence`
   - `multiple_candidates`
   - 其他
2. 红点和绿点在黑胶带区域是否表现出不同失效模式。
3. bright-core + color-ring 关联规则是否已经足够区分真实点和噪声。
4. 当前 temporal score 是否足够抑制跳点。
5. 当前 `multi_candidate_margin` 是否过严或过松。

## 6. 下一阶段工作原则

### 6.1 必须先 systematic debugging，不要直接猜阈值

下一线程必须遵循：

```text
先收集证据
再定位主要失败模式
最后再做最小改动
```

不要一上来就同时改：

- HSV saturation/value
- area_px_min
- confidence_min
- temporal scoring
- multi-candidate 判定

否则很难知道到底是什么起作用。

### 6.2 优先使用 DEBUG 路径采集证据

建议先通过 `preview_vision_video.py --path-mode debug` 观察：

- strict red / green 在黑胶带区域如何失败
- relaxed red / green 是否看到了真实候选
- relaxed 候选里 best / second_best 是否接近
- `candidate_scores` 是否存在明显误候选

### 6.3 formal runtime 不要回退到全图 relaxed fallback

即使为了调试方便，也不要把下面这种逻辑重新引回 formal runtime：

```text
strict fail
  -> unconstrained full-frame relaxed search
```

这类逻辑只能留在 `DEBUG` 证据路径中。

### 6.4 保持 Vision Layer 边界

下一线程不得引入：

- `ScreenPoint`
- `ScreenQuad`
- `MotorCommand`
- Control Layer
- Kalman 强依赖

## 7. 建议优先排查的方向

建议优先顺序如下。

### 7.1 先看失败模式分布

先弄清楚黑胶带区域上的丢失主要是：

- 候选没生成
- 候选生成了但 `combined_area` 不够
- 候选亮度不够
- 候选分数不够
- 候选太接近导致 `multiple_candidates`

### 7.2 再看 core/ring 关联质量

重点关注：

- `bright_core_area_px` 是否常常很小但仍能稳定提供中心
- `color_area_px` 是否在黑胶带区域明显缩小
- `combined_area_px` 是否足以 rescue 小 ring
- 是否存在大量“白 core + 非真实 color 噪声”的误关联

### 7.3 再看 temporal 与跳点问题

重点关注：

- 黑胶带区域前后是否出现候选突然切换
- `previous_detection` 是否足够抑制跳点
- `distance / 100.0` 这一 temporal penalty 是否合理

### 7.4 最后才调阈值和权重

在证据足够后，可能需要最小改动测试：

- `area_px_min`
- `confidence_min`
- bright core 阈值
- `multi_candidate_margin`
- `core_score` 或 `temporal_score` 权重

但不要多项一起改。

## 8. 推荐先读的文件

新线程开始时建议先读：

```text
2023e/README.md
2023e/docs/hands-off/2026-07-07-real-time-runtime-modes-handoff.md
2023e/docs/hands-off/2026-07-07-laser-black-tape-stability-handoff.md
2023e/docs/superpowers/specs/2026-07-07-laser-core-ring-detection-design.md
2023e/src/vision/laser_detector.py
2023e/src/vision/pipeline.py
2023e/src/vision/state_tracker.py
2023e/scripts/test/preview_vision_video.py
2023e/tests/vision/test_laser_detector.py
2023e/tests/vision/test_pipeline.py
2023e/tests/vision/test_state_tracker.py
```

## 9. 当前可直接使用的命令

### 9.1 debug 路径采证

```powershell
conda run -n opencv-learning python "scripts/test/preview_vision_video.py" --path-mode debug --no-window --max-frames 120
```

### 9.2 第四个 formal runtime mode 观察

```powershell
conda run -n opencv-learning python "scripts/test/preview_runtime_a4_track_red_green.py" --no-window --max-frames 120
```

### 9.3 全 Vision tests

```powershell
$env:PYTHONPATH="src"; conda run -n opencv-learning python -m pytest "tests/vision" -q
```

## 10. 新线程提示词

可以在新线程中直接使用下面这段提示词：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案。当前 Vision Layer 的 runtime modes 重构已经完成，下一步要继续调 red_laser / green_laser，让它们在黑色胶带区域上也能更稳定地跟踪。

请先读取以下文件：
1. 2023e/README.md
2. 2023e/docs/hands-off/2026-07-07-real-time-runtime-modes-handoff.md
3. 2023e/docs/hands-off/2026-07-07-laser-black-tape-stability-handoff.md
4. 2023e/docs/superpowers/specs/2026-07-07-laser-core-ring-detection-design.md
5. 2023e/src/vision/laser_detector.py
6. 2023e/src/vision/pipeline.py
7. 2023e/src/vision/state_tracker.py
8. 2023e/scripts/test/preview_vision_video.py
9. 2023e/tests/vision/test_laser_detector.py
10. 2023e/tests/vision/test_pipeline.py
11. 2023e/tests/vision/test_state_tracker.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. formal runtime mode 已改成 4 个题目驱动模式：
   - RUNTIME_SCREEN_RED
   - RUNTIME_A4_RED
   - RUNTIME_TRACK_RED_GREEN
   - RUNTIME_A4_TRACK_RED_GREEN
3. DEBUG / preview 不属于 <= 30 ms/frame 的正式 runtime 目标。
4. 当前 runtime 与 debug 已解耦，formal runtime 中不允许重新引入 strict 失败后全图 relaxed fallback。
5. A4 support 已做低频缓存复用，bounded reacquire 也已落地。
6. 当前 LaserDetector 已做过一轮 bright-core + color-ring 改造，tests/vision/test_laser_detector.py 当前通过。
7. 当前真正要继续解决的问题是：red_laser / green_laser 在黑色胶带区域上的稳定性仍需继续调优。
8. 进入黑胶带区域后，优先关注真实失败原因到底是 too_small、low_brightness、low_confidence、multiple_candidates，还是 temporal 选错候选。
9. 下一步请先做 systematic debugging，不要直接猜阈值。
10. 优先利用 DEBUG 路径收集 strict / relaxed / candidate diagnostics，先确认黑胶带区域上的主要失效模式，再做最小改动。
11. 保持 Vision Layer 边界，不引入 ScreenPoint、MotorCommand、Control Layer 或 Kalman 强依赖。

请先检查当前工作区状态，然后从“黑胶带区域上的 red/green laser 稳定性调试”开始，按测试驱动方式推进。如果需要修改 detector，请先补失败测试或最小复现，再做最小实现修改。
```

## 11. 注意事项

- 当前仓库有未提交和未跟踪文件，不要随意清理或回退。
- `conda` 环境使用 `opencv-learning`。
- 多个 `conda run` 并行时可能不稳定，尽量顺序执行。
- 当前项目里 `preview_vision_video.py` 已支持 `--path-mode debug` 与 `--path-mode runtime`，调试阶段优先用 `debug` 路径收集证据。
