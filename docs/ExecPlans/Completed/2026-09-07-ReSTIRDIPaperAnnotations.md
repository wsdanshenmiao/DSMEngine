# ReSTIR DI 论文映射与代码注释执行计划

## Summary

- 目标：解释当前 `Samples/RayTracing/RestirDI` 的完整实现，并在核心 C++/HLSL 中加入中文算法注释，标明与 Bitterli 等人 2020 年 ReSTIR DI 原论文的章节、公式和算法对应关系；同时用 HLSL 枚举替换 Shader 中适合具名化的模式、来源和标志位常数。
- 范围：只补充 RestirDI Sample、Shader 注释和本执行计划；不得修改算法行为、`DSMEngine/Runtime/Graphics/**` 或 `ThirdParty/**`。
- 当前状态：`ready-to-archive`，代码、构建和固定场景验证均已完成。

## Context

当前实现采用实用有偏 ReSTIR DI：DXR 生成主表面，Compute 完成 Initial RIS、Temporal 和 Spatial 重采样，DXR 对最终样本追踪可见性射线，全屏 Pass 显示结果。需要把代码中的 `q`、`pHat`、`weightSum`、`M`、`W`、重采样权重和最终估计量，与原论文第 3 至第 5 节建立可核对的映射，同时说明本项目的多候选域、M-capping、SPP lanes 等工程扩展。

## Implementation Plan

1. 获取并完整检索原论文，整理章节、公式与 Algorithm 1-5 的准确含义。
2. 审查场景转译、候选生成、Reservoir 更新、Temporal/Spatial、Visibility、SPP 和 History 生命周期。
3. 在核心 C++/HLSL 函数前及关键公式旁添加中文注释；明确哪些是论文公式，哪些是实用有偏近似或项目扩展。
4. 为 Source Type、Render Mode、Debug View、Surface Flags 和解析灯类型增加 HLSL 枚举，并与 C++ 共享定义逐项核对。
5. 运行 `git diff --check`、Debug 构建和 `--validate-render`，确认枚举替换不改变行为。
6. 更新结果与证据并归档计划。

## Validation

- `git diff --check`。
- 检查本任务 diff 仅包含注释和计划文档。
- `xmake f -m debug`、`xmake build RayTracing`。
- 从 `bin/debug/RayTracing` 运行 `RayTracing.exe --validate-render --output <attempt>`。

## Progress

- [x] 已加载项目规则和 PDF 技能。
- [x] 完成论文定位与实现映射（原论文本地副本：`D:\Notes\ReSTIR\Bitterli_2020_ReSTIR_Original.pdf`；已核对 Eq. (2)、(5)、(6)、Algorithm 1--5 及第 5 节偏置复用选择）。
- [x] 完成代码注释。
- [x] 完成静态、构建和运行验证。

## Surprises & Discoveries

- HLSL 工程已经使用 DXC Shader Model 6.6，支持无底层类型声明的 `enum`；实际创建 DXR/Compute Pipeline 的运行验证确认枚举语法可用。
- Debug 链接阶段生成约 70 MB PDB，耗时约 335 秒；这是链接器开销，不是编译或 Shader 错误。
- 本次枚举替换保持所有协议值不变：SourceType 0/1/2/3、RenderMode 0/1/2、DebugView 0--11、Primary/Shadow mask 1/2。验证指标与前一轮标准化结果一致。
- Editor 验证从仓库根目录启动时会因相对字体路径触发 ImGui 断言；改从 `bin/debug/RayTracing` 启动后通过，未改动 Editor 或字体加载逻辑。

## Decision Log

- 注释使用“论文节/公式/算法 + 当前实现差异”的形式，不逐行复述显而易见的语句。
- 不把当前实用有偏版本误标为论文严格无偏版本。
- 仅对具有封闭语义集合的 Shader 常数使用 enum；线程组尺寸、绑定槽、数学常数和位移量继续保留原有形式。

## Outcomes & Retrospective

- 改动位置：`RestirDIShared.h`、`RestirDISettings.h`、场景/别名表/环境/管线/验证 C++，以及 `Shaders/RestirDICommon.hlsli`、`RestirDICompute.hlsl`、`RestirDITrace.hlsl`。
- 静态：`git diff --check` 通过；本任务未修改 `DSMEngine/Runtime/Graphics/**` 或 `ThirdParty/**`。
- Debug：`xmake f -m debug` 与 `xmake build RayTracing` 通过，最终退出码 0。
- 运行：最终二进制的 `build/verification/restir-di-paper-annotations/2026-09-07/attempt-3/render/status.raw.json` 为 `passed=true`、`exit_code=0`；D3D12/DXGI 消息为空，数值和 SPP 检查通过。
- Editor：最终二进制的 `build/verification/restir-di-paper-annotations/2026-09-07/attempt-3/editor/status.raw.json` 为 `passed=true`、120/120 帧、120/120 UI 帧、相机移动成功、捕获成功，D3D12/DXGI 消息为空。
- 关键指标：有限值比例 1.0；有效 Reservoir 比例约 0.999964；Temporal 接受率约 0.999964；空间 accepted/rejected 同时存在；ReSTIR 与参考图平均亮度相对误差约 3.14%，分块 NRMSE 约 6.25%。SPP 1/2/4/8 的色调映射 RMSE 分别约 0.200/0.154/0.127/0.103，随 SPP 总体下降。
- 仍需在说明中明确：当前实现是论文 Algorithm 5 的实用有偏变体；没有实现第 4.3/4.4 节的 MIS 修正和无偏多像素合并，也没有启用初始可见性复用。
