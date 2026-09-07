# ReSTIR DI 标准化修正执行计划

## Summary

- 目标：审查并修正 `Samples/RayTracing/RestirDI`，使其遵循标准 ReSTIR DI 的候选采样、Reservoir 更新、时间/空间复用和最终估计定义。
- 范围：优先修改 RestirDI Sample 及其 HLSL；不得修改 `DSMEngine/Runtime/Graphics/**` 或 `ThirdParty/**`。
- 当前状态：`completed`。

## Context

当前实现已经包含 DXR 主可见性/阴影、Compute 初始 RIS、时间重用、空间重用和 SPP 循环，但需要核对以下统计契约：候选域混合 PDF、`pHat/q` 权重、Reservoir 的 `M/W` 归一化、重用时的目标函数重算、历史有效性与样本上限，以及 SPP 是否为独立估计量平均。此前出现的高噪声和 SPP 性能/质量异常可能由这些契约不一致或无效候选进入 Reservoir 引起。

## Implementation Plan

1. 读取并标注当前 C++/HLSL 数据流和资源生命周期，确认修改边界。
2. 统一候选记录与评估结果：显式区分候选的 proposal PDF、未遮挡目标函数 `pHat` 和 RGB 未遮挡贡献；所有无效/非有限候选不得改变有效 Reservoir 的统计量。
3. 修正 Reservoir 合并与 `M` 上限：保持 `weightSum/M/W/selectedPHat` 内部一致，限制来源 Reservoir 时按有效来源样本数缩放，避免超过上限或以零权候选污染统计。
4. 修正时间/空间重用门控和重投影，保证只有当前表面重新评估有效的历史候选才参与合并；保留标准实用 ReSTIR DI 的有偏空间复用定义。
5. 核对 SPP：每个 lane 必须是独立的完整 ReSTIR DI 估计量，最终 RGB 只平均直接光而不重复平均环境背景；避免改变 SPP 后复用错误历史或累加错误的 Reservoir。
6. 运行静态检查、Debug/Release 编译和可用的验证入口；若失败，保留证据并按根因迭代。

## Validation

- `git diff --check`。
- 检索 RestirDI 是否误用 Forward/Deferred/CommonPass，以及 `Runtime/Graphics/**` 是否保持未修改。
- Debug：`xmake f -m debug`、构建 RayTracing/RestirDI 实际目标。
- Release：`xmake f -m release`、构建实际目标。
- 若设备/运行环境允许，执行 `--validate-render` 和 `--validate-editor`；否则记录阻塞原因，不宣称运行验证通过。

## Progress

- [x] 已加载 `AGENTS.md`、`PLANS.md` 和 `docs/verification.md`。
- [x] 已完成第一轮代码审查，确认实现是实用 ReSTIR DI 框架而非可直接视为严格标准实现。
- [x] 已修正 proposal PDF、Reservoir 合并、M-capping、复用门控和数值健壮性。
- [x] 已补齐自发光分布变化的历史失效判断。
- [x] 已完成静态检查、Debug/Release 编译、PBR 回归、Render 与 Editor DebugLayer 验证。
- [x] 已人工检查 ReSTIR、Reference、1/8 SPP、Source Debug 和 Alpha 输出图。

## Surprises & Discoveries

- 当前工程的 xmake 目标仍由 `Samples/RayTracing/xmake.lua` 编入 `RayTracing`，RestirDI 是该目标内的独立管线，而不是单独的 xmake binary。
- RHI 命令列表在绑定集设置时会自动申请资源状态；本计划不修改 RHI。
- 旧实现把总 M 限制成“剩余容量”；Temporal 达到上限后 Spatial 的 `sourceM` 恒为零，导致空间复用实际上停止。
- 旧实现用 `max(pdf, 1e-6)`、`max(area, 1e-6)` 和 `max(solidAngle, 1e-6)` 改写真实概率密度；环境贴图极区及小三角形会因此得到错误权重。
- 旧实现只在场景拓扑、材质或光源功率分布变化时清历史；缩放自发光实例会改变三角形面积分布，却没有使历史失效。
- 原 SPP 质量测试给 1 SPP 预热 64 帧、其余档位仅 32 帧，比较条件不等价；此外单次随机估计的逐档 raw-pixel NRMSE 不保证严格单调。
- Editor 验证必须从 `bin/debug/RayTracing` 启动，因为 Editor 字体使用相对于可执行目录的 `Assets/Fonts/Roboto.ttf`。

## Decision Log

- 采用标准实用（biased）ReSTIR DI：初始候选用 `w=pHat/q`，重用候选用 `pHat_current * W_source * M_source`，最终用 `W * contribution * visibility`。
- 不把空间重用改成无偏 pairwise-MIS 版本；先保证论文中常用的实用 ReSTIR DI 统计定义和代码一致。
- M-capping 在每次 Temporal/Spatial 合并完成后缩放 `weightSum` 和 `M`，保持 `W` 不变；Temporal 输入仍按历史上限裁剪来源置信度。
- Temporal 使用稳定对象 ID、材质、法线和深度验证重投影；Spatial 不要求同一对象，只使用材质、法线和深度相似性，以允许标准的跨表面邻域复用。
- SPP 比较统一使用 32 帧预热；判据要求高 SPP 相对 1 SPP 总体改善、能量稳定，并允许单次随机样本的中间档位波动。

## Outcomes & Retrospective

- `git diff --check` 通过；RestirDI 中未引用 Forward、Deferred、CommonPass 或 RenderResource，`DSMEngine/Runtime/Graphics/**` 与 `ThirdParty/**` 均无修改。
- Debug：`xmake f -m debug`、`xmake build -r RayTracing`、`xmake build PBR` 全部通过。
- Release：`xmake f -m release`、`xmake build RayTracing` 通过；之后已恢复 Debug 配置并再次构建成功。
- Render 验证：从 `bin/debug/RayTracing` 执行 `RayTracing.exe --validate-render --output D:\Code\DSMEngine\build\verification\restir-di-standardization\2026-09-07\attempt-6\render`，退出码 0。有限像素比例 1.0、有效 Reservoir 0.999964、Temporal 接受率 0.999964、Spatial 接受/拒绝分别为 166219/409781；相对 Reference 的平均亮度误差 3.14%，16×16 block NRMSE 6.25%。1/2/4/8 SPP 色调映射 RMSE 为 0.2003/0.1544/0.1271/0.1031。D3D12/DXGI 消息为空。
- Editor 验证：从同一目录执行 `RayTracing.exe --validate-editor --frames 120 ...`，退出码 0；120 帧渲染与 UI 均完成，相机移动和捕获成功，DebugLayer 无警告。
- 人工检查确认 ReSTIR 与 Reference 的光照方向、阴影接触、环境背景一致，8 SPP 明显比 1 SPP 平滑，Source Debug 覆盖三个候选域，Alpha 场景输出有效。
- 遗留限制：这是论文常用的实用有偏 ReSTIR DI，目标函数不含可见性且没有 pairwise/contribution MIS；没有时域/空域降噪器，因此 1 SPP 单帧仍会保留最终可见性噪声。若要求严格无偏，应另立任务实现 MIS bias correction，而不是把本版本描述为严格无偏。
