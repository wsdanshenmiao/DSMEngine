# RestirDI 纯光追 Sample

状态：completed（2026-08-25）

## 摘要 (Summary)

在 `Samples/RayTracing/RestirDI` 新增独立的 `RestirDI` Sample。该 Sample 使用 DSMEngine 的 Scene、Mesh、Material、Light、Camera、Editor 和现有 RHI 构建完整的 DXR 主可见性与阴影管线，使用 Compute 实现 ReSTIR DI 初始采样、时间复用和空间复用，只以一个全屏显示 Pass 将 HDR 结果呈现到 EditorViewport。

完成结果必须包含解析灯、自发光三角形和环境贴图三类候选、可调参数面板、自动验证模式、DebugLayer 渲染验证和可复查图像/指标。实现和修复过程中禁止修改 `DSMEngine/Runtime/Graphics/**` 与 `ThirdParty/**`。

## 背景 (Context)

- 工程根目录：`D:\Code\DSMEngine`。
- 新目标目录：`Samples/RayTracing/RestirDI`。
- 现有 `Samples/RayTracing/main.cpp` 和 `TestTriangle.h` 是用户已有暂存改动，实施期间保持其 SHA256 分别为 `B0915D1AEE6DC404D39C45CFEACBCC0761CE0931A597E9AF983066300C6D843D` 与 `3DBD73D5C9BA2DE928950C3D6D97B5218E119CA408697ABECB0799AE931AF32A`。
- 仓库没有有效 P4 workspace 配置，按 Git 工作区处理，不执行 P4 操作。
- 当前 RHI 已支持 BLAS/TLAS、DXR Pipeline、ShaderTable、DispatchRays、Compute、StructuredBuffer 和 Buffer Readback；验证输出采用 StructuredBuffer，避免增加 Texture Readback RHI。
- EditorViewport 从 `GraphicsRenderer::GetColorTexture()` 显示结果，因此只有最终 ACES 显示 Pass 使用光栅化。

## 实施计划 (Implementation Plan)

### 里程碑 1：目标、入口和验证所需公共接口

- 在 `Samples/RayTracing/xmake.lua` 注册子目录；在新目录定义 `RestirDI` 目标并按工程根路径复制 Sample Shader、Engine Shader、Assets 和 ImGui 布局。
- 新增正常 Editor 入口、`--validate-render` 和 `--validate-editor` 命令行入口。
- 在 `EngineParameters` 增加可选图形消息回调透传；在 `DSMEditor` 提取 `RunFrame()`，保持原 `Run()` 行为不变。
- 判定成功：RestirDI 和 PBR 均能编译，原 RayTracing 用户文件哈希不变，Runtime/Graphics 无 diff。

### 里程碑 2：场景转译与 DXR

- 从 Scene 收集 MeshRenderer、Transform 和 Light，构建 Sample 自有顶点、索引、实例、Geometry、Material 和 Texture Descriptor 数据。
- 每个唯一 Mesh 构建 BLAS；按 ObjectID 排序构建 TLAS；拓扑变化重建，变换变化使用 TLAS update。
- 实现 Primary、Visibility、Alpha any-hit 和 Reference RayGen；Primary/Visibility 使用不同实例掩码。
- 判定成功：验证场景能生成稳定的命中数据，Alpha 和 CastShadow 探针符合预期，DebugLayer 无资源状态或 ShaderTable 错误。

### 里程碑 3：完整候选域与 ReSTIR

- 为解析灯、自发光三角形和环境 texel 建立候选表与 Alias Table，正确计算混合 PDF。
- 实现 Initial RIS、Temporal、Spatial、History Ping-Pong、M Clamp、可见性求值及 Debug 输出。
- 支持 daylight 六面图和外部 Radiance HDR，环境统一转换为经纬度线性纹理。
- 判定成功：三类候选可独立产生能量，Reservoir 无 NaN/Inf，Reference 与 ReSTIR 指标处于验收阈值内。

### 里程碑 4：Editor 面板与验证场景

- 实现参数分组、Debug View、历史重置、环境加载和统计展示。
- 实现需确认且不自动保存的验证场景，覆盖多灯、自发光、Alpha、阴影和运动。
- 判定成功：Editor 自动运行 120 帧，Viewport 完成 Resize，RenderUI 和 Present 路径执行且无 DebugLayer 消息。

### 里程碑 5：闭环验证和归档

