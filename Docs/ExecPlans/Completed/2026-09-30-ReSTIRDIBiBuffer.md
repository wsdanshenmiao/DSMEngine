# ReSTIR DI Reservoir 双缓冲

- 状态：`completed`
- 开始日期：2026-09-30
- 完成日期：2026-09-30
- 工作目录：`D:\Code\DSMEngine`

## 摘要 (Summary)

将 `Projects/RayTracing` 当前 ReSTIR DI 的 `reservoirSamples` 与 `reservoirStats` 从三个整屏 GPU Buffer 改为各两个。Initial RIS 写入当前帧工作槽；Temporal 在工作槽对应的 `RWStructuredBuffer` 中按像素读局部值并原地写回，不把同一物理 Buffer 同时作为当前 SRV 和输出 UAV；Spatial 从稳定工作槽 SRV 读取中心及邻居并写入另一槽。保留无偏 Algorithm 6、1 spp、最终 visibility、Surface/acceptance/HDR 布局与 C++/HLSL ABI。

同时扩展 `--validate-render`，覆盖 Temporal/Spatial 四种组合。每个组合在 Reset 后采集首帧和 3 个后续帧，检查有限 HDR、有效 Reservoir、`M > 0`、`0 < Z <= M`、visibility 计数及预期 Temporal/Spatial 接受状态，并保留 Reference、motion、Resize、shadow、HDR 与 DebugLayer 检查。

## 背景 (Context)

当前入口位于 `Projects/RayTracing/Source/RayTracing/RestirDI/RestirDIRenderPipeline.cpp`，资源数组原为三个 Reservoir 槽，CPU 以 `workA/workB` 轮换；`RestirDICompute.hlsl` 的 Temporal 原为读取 `g_ReservoirCurrent*` SRV 并写入另一槽 UAV。当前主工作树 HEAD 为 `3cbee26da7f16991b586b68d61d9bddcbc0791ee`，用户已有改动位于 `Projects/RayTracing/Content/` 与 `RayTracing.dsmproj`，实施期间未覆盖或清理。

目标槽位规则：

- `work = oldHistory >= 0 ? 1 - oldHistory : 0`；Initial 写 `work`。
- Temporal 若启用，读/写 `work`，历史从 `oldHistory` SRV 读取。
- Spatial 若启用，读稳定当前槽 `work`，写 `1 - work`。
- 最终 `lastReservoir`/`reservoirHistory` 指向实际完成的槽。
- Reset、Resize、Reference 及 Temporal/Spatial 四组合均不得越界或读取无效历史。

明确不修改：`Engine/ThirdParty/`、Graphics/RHI、构建图、acceptance/surface/HDR GPU ABI。

## 实施计划 (Implementation Plan)

1. **资源与调度契约**
   - 将 `reservoirSamples`、`reservoirStats` 改为 `std::array<..., 2>`。
   - 重写 Initial/Temporal/Spatial 槽位计算，删除三槽 `workA/workB` 逻辑。
   - Temporal BindingSet 不绑定当前槽 SRV；以输出 UAV 作为当前槽唯一访问路径。历史继续绑定 SRV。
   - 通过现有自动资源状态跟踪确保 Initial→Temporal 的 UAV 顺序、Temporal→Spatial 的 SRV/UAV 转换及 visibility 的 SRV 状态。
   - 更新 Capture 读回与 reset/resize 状态。

2. **Shader 实现**
   - Temporal 从 `g_ReservoirSampleOutput[index]`/`g_ReservoirStatsOutput[index]` 读取局部值，再写回同一索引；保持 Algorithm 6 计算。
   - 删除 Temporal 对当前槽 `g_ReservoirCurrent*` SRV 的依赖；Spatial 仍从 `g_ReservoirCurrent*` 稳定 SRV 读取。
   - 保持寄存器、结构体、常量与 ABI 不变。

3. **验证门扩展**
   - 增加四种 Temporal/Spatial 组合的首帧与 3 个后续帧采样，单独输出组合指标/图像。
   - 增加有限 HDR、有效 Reservoir、`M > 0`、`0 < normalizationM <= M`、visibility 上限与预期接受状态断言；保留 Reference、source modes、motion、Resize、shadow、HDR fixture 和 DebugLayer 检查。
   - 运行 `git diff --check`、RayTracing/PBR 实际构建、`--validate-render`、`--validate-editor --frames 120`。

4. **收口**
   - 检查 diff 仅涉及任务范围且用户内容未被覆盖。
   - 更新本计划的验证、决策、结果与复盘；全部 gate 完成后归档。

## 验证 (Validation)

默认工作目录为 `D:\Code\DSMEngine`。临时产物位于 `build/verification/restir-di-two-reservoir-buffers/2026-09-30/attempt-N/`，包括 `status.raw.json`、`metrics.json`、四组合 JSON/BMP、DebugLayer 消息和运行日志。

