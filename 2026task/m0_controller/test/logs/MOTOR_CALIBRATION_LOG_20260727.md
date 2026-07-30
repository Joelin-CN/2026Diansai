# 电机方向校准日志

**日期**: 2026-07-27  
**任务**: 电机软件编号映射和方向校准  
**状态**: ✅ 已完成

---

## 问题描述

### 初始问题
1. **软件编号与物理编号不对应**
   - 软件M1 → 物理M2（左后）
   - 软件M2 → 物理M1（左前）
   - 软件M3 → 物理M3（右后）✓
   - 软件M4 → 物理M4（右前）✓

2. **电机方向不一致**
   - 调用 `Motor_SetFour(300, 300, 300, 300)` 时，部分电机反转
   - 车辆无法正常前进

3. **硬件问题**
   - 初期测试发现物理M2完全不转动
   - 经检查为接线问题

---

## 物理布局

```
车头方向 ↑
[ M1(左前)  ,   M4(右前) ]
[ M2(左后)  ,   M3(右后) ]
```

---

## 调试过程

### 阶段1: 创建识别测试程序
**文件**: `tests/test_motor_identify.c`

创建了一个逐个测试电机的程序，用于识别软件编号与物理电机的映射关系。

**测试流程**:
- Phase 0: 全部停止 (2s)
- Phase 1: 仅M1运行 (3s)
- Phase 2: 停止 (1s)
- Phase 3: 仅M2运行 (3s)
- Phase 4: 停止 (1s)
- Phase 5: 仅M3运行 (3s)
- Phase 6: 停止 (1s)
- Phase 7: 仅M4运行 (3s)
- Phase 8: 全部停止

**配置修改**:
- `src/main.c`: 添加 `TEST_MODE_MOTOR_IDENTIFY (8)`
- `.eide/eide.yml`: 排除所有其他测试文件

### 阶段2: 识别硬件问题
**问题**: Phase 5 (软件M3) 测试时，物理M2完全不转动

**创建调试程序**: `tests/test_motor_m2_debug.c`
- 以不同功率测试物理M2: 30%, 50%, 100%, -30%
- 确认为接线问题

**结果**: 用户修复接线后，所有电机可以转动

### 阶段3: 重新识别映射关系
**第一次测试结果**（接线修复后）:
| 软件编号 | 物理电机 | 转动方向 |
|---------|---------|---------|
| M1 | M2 (左后) | 反转 |
| M2 | M1 (左前) | 反转 |
| M3 | M3 (右后) | 反转 |
| M4 | M4 (右前) | 反转 |

**问题**: 
1. M1和M2的硬件接线交叉
2. 所有电机都反转

### 阶段4: 软件映射修正
**需求**: 用户希望软件M1对应物理M1，而不是修改硬件接线

**解决方案**: 在 `src/motor.c` 的 `Motor_SetFour()` 函数中重新映射

**修改内容**:
```c
// 交换M1和M2的硬件引脚分配
// Software M1 → Hardware M2 pins → Physical M1 (left front)
Motor_SetOne(m1Speed, MOTOR_M1_REVERSED != 0, MOTOR_DIR_M2_IN1_PORT, ...

// Software M2 → Hardware M1 pins → Physical M2 (left rear)  
Motor_SetOne(m2Speed, MOTOR_M2_REVERSED != 0, MOTOR_DIR_M1_IN1_PORT, ...
```

**结果**: 软件编号成功对应物理编号

### 阶段5: 方向校准
**创建最终验证程序**: `tests/test_motor_final_verify.c`

**迭代测试**:

| 测试轮次 | 配置 (M1,M2,M3,M4) | 结果 | 问题 |
|---------|-------------------|------|------|
| 1 | (0,0,1,1) | M1✓, M2✗, M3✓, M4✗ | M2/M4反转 |
| 2 | (0,1,0,1) | M1✓, M2✓, M3✗, M4✗ | M3/M4反转 |
| 3 | (0,1,1,1) | M1✓, M2✓, M3✓, M4✗ | M4反转 |
| 4 | (0,1,1,0) | M1✓, M2✓, M3✓, M4✓ | ✅ 全部正确 |

---

## 最终配置

