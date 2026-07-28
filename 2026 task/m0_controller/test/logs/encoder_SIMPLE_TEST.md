# 简化版编码器测试 - 2026-07-27 18:10

## 问题

原测试程序 `test_encoder_auto.c` 在M2测试后跳转到 `STATE_DONE` (state=12)，没有执行M3和M4测试。

可能原因：
- 状态机逻辑中某处出错
- `elapsedMs` 计算异常
- 某个case没有break

## 解决方案

创建简化版测试程序 `test_encoder_simple.c`：
- 使用简单的线性步骤（0-8）
- 每步明确的时间检查
- 内联的DelayMs函数（持续调用Encoder_Poll）
- 更清晰的输出格式

## 使用方法

### 在主程序中调用

修改 `src/main.c`:

```c
// 在文件开头添加
extern void test_encoder_simple_init(void);
extern void test_encoder_simple_main_loop(void);

int main(void)
{
    SYSCFG_DL_init();
    
    test_encoder_simple_init();
    
    while (1) {
        test_encoder_simple_main_loop();
    }
}
```

### 或在EIDE中设置

1. 临时重命名 `src/main.c` 的 `main()` 为 `main_original()`
2. 在 `tests/test_encoder_simple.c` 末尾添加：
   ```c
   int main(void)
   {
       SYSCFG_DL_init();
       test_encoder_simple_init();
       while (1) {
           test_encoder_simple_main_loop();
       }
   }
   ```
3. 编译运行

## 预期输出

```
=====================================
 Simple Encoder Test (M1-M4)        
=====================================
Board: MSPM0G3507
Method: Polling @ 1kHz

✓ Motor driver initialized
✓ Encoder polling initialized

Starting sequential test...

=== Test 1: M1 Forward @ 50% for 2s ===
Result: M1=1200  M2=0  M3=0  M4=0

=== Test 2: M2 Forward @ 50% for 2s ===
Result: M1=0  M2=1200  M3=0  M4=0

=== Test 3: M3 Forward @ 50% for 2s ===
Result: M1=0  M2=0  M3=1200  M4=0

=== Test 4: M4 Forward @ 50% for 2s ===
Result: M1=0  M2=0  M3=0  M4=1200

=====================================
 Test Complete!                     
=====================================
Final encoder counts:
  M1=  1200  M2=  1200  M3=  1200  M4=  1200

Press RESET to run again.
```

## 优势

1. **简单可靠** - 线性步骤，无复杂状态机
2. **持续轮询** - DelayMs中持续调用Encoder_Poll()
3. **清晰输出** - 每个测试的增量计数
4. **易于调试** - 步骤编号清晰

---

**创建时间**: 2026-07-27 18:10
