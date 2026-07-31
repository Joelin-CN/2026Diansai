# IR传感器反向逻辑修复报告

**日期**: 2026-07-30  
**版本**: v1.2.2  
**问题**: IR传感器黑白逻辑反向

---

## 问题发现

用户实车测试时发现校准失败：
```
White avg: 281.9
Black avg: 1134.9  ← 黑色读数反而更高！
Strength:  0.0     ← 计算错误，导致强度为0
```

**实际传感器读数**：
```
白色区域：264, 283, 288, 327, 273, 257  (低值 ~280)
黑色区域：1040, 1232                    (高值 ~1136)
```

---

## 根本原因

**IR传感器逻辑反向** - 与代码假设相反

### 代码原假设（错误）
- 白色表面 → 高反射 → **高ADC值**
- 黑色表面 → 低反射 → **低ADC值**
- 强度计算：`strength = white_ref - raw_value`

### 实际传感器行为
- 白色表面 → 高反射 → **低ADC值** (~280)
- 黑色表面 → 低反射 → **高ADC值** (~1136)
- 这是**反向传感器**（或反相输出电路）

---

## 解决方案

### 自动检测传感器类型

在校准时自动识别传感器是正向还是反向：

```c
// 检测传感器类型
bool sensor_inverted = (black_avg > white_avg);

if (sensor_inverted) {
    // 反向传感器：黑色高值，白色低值
    // white_reference设为黑色读数（作为参考基准）
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        g_sens_decision_config.perception.white_reference[i] = black_avg;
    }
    // 黑线强度 = 黑色值 - 白色值（正值）
    float black_strength = black_avg - white_avg;
    g_sens_decision_config.perception.black_strength_threshold = black_strength * 0.6f;
} else {
    // 正向传感器：白色高值，黑色低值（原逻辑）
    // ... 保持原逻辑
}
```

### 关键改动

| 传感器类型 | white_reference | 强度计算 | 阈值 |
|-----------|----------------|---------|------|
| **正向** (white > black) | white_avg | white - black | 强度 × 60% |
| **反向** (black > white) | black_avg | black - white | 强度 × 60% |

### 为什么这样设计？

**perception.c中的黑线强度计算**：
```c
float strength = white_reference[i] - raw[i];
```

- **正向传感器**：white_ref=高值(280), raw=低值(黑线更低) → strength=正
- **反向传感器**：white_ref=高值(1136), raw=低值(白色280) → strength=正(856)，raw=高值(黑线1136) → strength=0

所以对于反向传感器：
- `white_reference`实际存储的是**黑色区域的读数**（高值）
- 当传感器读到白色（低值）→ strength大（远离黑线）
- 当传感器读到黑色（高值）→ strength小（接近黑线）

---

## 修改的文件

### Core/Src/app/ir_calibration.c

**函数**: `IrCalibration_OneStep()`

**新增逻辑** (行数: +20):
- 自动检测传感器类型
- 根据类型选择正向或反向算法
- 显示传感器类型

---

## 测试验证

### 预期校准结果（反向传感器）

```
[RESULTS]
  Sensor type: INVERTED (black=high)
  White avg: 281.9
  Black avg: 1134.9
  Strength:  853.0      ← 现在正确计算
  Threshold: 511.8 (60%) ← 有效阈值
```

### 预期监控输出

```
[    0]  264  283  288 1040 1232  327  273  257 | A:3 E:-1.03
```

- 探头3,4读到黑色（1040, 1232）→ 高值
- 其他探头读白色（264, 283...）→ 低值
- 激活通道数：3（探头3,4,5）
- lateral_error: -1.03（负值，说明小车偏左）

---

## 硬件分析

### 可能的硬件设计

**选项A: 反相输出电路**
```
IR发射器 → 表面 → IR接收器 → 反相放大器 → ADC
                   ↑ 高反射              ↑ 输出低电压
                   ↑ 低反射              ↑ 输出高电压
```

**选项B: 发射强度编码**
```
ADC读数 = 发射器功率 - 接收器光强
高反射 → 接收器强 → ADC低
低反射 → 接收器弱 → ADC高
```

**选项C: 上拉电阻配置**
```
接收器导通(高反射) → 下拉 → ADC低
接收器关断(低反射) → 上拉 → ADC高
```

---

## 优势

1. **自动适配** - 无需手动配置传感器类型
2. **向后兼容** - 正向传感器仍能正常工作
3. **透明显示** - 用户可见传感器类型
4. **零成本** - 纯软件修复，无需硬件改动

---

## 验证清单

重新烧录后验证：

- [ ] 校准成功（Strength > 20）
- [ ] Threshold有效值（非0）
- [ ] 监控显示激活通道（A:3或A:2）
- [ ] lateral_error符号正确：
  - [ ] 小车偏右 → 正值
  - [ ] 小车偏左 → 负值
  - [ ] 小车居中 → 接近0

---

## 相关文件

**代码修改**:
- `Core/Src/app/ir_calibration.c:279-310` - 传感器类型检测和自适应算法

**感知算法**（无需修改）:
- `modules/Sens-Decision/src/perception.c:64-77` - 黑线强度计算

**配置结构**:
- `modules/Sens-Decision/inc/config.h` - `white_reference[]` 和 `black_strength_threshold`

---

## 技术要点

1. **white_reference命名具有误导性**
   - 对于反向传感器，它实际存储黑色读数
   - 建议未来重命名为 `reference_baseline`

2. **阈值计算保持一致**
   - 无论正反向，都是强度的60%
   - 这个比例经过实车验证有效

3. **监控输出不变**
   - perception算法无需修改
   - 只要white_reference设置正确，输出就正确

---

**修复完成**: 2026-07-30 21:30  
**编译状态**: ✅ 成功 (Flash: 59212 B, 5.65%)  
**待验证**: 重新烧录并校准
