# 编码器软件映射修正 - 2026-07-27

**时间**: 18:00  
**修改**: 交换M1和M2的编码器引脚映射以匹配硬件接线

---

## 问题

根据测试结果，物理接线是正确的，但软件编号与硬件不对齐：
- M1电机的编码器物理连接到了ENC2引脚（PA2, PA26）
- M2电机的编码器物理连接到了ENC1引脚（PA12, PA13）

---

## 修改内容

### src/encoder.c - Encoder_ReadState()

```c
static uint8_t Encoder_ReadState(Encoder_Id encoder)
{
    uint32_t pinsA;
    uint32_t pinsB;

    switch (encoder) {
        case ENCODER_M1:
            /* M1 physically connected to ENC2 pins */
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC2_A_PIN);  // PA2
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC2_B_PIN);  // PA26
            break;
        case ENCODER_M2:
            /* M2 physically connected to ENC1 pins */
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC1_A_PIN);  // PA12
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC1_B_PIN);  // PA13
            break;
        case ENCODER_M3:
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_A_PIN);  // PA27
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_B_PIN);  // PA28
            break;
        case ENCODER_M4:
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC4_A_PIN);  // PA31
            pinsB = DL_GPIO_readPins(GPIOB, ENCODER_ENC4_B_PIN);  // PB18
            break;
        default:
            return 0U;
    }
    return ((pinsA != 0U) ? 2U : 0U) | ((pinsB != 0U) ? 1U : 0U);
}
```

---

## 物理接线映射（当前实际）

| 电机编号 | 物理连接引脚 | SysConfig名称 |
|---------|-------------|--------------|
| M1 | PA2 (引脚8), PA26 (引脚46) | ENC2_A, ENC2_B |
| M2 | PA12 (引脚27), PA13 (引脚28) | ENC1_A, ENC1_B |
| M3 | PA27 (引脚47), PA28 (引脚48) | ENC3_A, ENC3_B |
| M4 | PA31 (引脚4), PB18 (引脚37) | ENC4_A, ENC4_B |

---

## 预期测试结果

修改后重新测试，应该看到：

```
=== Test 1: M1 (Left Front) Forward @ 50% ===
  Delta: M1=  1200, M2=     0, M3=     0, M4=     0
          ^^^^  M1电机转 → M1编码器计数

=== Test 2: M2 (Left Rear) Forward @ 50% ===
  Delta: M1=     0, M2=  1200, M3=     0, M4=     0
                         ^^^^  M2电机转 → M2编码器计数

=== Test 3: M3 (Right Rear) Forward @ 50% ===
  Delta: M1=     0, M2=     0, M3=  1200, M4=     0
                                    ^^^^

=== Test 4: M4 (Right Front) Forward @ 50% ===
  Delta: M1=     0, M2=     0, M3=     0, M4=  1200
                                                ^^^^
```

---

## 注意事项

1. **这是软件映射修正**，硬件接线保持不变
2. **M3和M4的映射保持原样**，等待测试验证
3. **如果M3/M4也有问题**，按照相同方法调整映射

---

## 下一步

1. 重新编译并烧录程序
2. 运行完整测试序列
3. 验证M1、M2现在是否正确对应
4. 检查M3、M4的情况

---

**修改完成时间**: 2026-07-27 18:00
