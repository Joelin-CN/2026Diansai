# 2026-07-08 LAB 3-Mask 原型交接文档

## 1. 当前项目位置

当前工作目录：

```text
2023e/
```

本轮从 dual-pathway blob (HSV) 隔离实验转向 LAB 色彩空间 + 3-mask 原型验证。

## 2. 目录规范和边界

继续遵守当前项目目录职责：

- `scripts/test/`: 放隔离实验脚本和调试脚本。
- `tests/scripts/`: 放脚本级测试。
- `outputs/`: 放脚本生成的图片、CSV、TXT 证据。
- `docs/hands-off/`: 放交接文档。
- `docs/superpowers/specs/`: 放设计文档。
- `docs/superpowers/plans/`: 放实施计划。

本轮仍然不修改正式 Vision Layer 行为：

- 不接入 `VisionPipeline`。
- 不修改 formal runtime 主链。
- 不把 LAB / 3-mask 方案接入 `src/vision`。
- 不重新引入 full-frame relaxed fallback。
- 当前所有 white-core / blob 逻辑仍只属于隔离脚本和 DEBUG 采证。

注意：当前 worktree 很脏，有无关改动、输出目录、pycache、`.superpowers/sdd` 等，不要清理、回退或重置无关文件。

## 3. 背景结论

### 3.1 v3 dual-pathway blob (HSV) 效果

绿色 laser 候选覆盖较好，但红色 laser 极少出现在候选列表中。

### 3.2 红色 laser 根因诊断

对 v3 的 20 帧做逐帧 relaxed red detector 对比，5 帧能定位到红色 laser：

| 帧 | 中心 (u,v) | V | S | 所在白色连通域面积 | 连通域质心偏移 | 最近 blob 距离 |
|---|------------|---|---|---------------------|----------------|---------------|
| 50 | (608,431) | 187 | 30 | 10700 px | (46, -35) | 51 px |
| 416 | (571,420) | 186 | 30 | 10744 px | (53, -28) | 92 px |
| 470 | (598,382) | 185 | 30 | 10742 px | (27, 8) | 89 px |
| 486 | (591,371) | 189 | 30 | 10717 px | (21, 31) | 99 px |
| 590 | (613,351) | 185 | 30 | 10717 px | (21, 31) | 88 px |

**根因**：红色 laser 核心像素 V=185-189、S=30，完全满足 LOW 通路条件。但在掩码中和投影屏幕白色背景连通为同一个 ~10700 px 的大区域。SimpleBlobDetector 取全域质心，离真实 laser core 偏移 30-50 px。

本质问题：**不是"不够白找不着"，而是"太白了和背景连成一片"**。

### 3.3 色彩空间分析结论

HSV 对当前场景的局限性：

- V 通道受颜色干扰：纯红和纯绿的 V 值和白色一样高
- H 通道红色跨 0/180 边界
- S 在接近白色时趋于 0，laser core S≈30 与背景 S≈0 差值不足以断开连通

LAB 的优势：

- **L 通道**：纯感知亮度，不被颜色干扰。laser core L 一定高于邻域（局部峰值）
- **A 通道**：绿→红轴。红色 laser halo → A >> 0；绿色 laser halo → A << 0；白屏 → A ≈ 0。**一个通道分离红绿**
- **B 通道**：蓝→黄轴，红绿均不占，可忽略

## 4. 新原型设计

### 4.1 脚本

```text
scripts/test/isolated_lab_3mask.py
```

独立新文件，不修改 `scripts/test/isolated_white_core_correlation.py`。

### 4.2 算法三步

**Step 1 — 白色核心：L 通道局部峰值**

```text
BGR → LAB
L 通道：找局部极大值（半径 5 px 内比所有邻域亮）
每峰过滤条件：
  - L >= 80（排除暗区噪声）
  - |A| <= 10（确认是白色不是有色光晕）
  - |B| <= 10
峰值间非极大值抑制（距离 < min_dist_px 的保留更亮的）
```

注：用局部峰值替代连通域/blob，彻底避开"白色 core 被屏幕背景吞并"的问题。

**Step 2 — 红色光晕：A 通道 blob**

```text
A > 15 → 二值掩码
SimpleBlobDetector（minArea=2, maxArea=300）
得到红色 halo blob 列表
```

**Step 3 — 绿色光晕：A 通道 blob**

```text
A < -15 → 二值掩码
SimpleBlobDetector（minArea=2, maxArea=300）
得到绿色 halo blob 列表
```

**关联**：

```text
白核 peak 和最近的红/绿 halo blob 距离 <= 25 px
  → 关联为 red/green laser candidate
白核 peak 无 halo 关联 → 标记为 unknown white core
halo blob 无白核关联 → 标记为 orphan halo
```

### 4.3 输出内容

| 文件 | 内容 |
|------|------|
| `frame_XXXX_orig.png` | 原始帧 |
| `frame_XXXX_annot.png` | 白色十字=白核, 红色圈=红激光候选, 绿色圈=绿激光候选, 编号 |
| `frame_XXXX_masks.png` | L/A/B 三通道伪彩色并排 + L 峰值标记 + A blob 轮廓 |
| `summary.csv` | 每帧每候选项的坐标、L值、A值、关联类型、距离 |
| `summary.txt` | 总体统计（成功/失败帧数、各类候选数、阈值参数） |

