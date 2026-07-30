# IR校准快速开始卡 ⚡

> 打印/截图此页，贴在调试台旁边

---

## 🎯 一句话目标
校准红外传感器，确保黑线检测准确、lateral_error符号正确

---

## ⚙️ 代码设置 (1分钟)

`Core/Src/freertos.c`:
```c
#define TEST_MODE_IR_CALIBRATION     // ← 激活
// #define TEST_MODE_TRACK_CONTROL   // ← 注释掉
```
编译 → 烧录 → 连接串口(115200)

---

## 📋 5步校准 (~15分钟)

| Step | 做什么 | 怎么判对 | 串口关键字 |
|------|--------|---------|-----------|
| **1** 初始化 | 上电等30秒 | 无ERROR | `Initialization complete!` |
| **2** 白平衡 | 小车放白色表面 | 8通道值都在200-300 | `Calibration successful (100/100)` |
| **3** 黑线阈值 | 小车居中放黑线上 | 最大强度 > 50 | `Threshold set to: XX` |
| **4** 查看结果 | 记录数值 | 无异常 | `IR Calibration Configuration` |
| **5** 验证符号 | 手动左右移动小车 | **右移→正值, 左移→负值** | `Lateral error: +X.XXX` |

---

## ✅ 验证检查 (Step 5 关键!)

```
右移小车 → lateral_error > 0  ✅
左移小车 → lateral_error < 0  ✅
居中黑线 → lateral_error ≈ 0  ✅
离开黑线 → LINE LOST         ✅
```

> ⚠️ 如果符号反向 → `config.c:78-80` 所有权重取反

---

## 📝 记录你的校准值

| 参数 | 你的值 |
|------|--------|
| White Ref (ch0-7) | `___` `___` `___` `___` `___` `___` `___` `___` |
| Black Threshold | `___` |

固化到 `config.c:364` 和 `config.c:399`

---

## 🔄 完成后

```c
// freertos.c - 切换回循迹模式
// #define TEST_MODE_IR_CALIBRATION
#define TEST_MODE_TRACK_CONTROL
```

重新编译烧录 → 开始循迹测试

---

## 🆘 常见问题速查

| 症状 | 快速解决 |
|------|---------|
| 读取成功率 < 50% | 检查USART2接线(PA2/PA3)，传感器供电5V |
| 所有通道读数为0 | `IrUartSensor_RequestAnalogMode()` + 等2秒 |
| 黑线强度 < 20 | 先做白平衡！检查传感器距地面距离 |
| lateral_error符号反 | `config.c:78-80` 权重全部取反 |

---

**v1.2.1** | 详见: `docs/IR_CALIBRATION_PROCEDURE_2026-07-30.md`
