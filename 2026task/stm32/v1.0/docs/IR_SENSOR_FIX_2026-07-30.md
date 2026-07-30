# 红外传感器黑线检测算法修复报告

**日期:** 2026-07-30  
**作者:** joelin-CN  
**版本:** 1.0  
**状态:** ✅ 已修复

---

## 执行摘要

红外传感器黑线检测算法存在**严重的阈值逻辑错误**，导致无法正确检测黑线。本次修复采用**黑线强度反转算法**，从根本上解决问题，并添加了白平衡校准工具。

**核心修复:**
- ❌ **修复前:** 阈值 `raw > 0.5` 永远为真 → 所有通道永远激活 → 误判为"路口"
- ✅ **修复后:** 黑线强度 = 白色参考 - 原始值 → 物理意义明确 → 正确检测

---

## 1. 问题诊断

### 1.1 传感器数据特征

| 表面类型 | 原始ADC值 | 物理含义 |
|---------|----------|---------|
| 白色背景 | 250-270  | 高反射率 |
| 黑色线条 | ~100     | 低反射率 |
| **规律** | **黑线 = 数值降低** | 黑色吸收光线 |

### 1.2 核心问题1：阈值判断完全失效

**错误代码** (`perception.c:53`):
```c
if (v > 0.5f) {  // ❌ 致命错误！
    ++active_count;
}
```

**问题分析:**
- `v` 是原始ADC值（0-4095范围），从 `sensor_adapter.c:78` 传入
- 阈值 `0.5f` 远小于任何实际传感器值
- **结果:** 所有传感器永远激活 (`active_count = 8`)
- **后果:** 系统一直判定为"路口"，无法跟踪黑线

**数据验证:**
```
白色: 270 > 0.5  ✓ 激活（错误！）
黑线: 100 > 0.5  ✓ 激活（错误！）
→ 无法区分黑线和白色
```

### 1.3 核心问题2：质心计算被白色主导

**错误代码** (`perception.c:52`):
```c
weighted_sum += weight * v;  // ❌ 直接使用原始值
```

**问题分析:**
```
白色传感器贡献: weight × 270
黑线传感器贡献: weight × 100

白色权重 = 270/100 = 2.7倍黑线权重
→ 质心被白色背景拉偏
→ 黑线信号被压制
```

### 1.4 核心问题3：缺少白平衡校准

- 没有记录"白色参考值"
- 固定阈值无法适应光照变化
- 传感器个体差异无法补偿

---

## 2. 修复方案

### 2.1 方案对比

| 方案 | 描述 | 优点 | 缺点 |
|-----|------|-----|------|
| **A: 简单阈值反转** | 将 `>` 改为 `<`，降低阈值 | 简单快速 | 固定阈值不适应光照 |
| **B: 黑线强度反转** ⭐ | 计算 `black_strength = white_ref - raw` | 物理意义明确，支持校准 | 需要校准工具 |
| **C: 自适应阈值** | 动态计算阈值 | 完全自适应 | 全黑/全白时可能失效 |

**选择:** 方案B（推荐）- 平衡了准确性和鲁棒性

### 2.2 黑线强度反转算法

**核心思想:**
```
black_strength[i] = white_reference[i] - current_value[i]

白色区域: black_strength ≈ 0   (270 - 270 = 0)
黑线区域: black_strength ≈ 170 (270 - 100 = 170)
```

**优势:**
1. **物理意义明确:** 黑线强度 = 黑色程度
2. **白色抑制:** 白色区域强度接近0，不参与质心计算
3. **黑线增强:** 黑线强度高，主导质心计算
4. **支持校准:** 可适应不同光照和传感器差异

---

## 3. 修复实施

### 3.1 配置结构修改

**文件:** `modules/Sens-Decision/inc/config.h`

```c
typedef struct {
    // ... 现有字段 ...
    float white_reference[SD_IR_CHANNEL_COUNT];  // 白色背景参考值
    float black_strength_threshold;              // 黑线强度阈值
} sd_perception_config_t;
```

### 3.2 默认配置初始化

**文件:** `modules/Sens-Decision/src/config.c`

```c
// 白色参考值（初始估计，建议校准）
for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
    g_sens_decision_config.perception.white_reference[index] = 270.0f;
}

// 黑线强度阈值（保守值）
g_sens_decision_config.perception.black_strength_threshold = 50.0f;
```

### 3.3 感知算法核心修复

**文件:** `modules/Sens-Decision/src/perception.c`

**修复前:**
```c
for (i = 0; i < 8; ++i) {
    float v = ir_data->values[i];
    weighted_sum += weight * v;           // ❌ 白色主导
    if (v > 0.5f) {                       // ❌ 阈值错误
        ++active_count;
    }
}
```