### 4.4 CLI 参数（全部可调）

```text
--video           默认 E:\B306\Visual\openCV\project\videos\2023e.mp4
--start-frame     默认 41
--sample-count    默认 40
--seed            默认 7
--output-dir      默认 outputs/lab_3mask_v1/

--l-peak-radius   L 局部峰值半径 (default 5)
--l-peak-floor    最小 L 值 (default 80)
--l-ab-neutral    |A|/|B| 中性最大值 (default 10)
--a-red-thresh    A 通道红色阈值 (default 15)
--a-green-thresh   A 通道绿色阈值，取负 (default 15)
--halo-blob-min   halo blob 最小面积 (default 2)
--halo-blob-max   halo blob 最大面积 (default 300)
--assoc-radius    白核-halo 关联距离 (default 25)
```

### 4.5 默认运行参数

```text
--start-frame 41 --sample-count 40 --seed 7 --output-dir "outputs\lab_3mask_v1"
```

## 5. 输出目录

默认：

```text
outputs/lab_3mask_v1/
```

清空输出目录时只清当前实验目录：

```powershell
python -c "from pathlib import Path; d = Path('outputs/lab_3mask_v1'); [f.unlink() for f in d.glob('*')] if d.exists() else None"
```

## 6. 当前 worktree 注意事项

- worktree 很脏，不要清理、回退或重置无关文件
- 已有诊断脚本：`scripts/test/_diag_red_core.py`、`scripts/test/_diag_mask.py`
- 已有输出目录：`outputs/white_core_blob_v3/`、`outputs/red_core_diag/`、`outputs/red_core_mask_diag/`
- 不要清理 `src/vision/laser_detector.py` 或 `scripts/test/isolated_white_core_correlation.py`

## 7. 常用命令

跑原型：

```powershell
$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" "scripts/test/isolated_lab_3mask.py" --start-frame 41 --sample-count 40 --seed 7 --output-dir "outputs\lab_3mask_v1"
```

## 8. 新线程提示词

可以在新线程中直接使用下面这段：

```text
我们继续 2023 年电赛 E 题"运动目标控制与自动追踪系统"的 OpenCV 方案。当前仍然只做隔离实验，不修改正式 Vision Layer。

请先读取以下文件：
1. README.md
2. docs/hands-off/2026-07-08-white-core-blob-dual-pathway-handoff.md
3. docs/hands-off/2026-07-08-lab-3mask-handoff.md
4. scripts/test/isolated_white_core_correlation.py

当前已确认：
1. Vision Layer 只输出 ImagePoint / ImageQuad / Detection / diagnostics，不输出 ScreenPoint / ScreenQuad / MotorCommand。
2. formal runtime mode 已改成 4 个题目驱动模式：RUNTIME_SCREEN_RED、RUNTIME_A4_RED、RUNTIME_TRACK_RED_GREEN、RUNTIME_A4_TRACK_RED_GREEN。
3. DEBUG / preview 不属于 <= 30 ms/frame 的正式 runtime 目标。
4. formal runtime 中不允许重新引入 strict 失败后全图 relaxed fallback。
5. white-core-first 当前只做隔离实验和 DEBUG 采证，不直接替换 formal runtime 主链。
6. 当前目标不是红绿区分，而是先让每帧 candidate 列表尽量包含红绿两个 laser 中心。
7. v3 dual-pathway blob (HSV) 已确认：绿色候选覆盖较好，红色 laser 极少覆盖。
8. 红色 laser 根因已诊断：laser core 像素 V≈185/S≈30 完全满足 LOW 通路，但在掩码中和屏幕白色背景连通为 ~10700 px 大区域，blob centroid 偏移 30-50 px。
9. 分析结论：LAB 色彩空间更适合当前场景——L 通道纯亮度不受颜色干扰，A 通道单轴分离红绿。
10. 当前 worktree 很脏，有无关改动、pycache、outputs 和 .superpowers/sdd，不要清理、回退或重置无关文件。

请先读取交接文档 docs/hands-off/2026-07-08-lab-3mask-handoff.md 确认设计细节，然后编写新独立脚本 scripts/test/isolated_lab_3mask.py，实现 LAB 色彩空间 + 3-mask（白色核心、红色光晕、绿色光晕）的激光候选检测原型。

脚本要求：
- 独立新文件，不修改任何现有文件。
- 所有关键阈值通过 CLI 暴露。
- 输出 3 张图：orig.png（原始帧）、annot.png（标注候选）、masks.png（L/A/B 三通道诊断全景）。
- 输出 summary.csv 和 summary.txt。
- 复用 v3 的帧采样、PNG 写入等工具函数（可从 isolated_white_core_correlation.py 复制关键函数）。
- 优先使用：$env:PYTHONPATH="src"; & "E:\Softwares\Anaconda\envs\opencv-learning\python.exe" ...
- 避免使用 conda run。

编写完成后，用默认参数（--start-frame 41 --sample-count 40 --seed 7 --output-dir "outputs\lab_3mask_v1"）跑一遍看结果。
```

(End of file - total 225 lines)