- 依次执行静态、Debug/Release 编译、Runtime DebugLayer、Editor DebugLayer 和视觉检查。
- 每次失败保留独立 attempt 证据，修复根因后先重跑失败 Gate，再重跑全量 Gate。
- 全部通过后更新本文结果，将文件移入 `docs/exec-plans/completed/`。

## 验证 (Validation)

所有命令从 `D:\Code\DSMEngine` 执行。验证产物写入 `build/verification/restir-di/<timestamp>/attempt-<N>/`，每次至少包含 `status.raw.json`、构建日志、运行日志、DebugLayer 消息、指标 JSON 和 BMP。

静态 Gate：

- `git diff --check`
- 检查 RestirDI 不引用 Forward/Deferred/CommonPass。
- 检查 `DSMEngine/Runtime/Graphics/**`、`ThirdParty/**` 和两个受保护 RayTracing 文件无改动。
- 审查 PDF/Reservoir 数学、零除、资源和 AS Heap 生命周期、状态屏障、Descriptor 索引及 C++/HLSL 布局。

编译 Gate：

- `xmake f -m debug`
- `xmake build RestirDI`
- `xmake build PBR`
- `xmake project -k vsxmake2022`
- `xmake f -m release`
- `xmake build RestirDI`
- 最后切回 Debug 并重建 RestirDI。

运行 Gate：

- `bin/debug/RestirDI/RestirDI.exe --validate-render --output <attempt-dir>`
- `bin/debug/RestirDI/RestirDI.exe --validate-editor --frames 120 --output <attempt-dir>`
- 两种模式必须启用 D3D12 DebugLayer/GPU-Based Validation，并以 0 退出。
- 所有 HDR 像素有限；有效 Reservoir 比例至少 90%；静态 Temporal 接受率至少 50%；三类候选均产生有效样本；Resize、Alpha 和 CastShadow 探针通过。
- Reference 使用每像素 256 个带可见性候选；ReSTIR 预热 64 帧后平均亮度误差不超过 10%，16x16 分块 NRMSE 不超过 20%，过曝比例不超过 5%。
- 实际打开完整 ReSTIR、Reference、Source Debug、运动和 Alpha 图像，检查光照方向、阴影、自发光、Alpha 轮廓、噪声和拖影。

## 进展 (Progress)

- [x] 读取仓库规则、P4 skill、PLANS 和验证协议。
- [x] 确认无有效 P4 workspace，记录受保护文件哈希。
- [x] 建立 RestirDI 目标和最小公共接口。
- [x] 完成场景转译和 DXR。
- [x] 完成完整候选域和 ReSTIR。
- [x] 完成 Editor 面板和自动验证入口。
- [x] 完成所有 Gate、视觉验收和归档。

最终验证证据：

- `build/verification/restir-di/2026-08-25/attempt-13/`：菜单契约修复后的最终全量 Gate 证据，顶层 `status.raw.json` 为通过。
- `attempt-13/render/`：离屏渲染验证以 0 退出，D3D12、DXGI 和 RHI Warning 均为 0；有效 Reservoir 比例 99.64%，Temporal 接受率 100%，ReSTIR/Reference 平均亮度误差 2.56%，16x16 分块 NRMSE 4.63%，过曝比例 0.226%。三类候选、Resize、Alpha、运动和 CastShadow 探针全部通过。
- `attempt-13/editor/`：Editor 120/120 帧、渲染 120 帧、ReSTIR 参数面板 120 帧，Viewport 读回成功，D3D12、DXGI 和 RHI Warning 均为 0。
- 已实际打开检查完整 ReSTIR、Reference、Source Debug、运动、Alpha、CastShadow 和 Editor 输出；场景构型、三候选域、Alpha 轮廓和阴影掩码与探针结果一致，ReSTIR 与 Reference 的一采样高频噪声分布一致。

attempt-13 全部 Gate 已通过；计划归档到 `docs/exec-plans/completed/`。

## 意外与发现 (Surprises & Discoveries)

