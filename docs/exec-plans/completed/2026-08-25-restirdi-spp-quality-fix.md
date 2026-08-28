# RestirDI SPP 质量退化修复

状态：completed

## 摘要 (Summary)

修复 RestirDI 提高 SPP 后画面质量可能下降的问题。当前实现只让第 0 条样本经过 ReSTIR 时空复用，额外 SPP 使用高方差的局部 Initial RIS Reservoir，再与低方差复用结果等权平均。目标实现让每条 SPP 都拥有独立随机流和独立历史，并完整执行 Initial、Temporal、Spatial 与 Visibility；输出按 SPP 正确平均，使固定场景下增加 SPP 能降低误差和噪声。

继续保持 `DSMEngine/Runtime/Graphics/**`、`ThirdParty/**` 只读，保留用户已有 `Samples/RayTracing/main.cpp` 与 `TestTriangle.h` 改动。

## 背景 (Context)

- 问题代码位于 `Samples/RayTracing/RestirDI/Shaders/RestirDITrace.hlsl`：lane 0 读取最终复用 Reservoir，lane 1..N-1 调用 `GenerateIndependentReservoir`，后者只执行 Initial RIS。
- 两类估计虽可平均，但方差差异很大；额外的高方差估计可能使有限 SPP 的结果比单条低方差 ReSTIR 估计更差。
- 原自动验证只要求 SPP=4 的 visibility count 达到 4、HDR 有限且能量非零，没有测量相对 Reference 的像素误差是否随 SPP 下降。
- P4 skill 无法在该非 UE/Git 工程检测到 P4 workspace，本任务在现有 Git 工作区内实施。

## 实施计划 (Implementation Plan)

### 里程碑 1：独立完整 ReSTIR 样本流

- 为当前 SPP 动态创建 `SPP + 2` 组 Reservoir Sample/Stats Buffer：每条 lane 保留一组历史，另外两组作为共享 Ping-Pong 工作缓冲。
- 每帧顺序处理所有 lane；每条 lane 均执行 Initial RIS、对应历史的 Temporal Reuse、同 lane 邻域的 Spatial Reuse及一条 Visibility Ray。
- 每条 lane 完成后用句柄交换把最终 Reservoir 变为该 lane 的下一帧历史，把旧历史归还工作池；不做整屏历史复制。
- SPP 或分辨率变化时按实际 SPP 重建采样资源并清空全部 lane 历史；默认 1 SPP 的资源规模和原实现接近。

### 里程碑 2：正确累积与调试数据

- Frame Constants 记录当前 lane；Initial、Temporal、Spatial 的随机种子都混入 lane，避免不同 SPP 相关。
- Visibility 每次只处理当前 lane，将直接光除以总 SPP 后顺序累加；第 0 条 lane 写入背景/自发光基值，其余 lane 只增加直接光。
- 使用已有自动 UAV barrier 保序，并在 Sample 侧显式声明 HDR/Acceptance UAV 依赖；不修改 Graphics/RHI。
- 单独的 Acceptance Aggregate Buffer 汇总所有 lane 的 Temporal、Spatial 与 Visibility 计数，验证读回继续保持每像素一项。

### 里程碑 3：质量回归与闭环验证

- 自动生成同一验证场景的 1、2、4、8 SPP 图片，以相同预热策略与高 SPP Reference 比较。
- 增加像素亮度 NRMSE，并要求高 SPP 相对 1 SPP 明确改善，同时检查平均能量没有随 SPP 缩放。
- 执行静态检查、Debug/Release RestirDI、PBR、VS 工程、DebugLayer Render、120 帧 Editor 和人工目检；失败则保留 attempt 并循环修复。

## 验证 (Validation)

证据目录：`build/verification/restir-di-spp-quality/2026-08-25/attempt-<N>/`。

- `git diff --check`
- 检查 `Runtime/Graphics/**`、`ThirdParty/**` 无改动，两个受保护文件 SHA256 不变。
- 检查 RestirDI 无 Forward、Deferred、CommonPass 或 RenderResource 引用。
- `xmake f -m debug`、`xmake build RestirDI`、`xmake build PBR`、`xmake project -k vsxmake2022`
- `xmake f -m release`、`xmake build RestirDI`，随后恢复 Debug。
- `RestirDI.exe --validate-render --output <attempt>/render`
- `RestirDI.exe --validate-editor --frames 120 --output <attempt>/editor`
- 检查 D3D12、DXGI、RHI 零 Warning/Error，检查 1/2/4/8 SPP 的有限值、样本计数、能量和 Reference 误差。
- 目检 `spp-1.bmp`、`spp-2.bmp`、`spp-4.bmp`、`spp-8.bmp` 与 `reference.bmp`，确认噪声随 SPP 总体下降且无亮度漂移、拖影或新伪影。

最终证据位于 `build/verification/restir-di-spp-quality/2026-08-25/attempt-9/`：

