# ReSTIR DI 标准无偏实现

状态：已完成  
开始日期：2026-09-08  
负责人：Codex

## Summary

将 `Samples/RayTracing/RestirDI` 当前采用实用有偏时空合并的实现，替换为 Bitterli 等人在原始 ReSTIR DI 论文 Algorithm 6 中给出的简单无偏 reservoir 合并。实现使用统一 MIS：合并阶段仍以 `pHat_q(y) * W * M` 选择代表样本，归一化阶段针对最终选中样本重新计算所有输入流的支持质量 `Z`，并使用 `W = weightSum / (Z * pHat_q(y))`。默认配置对齐论文：32 个初始候选、1 次 spatial reuse、3 个邻居、30 px 半径。当前仓库将该目录编入 `RayTracing` 可执行目标，因此所有编译和运行 Gate 均以 `RayTracing` 为目标。

本任务不加入 visibility reuse；最终样本仍只追踪一条可见性射线。因此支持测试只判断候选在各输入表面的目标函数是否非零，不需要为 MIS 额外追踪阴影射线。Temporal history 的 `M` 上限仍保留，因为它是论文用于控制历史长度和响应速度的稳定性策略，不改变支持域归一化的定义。

## Context

当前 `RestirDICompute.hlsl` 的 temporal 与 spatial 阶段直接复制当前 reservoir 的 `weightSum`，再按 Algorithm 4 合并其他 reservoir，最后始终以总 `M` 归一化。这只在所有输入分布对最终样本具有相同支持域时无偏；场景中法线、遮挡关系或 BSDF 支持域不同会导致暗偏。

无偏替换有两个关键约束：

1. 每个输入 reservoir（包括当前像素自身）必须作为一个原子候选，以 `pHat_q(y) * source.W * source.M` 重新参与选择；不能复用来源 reservoir 的原始 `weightSum`。
2. 选出代表样本后必须重放同一组输入来源，计算其中对该样本满足 `pHat_qi(y) > 0` 的 `M` 之和 `Z`，再以 `Z` 而不是总 `M` 归一化。

修改范围限于 `Samples/RayTracing/RestirDI/**`、对应知识文档和本计划。禁止修改 `DSMEngine/Runtime/Graphics/**` 与 `ThirdParty/**`，并保留所有既有用户改动。

## Implementation Plan

1. 重构共享 reservoir 数据和函数：将统计数据中的缓存 `selectedPHat` 改为 `normalizationM`，明确区分初始 RIS 的 `Z=M` 与时空合并的 `Z`；无有效代表样本时仍保留输入流的 `M`。
2. 重写 temporal reuse：从空 reservoir 开始，将当前与历史 reservoir 都作为原子输入合并；选择结束后对同一输入集合计算支持质量 `Z`，再执行 Algorithm 6 归一化。历史 `M` 只在输入前按上限裁剪。
3. 重写 spatial reuse：固定一次 spatial pass；使用可重放的邻居选择随机序列，第一遍合并，第二遍对完全相同的来源计算 `Z`。移除仅为有偏实现服务的几何/材质邻居拒绝，保留边界与有效表面判断。
4. 精简公开设置和 UI：使用 `UnbiasedRestir` 命名，删除多 spatial pass 与未实现 visibility reuse 的设置；加入 `Support M (Z)` 调试视图，默认改为论文配置。
5. 扩展自动验证：读回并检查 `0 < Z <= M`、有限权重和支持修正实际发生；加入支持域不匹配场景/指标，继续与 256 候选 Reference 比较能量和分块误差。
6. 更新中英文知识文档，明确公式、实现映射、限制和参数含义。

## Validation

每次失败保留独立 attempt 证据，修复失败 Gate 后从静态检查重新回归。产物写入 `build/verification/restir-di-unbiased/2026-09-08/attempt-<N>/`。

1. 静态检查：`git diff --check`；审查修改范围；确认未改 `Runtime/Graphics`、`ThirdParty`；检索旧有 biased finalize、重复 spatial pass 和错误的 raw `weightSum` 复制。
2. Debug 编译：配置 debug，构建 `RayTracing` 与 `PBR`。
3. Release 编译：配置 release，构建 `RayTracing`。
4. DebugLayer 渲染：切回 debug，运行 `RayTracing.exe --validate-render --output <attempt>`，要求退出码 0，D3D12/DXGI/RHI 无 Warning、Error、Corruption。
5. Editor 验证：运行 `RayTracing.exe --validate-editor --frames 120 --output <attempt>`，要求退出码 0。
6. 视觉检查：打开完整 ReSTIR、Reference、Source、Support M、运动与 Alpha 图，核对能量、阴影、Alpha、拖影和异常噪点。

