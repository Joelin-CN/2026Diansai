# Claude Code 工作指南

本文档记录与 Claude 协作开发本项目的工作模式、规范和最佳实践。

---

## 工作模式

### 多Agent协作模式（推荐）

本项目采用**主持人-分发-总结**的工作模式，适用于复杂的多步骤任务：

```
用户 → 主持人Claude
       ↓
       ├─ Agent-A (专项任务1) → 独立完成，返回结果
       ├─ Agent-B (专项任务2) → 独立完成，返回结果
       ├─ Agent-C (专项任务3) → 独立完成，返回结果
       └─ Agent-Doc (文档总结) → 汇总所有修改，更新文档
```

#### 典型工作流程

**阶段1：问题分析与任务分解**
- 主持人Claude接收用户需求
- 深入分析问题范围和依赖关系
- 将复杂任务拆解为独立的子任务
- 明确各子任务的输入/输出

**阶段2：并行/串行执行**
- 启动多个专项Agent
- 独立Agent各自完成特定领域的工作（代码修改、分析、验证）
- 主持人监控进度，协调依赖关系

**阶段3：文档同步**
- 所有代码修改完成后，启动文档Agent
- 文档Agent读取所有修改记录
- 统一更新CHANGELOG、README、API文档等
- 创建会话总结日志

#### 本项目实例

**v1.2.0 修复（第一轮Agent集群）**
```
Phase 1 (P0阻塞性问题):
  ├─ Agent-8: 传感器配置双轮迁移修复
  └─ Agent-9: 初始化失败处理加固

Phase 2 (P1功能性问题):
  ├─ Agent-10: 红外传感器算法修复
  └─ Agent-11: 速度参数配置传递修复

Phase 3 (P2文档同步):
  └─ Agent-12: 文档更新（CHANGELOG, README, API_PITFALLS_GUIDE等）
```

**v1.2.1 修复（第二轮Agent集群）**
```
Phase 1: 几何参数更新
  └─ Agent: 更新轮距、IR阵列位置、编码器位置（4个代码文件 + 7个文档）

Phase 2: 坐标系深度分析
  └─ Agent: 只读分析，生成713行分析报告，识别3个Critical问题

Phase 3: 坐标系问题修复
  └─ Agent: 交互式讨论，修正文档和注释（代码正确，文档错误）

Phase 4: 会话总结
  └─ Agent: 创建会话修复日志、更新CHANGELOG、创建验证清单
```

---

## 目录与文件组织规范

### 核心目录结构

```
项目根目录/
├── CLAUDE.md                   # 本文件 - Claude协作指南
├── CHANGELOG.md                # 版本变更历史（Keep a Changelog格式）
├── README.md                   # 项目主文档
├── API_PITFALLS_GUIDE.md       # API调试避坑指南
│
├── docs/                       # 详细文档目录
│   ├── *_2026-07-30.md        # 带日期的修复/分析报告
│   ├── SESSION_FIX_LOG_*.md   # 会话修复日志
│   ├── PARAMETER_*.md         # 参数相关文档
│   ├── VALIDATION_*.md        # 验证清单
│   └── QUICK_START_*.md       # 快速开始指南
│
├── logs/                       # 开发日志（历史记录）
│   └── 2026-07-*_*.md         # 按日期命名的调试日志
│
├── modules/                    # 功能模块代码
│   ├── Sens-Decision/         # 传感器-决策模块
│   │   ├── inc/               # 头文件
│   │   └── src/               # 源文件
│   └── MotionControl/         # 运动控制模块
│       ├── inc/
│       └── src/
│
└── Core/                       # STM32 HAL核心代码
    ├── Inc/
    └── Src/
        └── app/                # 应用层代码
```

### 文档命名规范