**修复后:**
```c
// 步骤1: 计算黑线强度
for (i = 0; i < 8; ++i) {
    float raw_value = ir_data->values[i];
    float white_ref = config->white_reference[i];
    black_strength[i] = white_ref - raw_value;  // ✅ 反转逻辑
    if (black_strength[i] < 0.0f) {
        black_strength[i] = 0.0f;  // 防止负值
    }
}

// 步骤2: 黑线检测和质心计算
for (i = 0; i < 8; ++i) {
    float strength = black_strength[i];
    
    if (strength > config->black_strength_threshold) {  // ✅ 正确阈值
        ++active_count;
        weighted_sum += weight * strength;  // ✅ 黑线主导
        strength_sum += strength;
    }
}

// 步骤3: 质心归一化
if (strength_sum > 1e-6f) {
    lateral_error = weighted_sum / strength_sum;  // ✅ 物理意义明确
}
```

### 3.4 配置验证增强

**文件:** `modules/Sens-Decision/src/config.c`

```c
// 验证白色参考值和阈值
for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
    if (!positive_finite(config->perception.white_reference[index])) {
        return SD_ERR_INVALID_ARGUMENT;
    }
}
if (!positive_finite(config->perception.black_strength_threshold)) {
    return SD_ERR_INVALID_ARGUMENT;
}
```

---

## 4. 校准工具

### 4.1 白平衡校准

**文件:** `Core/Inc/app/ir_calibration.h`, `Core/Src/app/ir_calibration.c`

**功能:** 自动测量白色背景的传感器值

**使用方法:**
```c
// 1. 将小车放在纯白色表面
// 2. 调用校准函数
IrCalibration_WhiteBalance();

// 输出示例:
// [INFO] White reference values:
//   Channel:  0      1      2      3      4      5      6      7
//   Value:    268.3  271.5  269.2  270.8  267.9  272.1  268.5  271.0
// [SUCCESS] White balance calibration complete!
```

**实现细节:**
- 稳定等待1秒
- 采集100次样本
- 计算平均值
- 自动更新配置

### 4.2 黑线强度阈值校准

**功能:** 自动测量黑线强度并设置阈值

**使用方法:**
```c
// 1. 确保已完成白平衡校准
// 2. 将小车居中对准黑线
// 3. 调用校准函数
IrCalibration_BlackThreshold();

// 输出示例:
// [INFO] Maximum black strength: 172.5
// [INFO] Threshold set to: 86.3 (50% of max)
// [SUCCESS] Black threshold calibration complete!
```

### 4.3 实时监控工具

**功能:** 实时显示传感器状态，用于调试

**使用方法:**
```c
// 监控5秒，每100ms采样一次
IrCalibration_Monitor(5000, 100);

// 输出示例:
// [1200 ms] Sample #12:
//   Raw:      270  268  105   98  102  272  269  271
//   Strength:   0    2  165  172  168    0    1    0
//   Active channels: 3/8
//   Lateral error: -2.847
//   Status: LINE TRACKING
```

---

## 5. 测试验证计划

### 5.1 单元测试场景

| 测试场景 | 预期原始值 | 预期黑线强度 | 预期激活通道 | 预期横向偏差 |
|---------|----------|-------------|------------|------------|
| **白色表面** | 全部 ~270 | 全部 ~0 | 0 | N/A (LINE_LOST) |
| **居中对准黑线** | 中间 ~100 | 中间 ~170 | 2-3 (中间) | ≈ 0 |
| **车体偏左** | 右侧 ~100 | 右侧 ~170 | 2-3 (右侧) | < 0 (负值) |
| **车体偏右** | 左侧 ~100 | 左侧 ~170 | 2-3 (左侧) | > 0 (正值) |
| **路口（全黑）** | 全部 ~100 | 全部 ~170 | ≥6 | N/A (INTERSECTION) |

### 5.2 集成测试步骤

**步骤1: 校准**
```c
// 1. 白平衡校准
IrCalibration_WhiteBalance();

// 2. 黑线阈值校准
IrCalibration_BlackThreshold();

// 3. 验证配置
IrCalibration_PrintConfig();
```

**步骤2: 静态验证**
```c
// 手动移动小车，观察横向偏差符号
// - 向右移动 → lateral_error 应为负
// - 向左移动 → lateral_error 应为正
```

**步骤3: 动态验证**
```c
// 启动循迹模式，观察：
// 1. 直线跟踪是否稳定
// 2. 横向偏差是否合理（-4 ~ +4范围）
// 3. 路口检测是否触发（6通道以上激活）
// 4. 丢线检测是否正常（0通道激活）
```

### 5.3 验证通过标准

- ✅ 白色表面：`active_channels = 0`，触发 `LINE_LOST`
- ✅ 居中对准：`lateral_error ≈ 0 ± 0.5`
- ✅ 偏左/偏右：`lateral_error` 符号正确
- ✅ 路口：`active_channels ≥ 6`，触发 `INTERSECTION`
- ✅ 循迹稳定：无异常振荡

