# 验证入口

本文定义仓库级验证协议：如何找到稳定验证入口、验证产物应如何组织、失败时按什么路线排查。

具体测试用例、fixture、场景、阈值和命令参数不在本文维护；这些内容放在 `docs/guides/verification-workflows.md`、对应 workflow 文档或未来的 `tests/*/README.md` 中。

## 稳定入口

从改动范围反推最小可证明验证：

- 文档、Harness 目录或 README 改动：验证文档可读性、路径引用、目录形态和 `git diff --check`，不要伪造运行态结论。
- C++ 源码或头文件改动：先运行 `xmake` 或 `xmake build PBR`。
- 渲染、着色器、资源生命周期、场景或编辑器行为改动：先运行 `xmake build PBR`，再运行 `xmake run PBR` 做人工观察。
- 构建图、目标或工程生成规则变化：运行 `xmake`；需要刷新 Visual Studio 工程时，再运行 `xmake project -k vsxmake2022`。
- release、性能、优化或配置敏感变更：运行 `xmake f -m release && xmake`，必要时再切回 debug 配置。
- 多阶段、跨模块或需要多条验证路径的任务：在 `docs/exec-plans/active/` 的 ExecPlan 中写明验证矩阵。

新增或改造 gate 时，必须同时明确：

- 入口脚本或命令所在位置。
- 默认工作目录。
- 输出目录规则。
- 退出码语义。
- `status.raw.json` 的关键字段。
- 失败时优先读取的日志、截图、trace 或中间产物。

## 产物协议

验证产物分为临时产物和持久记录：

- 临时产物放在生成目录或任务专属输出目录中，例如 `build/verification/<task-slug>/`。
- 持久记录写入当前 ExecPlan 的 `验证 (Validation)`、`进展 (Progress)` 或 `结果与复盘 (Outcomes & Retrospective)` 章节。
- 没有 active ExecPlan 的轻量任务，可以把独立验证记录放入 `docs/reviews/`。
- 不要把大型日志、截图、trace 或二进制产物直接放进 `docs/`。
- 如果验证产物必须长期保留，只在 `docs/` 中保存摘要、路径、命令和结论。

推荐的 `status.raw.json` 字段：

```json
{
  "gate": "xmake build PBR",
  "status": "pass | fail | blocked",
  "cwd": "<repo-root>",
  "command": "<exact command>",
  "started_at": "<timestamp>",
  "ended_at": "<timestamp>",
  "exit_code": 0,
  "artifacts": [],
  "diagnostics": []
}
```

## 运行约束

- 默认从仓库根目录执行验证命令。
- 命令如果必须在特定目录执行，必须在 ExecPlan 或验证记录中明确写出。
- 不要因为构建耗时就声称已运行；未运行必须说明原因。
- 不要清理与当前任务无关的 `bin/`、`build/`、`.xmake/` 或用户输出。
- 不要覆盖 `Projects/` 中的场景或资源，除非任务明确要求。
- `xmake run PBR` 属于人工观察入口，必须记录场景、视角、操作路径和观察结果。
- 涉及 RenderDoc、截图、录像回放或其他外部工具时，记录工具版本、输入文件、输出目录和检查结论。

## 失败排查路线

1. 定位输出目录，记录命令、工作目录、退出码和运行时间。
2. 先读 `status.raw.json`；没有时，确认 `result`、失败 gate、`diagnostics` 和关键 artifact 路径。
3. 再读主日志和辅助证据，按时间线确认失败发生在配置、编译、链接、启动、加载、流程推进、采样、阈值判定还是收口阶段。
4. 将失败归类为输入错误、环境错误、资源缺失、流程未到达、阈值回归、运行期崩溃、超时或疑似 flaky。
5. 只在能增加信息量时重跑；重跑时保留新旧输出目录，避免覆盖失败证据。
6. 可复用的排查经验写入 `docs/knowledge/`；暂不处理的问题写入 `docs/exec-plans/tech-debt-tracker.md`；复杂修复写入 `docs/exec-plans/active/`。

## 文档分工

- `docs/verification.md`：只维护仓库级验证协议、产物规则和失败排查路线。
- `docs/guides/verification-workflows.md`：维护 DSMEngine 常用验证 workflow。
- `docs/exec-plans/active/`：维护复杂任务的验证矩阵和实时证据。
- `docs/reviews/`：保存独立 review 和轻量验证记录。
- `docs/knowledge/`：保存可复用排查经验。
- `docs/exec-plans/tech-debt-tracker.md`：保存延期处理的问题。