| 类型 | 命名格式 | 示例 | 位置 |
|------|---------|------|------|
| 会话总结 | `SESSION_FIX_LOG_YYYY-MM-DD_PART*.md` | `SESSION_FIX_LOG_2026-07-30_PART2.md` | `docs/` |
| 修复报告 | `<MODULE>_FIX_YYYY-MM-DD.md` | `COORDINATE_FIX_2026-07-30.md` | `docs/` |
| 分析报告 | `<MODULE>_ANALYSIS_YYYY-MM-DD.md` | `COORDINATE_SYSTEM_ANALYSIS_2026-07-30.md` | `docs/` |
| 更新记录 | `<MODULE>_UPDATE_YYYY-MM-DD.md` | `GEOMETRY_UPDATE_2026-07-30.md` | `docs/` |
| 验证清单 | `VALIDATION_AFTER_*.md` | `VALIDATION_AFTER_SESSION_2026-07-30.md` | `docs/` |
| 快速指南 | `QUICK_START_*.md` | `QUICK_START_AFTER_V1.2.0.md` | `docs/` |
| 历史日志 | `YYYY-MM-DD_<topic>.md` | `2026-07-30_motor_debug_complete.md` | `logs/` |

---

## Agent任务设计原则

### 1. 单一职责原则
每个Agent应该：
- 专注于一个明确的技术领域（如"EKF修复"、"坐标系分析"）
- 有清晰的输入和输出定义
- 不依赖其他Agent的中间结果（如果有依赖，使用串行执行）

### 2. Agent类型分类

| 类型 | 特征 | 示例 |
|------|------|------|
| **修复型Agent** | 修改代码，解决特定问题 | Agent-8（传感器配置修复） |
| **分析型Agent** | 只读分析，生成报告 | Agent（坐标系深度分析） |
| **交互型Agent** | 与用户讨论，确认方案后修复 | Agent（交互式坐标系修复） |
| **文档型Agent** | 汇总修改，更新所有文档 | Agent-12（文档更新） |

### 3. Agent Prompt设计要点

**必须包含的信息：**
- 背景信息（当前项目状态，之前的工作）
- 具体任务（要做什么，不要做什么）
- 文件范围（需要读取/修改哪些文件）
- 输出要求（生成什么文档，修改哪些代码）
- 验证标准（如何确认任务完成）

**典型结构：**
```markdown
你的任务是[具体任务描述]。

## 背景信息
[项目当前状态，相关历史]

## 你的任务
### 第1步：[步骤名]
[详细说明]

### 第2步：[步骤名]
[详细说明]

## 输出要求
1. [文档/代码A]
2. [文档/代码B]

## 验证标准
- [ ] 检查项1
- [ ] 检查项2
```

### 4. 文档Agent的特殊职责

文档Agent是最后执行的Agent，负责：
- **读取所有修改记录**（从其他Agent的输出文档）
- **统一更新核心文档**：
  - `CHANGELOG.md` - 添加新版本条目
  - `README.md` - 更新参数、示例、最近修改
  - `API_PITFALLS_GUIDE.md` - 添加新发现的陷阱
  - `docs/PARAMETER_TRACEABILITY.md` - 添加新参数追溯
- **创建会话总结**：
  - `docs/SESSION_FIX_LOG_YYYY-MM-DD_PART*.md`
  - 包含：修复时间线、参数对比表、文件清单、Agent统计、待验证项
- **创建验证清单**：
  - `docs/VALIDATION_AFTER_*.md`
  - 包含：编译步骤、测试步骤、回滚方案

---

## 版本管理规范

### Semantic Versioning

本项目遵循语义化版本 `MAJOR.MINOR.PATCH`：

| 版本位 | 变更类型 | 示例 |
|--------|---------|------|
| MAJOR | 不兼容的API变更 | 2.0.0 |
| MINOR | 向后兼容的功能新增 | 1.3.0 |
| PATCH | 向后兼容的问题修复 | 1.2.1 |

### 版本历史（本项目）

| 版本 | 日期 | 主要内容 |
|------|------|----------|
| v1.0.0 | 2026-07-29 | 初始版本，基础控制流水线 |
| v1.1.0 | 2026-07-30 | EKF修复、栈溢出预防、频率优化、中断优先级 |
| v1.2.0 | 2026-07-30 | 双轮迁移、初始化加固、红外算法、速度配置 |
| v1.2.1 | 2026-07-30 | 几何更新（轮距214mm）、坐标系文档修正 |

---

## 开发环境与工具

