# 2026-07-08 White-Core Blob Dual-Pathway 交接文档

## 1. 当前项目位置

当前工作目录：

```text
2023e/
```

本轮继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案，仍处于隔离实验阶段。

当前实验目标已经从“white-core local red/green correlation”暂时收敛为：

```text
先稳定找到 white-core candidate，
确保每帧候选列表里尽量包含红绿两个真实 laser 中心，
暂不讨论红绿颜色区分。
```

## 2. 目录规范和边界

继续遵守当前项目目录职责：

- `scripts/test/`: 放隔离实验脚本和调试脚本。
- `tests/scripts/`: 放脚本级测试。
- `outputs/`: 放脚本生成的图片、CSV、TXT 证据。
- `docs/hands-off/`: 放交接文档。
- `docs/superpowers/specs/`: 放设计文档。
- `docs/superpowers/plans/`: 放实施计划。

本轮仍然不修改正式 Vision Layer 行为：

- 没有接入 `VisionPipeline`。
- 没有修改 formal runtime 主链。
- 没有把 blob white-core 方案接入 `src/vision`。
- 没有重新引入 full-frame relaxed fallback。
- 当前所有 white-core / blob 逻辑仍只属于隔离脚本和 DEBUG 采证。

注意：当前 worktree 很脏，有无关改动、输出目录、pycache、`.superpowers/sdd` 等，不要清理、回退或重置无关文件。

## 3. 背景结论

上一轮 `findContours` 版本效果较差，主要问题：

- 红色 laser 打在黑色胶带上时 white-core 极小，肉眼估计只有约 3 px 甚至更小。
- `findContours` + 形态学操作对这种极小 blob 不稳。
- 3x3 `MORPH_OPEN` 可能直接把小 laser core 腐蚀掉。
- 旧版 color correlation 在错误 candidate 上算红绿分数，导致后续 red/green 判断没有意义。

本轮通过 `scripts/test/diagnose_white_core.py` 验证：

- 去掉形态学后，小点保留更多。
- `SimpleBlobDetector` 比 `findContours` 更适合当前的极小 white-core blob。
- 用户人工观察认为 blob 方案相对上一版有明显进步。

## 4. 当前隔离脚本状态

核心脚本：

```text
scripts/test/isolated_white_core_correlation.py
```

当前脚本已经从“red/green correlation”临时改成：

```text
dual-pathway white-core blob candidate detection
```

当前不再计算 `red_score` / `green_score`，也不做 red/green 分类。

### 4.1 双通路 white-core blob

当前两条通路：

```text
HIGH: S <= s_high, V >= v_high
默认: S <= 80, V >= 180

LOW:  S <= s_low, V >= v_low
默认: S <= 100, V >= 120
```

每条通路分别：

- HSV threshold 得到 white-ish mask。
- 不做形态学操作。
- 使用 `cv2.SimpleBlobDetector` 找 blob。
- `minArea = 1.0`，`maxArea = 300.0`。
- `minDistBetweenBlobs = 2.0`。
- 不过滤 circularity / convexity / inertia。

之后合并两路候选：

```text
MERGE_RADIUS_PX = 8.0
```

8 px 内认为是同一个 candidate，保留 brightness 更高的候选。

候选按 `brightness` 降序排序。

### 4.2 输出图片

当前每帧输出两张图：

```text
frame_XXXX_orig.png
frame_XXXX_annot.png
```

含义：

- `frame_XXXX_orig.png`: 原始帧，无标注。
- `frame_XXXX_annot.png`: 标注候选帧。

标注颜色：

- 黄色圈: HIGH 通路候选。
- 橙色圈: LOW 通路候选。

标注文字：

```text
idx B=<brightness> [high/low]
```

当前输出目录：

```text
outputs/white_core_blob_v3/
```

该目录在最后一次运行前已清空，当前应包含：

- 20 张 `*_orig.png`
- 20 张 `*_annot.png`
- `summary.csv`
- `summary.txt`

## 5. 最后一次运行结果

最后一次运行命令：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 20 --seed 7 --output-dir "outputs\white_core_blob_v3"
```

运行结果：

```text
Pathway HIGH: V>=180, S<=80
Pathway LOW:  V>=120, S<=100
Output: outputs\white_core_blob_v3
Sampled: 20  Failed: 0
Pathway HIGH candidates: 72  LOW: 404  Merged: 476
```

抽样帧：

```text
[80, 91, 101, 113, 116, 130, 138, 196, 261, 288, 373, 416, 446, 470, 486, 561, 590, 638, 708, 882]
```

用户当前需要人工查看 `outputs/white_core_blob_v3/`：

- 对比 `frame_XXXX_orig.png` 和 `frame_XXXX_annot.png`。
- 判断黄色 HIGH 候选是否覆盖绿色 laser 中心。
- 判断橙色 LOW 候选是否能覆盖很小的红色 laser 中心。
- 判断 LOW 通路噪声是否过多，是否需要调 `--v-low` / `--s-low`。

## 6. 当前重要观察

用户已经观察到：

- 绿色候选效果较好，大体能落在绿色 laser core 上。
- 红色 relaxed GT 大圈是不可靠的，之前拿它做红色验证是错误方向。
- 红色 laser 中心确实非常小，尤其在黑色胶带上。
- 当前目标不是红绿区分，而是 candidate 列表里能包含两个 laser 中心。
- 现场最终会脱机调阈值，所以脚本参数需要保持可调。

因此下一步应优先围绕以下问题人工验收：

- LOW 通路默认 `V>=120, S<=100` 是否已经把红色 laser core 纳入候选。
- 如果没有，尝试降低 `--v-low`，例如 100、90、80。
- 如果噪声太多，尝试降低 `--s-low` 或提高 `--v-low`。
- 暂时不要重新引入 red/green color scoring。

## 7. 可调命令

默认双通路：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 20 --seed 7 --output-dir "outputs\white_core_blob_v3"
```

