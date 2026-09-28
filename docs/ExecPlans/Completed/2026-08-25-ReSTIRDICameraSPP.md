# RestirDI 相机控制与可调 SPP

状态：completed

## 摘要 (Summary)

为 `Samples/RayTracing/RestirDI` 增加可用的 Editor 飞行相机控制，并在 ReSTIR DI 面板中增加真正的每像素直接光采样数（SPP）调节。ReSTIR/Independent RIS 的每个额外 SPP 都生成独立的局部 Reservoir 并追踪一条最终阴影射线，多个直接光估计取平均；Reference 的每像素可见性候选数也改为可调，默认仍为 256。

实现继续保持 `DSMEngine/Runtime/Graphics/**` 和 `ThirdParty/**` 只读，不复用 Forward/Deferred 渲染资源。保留用户已有 `Samples/RayTracing/main.cpp` 和 `TestTriangle.h` 改动。

## 背景 (Context)

- ReSTIR 管线当前没有创建或更新 `CameraController`，因此 Editor 中无法用现有 WASD/RMB 控制相机。
- `Initial candidates` 只改变无阴影候选评估数量，不是 SPP；当前 ReSTIR 每像素只有一个最终可见性样本。
- 现有 `CameraController` 已提供 RMB 旋转、WASD 移动、速度和灵敏度接口；Render 验证模式没有 ImGui Context，因此只能在完整 Editor 模式启用。
- Reservoir Buffer 继续保持每像素一个可时空复用 Reservoir。SPP=1 使用该 Reservoir；SPP>1 的额外样本在 Visibility RayGen 中构建独立 Initial RIS Reservoir，从而避免按 SPP 成倍扩大历史显存。
- 当前 P4 skill 无法在该非 UE/Git 仓库检测到 P4 workspace，本任务按 Git 工作区处理。

## 实施计划 (Implementation Plan)

### 里程碑 1：相机控制

- 在 `RestirDIRenderPipeline` 的 Editor 模式中持有并初始化项目现有 `CameraController`。
- 每帧渲染前更新相机；离屏 `--validate-render` 不访问 ImGui 或相机输入。
- 在 ReSTIR DI 面板显示操作说明，并提供启用开关、移动速度和鼠标灵敏度。
- Editor 自动验证通过 ImGui 输入队列注入短暂 W 键事件，确认相机位置确实变化，再恢复固定验证视角。

### 里程碑 2：真实直接光 SPP

- `Settings` 新增 `samplesPerPixel`（默认 1）与 `referenceSamplesPerPixel`（默认 256）。
- C++/HLSL 共享 Frame Constants 新增 sampling `uint4`，保持 16 字节布局并更新大小/偏移断言。
- ReSTIR/Independent RIS：第一个样本使用现有最终 Reservoir；其余样本各自执行 Initial RIS、追踪最终阴影射线，并平均直接光贡献。范围设为 1–8，避免交互模式意外产生极端 GPU 负载。
- Reference：候选/可见性样本数由固定 256 改为 UI 可调 1–512，自动验证仍固定使用 256。
- Visibility Debug 将可见样本数归一化显示；Acceptance Buffer 的 visibility 字段保存可见样本计数，SPP=1 的既有语义不变。

### 里程碑 3：验证与归档

- 自动 Render 验证增加 SPP=4 分支，输出 `spp-4.bmp`，并要求至少存在 visibility count 大于 1 的像素。
- Editor 验证增加相机移动断言，并继续要求 120 帧 UI/Viewport/Present 与 DebugLayer 零 Warning。
- 完成静态、Debug/Release、PBR、VS 工程、Render、Editor 和视觉检查后归档本文。

## 验证 (Validation)

验证目录：`build/verification/restir-di-camera-spp/2026-08-25/attempt-<N>/`。

- `git diff --check`
- 检查 `DSMEngine/Runtime/Graphics/**`、`ThirdParty/**` 无 diff，受保护文件 SHA256 不变。
- 检查 RestirDI 不引用 Forward/Deferred/CommonPass/RenderResource。
- `xmake f -m debug`
- `xmake build RestirDI`
- `xmake build PBR`
- `xmake project -k vsxmake2022`
- `xmake f -m release`
- `xmake build RestirDI`
- 恢复 Debug 并重新构建 RestirDI。
- 在 `bin/debug/RestirDI` 执行 `RestirDI.exe --validate-render --output <dir>`。
- 执行 `RestirDI.exe --validate-editor --frames 120 --output <dir>`。
- 检查原有数值阈值、SPP=4 可见样本计数、相机移动断言以及 D3D12/DXGI/RHI 消息。
- 打开 SPP=1、SPP=4、Reference 与 Editor BMP，检查 SPP=4 没有能量缩放错误，并确认噪声分布符合多样本平均预期。