- 静态：`git diff --check` 通过；检查确认 C++ 两个 Reservoir 数组均为 2 槽、Temporal BindingSet 没有当前槽 SRV，shader 通过输出 UAV 读写当前槽。
- 构建：`xmake build RayTracing` 通过；`xmake build PBR` 通过。未运行 CMake 或 Release 构建，因为本次没有修改构建图，且用户要求的实际目标构建已完成。
- 渲染：
  - 命令：`Projects/RayTracing/Binaries/Debug/RayTracing.exe --validate-render --output build/verification/restir-di-two-reservoir-buffers/2026-09-30/attempt-3/render`
  - `status.raw.json`：`passed=true`、`exit_code=0`、`numeric_passed=true`、`debug_layer_passed=true`、`artifacts_ok=true`。
  - `reuse-combinations.json`：`temporal-spatial`、`temporal-only`、`spatial-only`、`initial-only` 四项全部通过；每项包含 Reset 后首帧和 3 个后续帧，接受状态断言全部为 true。
  - `metrics.json`：`finite_ratio=1`、`valid_reservoir_ratio=1`、`positive_m_ratio=1`、`z_range_ratio=1`、`max_visibility_samples=1`、`reuse_combinations_passed=true`；Reference、motion、Resize、HDR、alpha、shadow 检查均纳入最终数值 gate。
  - `d3d12-dxgi-messages.json` 的 D3D12/DXGI 数组均为空；`runtime-messages.json` 只有 GPU 选择信息，无 warning/error。
- 编辑器：
  - 命令：`Projects/RayTracing/Binaries/Debug/RayTracing.exe --validate-editor --frames 120 --output build/verification/restir-di-two-reservoir-buffers/2026-09-30/attempt-2/editor`
  - `status.raw.json`：`passed=true`、`exit_code=0`、`executed_frames=120`、`rendered_frames=120`、`ui_frames=120`、`camera_moved=true`、`debug_layer_passed=true`；D3D12/DXGI 消息为空。
- 未执行的验证：没有额外的 CMake、Release 或人工 `xmake run` 观察；这些不是本次已完成 gate，不能据此推断其结果。

## 进展 (Progress)

- [x] 已确认工作目录为 `D:\Code\DSMEngine`、HEAD 为 `3cbee26`。
- [x] 已读取 `AGENTS.md`、`PLANS.md`、`Docs/Verification.md` 和 `Docs/Guides/VerificationWorkflows.md`。
- [x] 已检查 Git 状态并确认用户已有 `Projects/RayTracing` 场景/项目/模型/纹理改动，且未回退。
- [x] 已阅读当前 RestirDI C++/HLSL 资源、调度、状态跟踪和验证入口。
- [x] 完成双缓冲 C++/HLSL 实现。
- [x] 完成四组合验证 gate 与 metrics。
- [x] 完成 `git diff --check`、RayTracing/PBR 构建、render 验证和 editor 120 帧验证。
- [x] 完成计划收口并归档。

## 意外与发现 (Surprises & Discoveries)

- 当前 GPU 自动状态跟踪在同一资源保持 UAV 状态时会通过 `RequireBufferState` 生成 UAV barrier；让 Initial 与 Temporal 的 BindingSet 都正确声明同一工作槽 UAV 后，双缓冲路径无需改 Graphics/RHI。
- 当前 Temporal 同时绑定 current SRV 与 output UAV 的三槽实现，改为双槽后必须移除 current SRV，否则同一物理 Buffer 会产生非法 SRV/UAV 别名绑定。
- `--validate-render` 实际运行确认四组合均可用；每组合保存 Reset 后首帧和 3 帧后续读回，所有组合通过 HDR/Reservoir/接受状态断言。
- 双缓冲下实际 DebugLayer 运行未产生 D3D12/DXGI warning/error，Temporal→Spatial 的 SRV/UAV 状态转移有效。

## 决策记录 (Decision Log)

- 2026-09-30：以当前 D 盘源码为唯一写入基线；旧 worktree 仅作只读思路参考，不复制路径或整文件。
- 2026-09-30：不修改 `GpuReservoirSample`、`GpuReservoirStats`、`GpuAcceptance` 或寄存器编号，双缓冲仅是资源数量与绑定/调度变化。
- 2026-09-30：沿用已有自动 barrier 机制，使用资源状态声明触发 UAV barrier 与 SRV/UAV transition，不改底层状态跟踪。
- 2026-09-30：Temporal 组合验证按实际接受语义检查 Reset 首帧为 0、后续帧为正；Spatial 开关关闭时检查接受/拒绝计数为 0，开启时检查接受与拒绝均出现。

## 结果与复盘 (Outcomes & Retrospective)

实现已完成：

- `RestirDIRenderPipeline.cpp` 的 `reservoirSamples`/`reservoirStats` 各从 3 槽减为 2 槽；槽位严格采用 `work = oldHistory >= 0 ? 1 - oldHistory : 0`。Initial 写 work，Temporal 原地更新 work，Spatial 读 work 写 `1 - work`，最终读回使用 `lastReservoir`。
- `RestirDICompute.hlsl` 的 Temporal 不再读取当前 Reservoir SRV，而是从输出 UAV 读局部值后写回；历史 Reservoir 仍通过 SRV 读取；C++/HLSL 结构布局和寄存器 ABI 未改。
- `RestirDIValidation.cpp` 增加四种开关组合、Reset 首帧/3 个后续帧、有限 HDR、`M`/`Z` 范围、visibility 与接受状态检查；保留 Reference、source mode、motion、Resize、HDR fixture、alpha、shadow 和 DebugLayer 检查。

用户已有的 `Projects/RayTracing/Content/` 与 `RayTracing.dsmproj` 改动保持不变；本次新增/修改仅涉及 ReSTIR DI 源码、shader、验证与 ExecPlan。