### 软件映射 (`src/motor.c`)
```c
void Motor_SetFour(int16_t m1Speed, int16_t m2Speed, int16_t m3Speed, int16_t m4Speed)
{
    // Software M1 → Hardware M2 pins → Physical M1 (left front)
    Motor_SetOne(m1Speed, MOTOR_M1_REVERSED != 0, MOTOR_DIR_M2_IN1_PORT, ...);
    
    // Software M2 → Hardware M1 pins → Physical M2 (left rear)
    Motor_SetOne(m2Speed, MOTOR_M2_REVERSED != 0, MOTOR_DIR_M1_IN1_PORT, ...);
    
    // Software M3 → Hardware M3 pins → Physical M3 (right rear)
    Motor_SetOne(m3Speed, MOTOR_M3_REVERSED != 0, MOTOR_DIR_M3_IN1_PORT, ...);
    
    // Software M4 → Hardware M4 pins → Physical M4 (right front)
    Motor_SetOne(m4Speed, MOTOR_M4_REVERSED != 0, MOTOR_DIR_M4_IN1_PORT, ...);
}
```

### 反转标志 (`inc/motor.h`)
```c
#define MOTOR_M1_REVERSED (0)  // Software M1 → Physical M1 (left front) → correct
#define MOTOR_M2_REVERSED (1)  // Software M2 → Physical M2 (left rear) → needs reverse
#define MOTOR_M3_REVERSED (1)  // Software M3 → Physical M3 (right rear) → needs reverse
#define MOTOR_M4_REVERSED (0)  // Software M4 → Physical M4 (right front) → correct
```

---

## 验证结果

### Test 1: 所有电机同时运行
✅ 四个轮子同时向"前进"方向转动，方向一致

### Test 2: 逐个电机测试
- ✅ M1 (左前): 正转
- ✅ M2 (左后): 正转
- ✅ M3 (右后): 正转
- ✅ M4 (右前): 正转

### Test 3: 前进运动测试
✅ 车辆正常前进，无偏转

---

## 创建的文件

### 测试程序
- `tests/test_motor_identify.c` - 电机识别测试
- `tests/test_motor_m2_debug.c` - M2硬件调试测试
- `tests/test_motor_final_verify.c` - 最终方向验证测试

### 配置文件修改
- `src/main.c` - 添加测试模式定义和切换逻辑
- `.eide/eide.yml` - 更新测试文件排除列表

### 核心修改
- `src/motor.c` - 软件映射修正（交换M1/M2硬件引脚）
- `inc/motor.h` - 反转标志配置

---

## 技术要点

### 1. 硬件映射层次
```
软件编号 (用户层)
    ↓ Motor_SetFour() 映射
硬件引脚 (SysConfig生成)
    ↓ 物理接线
物理电机 (实际硬件)
```

### 2. 为什么不修改 `ti_msp_dl_config.h`
该文件由 TI SysConfig 工具自动生成，文件头明确标注 "DO NOT EDIT"。修改后会在下次 SysConfig 重新生成时被覆盖。

正确的做法是在 `motor.c` 中进行软件层映射。

### 3. 反转标志的作用
在 `Motor_SetOne()` 函数中：
```c
forward = (speed >= 0);
if (reversed) forward = !forward;  // 反转标志在此生效

Motor_WritePin(in1Port, in1Pin, forward);
Motor_WritePin(in2Port, in2Pin, !forward);
```

通过翻转 `forward` 标志来反转电机方向。

---

## 经验教训

1. **分阶段调试**: 先解决硬件问题，再解决映射问题，最后校准方向
2. **逐个测试**: 使用识别测试程序能快速定位每个电机的实际连接情况
3. **软件映射优于硬件返工**: 当硬件接线已固定时，软件重映射是更高效的解决方案
4. **迭代验证**: 每次只改一个配置，立即验证结果，避免混淆

---

## 后续维护

### 如果需要重新校准
1. 运行 `TEST_MODE_MOTOR_IDENTIFY` 测试，记录映射关系
2. 检查 `motor.c` 中的映射是否正确
3. 运行 `TEST_MODE_MOTOR_FINAL_VERIFY` 测试，调整 `motor.h` 中的反转标志
4. 重复验证直到所有电机方向正确

### 注意事项
- SysConfig 重新生成后，`motor.c` 中的映射不会被影响
- 如果物理接线改变，需要重新运行识别测试
- 反转标志 `motor.h` 需要手动维护，不会被工具覆盖

---

## 相关文档

- **任务交接文档**: `docs/handoff/HANDOFF-20260724-MotorDebug.md`
- **电机驱动接口**: `inc/motor.h`, `src/motor.c`
- **测试程序**: `tests/test_motor_*.c`

---

**校准完成时间**: 2026-07-27  
**最终状态**: ✅ 所有电机软件编号正确对应物理编号，方向校准完成
