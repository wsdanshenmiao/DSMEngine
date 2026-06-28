## 工程概况

DSMEngine 是基于 Xmake 和 MSVC 的 Windows C++23 / Direct3D 12 引擎。

- 第一方引擎代码：`DSMEngine/`
- 运行时系统：`DSMEngine/Runtime/`
- 编辑器代码：`DSMEngine/Editor/`
- HLSL 着色器：`DSMEngine/Shaders/`
- 示例程序：`Samples/PBR`
- 工程与场景数据：`Projects/`
- 第三方依赖：`ThirdParty/`
- 生成产物：`bin/`、`build/`、`.xmake/`、`vsxmake2022/`

运行时模块包括 `Core`、`Framework`、`Graphics`、`Render`、`Math`、`Platform`、`Event` 和 `Utils`。

## 文档地图

- 加载本文档后输出 `已加载 AGENTS.md` ,从本文档开始，了解项目规则和文档路由。
- `PLANS.md`: Exec-Plan 写法、状态、和关闭标注。
- `docs/README.md`: Harness 的目录结构与写入规则。
- `docs/verification.md`: 稳定验证入口，产物协议和失败排查路线。
- `docs/exec-plans/`: 复杂任务计划和归档。
- `docs/exec-plans/tech-debt-tracker.md`: 技术债和待处理的问题。
- `docs/knowledge/`: 工程经验、踩坑记录与高频复用的活知识放在这里。
- `docs/reviews/`: Review 记录和验证证据。
- `.agents/skills/`: 项目本地 Skill 说明。
- `tools/`: 小型辅助脚本或 CLI 包装。
- `.tmp`: 临时产物、编译日志和验证输出

## 工程边界

- `TARGET_ROOT`: `E:\Code\DSMEngine`，当前项目的的工程根目录。
- `TARGET_ENGINE`: `E:\Unreal\UnrealEngine`，UE5 Engine 源码。

## 工程约束

- 保持 `AGENTS.md` 的简短；长期事实、流程细节和规则说明放进对应文档或工具。
- 改动完成后必须运行可用的最小验证，并记录无法验证的原因。
- 除非任务明确要求升级依赖或修复 vendor 问题，否则不要修改 `ThirdParty/`。
- 保留用户已有改动。若文件已被修改，先阅读再编辑，并保持改动范围聚焦。
- 只有新增文件不在现有 glob 覆盖范围内，或确实改变目标/构建规则时，才更新 `xmake.lua`。

## ExecPlans

复杂功能、跨模块修改、显著重构应先按 `PLANS.md` 写 ExecPlan 并在设计、实施和验证过程中持续维护。

## 构建与验证

常用命令：

- `xmake`：构建默认 debug 配置。
- `xmake build PBR`：构建 PBR 示例目标。
- `xmake run PBR`：运行示例，做手动渲染/编辑器验证。
- `xmake f -m release && xmake`：性能或 release-only 行为相关任务使用 release 构建。
- `xmake project -k vsxmake2022`：构建图变化后重新生成 Visual Studio 工程。

## 工程约定

- 使用 `DSM` 命名空间。
- 文件保持 UTF-8 编码。
- 类型和方法使用 PascalCase，例如 `RenderResource`、`UpdateRenderResource`。
- 成员变量使用 `m_` 前缀。
- 项目注释必须使用中文编写，对话与中间的临时输出必须使用中文进行交流。
- 开启计划模式后不能直接修改文件，必须先按照 ExecPlans 的要求指定计划，我批准后才可以执行

## 任务流

- 开启计划模式后必须遵循 `PLANS.md` 执行。
- 非计划模式小范围局部修改可以直接处理。
- 非计划模式中等复杂度任务应先识别影响模块，再运行最小有效验证。
- 复杂任务遵循 `PLANS.md`：创建 Exec-Plan，循环执行与验证，持续更新状态和证据，完成后归档。
- 不熟悉的领域先探索：阅读入口、列出假设、验证关键事实，再决定进入计划或实现。