---

## 6. 调试指南

### 6.1 问题：白色也被识别为黑线

**症状:** `active_channels` 过高

**诊断:**
```c
IrCalibration_Monitor(3000, 100);
// 检查白色区域的 black_strength 是否 > 阈值
```

**解决方案:**
1. 重新执行白平衡校准（光照可能变化）
2. 提高 `black_strength_threshold`（降低灵敏度）

### 6.2 问题：黑线检测不到

**症状:** `active_channels = 0` 或过低

**诊断:**
```c
IrCalibration_Monitor(3000, 100);
// 检查黑线区域的 black_strength 是否 < 阈值
```

**解决方案:**
1. 确认已完成白平衡校准
2. 降低 `black_strength_threshold`（提高灵敏度）
3. 检查黑线对比度（可能黑线不够黑）

### 6.3 问题：横向偏差符号错误

**症状:** 向右移动但 `lateral_error > 0`（应为负）

**诊断:**
```c
// 手动测试权重符号
// 右侧传感器激活应产生负偏差
```

**解决方案:**
1. 检查 `ir_weights[]` 符号是否正确
2. 参考 `config.c` 中的权重验证方法
3. 可能需要将所有权重取反

---

## 7. 性能影响分析

### 7.1 计算复杂度

**修复前:**
```
1次循环: 8次乘法 + 8次加法 + 8次比较
```

**修复后:**
```
2次循环: 8次减法 + 16次乘法 + 16次加法 + 16次比较
```

**增加量:** ~2倍计算量  
**影响:** 可忽略（仍在微秒级）

### 7.2 内存占用

**新增数据:**
- `white_reference[8]`: 32 bytes
- `black_strength_threshold`: 4 bytes
- `black_strength[8]` (临时): 32 bytes (栈)

**总增量:** 68 bytes  
**影响:** 可忽略

---

## 8. 未来优化方向

### 8.1 自适应阈值（可选）

```c
// 动态计算阈值（无需手动校准）
float mean = 0.0f, max_val = 0.0f;
for (i = 0; i < 8; i++) {
    mean += raw[i];
    if (raw[i] > max_val) max_val = raw[i];
}
mean /= 8.0f;
float adaptive_threshold = (mean + max_val) / 2.0f;
```

**优势:** 完全自适应  
**风险:** 全黑/全白时可能失效

### 8.2 传感器个体校准

```c
// 为每个通道设置独立阈值
float black_strength_threshold[8];
```

**优势:** 补偿传感器差异  
**复杂度:** 增加

### 8.3 滤波平滑

```c
// 对黑线强度进行低通滤波
black_strength_filtered[i] = alpha * black_strength_filtered[i] + 
                             (1 - alpha) * black_strength[i];
```

**优势:** 减少噪声  
**风险:** 增加延迟

---

## 9. 相关文件清单

### 9.1 修改的文件

| 文件路径 | 修改内容 | 行数变化 |
|---------|---------|---------|
| `modules/Sens-Decision/inc/config.h` | 添加白色参考值和阈值字段 | +2 |
| `modules/Sens-Decision/src/config.c` | 初始化新配置参数，添加验证 | +70 |
| `modules/Sens-Decision/src/perception.c` | 完全重写感知算法 | +60 |

### 9.2 新增的文件

| 文件路径 | 功能 | 代码行数 |
|---------|------|---------|
| `Core/Inc/app/ir_calibration.h` | 校准工具头文件 | ~70 |
| `Core/Src/app/ir_calibration.c` | 校准工具实现 | ~250 |
| `docs/IR_SENSOR_FIX_2026-07-30.md` | 本修复报告 | ~600 |

---

## 10. 结论

### 10.1 修复总结

本次修复从根本上解决了红外传感器黑线检测失效的问题：

1. **阈值逻辑修正:** 从错误的"原始值 > 0.5"改为正确的"黑线强度 > 阈值"
2. **质心算法修正:** 从白色主导改为黑线强度主导
3. **增加校准支持:** 提供白平衡和阈值校准工具
4. **物理意义明确:** 黑线强度 = 白色参考 - 原始值

### 10.2 预期效果

- ✅ 正确区分黑线和白色背景
- ✅ 准确计算横向偏差
- ✅ 可靠检测路口和丢线
- ✅ 适应不同光照条件（需校准）

### 10.3 后续行动

1. **立即:** 编译并烧录固件
2. **第一步:** 执行白平衡校准
3. **第二步:** 执行黑线阈值校准
4. **第三步:** 运行实时监控验证
5. **最后:** 实车循迹测试

---

**修复完成时间:** 2026-07-30  
**状态:** ✅ 代码已修复，待实车验证
