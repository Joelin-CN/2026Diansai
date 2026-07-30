# 如何测试轮询编码器方案

**日期**: 2026-07-27  
**目标**: 在EIDE中编译并运行 `test_encoder_polling.c`

---

## 已完成的修改

✅ 1. `src/encoder.c` - 禁用GPIO中断，添加 `Encoder_Poll()` 函数  
✅ 2. `inc/encoder.h` - 添加 `Encoder_Poll()` 函数声明  
✅ 3. `tests/test_encoder_polling.c` - 新建轮询测试程序

---

## 在EIDE中测试的步骤

### 方法A：修改主程序入口点（推荐用于快速测试）

1. **临时更改 `src/main.c` 的入口点**

   打开 `src/main.c`，找到 `main()` 函数，临时重命名为 `main_original()`：
   
   ```c
   // 临时重命名
   int main_original(void)
   {
       // 原有代码...
   }
   ```

2. **在 `tests/test_encoder_polling.c` 中包含必要的文件**

   测试文件已经包含了 `main()` 函数，EIDE会自动找到它。

3. **在EIDE中构建**
   
   - 打开 EIDE (Embedded IDE)
   - 选择项目 `NewProject1`
   - 点击 "Build" 或按 `F7`
   - 等待编译完成

4. **烧录和运行**
   
   - 连接调试器（XDS110）
   - 点击 "Download" 烧录程序
   - 打开串口终端（115200 baud）
   - 按复位按钮
   - 观察输出

5. **恢复原主程序**
   
   测试完成后，将 `main_original()` 改回 `main()`。

---

### 方法B：使用EIDE的多目标配置（更专业的方法）

1. **创建新的构建目标**

   在 `.eide/eide.yml` 中添加新的目标：
   
   ```yaml
   targets:
     Debug:
       # 现有配置...
     
     TestEncoderPolling:
       cppPreprocessAttrs:
         # 复制Debug的配置...
       excludeList:
         - "**/src/main.c"  # 排除原main.c
         - "**/example_usage.c"
         - "**/temp/**"
       # 其他配置与Debug相同...
   ```

2. **在EIDE中切换目标**
   
   - 在EIDE界面中选择目标：`TestEncoderPolling`
   - 构建并烧录

---

### 方法C：使用条件编译（最灵活）

1. **修改 `src/main.c`**

   在文件开头添加：
   
   ```c
   // 如果定义了TEST_ENCODER_POLLING，使用测试main
   #ifdef TEST_ENCODER_POLLING
   #define main main_disabled
   #endif
   
   int main(void)
   {
       // 原有代码...
   }
   ```

2. **修改 `.eide/eide.yml`**

   在 `defineList` 中添加：
   
   ```yaml
   cppPreprocessAttrs:
     defineList:
       - __MSPM0G3507__
       - TEST_ENCODER_POLLING  # 添加这一行
   ```

3. **构建项目**

   EIDE会编译使用 `test_encoder_polling.c` 中的 `main()`。

4. **恢复正常模式**

   注释掉或删除 `TEST_ENCODER_POLLING` 定义。

---

## 预期输出

如果轮询方案工作正常，你应该看到：

```
Polling-Based Encoder Test
===========================
Board: MSPM0G3507
Encoder Poll Rate: 1kHz

Motor driver initialized
Encoder polling ready

=== M1 Forward @ 50% ===
[500 ms] State=1, Counts: M1=123 M2=0 M3=0 M4=0
[1000 ms] State=1, Counts: M1=256 M2=0 M3=0 M4=0
[1500 ms] State=1, Counts: M1=389 M2=0 M3=0 M4=0
M1 FWD: 512 counts in 2000ms

=== M1 Reverse @ -50% ===
[2500 ms] State=2, Counts: M1=-134 M2=0 M3=0 M4=0
M1 REV: -520 counts in 2000ms

=== M2 Forward @ 50% ===
...
```

**关键观察点**：
- ✅ 程序不再卡死
- ✅ 每500ms有调试输出
- ✅ 编码器计数在电机运行时增加/减少
- ✅ 正转为正值，反转为负值

---

## 故障排查

### 问题1：程序仍然卡死

**可能原因**：
- `Encoder_Init()` 中的 `NVIC_EnableIRQ()` 没有被注释掉
- 使用了旧版本的 `encoder.c`

**解决方法**：
```bash
# 检查encoder.c中是否有这两行被注释
grep "NVIC_EnableIRQ" ../../../2025e/m0_controller/src/encoder.c
```

应该看到：
```c
// NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN);
// NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN);
```

---

### 问题2：编码器计数始终为0

**可能原因**：
- 编码器未连接
- 编码器引脚配置错误
- `Encoder_Poll()` 未被调用

**解决方法**：
1. 检查接线（参考 `WIRING_AND_SYSCONFIG.md`）
2. 在 `Encoder_Poll()` 中添加调试输出：
   ```c
   void Encoder_Poll(void)
   {
       static uint32_t pollCount = 0;
       if (++pollCount % 1000 == 0) {
           printf("Encoder_Poll called 1000 times\n");
       }
       // 原有代码...
   }
   ```

---

### 问题3：编译错误 "multiple definition of main"

**原因**：
同时存在多个 `main()` 函数。

**解决方法**：
使用上述三种方法之一排除冲突。

---

## 下一步调试

如果轮询方案工作正常，可以：

1. **集成到主程序**：
   - 在 `TIMG0_IRQHandler` 中调用 `Encoder_Poll()`（2ms = 500Hz也足够）
   - 或在FreeRTOS任务中调用

2. **测试精度**：
   - 记录不同PWM下的计数值
   - 与电机规格表对比

3. **优化轮询频率**：
   - 如果发现丢失计数，提高到2kHz或5kHz

4. **未来升级到硬件QEI**：
   - 研究MSPM0的Timer Capture模式
   - 参考 `logs/encoder_polling_solution.md` 的"长期方案"

---

## 相关文档

- **问题诊断**: `logs/encoder_interrupt_hang_debug.md`
- **解决方案详解**: `logs/encoder_polling_solution.md`
- **接线说明**: `WIRING_AND_SYSCONFIG.md` (在项目根目录)

---

## 快速命令参考

```bash
# 检查encoder.c是否已更新
grep -A 5 "void Encoder_Init" ../../../2025e/m0_controller/src/encoder.c

# 检查encoder.h是否包含Encoder_Poll
grep "Encoder_Poll" ../../../2025e/m0_controller/inc/encoder.h

# 列出测试文件
ls -la ../../../2025e/m0_controller/tests/test_encoder*
```

---

**总结**：轮询方案已准备就绪，选择上述任一方法在EIDE中测试即可。