- 静态检查通过：`git diff --check` 为 0；`Runtime/Graphics/**`、`ThirdParty/**` 无改动；受保护文件 SHA256 未变化；RestirDI 无 Forward、Deferred、CommonPass 或 RenderResource 引用。
- Debug RestirDI、Debug PBR、`vsxmake2022`、Release RestirDI 均构建成功，最终配置已恢复 Debug。
- Render 验证退出码为 0，`numeric_passed`、`spp_passed`、`spp_quality_passed`、`debug_layer_passed` 全部为 true。
- 1/2/4/8 SPP 的实际最大 Visibility 样本计数分别为 1/2/4/8；平均亮度分别为 0.20907、0.20998、0.20960、0.20906，没有随 SPP 发生能量缩放。
- 相对 8×512、有效 4096 SPP Reference，HDR pixel NRMSE 为 0.28133、0.20888、0.16099、0.11421；色调映射 RMSE 为 0.21399、0.16252、0.13248、0.10792，两条曲线均随 SPP 逐级下降。
- Editor 验证退出码为 0，120 帧 Render/UI 全部执行，`camera_moved` 为 true；Render 与 Editor 的 D3D12、DXGI、RHI 消息均无 Warning/Error。
- 目检 1/2/4/8 SPP、Reference 与 Editor 图片：采样噪声随 SPP 明显下降，平均亮度、阴影方向、环境、自发光和 Alpha 轮廓保持稳定，无新增拖影或累积伪影。

## 进展 (Progress)

- [x] 读取项目规则、P4 skill 与当前 ReSTIR 数据流。
- [x] 定位混合低方差复用 Reservoir 与高方差 Initial-only Reservoir 的根因。
- [x] 实现每条 SPP 的完整独立 ReSTIR 历史与顺序累积。
- [x] 增加质量单调性验证。
- [x] 完成全部 Gate、视觉验收并归档。

下一步：无；本计划关闭。

## 意外与发现 (Surprises & Discoveries)

- 上一轮的 SPP 验证证明了射线数量，却没有证明质量；`max_visibility_samples == SPP` 不能替代误差或方差指标。
- 为每条 lane 各分配三组 Reservoir 会产生 `3×SPP` 显存增长；使用 `SPP` 组历史加两组共享工作 Buffer 可将其降为 `SPP+2`。
- attempt-1 暴露 DXC 不支持结构体三元选择，改用显式初始化与分支；失败证据已保留。
- attempt-2 初次有效结果中，色调映射 RMSE 随 1/2/4/8 SPP 从 0.2435、0.2035、0.1807 降至 0.1646，且平均亮度保持约 0.209。
- Reference RayGen 原先遗漏 frameIndex，导致每帧重复同一随机序列；修复后 8 帧 × 512 SPP 的参考平均真正等效 4096 个独立参考样本。
- 干净 Reference 下，HDR pixel NRMSE 为 0.2813、0.2089、0.1610、0.1142，色调映射 RMSE 为 0.2140、0.1625、0.1325、0.1079，两条曲线均随 SPP 下降。
- attempt-5 的 Debug 链接因 `mspdbsrv` 退出而挂起；只中断本轮 xmake/link 后，attempt-6 增量链接成功。该问题不涉及源码或运行结果，失败日志保留。

## 决策记录 (Decision Log)

- 2026-08-25：不把 SPP 退化为 Initial Candidate Count；继续保持真正的多最终可见性样本语义。
- 2026-08-25：不保留 Initial-only 额外 lane fallback；所有 ReSTIR lane 使用相同完整算法，保证外部契约一致。
- 2026-08-25：资源按当前 SPP 动态分配，而不是始终按最大 8 SPP 分配。
- 2026-08-25：质量 Gate 同时约束 HDR pixel NRMSE、色调映射 RMSE、平均能量和实际 Visibility 样本数，避免再次用“射线数正确”替代“质量正确”。

## 结果与复盘 (Outcomes & Retrospective)

根因已经消除：不再把低方差的完整 ReSTIR 估计与高方差 Initial-only 估计混合。每条 SPP 都拥有独立随机流、独立上一帧 Reservoir，并执行相同的 Initial、Temporal、Spatial 和 Visibility 流程；多条直接光估计按 SPP 正确平均。

资源采用当前 `SPP + 2` 组 Reservoir Sample/Stats：SPP 组保存 lane 历史，两组供逐 lane Ping-Pong；通过句柄交换保留历史，不做全屏复制。相对原错误实现，计算量现在会随 SPP 近似线性增长，Reservoir 历史显存也随 SPP 增长，这是获得真实完整 ReSTIR 多样本的必要成本；默认 1 SPP 保持原有资源规模，UI 上限 8 避免误设极端负载。

验证系统现在同时检查样本数、有限值、能量、HDR NRMSE 和可感知 RMSE 的 SPP 趋势，并使用真正独立的 4096 有效 SPP Reference。没有遗留功能阻塞。
