# ReSTIR DI 固定 1 SPP 精简

状态：completed  
开始日期：2026-09-13  
负责人：Codex

## 摘要 (Summary)

把 `Samples/RayTracing/RestirDI` 的正式 ReSTIR DI 路径固定为论文的最简交互配置：每像素一个 Reservoir、一份历史以及一条最终可见性射线。删除 1～8 条独立 lane、Independent RIS 生产变体的设置、资源、调度、累加和验证代码。Reference RayGen 只作为内部数值基准保留，样本数固定，不暴露为生产设置。

用户可见结果是 UI 不再提供正式路径的 SPP 滑块，代码中不存在多 lane 循环，运行成本不再随该参数变化。无偏 Algorithm 6、三候选域、Temporal/Spatial reuse、相机控制和内部 Reference 对比保持不变。

## 背景 (Context)

当前实现为了实验加入 `samplesPerPixel=1..8`：CPU 为每条 lane 顺序执行完整 Initial、Temporal、Spatial、Visibility 链，每条 lane 维护独立历史，最终在 HDR Buffer 中平均。这不是原论文最基本的实时 1 SPP 交付，并引入 `reservoirLaneCount`、`reservoirHistoryIndices`、`sampleIndex`、额外常量字段和 SPP 扫描验证。

修改范围限于 `Samples/RayTracing/RestirDI/**`、对应知识文档和 ExecPlan。保留当前尚未提交的 Algorithm 6 无偏修正。禁止修改 `DSMEngine/Runtime/Graphics/**`、`ThirdParty/**` 及其他用户改动。论文核对依据为 `D:/Notes/ReSTIR/Bitterli_2020_ReSTIR_Original.pdf`：交互配置采用 N=1，初始候选与时空 Reservoir 仍独立于最终 spp。

## 实施计划 (Implementation Plan)

1. 收敛公开设置：删除 `Settings::samplesPerPixel`、Independent RIS 模式和正式路径 SPP 滑块；Reference 样本数改为内部固定常量。
2. 收敛 GPU 协议：删除 lane/sampleIndex/sampling 常量，固定 Reference 常量；移除所有以 lane 混入的随机种子。
3. 收敛资源和调度：固定三组 Reservoir Buffer（旧历史及两个工作 Buffer），使用单个 history index；每帧只记录一次 Initial、可选 Temporal、可选 Spatial 和一次 Visibility。
4. 收敛 Shader：Visibility 直接写 `emissive + contribution * W * visibility`，删除跨 lane HDR/acceptance 累加和 `1/N`。
5. 收敛自动验证：删除 2/4/8 SPP 图像及收敛 Gate，新增“正式路径每像素可见性样本不超过 1”的固定预算检查；保留 Reference 和数值误差 Gate。
6. 更新 Markdown/HTML，明确正式路径固定 1 SPP，并把此前多 lane 方案标记为已移除。

## 验证 (Validation)

从 `D:\Code\DSMEngine` 执行：

1. `git diff --check`，检索 `samplesPerPixel`、`reservoirLaneCount`、`reservoirHistoryIndices`、`sampleIndex` 和 Shader `sampling`，确认正式多 lane 代码清零；确认未改 `Runtime/Graphics` 与 `ThirdParty`。
2. `xmake f -m debug`，`xmake build RayTracing`，`xmake build PBR`。
3. `xmake f -m release`，`xmake build RayTracing`，最后恢复 debug 配置。
4. 运行 `bin/debug/RayTracing/RayTracing.exe --validate-render --output build/verification/restir-di-one-spp/2026-09-13/attempt-1`，要求退出码 0、固定 1 SPP Gate 和 DebugLayer 通过。
5. 运行 `bin/debug/RayTracing/RayTracing.exe --validate-editor --frames 120 --output build/verification/restir-di-one-spp/2026-09-13/editor-attempt-1`，要求退出码 0。
6. 打开最终 ReSTIR、Reference、Support ratio、Motion 和 Alpha 图，检查能量、阴影、支持域、拖影和 Alpha 轮廓。

## 进展 (Progress)

- [x] 对齐固定 1 SPP 外部契约并定位全部多 lane 依赖。
- [x] 精简设置、CPU 调度、GPU 协议和 Shader。
- [x] 精简验证器并更新文档。
- [x] 完成静态、Debug/Release、Render、Editor 与视觉验证。
- [x] 归档计划。

## 意外与发现 (Surprises & Discoveries)

- 当前正式多 SPP 是 CPU 顺序重复完整 ReSTIR 链，不只是增加一条阴影射线，因此它同时放大 Compute、资源绑定、历史显存和 Visibility 成本。
- 用户追加要求“只保留原始论文一致的最精简实现”后，Independent RIS 不再作为生产 RenderMode；Reference 仅保留在验证器内部，避免删除正确性基准。
- 实际 Xmake 目标名为 `RayTracing`，不是旧计划中的 `RestirDI`；本次没有改变构建图。
- Editor 首次验证从仓库根启动时找不到相对字体；统一把运行目录切到 exe 部署目录后解决。
- Editor 第二次验证命中 ImGui DX12 多窗口后端等待 fence 0 的警告；自动验证不需要平台辅助窗口，关闭该标志后主窗口 DockSpace、Viewport、UI 与 Present 仍完整覆盖，且 DebugLayer 清零。

## 决策记录 (Decision Log)

- 2026-09-13：正式 ReSTIR 固定 1 SPP；移除 Independent RIS 生产模式；Reference 样本数固定且仅供验证器使用。
- 2026-09-13：保留三个 Reservoir Buffer，而不是退化到两个。Temporal 同时读取旧历史和当前 Initial 输出，随后 Spatial 还需要独立输出，三 Buffer 是无整屏复制下的最小安全集合。

## 结果与复盘 (Outcomes & Retrospective)

正式渲染现为单链固定 1 SPP：三个 Reservoir Buffer 足以容纳旧历史和两个 ping-pong 工作集，历史索引由 vector 收敛为一个整数；Frame Constants 删除 `sampling`，Visibility 删除 HDR 跨 lane 读改写和 `1/N`；UI、验证器和文档中的 1～8 SPP/Independent RIS 分支均已移除。

全量回归通过：Debug/Release `RayTracing`、Debug `PBR` 均编译成功；Render 验证见 `build/verification/restir-di-one-spp/2026-09-13/attempt-2`，退出码 0、正式 SPP=1、最大 Visibility=1、平均亮度相对误差 0.003270、分块 NRMSE 0.039933、D3D12/DXGI 0 消息；Editor 验证见 `editor-attempt-4`，120/120 帧、相机移动和捕获通过、D3D12/DXGI 0 消息。

视觉检查确认能量、阴影、环境、自发光、Alpha 轮廓、运动与支持域图方向正确。1 SPP 最终图仍有明显可见性噪声，这是当前不启用 visibility reuse/denoiser 的预期方差，不应伪装成已达到 Reference 的像素级平滑度。
