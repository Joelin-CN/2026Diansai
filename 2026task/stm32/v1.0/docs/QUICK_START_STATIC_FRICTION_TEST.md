# 静摩擦测试快速启动指南

## 5分钟上手

### 1. 烧录程序（已完成 ✓）

程序已编译成功：`cmake-build-debug/v1.0_freeRTOS.elf`

使用 STM32CubeProgrammer 或 Keil 烧录到STM32。

### 2. 连接串口

- 波特率：**115200**
- 数据位：8
- 停止位：1
- 校验：无

### 3. 复位STM32

看到以下输出表示成功：

```
╔════════════════════════════════════════════════════════════════╗
║   Static Friction Calibration - FF_K_STATIC Test Mode         ║
╚════════════════════════════════════════════════════════════════╝

[Init] Initializing hardware...
[OK] Hardware ready

========================================
  FF_K_STATIC Calibration Test
  Static Friction Manual Test Mode
========================================

Ready>
```

### 4. 快速测试流程

#### 测试左轮示例：

```
Ready> C          ← 清零编码器
Ready> L 50       ← 左轮PWM=50
Ready> E          ← 等待2秒后输入，读取编码器
[Encoder] Left: 8, Right: 0
          Left: NOT rotated

Ready> S          ← 停止
Ready> C          ← 清零

Ready> L 60       ← 增加PWM
Ready> E          ← 等待2秒后读取
[Encoder] Left: 15, Right: 0
          Left: NOT rotated

Ready> S
Ready> C

Ready> L 65       ← 继续增加
Ready> E          ← 等待2秒后读取
[Encoder] Left: 215, Right: 0
          Left: ROTATED ✓     ← 找到了！记录65

Ready> S
```

#### 重复验证（重要！）：

```
Ready> C
Ready> L 65
Ready> E          ← 再次测试，确认稳定
[Encoder] Left: 208, Right: 0
          Left: ROTATED ✓     ← 再次成功

重复3-5次，确保每次都能转动
```

#### 测试右轮：

```
Ready> C
Ready> R 50       ← 右轮PWM=50
Ready> E
...
（流程同左轮）
```

### 5. 计算结果

假设测试结果：
- 左轮最小PWM：65
- 右轮最小PWM：68

计算：
```
左轮最终 = 65 × 1.2 = 78
右轮最终 = 68 × 1.2 = 81.6 ≈ 82
FF_K_STATIC = max(78, 82) = 82 PWM
```

### 6. 修改代码

编辑 `modules/MotionControl/inc/motion_config.h`:

```c
#define FF_K_STATIC  82.0f  // 从80.0f更新
```

### 7. 重新编译

```bash
cmake --build cmake-build-debug --target v1.0_freeRTOS -j 8
```

### 8. 验证

切换测试模式：

编辑 `Core/Src/freertos.c`:

```c
// #define TEST_MODE_STATIC_FRICTION       /* 注释掉 */
#define TEST_MODE_PLAYGROUND_TRACK         /* 启用循迹模式 */
```

重新编译、烧录，观察启动表现。

---

## 常见命令

| 命令 | 功能 |
|------|------|
| `L 65` | 左轮PWM=65 |
| `R 70` | 右轮PWM=70 |
| `S` | 停止 |
| `E` | 读取编码器 |
| `C` | 清零编码器 |
| `H` | 帮助 |

---

## 判断标准

✓ **转动**：编码器计数 ≥ 20  
✗ **未转动**：编码器计数 < 20

---

## 注意事项

1. 小车必须放在**地面上**（不要悬空）
2. 测试前确保小车**完全静止**
3. 施加PWM后等待**2秒**再读取编码器
4. 找到最小PWM后，**重复测试5次**确认稳定
5. 左右轮分别测试

---

## 相关文档

- 详细指南：`docs/STATIC_FRICTION_TEST_GUIDE.md`
- 数据记录表：`docs/STATIC_FRICTION_TEST_DATA.md`

**祝测试顺利！**
