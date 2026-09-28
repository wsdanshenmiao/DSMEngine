# DSMEngine 项目规则入口

本文件是 DSMEngine 的**唯一项目级规则来源**。开始任何任务前先读取本文档，再按文档地图加载所需的流程和验证文档。

## 工程概况

DSMEngine 是基于 Xmake/CMake、MSVC、C++23 和 Direct3D 12 的 Windows 引擎。

- Engine 代码：`Engine/Source/`
- Runtime：`Engine/Source/Runtime/`
- Editor：`Engine/Source/Editor/`
- Engine Shader：`Engine/Shaders/`
- Engine Content：`Engine/Content/`
- Engine 第三方依赖：`Engine/ThirdParty/`
- 项目：`Projects/PBR/`、`Projects/RayTracing/`
- 项目描述：`Projects/*/*.dsmproj`
- 构建产物：`bin/`、`build/`、`.xmake/`、`Projects/*/Binaries/`、`Projects/*/Intermediate/`

Runtime 模块包括 `Core`、`Framework`、`Graphics`、`Render`、`Math`、`Platform`、`Event` 和 `Utils`。

当前 Engine 和 Editor 构建为静态库；PBR、RayTracing 和测试目标分别提供可执行入口。Engine 尚未提供独立的 Editor Host 可执行程序。

## 文档地图

- `PLANS.md`：ExecPlan 协议、状态和归档规则。
- `Docs/README.md`：Harness 文档目录和维护规则。
- `Docs/Verification.md`：仓库级验证入口、产物协议和失败排查。
- `Docs/ExecPlans/Active/`：进行中的复杂任务计划。
- `Docs/ExecPlans/Completed/`：已完成计划归档。
- `Docs/ExecPlans/TechDebtTracker.md`：延期技术债和后续事项。
- `Docs/Knowledge/`：稳定工程知识和可复用排查经验。
- `Docs/Reviews/`：Review 记录和验证证据。
- `.agents/skills/`：项目本地 Skill 说明。
- `Tools/`：小型辅助脚本或 CLI 包装。
- `.tmp/`：临时日志、清单和验证输出。

`Docs/` 下的分类目录和说明性文档使用 PascalCase；`README.md`、`.gitkeep` 和日期前缀中的连字符保留约定格式。

## 工程边界

- `TARGET_ROOT`：`D:\Code\DSMEngine`
- `TARGET_ENGINE`：本机 Unreal Engine 源码路径由具体任务或环境配置提供，不在本文件硬编码为 DSMEngine 路径。

## 工程约束

- 保留用户已有改动；编辑已修改文件前先阅读并保持范围聚焦。
- 除非任务明确要求，不修改 `Engine/ThirdParty/` 中的 vendor 源码或依赖版本。
- 代码、脚本和文档保持 UTF-8；项目注释使用中文；使用 `DSM` 命名空间。
- 改动完成后运行可用的最小验证，并记录无法验证的原因。
- 不为 C++ 类型系统已经保证的行为额外编写测试。
- 只有新增文件不在现有 glob 覆盖范围内，或确实改变目标/构建规则时，才更新 `xmake.lua`。

## 计划和重构

复杂功能、跨模块修改和显著重构必须先阅读 `PLANS.md`，在 `Docs/ExecPlans/Active/` 创建并持续维护 ExecPlan，完成后移入 `Docs/ExecPlans/Completed/`。

重构时先对齐外部契约：边界、接口、行为和交付形态；再替换内部实现。不为兼容旧实现保留无价值的旧路径、fallback 或冗余中间层。

## 常用构建与验证

默认工作目录：`D:\Code\DSMEngine`。

```powershell
xmake
xmake build DSMEngine
xmake build DSMEditor
xmake build PBR
xmake build RayTracing
xmake build VirtualFileSystemTests

cmake --preset engine
cmake --preset pbr
cmake --preset raytracing
cmake --build build\cmake\pbr --config Debug --target PBR
cmake --build build\cmake\raytracing --config Debug --target RayTracing

.\tools\dsm.ps1 check Projects\PBR\PBR.dsmproj
.\tools\dsm.ps1 build Projects\PBR\PBR.dsmproj -BuildSystem xmake
```

渲染、资源、场景或编辑器改动还应根据 `Docs/Verification.md` 和对应 workflow 执行运行验证。涉及 Release 或性能时，使用：

```powershell
xmake f -m release
xmake
```

## 任务流

- 小范围局部修改可以直接处理，但仍需先识别影响模块并运行最小验证。
- 不熟悉的领域先探索入口、调用链和约束，再决定实施方案。
- 开启计划模式后不能直接修改文件；必须先按 `PLANS.md` 创建计划并等待批准。
- 对话和中间临时输出使用中文。
