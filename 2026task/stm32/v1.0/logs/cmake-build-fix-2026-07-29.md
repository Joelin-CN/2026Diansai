# 修复日志：cmake-build-fix-2026-07-29

- **模块**：CMake 构建配置
- **文件**：`cmake/stm32cubemx/CMakeLists.txt`
- **芯片**：STM32F407VGT6
- **日期**：2026-07-29
- **状态**：✅ 已修复

---

## 问题描述

工程45个文件全部编译成功，最后链接阶段失败，退出码1。

报错信息：
```
[build] FAILED: v1.0_freeRTOS.elf
ld.exe: CMakeFiles/v1.0_freeRTOS.dir/Core/Src/freertos.c.obj:
  in function `StartDefaultTask':
freertos.c:118: undefined reference to `ATK_BLE02_Start'
(ATK_BLE02_Start): Unknown destination type (ARM/Thumb)
freertos.c:118: dangerous relocation: unsupported relocation
collect2.exe: error: ld returned 1 exit status
```

---

## 根本原因

`Core/Src/atk_ble02.c` 实现了 `ATK_BLE02_Start`，但该文件未添加到
`cmake/stm32cubemx/CMakeLists.txt` 的 `MX_Application_Src` 列表中。

链接器从未收到 `atk_ble02.c.obj`，因此无法解析 `freertos.c:118` 处的符号引用。

调用链：

```
freertos.c:118  StartDefaultTask()
    → ATK_BLE02_Start()          ← 声明在 Core/Inc/atk_ble02.h
                                  ← 实现在 Core/Src/atk_ble02.c（未编译）
```

`atk_ble02.c` 文件本身存在且实现完整，纯粹是 CMakeLists 漏写导致。

---

## 修复内容

**文件**：`cmake/stm32cubemx/CMakeLists.txt`

### 修改前

```cmake
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/freertos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/dma.c
    ...
)
```

### 修改后

```cmake
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/freertos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/atk_ble02.c   # ← 新增
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/dma.c
    ...
)
```

### 变更说明

| 项 | 操作 | 说明 |
|----|------|------|
| `atk_ble02.c` | **新增至 MX_Application_Src** | 补充漏掉的 BLE02 驱动源文件，使链接器可以找到 `ATK_BLE02_Start` 实现 |

---

## 环境信息

- IDE：VSCode
- 构建系统：CMake + Ninja（cube-cmake）
- 工具链：arm-none-eabi-gcc 14.3.1+st.2
- FreeRTOS：CMSIS-RTOS V2
- 构建预设：Debug
