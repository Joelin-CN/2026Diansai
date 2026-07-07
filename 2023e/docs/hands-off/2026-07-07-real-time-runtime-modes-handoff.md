# 2026-07-07 Vision Layer 实时运行模式改造交接文档

## 1. 当前项目位置

当前项目目录：

```text
2023e/
```

本轮工作重点已经从“继续堆 detector 功能”切换为“按题目任务重构正式运行模式，并为 `<= 30 ms/frame` 的 runtime 目标做架构收敛”。

## 2. 已完成背景工作

### 2.1 Vision Layer 第一阶段基础能力

当前已存在正式模块：

```text
src/vision/types.py
src/vision/config.py
src/vision/laser_detector.py
src/vision/state_tracker.py
src/vision/diagnostics.py
src/vision/pipeline.py
src/vision/quad_utils.py
src/vision/tape_quad_detector.py
src/vision/screen_detector.py
```

### 2.2 A4 detector / screen detector 已正式迁移

已确认：

- A4 黑胶带检测已迁移到 `src/vision/tape_quad_detector.py`
- `found=True` 只有在 `outer_quad` 和 `inner_quad` 同时存在时成立
- `screen_detector.py` 已正式存在
- 当前视频中 `screen_square=False` 是正常现象

### 2.3 激光检测近期修改

已完成一轮针对“黑胶带上光斑中心发白、外圈着色”的 detector 改造：

- 在 `src/vision/laser_detector.py` 中加入 bright-core + color-ring 组合候选
- 保持 Vision Layer 边界不变
- 增强了 `LaserDetection.diagnostics`

新增设计与计划文档：

```text
docs/superpowers/specs/2026-07-07-laser-core-ring-detection-design.md
docs/superpowers/plans/2026-07-07-laser-core-ring-detection.md
```

验证结果：

```text
tests/vision/test_laser_detector.py -> 20 passed
tests/vision -> 41 passed
tests/scripts -> 1 passed
```

## 3. 新发现的问题

视频 smoke test 显示：

- 大约 `frame 1..40`，检测还能接受
- 从 `frame 41` 左右开始，进入黑胶带区域后：
  - 红/绿 strict 检测明显退化
  - relaxed fallback 候选数量和误候选问题变重
  - 单帧时间从 `55..115 ms` 激增到 `500..700 ms`

已根据证据判断：

```text
主要实时性问题不是 A4 detector 本身，
而是激光 strict 失败后，preview/debug 风格的 relaxed 搜索在大范围图像内生成了过多候选，
导致检测耗时和稳定性一起崩掉。
```

因此，当前问题已经不是“继续调阈值”，而是“runtime 架构与实时目标不匹配”。

## 4. 题目驱动的运行模式结论

已重新读取题目 PDF：

```text
2023e/docs/E_运动目标控制与自动追踪系统.pdf
```

从题目要求归纳，正式运行模式不应按 detector 任意组合，而应按任务拆成四个 formal runtime mode：

### 4.1 RUNTIME_SCREEN_RED

对应：

- 基本要求（1）红点复位到原点
- 基本要求（2）红点沿屏幕中心 0.5m 灰色铅笔方框运动

实时主链：

- red laser 高频
- gray square 只作为初始化/低频支持

### 4.2 RUNTIME_A4_RED

对应：

- 基本要求（3）红点沿 A4 黑胶带运动
- 基本要求（4）旋转 A4 后红点沿黑胶带运动

实时主链：

- red laser 高频
- A4 detector 初始化/低频支持

### 4.3 RUNTIME_TRACK_RED_GREEN

对应：

- 发挥部分（1）绿色光斑在 2 秒内追上红色光斑

实时主链：

- red laser 高频
- green laser 高频
- 不需要 runtime A4 detector

### 4.4 RUNTIME_A4_TRACK_RED_GREEN

对应：

- 发挥部分（2）红点重复 A4 任务，同时绿点自动追踪红点

实时主链：

- red laser 高频
- green laser 高频
- A4 detector 初始化/低频支持

### 4.5 DEBUG

仅用于：

- preview
- diagnostics
- strict/relaxed 对比
- overlay 验收

它不是 formal runtime path，不应该承载 `<= 30 ms/frame` 目标。

## 5. 当前设计原则

### 5.1 30ms 目标只约束 formal runtime

已确认：

```text
<= 30 ms/frame 的目标约束的是正式 runtime path，
不是 preview/debug。
```

### 5.2 高低频职责分离

Runtime 里应该区分：

高频：

- red laser detect
- green laser detect
- tracker ROI update

低频或初始化：

- A4 tape detect
- gray screen square detect

### 5.3 不允许 runtime 走 preview 风格全图 relaxed fallback

Runtime 应该只有：

```text
TRACKING -> 小 ROI
REACQUIRE -> 扩大但受限 ROI
```

而不是：

```text
strict 失败 -> relaxed 全图搜索
```

## 6. 下一阶段实施计划

已新增详细计划：

```text
docs/superpowers/plans/2026-07-07-real-time-vision-runtime-modes.md
```

该计划核心内容：

1. 把 `VisionMode` 和 `VisionPipeline` 改成四个题目驱动 runtime mode
2. 把高频激光链和低频边框链拆开
3. 加 runtime bounded reacquire，去掉 runtime 对全图 relaxed fallback 的依赖
4. 保留 DEBUG/preview 作为非实时分析路径
5. 增加阶段耗时指标，为 `<= 30 ms/frame` 提供证据

## 7. 新线程建议读取文件

新线程开始前建议先读：

```text
2023e/README.md
2023e/docs/hands-off/2026-07-07-vision-border-migration-handoff.md
2023e/docs/hands-off/2026-07-07-real-time-runtime-modes-handoff.md
2023e/docs/superpowers/specs/2026-07-07-laser-core-ring-detection-design.md
2023e/docs/superpowers/plans/2026-07-07-real-time-vision-runtime-modes.md
2023e/src/vision/types.py
2023e/src/vision/pipeline.py
2023e/src/vision/state_tracker.py
2023e/src/vision/laser_detector.py
2023e/scripts/test/preview_vision_video.py
2023e/tests/vision/test_pipeline.py
2023e/tests/vision/test_state_tracker.py
```

## 8. 下一线程的工作目标

下一线程建议直接从 `real-time-vision-runtime-modes` 计划开始，目标不是继续增加 detector 功能，而是：

- 先把 formal runtime modes 改对
- 再把 runtime hot path 缩短
- 再把 runtime 对 preview/debug 重逻辑解耦
- 最后才继续针对 `<= 30 ms/frame` 做数据驱动优化

## 9. 注意事项

- 当前仓库仍有未提交更改和生成文件，不要随意清理或回退
- 多个 `conda run` 并行仍可能触发临时文件冲突，顺序执行更稳
- PDF 在当前模型 `read` 路径中不可直接抽取文本；本轮已通过 `E:\Softwares\Anaconda\python.exe` + `pdfplumber` 读取题目
- 继续工作时应保持 Vision Layer 边界，不引入 `ScreenPoint` / `MotorCommand`