- 当前 RHI 没有 Texture-to-Buffer readback；采用 HDR StructuredBuffer 作为输出可完全绕开 RHI 修改。
- Editor 的连续运行将 Viewport Resize 处理内嵌在 `Run()` 中；为自动 Editor 验证需提取单帧接口。
- D3D12 后端当前未把 `GeometryTriangles` 的 vertex/index offset 传入底层 AS Geometry；因 Graphics/RHI 禁止修改，Sample 保留统一着色缓冲，同时为每个 BLAS 建立零偏移的局部 AS 构建缓冲。
- 使用动态增长的 bindless 描述符表会触发现有后端描述符堆扩容问题；Sample 改为容量 256 的完整固定纹理数组并缓存稳定 BindingSet，最终 DebugLayer 无描述符错误。
- 可执行目录中的 `imgui.ini` 会被正常 Editor 会话持久化，曾留下独立平台窗口；ImGui D3D12 多视口后端会在该窗口首帧提交 fence=0 的等待。Editor 自动验证改用输出目录中的确定性临时布局，既不修改 ThirdParty，也不受用户布局污染。
- attempt-11 的首次 PBR 回归遇到 MSVC `link.exe`/PDB 服务死锁：10 分钟内 CPU 时间仅 0.109 秒且输出时间戳不变。终止该次工具链进程并保留失败日志后，attempt-12 先重跑失败 Gate，再完成全量回归；PBR 最终两次成功，分别耗时 1104.953 秒和 197.437 秒。
- attempt-12 通过后进行交付前契约复查，发现 Editor 菜单仍假设当前管线为 Deferred。ReSTIR 状态下点击已选中的 Deferred 项不会执行切换；改为 Forward/Deferred 任一菜单项被点击时都显式重建对应管线，确保两条离开 ReSTIR 的路径均有效。

## 决策记录 (Decision Log)

- 2026-08-24：最终显示允许一个全屏光栅 Pass，所有场景可见性仍由 DXR 完成。
- 2026-08-24：支持解析灯、自发光三角形和环境贴图完整候选域。
- 2026-08-24：采用实用有偏时空复用，保留独立 RIS/Reference 验证模式。
- 2026-08-24：保留 Editor 原有 Forward/Deferred 菜单，不为 Sample 重构管线注册系统。
- 2026-08-24：将 `DSMEngine/Runtime/Graphics/**` 设为绝对不可写边界。
- 2026-08-25：对 RHI 未实现 AS Geometry offset 的限制采用 Sample 局部零偏移构建缓冲绕行，统一场景缓冲契约保持不变。
- 2026-08-25：材质纹理表采用 256 项固定数组；所有空槽填充有效 fallback 纹理，BindingSet 按资源组合复用。
- 2026-08-25：Editor 自动验证使用独立临时 ImGui 布局，避免读取或改写用户的窗口布局。

## 结果与复盘 (Outcomes & Retrospective)

已新增独立 `RestirDI` 目标和纯光追管线。主可见性、Alpha any-hit、最终阴影和 256 候选 Reference 使用 DXR；初始 RIS、Temporal 和 Spatial 使用 Compute；HDR StructuredBuffer 经 ACES 全屏 Pass 写入 EditorViewport。Sample 自己管理场景转译、统一着色缓冲、BLAS/TLAS、材质纹理表、解析灯/自发光/环境 Alias Table、Reservoir 历史与 Debug View，不复用 Forward/Deferred/CommonPass/RenderResource。

公共改动仅包括 Editor 单帧入口、Forward/Deferred 菜单显式切换、Engine 图形消息回调透传以及 MeshRenderer 的安全材质索引访问。`DSMEngine/Runtime/Graphics/**`、`ThirdParty/**` 与用户已有 RayTracing 文件均未修改；两个受保护文件 SHA256 与任务开始时一致。

最终验证结果：

- 静态检查：`git diff --check` 退出码 0；新文件无行尾空白；禁止依赖检索无结果；Graphics/RHI 和 ThirdParty 的工作区/暂存区 diff 为空。
- Debug：菜单修复后的 RestirDI `build ok, spent 24.765s`；PBR `build ok, spent 617.062s`；`xmake project -k vsxmake2022` 输出 `create ok!`。
- Release：菜单修复后的 DSMEngine 编译及 RestirDI 链接成功，`build ok, spent 28.281s`。
- 最终恢复 Debug 后 RestirDI `build ok, spent 25.235s`。
- Render 与 Editor 两种 DebugLayer/GPU-Based Validation 均以 0 退出，D3D12、DXGI 和 RHI 消息中无 Warning/Error/Corruption。
- 最终图像与指标位于 `build/verification/restir-di/2026-08-25/attempt-13/`；六张最终渲染图与已人工检查的 attempt-12 逐文件 SHA256 相同，所有阈值及人工视觉检查均通过。

实现保留两个明确的 Sample 局部上限/绕行：材质纹理数组容量为 256；BLAS 使用零偏移局部构建缓冲以绕开当前只读 RHI 未应用 Geometry offset 的限制。二者均在初始化时显式校验或由统一缓冲契约覆盖，不存在静默 fallback。
