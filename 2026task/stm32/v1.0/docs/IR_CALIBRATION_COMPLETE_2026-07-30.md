# IR传感器校准完成报告 - v1.2.2

**日期**: 2026-07-30  
**会话目标**: IR传感器黑白阈值校准  
**状态**: ✅ 完成

---

## 📊 校准参数（最终值）

### 实测数据
| 参数 | 值 | 说明 |
|------|-----|------|
| **传感器类型** | 反向传感器 | 黑色=高ADC值，白色=低ADC值 |
| **白色区域ADC** | 281.6 | Ch0,1,2,5,6,7平均值 |
| **黑色区域ADC** | 1136.1 | Ch3,4平均值 |
| **对比度** | 854.6 | black_avg - white_avg |

### 配置参数（已写入config.c）

```c
// modules/Sens-Decision/src/config.c

// 白色参考值（实际是黑色基准，反向传感器）
white_reference[0..7] = 1136.0f

// 黑线强度阈值（对比度的50%）
black_strength_threshold = 427.0f
```

---

## 🔧 修改的文件

### 1. Core/Src/app/ir_calibration.c
- **新增**: `IrCalibration_OneStep()` - 一步校准函数
- **功能**: 自动检测传感器类型（正向/反向）
- **优化**: 串口输出延迟，防止缓冲区溢出
- **阈值**: 反向传感器使用50%，正向传感器使用60%

### 2. Core/Inc/app/ir_calibration.h
- **新增**: `IrCalibration_OneStep()` 函数声明

### 3. Core/Src/freertos.c
- **修改**: TEST_MODE_IR_CALIBRATION 改用一步校准流程
- **简化**: 从5步简化到4步
- **优化**: 移除ASCII装饰，减少输出量80%

### 4. modules/Sens-Decision/src/config.c ✅ 已更新
- **white_reference**: 270.0f → **1136.0f**
- **black_strength_threshold**: 50.0f → **427.0f**

---

## 📝 校准流程回顾

### 问题1: UART缓冲区溢出
**现象**: 串口输出截断、重复、丢失
```
[INFO] Monitoring co       ← 截断
╔═════                     ← 边框截断
```

**解决**: 
- 移除所有ASCII装饰（`╔═╗`、Emoji）
- 每个printf后添加20-100ms延迟
- 输出量减少80%

### 问题2: 传感器逻辑反向
**现象**: 校准失败，Strength = 0
```
White avg: 281.9
Black avg: 1134.9  ← 黑色反而更高
Strength:  0.0
```

**解决**:
- 自动检测: `sensor_inverted = (black_avg > white_avg)`
- 反向传感器: `white_reference = black_avg`
- 强度计算正确: `1136 - 282 = 854`

### 问题3: 阈值设置不当
**现象**: 误判白色为黑线，A:6 CROSS
```
[0] 264 283 288 1043 1232 326 273 256 | A:6 E:+0.00 CROSS
```

**解决**:
- 反向传感器阈值从60%降到50%
- 512.7 → 427.3
- 预期: A:2（只有Ch3,4激活）

---

## ✅ 验证清单

已完成：
- [x] 编译成功（Flash: 59252 B, 5.65%）
- [x] 串口输出完整无截断
- [x] 传感器类型自动识别
- [x] 校准参数已写入config.c
- [x] 阈值优化到50%

待用户验证（下次会话）：
- [ ] 重新烧录固件
- [ ] 监控输出 A:2 或 A:3（不是A:6）
- [ ] 无误触发CROSS
- [ ] lateral_error符号验证（右移→正，左移→负）

---

## 🎯 下一步工作

### 本次会话目标已完成 ✅
- 校准阈值区分黑白
- 参数已固化到config.c
- 不修改循迹逻辑

### 后续会话任务
1. **验证lateral_error符号**
   - 手动左右移动小车
   - 确认符号正确（右+左-）
   
2. **切换到循迹模式**
   - 修改freertos.c:65-66
   - 注释TEST_MODE_IR_CALIBRATION
   - 取消注释TEST_MODE_TRACK_CONTROL

3. **PID参数调优**
   - 使用循迹测试模式
   - 调整Kp, Ki, Kd

---

## 📂 文档清单

本次会话创建的文档：
1. `docs/COMPILE_SUCCESS_2026-07-30.md` - 首次编译验证
2. `docs/UART_BUFFER_FIX_2026-07-30.md` - 串口溢出修复
3. `docs/IR_SENSOR_INVERSION_FIX_2026-07-30.md` - 反向传感器修复
4. `docs/IR_THRESHOLD_FIX_2026-07-30.md` - 阈值优化
5. `docs/IR_CALIBRATION_COMPLETE_2026-07-30.md` - 本文档

---

## 🔬 技术总结

### 反向传感器原理
```
IR发射器 → 表面 → IR接收器 → 电路 → ADC

白色（高反射）→ 强光 → 某种反相/编码 → 低ADC值 (~280)
黑色（低反射）→ 弱光 → 某种反相/编码 → 高ADC值 (~1136)
```

### 强度计算（统一公式）
```c
strength = white_reference - raw_value

正向传感器:
  white_ref = 高值(白色基准)
  白色: strength = 270 - 270 = 0
  黑色: strength = 270 - 100 = 170 ✓

反向传感器:
  white_ref = 高值(黑色基准)
  白色: strength = 1136 - 282 = 854 ✓
  黑色: strength = 1136 - 1136 = 0
```

### 阈值策略
| 传感器类型 | 比例 | 阈值 | 激活条件 |
|-----------|------|------|---------|
| 正向 | 60% | high | strength > 阈值 |
| 反向 | 50% | medium | strength < 阈值 |

---

## 💡 经验教训

1. **UART输出优化**
   - 装饰性字符占用大量带宽
   - 延迟是必要的（20-100ms）
   - Less is more

2. **传感器多样性**
   - 不能假设传感器逻辑（正向/反向）
   - 自动检测 > 手动配置
   - 软件适配 > 硬件限制

3. **阈值设计**
   - 没有万能的百分比
   - 需要根据传感器特性调整
   - 实测 > 理论

---

**报告创建**: 2026-07-30 21:45  
**编译状态**: ✅ 成功  
**烧录文件**: `cmake-build-debug/v1.0_freeRTOS.elf`  
**项目版本**: v1.2.2  
**下次会话**: lateral_error验证 + 循迹模式切换