更低 LOW 阈值，尝试抓更暗红点：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 20 --seed 7 --output-dir "outputs\white_core_blob_v3" --v-low 100 --s-low 100
```

更激进 LOW 阈值：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 20 --seed 7 --output-dir "outputs\white_core_blob_v3" --v-low 80 --s-low 100
```

降低 HIGH 阈值：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_white_core_correlation.py" --start-frame 41 --sample-count 20 --seed 7 --output-dir "outputs\white_core_blob_v3" --v-high 160 --s-high 90 --v-low 100 --s-low 100
```

清空输出目录时注意只清当前实验目录，不要清整个 `outputs/`：

```powershell
python -c "from pathlib import Path; d = Path('outputs/white_core_blob_v3'); [f.unlink() for f in d.glob('*')] if d.exists() else None"
```

## 8. 当前脚本注意事项

`scripts/test/isolated_white_core_correlation.py` 当前仍保留了一些历史 import，例如 `replace`、`HueRangeDeg`、`load_vision_config`、`LaserDetector`、`LaserDetection` 等，当前双通路版本已经不依赖 GT，这些 import 可以后续清理，但不是当前核心问题。

`tests/scripts/test_isolated_white_core_correlation.py` 仍可能覆盖旧的 correlation/classify 行为，当前脚本已临时转向 blob candidate 验证，测试可能需要后续同步更新。当前阶段优先人工验收输出图片。

`scripts/test/diagnose_white_core.py` 是诊断脚本，当前可用于查看：

- original
- white mask
- contour vs blob
- pixel zoom

但主验证输出以 `scripts/test/isolated_white_core_correlation.py` 为准。

## 9. 下一步建议

建议下一线程先做人工验收，不要立刻接入 Vision Layer：

1. 查看 `outputs/white_core_blob_v3/frame_XXXX_orig.png` 和 `frame_XXXX_annot.png`。
2. 判断默认 LOW 通路是否覆盖红色 laser core。
3. 如果红色仍漏检，按 `--v-low 100`、`--v-low 80` 逐步调低。
4. 如果 LOW 通路噪声太多，调小 `--s-low` 或调高 `--v-low`。
5. 只有当候选列表稳定包含红绿两个 laser center，再讨论重新加入 red/green local color correlation。
6. 只有隔离脚本验收稳定后，再考虑接入 `src/vision`，不要提前改 formal runtime。

## 10. 新线程提示词

可以在新线程中直接使用下面这段：

```text
我们继续 2023 年电赛 E 题“运动目标控制与自动追踪系统”的 OpenCV 方案。当前仍然只做隔离实验，不修改正式 Vision Layer。

请先读取以下文件：
1. README.md
2. docs/hands-off/2026-07-07-white-core-reacquire-handoff.md
3. docs/hands-off/2026-07-08-isolated-white-core-correlation-handoff.md
4. docs/hands-off/2026-07-08-white-core-blob-dual-pathway-handoff.md
5. scripts/test/isolated_white_core_correlation.py
6. scripts/test/diagnose_white_core.py
7. outputs/white_core_blob_v3/summary.txt
8. src/vision/laser_detector.py
9. scripts/test/preview_vision_video.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. formal runtime mode 已改成 4 个题目驱动模式：RUNTIME_SCREEN_RED、RUNTIME_A4_RED、RUNTIME_TRACK_RED_GREEN、RUNTIME_A4_TRACK_RED_GREEN。
3. DEBUG / preview 不属于 <= 30 ms/frame 的正式 runtime 目标。
4. formal runtime 中不允许重新引入 strict 失败后全图 relaxed fallback。
5. white-core-first 当前只做隔离实验和 DEBUG 采证，不直接替换 formal runtime 主链。
6. 当前目标不是红绿区分，而是先让每帧 candidate 列表尽量包含红绿两个 laser 中心。
7. findContours + 形态学对黑色胶带上的极小红色 laser core 不稳，SimpleBlobDetector 明显更合适。
8. scripts/test/isolated_white_core_correlation.py 当前已改成 dual-pathway blob candidate 脚本：HIGH 默认 V>=180/S<=80，LOW 默认 V>=120/S<=100，不做形态学，8 px 去重合并。
9. 当前输出目录是 outputs/white_core_blob_v3/，每帧有 frame_XXXX_orig.png 和 frame_XXXX_annot.png，最后一次统计 HIGH=72、LOW=404、总候选=476，failed frames 为 0。
10. 用户观察：绿色候选效果较好，红色 relaxed GT 不可靠，红色 laser core 很小；后续现场会脱机调阈值，所以脚本参数要保持可调。
11. 当前 worktree 很脏，有无关改动、pycache、outputs 和 .superpowers/sdd，不要清理、回退或重置无关文件。

下一步请先人工/脚本辅助检查 outputs/white_core_blob_v3/ 的 orig/annot 成对图片，判断 LOW 通路橙色候选是否包含红色 laser core。若要修改，请继续保持隔离脚本优先，不要直接改 formal Vision Layer。优先使用：
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" ...
避免使用 conda run，以免触发临时文件冲突。
```
