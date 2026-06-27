# PLANS.md

本文定义本仓库中 ExecPlan 的写法。`AGENTS.md` 和 `CLAUDE.md` 只负责把执行者路由到这里；具体计划规则以本文为准。

## 核心标准

ExecPlan 必须是自包含文档。一个开发者，或一个无状态的代码智能体，只拿到当前仓库和这份计划，也应能把任务正确做完。

ExecPlan 也必须是活文档。开始实施后，要持续维护，而不是把它当成一次性的设计稿。计划中应记录进展、发现、决策、验证结果和剩余问题。

## 必填章节

每份 ExecPlan 至少应包含这些章节：

- `摘要 (Summary)`：说明目标、范围和用户可见结果。
- `背景 (Context)`：说明现状、相关路径、约束和术语。
- `实施计划 (Implementation Plan)`：按里程碑拆解，写清文件、行为和边界变化。
- `验证 (Validation)`：写明具体命令、执行目录、产物和人工检查方式。
- `进展 (Progress)`：记录已完成、下一步和阻塞点。
- `意外与发现 (Surprises & Discoveries)`：记录隐藏约束和实施中的新发现。
- `决策记录 (Decision Log)`：记录关键取舍以及原因。
- `结果与复盘 (Outcomes & Retrospective)`：记录最终结果、剩余风险和后续建议。

## 写作规则

- 优先写可观察结果，不要只写模糊的实现意图。
- 明确点名文件、目录、脚本、命令、日志和产物。
- 每个里程碑都要写清楚完成后怎么判断成功。
- 说明哪些内容在范围内，哪些明确不做。
- 不要写“见前文讨论”“按聊天所说”这类依赖上下文的表述。
- 命令如果必须在特定目录执行，要明确写出来。
- 如果任务影响文档、工具、CI、UI、构建、启动、录像回放、RenderDoc 或自动化，也要在 `验证 (Validation)` 章节覆盖。

## 文件放置

- 活跃计划放在 `docs/exec-plans/active/`。
- 已完成计划放在 `docs/exec-plans/completed/`。
- 技术债、清理项和暂不处理的问题记录在 `docs/exec-plans/tech-debt-tracker.md`。
- 文件名建议使用日期前缀，例如 `2026-04-28-enable-harness-entrypoints.md`。
- `PLANS.md` 只定义协议，不维护任务列表。

本仓库采用 ExecPlan 目录规范，根目录为 `docs/exec-plans/`。

## 状态与关闭标准

每份 ExecPlan 应在文件中明确当前状态：`draft`、`active`、`blocked` 或 `completed`。

关闭一份 ExecPlan 前，至少满足：

- `验证 (Validation)` 中的自动或人工检查已经执行，并记录结果。
- `进展 (Progress)` 中没有未解释的关键待办。
- `决策记录 (Decision Log)` 已补齐重要取舍。
- `结果与复盘 (Outcomes & Retrospective)` 已记录最终结果、遗留问题和后续建议。