验证结果位于 `build/verification/restir-di-camera-spp/2026-08-25/attempt-1/`：

- 静态检查通过：`git diff --check` 退出码为 0；`Runtime/Graphics/**` 与 `ThirdParty/**` 无改动；受保护的 `main.cpp`、`TestTriangle.h` SHA256 未变化；RestirDI 无 Forward、Deferred、CommonPass 或 RenderResource 引用。
- Debug 的 RestirDI、PBR 构建通过，`vsxmake2022` 工程生成通过；Release RestirDI 构建通过；最终配置已恢复 Debug 并再次构建通过。
- `--validate-render` 退出码为 0，`numeric_passed`、`spp_passed`、`debug_layer_passed` 均为 true。4 SPP 的 `max_visibility_samples` 为 4、HDR finite ratio 为 1、平均亮度为 0.205916；1 SPP 平均亮度为 0.210647，Reference 为 0.204648。
- ReSTIR 相对 Reference 的平均亮度误差为 2.93%，16×16 分块 NRMSE 为 5.73%；D3D12、DXGI 与 RHI 均无 Warning/Error。
- `--validate-editor --frames 120` 退出码为 0，120 帧全部完成，Render/UI 计数均为 120，`camera_moved` 为 true，DebugLayer 无 Warning/Error。
- 目检 `restir.bmp`、`spp-4.bmp`、`reference.bmp`、`source-debug.bmp`、`alpha.bmp` 与 `editor.bmp`：4 SPP 噪声低于 1 SPP，整体能量与 Reference 一致，环境、解析灯、自发光、Alpha 轮廓和遮挡方向正常。

## 进展 (Progress)

- [x] 读取项目规则、PLANS、验证协议和 P4 skill。
- [x] 确认现有 CameraController 与 Reservoir/Visibility 数据流。
- [x] 完成相机控制和 UI。
- [x] 完成可调 SPP Shader 与共享布局。
- [x] 扩展自动验证。
- [x] 完成全部 Gate、视觉验收和归档。

下一步：无；本计划关闭。

## 意外与发现 (Surprises & Discoveries)

- 现有 CameraController 直接读取 ImGui IO，因此不能在无 Editor 的 Render 验证模式调用。
- 将所有历史 Reservoir 按 SPP 扩容会在高分辨率下造成显著显存放大；额外 SPP 使用局部 Initial RIS Reservoir，可提供真实额外阴影样本而不改变历史资源尺寸。
- Reference 的 Visibility Debug 原先只保存“是否存在可见样本”；SPP 可调后改为保存可见样本计数，再除以 Reference SPP 显示可见比例。

## 决策记录 (Decision Log)

- 2026-08-25：把用户所说 PPS 按上下文解释为 SPP。
- 2026-08-25：不把 `Initial candidates` 重命名为 SPP；两者保留独立语义和 UI。
- 2026-08-25：ReSTIR 交互 SPP 范围为 1–8，Reference SPP 范围为 1–512。
- 2026-08-25：仅 Editor 模式启用相机控制，保证 headless 验证不依赖 ImGui 输入。
- 2026-08-25：自动 Editor 验证注入 W 键后恢复固定相机并关闭本轮相机控制，避免惯性移动污染后续截图。

## 结果与复盘 (Outcomes & Retrospective)

RestirDI 现在使用项目既有 CameraController 提供 RMB 视角旋转和 WASD 移动，并在面板中提供启用开关、移动速度与鼠标灵敏度。Sampling 面板把 Initial candidates 与真正的 SPP 明确分开：ReSTIR/Independent RIS 可调 1–8 SPP，Reference 可调 1–512 SPP。

额外 ReSTIR SPP 使用局部独立 Initial RIS Reservoir，并各追踪一条阴影射线后与复用 Reservoir 的估计取平均，因此不会扩大历史 Reservoir Buffer。代价是每增加一个 SPP 都会增加一组候选评估和一条最终可见性射线，交互性能大体随 SPP 线性下降；默认保持 1 SPP。没有遗留功能阻塞。
