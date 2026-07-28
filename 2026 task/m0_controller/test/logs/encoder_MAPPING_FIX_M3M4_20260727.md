# 编码器软件映射最终修正 - 2026-07-27

**时间**: 18:36  
**修改**: 修正M3和M4的A/B相读取顺序，适配硬件现状

---

## 问题

测试发现M3和M4的方向反向：
- M3正转 → -1320 counts（应该是正值）
- M4正转 → -1102 counts（应该是正值）

**原因**: 硬件接线时M3和M4的编码器A/B相与预期相反

---

## 解决方案

**软件层面交换A/B相读取**，而不修改硬件接线。

### src/encoder.c - Encoder_ReadState()

```c
case ENCODER_M3:
    /* M3: swap A/B to correct direction (hardware A/B phase reversed) */
    pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_B_PIN);  // Read B as A
    pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_A_PIN);  // Read A as B
    break;

case ENCODER_M4:
    /* M4: swap A/B to correct direction (hardware A/B phase reversed) */
    pinsA = DL_GPIO_readPins(GPIOB, ENCODER_ENC4_B_PIN);  // Read B as A
    pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC4_A_PIN);  // Read A as B
    break;
```

---

## 最终引脚映射表

| 电机 | 物理引脚 | 软件读取为 | 说明 |
|------|---------|-----------|------|
| M1 | ENC2: PA2, PA26 | A=PA2, B=PA26 | 交换了编码器编号 |
| M2 | ENC1: PA12, PA13 | A=PA12, B=PA13 | 交换了编码器编号 |
| M3 | ENC3: PA27, PA28 | A=PA28, B=PA27 | 交换了A/B相 |
| M4 | ENC4: PA31, PB18 | A=PB18, B=PA31 | 交换了A/B相 |

---

## 预期测试结果

修改后重新测试，应该看到：

```
=== Test 1: M1 Forward @ 50% ===
  Delta: M1= +1200, M2=    0, M3=    0, M4=    0  ✅

=== Test 2: M2 Forward @ 50% ===
  Delta: M1=    0, M2= +1200, M3=    0, M4=    0  ✅

=== Test 3: M3 Forward @ 50% ===
  Delta: M1=    0, M2=    0, M3= +1200, M4=    0  ✅ (之前是负值)

=== Test 4: M4 Forward @ 50% ===
  Delta: M1=    0, M2=    0, M3=    0, M4= +1200  ✅ (之前是负值)

=== Test 5: All Motors Forward ===
  Delta: M1= +1800, M2= +1900, M3= +1800, M4= +1800  ✅ (全部正值)

=== Test 6: All Motors Reverse ===
  Delta: M1= -1800, M2= -1900, M3= -1800, M4= -1800  ✅ (全部负值)
```

---

## 技术说明

### 为什么交换A/B相能反转方向？

正交编码器通过A相和B相的相对关系判断方向：
- **正转**: A相领先B相90°
- **反转**: B相领先A相90°

如果硬件A/B相接反：
- 电机正转时，物理上B相领先A相
- 软件读取为"反转"，产生负值

**解决方法**：软件读取时交换A/B，相当于颠倒了相位关系，将"反转"变回"正转"。

---

## 下一步

1. **重新编译项目**
2. **烧录到板子**
3. **运行测试**
4. **验证所有电机正转产生正值，反转产生负值**

---

**修改完成时间**: 2026-07-27 18:36