### Python环境
- **环境**: Conda base环境
- **常用工具**: 所有标准Python工具（numpy, matplotlib等）已安装在base环境
- **调用方式**: 直接使用 `python` 命令，无需激活虚拟环境

### 构建系统
- **平台**: STM32CubeMX + Keil MDK / CMake
- **编译器**: ARM Compiler 5.06 update 5
- **目标芯片**: STM32F407VGT6

### 代码编辑器集成
- VSCode / JetBrains IDEs均支持
- 路径格式：Windows风格（`E:\B306\...`）

---

## 协作最佳实践

### 1. 修改前先分析
- 重大变更前，先启动只读分析Agent
- 生成分析报告供用户确认
- 确认后再启动修复Agent

### 2. 渐进式修复
- 按优先级分批执行（P0 → P1 → P2）
- 每批完成后等待用户确认
- 避免一次性修改过多文件

### 3. 保持文档同步
- 代码修改后立即更新文档
- 文档Agent应在所有代码修改完成后执行
- 确保CHANGELOG、README、API文档三者一致

### 4. 验证驱动开发
- 每次修复后创建验证清单
- 明确P0（必须）、P1（重要）、P2（可选）验证项
- 提供实车测试的详细步骤和预期结果

### 5. 问题追溯
- 重要修复必须创建带日期的报告文档
- 记录问题发现、根因分析、修复方案、验证方法
- 便于未来回溯和知识传承

---

## 典型工作流示例

### 场景：用户报告新问题

**Step 1: 问题确认与分析**
```
用户："循迹时车辆总是偏向一侧"
↓
主持人Claude：
1. 询问具体现象（始终偏左？偏右？）
2. 询问是否修改过参数
3. 检查相关代码文件
```

**Step 2: 启动分析Agent（可选，复杂问题推荐）**
```
Agent (只读分析):
- 读取perception.c, config.c等相关文件
- 分析IR权重、lateral_error计算
- 生成分析报告：可能的3个原因
↓
用户确认：原因2最可能
```

**Step 3: 启动修复Agent**
```
Agent (修复型):
- 修改config.c中的IR权重
- 更新注释
- 创建修复报告
```

**Step 4: 启动文档Agent**
```
Agent (文档型):
- 读取修复报告
- 更新CHANGELOG.md (v1.2.2)
- 更新API_PITFALLS_GUIDE.md
- 创建验证清单
```

**Step 5: 用户验证**
```
用户按照验证清单实车测试
→ 通过：合并到main分支
→ 失败：回滚，重新分析
```

---

## 文档模板

### 会话总结日志模板

```markdown
# Session Fix Log - Part N (YYYY-MM-DD)

## Executive Summary
[一段话总结本次会话的核心工作和成果]

## Phase 1: [阶段名称]
### Trigger
[触发原因]

### Changes
[关键变更列表]

### Files Modified
[文件清单]

## Phase 2: [阶段名称]
...

## Key Parameter Comparison
[参数对比表]

## Files Changed This Session
[完整文件清单，分代码/文档]

## Agent Summary
[Agent统计表：名称、Token、时长、状态]

## Pending Verification Items
[待验证清单，分P0/P1/P2]

## Document Index
[本次创建的所有文档索引]
```

### 修复报告模板

```markdown
# [Module] Fix Report - YYYY-MM-DD

## Problem Description
[问题描述]

## Root Cause
[根因分析]

## Fix Solution
[修复方案]

## Files Modified
[修改文件列表]

## Verification Method
[验证方法]

## Impact Analysis
[影响分析]
```

---

## 注意事项

### API错误处理
- 如果Agent遇到API错误（如thinking mode错误），主持人Claude应：
  1. 重启Agent并简化prompt
  2. 或直接接管完成任务
  3. 记录错误类型供未来参考

### Token预算管理
- 大型分析任务预留50k+ tokens
- 文档更新任务预留30k+ tokens
- 如遇token不足，分批执行

### 文件路径规范
- Windows路径使用反斜杠：`E:\B306\...`
- 代码中使用正斜杠：`modules/Sens-Decision/...`
- 文档中引用代码文件使用正斜杠

---

**文档版本**: 1.0
**创建日期**: 2026-07-30
**最后更新**: 2026-07-30