## Progress

- [x] 阅读项目规则、验证协议和原始论文 Algorithm 4/6、公式 19/20/22。
- [x] 确认当前实现是 Algorithm 4 风格的实用有偏版本，并定位到 raw `weightSum` 复用及 `M` 归一化问题。
- [x] 完成无偏 reservoir 合并和设置精简。
- [x] 完成自动验证与知识文档更新。
- [x] 完成 Debug/Release、DebugLayer、Editor 与视觉闭环。
- [x] 归档本计划。

## Surprises & Discoveries

- 只把最终分母从 `M` 改成 `Z` 仍然错误：无偏 reservoir 的原始 `weightSum` 不再等于下一阶段所需的原子合并权重，所有 reuse pass 都必须从空 reservoir 重新合并来源 reservoir。
- Algorithm 6 的支持修正是针对来源流而不是代表样本数量；即使某来源 reservoir 当前没有有效代表样本，它生成过的候选数量 `M` 仍应参与总流质量及可能的支持质量计算。
- 当前 xmake 构建图没有独立 `RestirDI` target；`RestirDI/**.cpp` 和 Shader 复制规则属于 `RayTracing` target。最初按旧计划名执行 `xmake build RestirDI` 得到“invalid target name”，随后改用实际目标。
- 固定 seed 下 2 SPP 的单次图像出现了比 1 SPP 更亮的长尾像素；这是无偏估计允许的非单调方差，而不是把结果 clamp 回有偏估计的理由。验证改为检查 4/8 SPP 的总体收敛、有限值和能量误差，并把该限制写入 UI/知识文档。
- 支持测试进一步要求 `HasValidProposalPdf` 与 `pHat>0` 同时成立，避免退化三角形或零立体角候选在 `q=0` 时被错误计入 `Z`；修复后指标保持一致。

## Decision Log

- 2026-09-08：选择原论文 Algorithm 6 的 uniform MIS，而不是 balance heuristic。原因是它是论文给出的最简无偏组合算法，数据需求少且易验证。
- 2026-09-08：不实现 visibility reuse。当前管线只延迟最终一条可见性射线，加入 visibility reuse 会需要额外支持域阴影查询，偏离“标准、简洁”的目标。
- 2026-09-08：固定最多一次 spatial reuse，并将默认邻居数改为 3。原因是与论文无偏配置一致，同时避免多 pass 中来源谱系与支持质量追踪复杂化。

## Outcomes & Retrospective

已完成：

- `RestirDICommon.hlsli`/`RestirDICompute.hlsl` 实现 Algorithm 6 uniform MIS；所有 reuse pass 从空 Reservoir 原子合并，第二遍计算 `Z`。
- 默认配置为 32 初始候选、1 次 spatial、3 邻居、30 px；删除多 pass 和未实现 visibility reuse 开关；增加 `Support ratio Z/M` 调试视图。
- C++/HLSL Reservoir 布局同步为 `normalizationM`，保留大小/偏移静态断言；未修改 Runtime Graphics/RHI 或 ThirdParty。
- Debug/Release `RayTracing`、Debug `PBR` 编译通过；DebugLayer Render 和 Editor 均退出码 0。

最终 Render 证据：`build/verification/restir-di-unbiased/2026-09-08/attempt-4/`。固定验证场景下 HDR 有限值比例 1.0、有效 Reservoir 1.0、`0<Z<=M` 合法率 1.0、`Z<M` 发生于 3343/27822（12.02%）命中像素，平均亮度相对误差 0.007518、16×16 分块 NRMSE 0.036795，D3D12/DXGI 消息 0 条。Editor 证据位于 `editor-attempt-3/`，120/120 帧和 UI 帧、相机移动均通过。

剩余限制：这是未启用 visibility reuse 的直接光无偏估计器，不包含 balance heuristic、间接光、累积器或 denoiser；无偏不保证单帧或每个 SPP 档位单调降噪。若要进一步降低长尾，应改善 proposal 或增加统计累积，而不是改动 `Z` 归一化。
