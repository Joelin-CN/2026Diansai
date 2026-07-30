# 修复日志：vscode-debug-fix-2026-07-29

- **模块**：VSCode 调试/烧录配置
- **文件**：`.vscode/launch.json`
- **芯片**：STM32F407VGT6
- **日期**：2026-07-29
- **状态**：✅ 已修复

---

## 问题描述

STM32 工程在 VSCode 中可以正常编译，但点击调试/烧录按钮后无法烧录，ST-Link 已正确连接。

报错信息：
```
boundoStepSettingsValidationFailed: Core not found:
run "Setup STM32Cube project()" command or verify "deviceCore" attribute
```

---

## 根本原因

`.vscode/launch.json` 由 STM32CubeMX 自动生成，该模板假设用户会通过 ST VSCode 扩展的 **Setup STM32Cube project()** 向导完整初始化项目，完成项目元数据注册。

本工程跳过了该向导（直接使用 CMake 编译），导致扩展依赖的三处配置全部失效：

1. **缺少 `deviceCore` 字段**：`stlinkgdbtarget` 调试适配器强制要求此字段以确定目标 CPU 核心类型，缺失时直接中止启动。
2. **elf 路径使用动态命令**：`${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}` 依赖项目注册信息，项目未注册时命令返回空值，烧录目标文件路径解析失败。
3. **`preBuild` 使用无效命令**：`${command:st-stm32-ide-debug-launch.build}` 同样依赖项目注册，执行时静默失败，干扰烧录流程。

---

## 修复内容

### 修改前
```json
{
    "type": "stlinkgdbtarget",
    "request": "launch",
    "name": "STM32Cube: Launch ST-Link GDB Server",
    "origin": "snippet",
    "cwd": "${workspaceFolder}",
    "preBuild": "${command:st-stm32-ide-debug-launch.build}",
    "runEntry": "main",
    "imagesAndSymbols": [
        {
            "imageFileName": "${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}"
        }
    ]
}
```

### 修改后
```json
{
    "type": "stlinkgdbtarget",
    "request": "launch",
    "name": "STM32Cube: Launch ST-Link GDB Server",
    "cwd": "${workspaceFolder}",
    "deviceCore": "Cortex-M4",
    "runEntry": "main",
    "imagesAndSymbols": [
        {
            "imageFileName": "${workspaceFolder}/build/Debug/v1.0_freertos.elf"
        }
    ]
}
```

### 变更说明

| 项 | 操作 | 说明 |
|----|------|------|
| `deviceCore` | **新增** | 指定目标核心为 `Cortex-M4`，消除 Core not found 报错 |
| `imageFileName` | **修改** | 改为硬编码路径，不再依赖扩展的项目注册机制 |
| `preBuild` | **删除** | CMake 工程独立编译，无需ST扩展托管构建流程 |
| `origin: snippet` | **删除** | 无实际功能，清理冗余字段 |

---

## 环境信息

- IDE：VSCode
- 扩展：stmicroelectronics.st-stm32-ide
- 后端工具：STM32CubeCLT（修复前未安装，修复过程中补装）
- 构建系统：CMake + Ninja
- 调试器：ST-Link（SWD）
