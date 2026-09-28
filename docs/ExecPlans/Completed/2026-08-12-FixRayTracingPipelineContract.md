# 修复 RayTracing 管线与 BLAS 构建契约

## 状态

已完成（2026-08-12）。

## 摘要

`xmake run RayTracing` 最初在 `Device::CreateRayTracingPipeline` 后无法取得 `ClosestHit` 的 DXR shader identifier。移除错误的独立 hit shader 导出后，程序仍输出全黑图；开启 D3D12 调试层和 DRED 后确认 BLAS 构建提交了空的顶点 GPU 地址并导致设备挂起。

本任务恢复 NVRHI 原有的接口语义：加速结构创建描述只负责声明布局和预分配容量；每次 BLAS 构建显式传入当前几何资源。与此同时，约束光追管线的普通 shader 导出类型，并保证虚拟 AS 缓冲区携带 D3D12 加速结构资源标志。

## 上下文与根因

涉及的主要边界为：

- `RT::PipelineDesc::shaders` 只包含可直接写入 ShaderTable 的 RayGeneration、Miss 和 Callable；hit shader 通过 hit group 引用。
- `RT::IAccelStruct::GetDesc()` 保存创建布局，但后端为避免长期持有易失效资源指针，会清空其中的几何 buffer 指针。
- `ICommandList::BuildBottomLevelAccelStruct` 必须像 NVRHI 一样在构建时接收几何数组，不能从已清理的创建描述恢复资源。
- 虚拟 buffer 在创建 placed resource 前提前返回，因此 AS 专用资源 flag 必须在该返回点之前设置。

当前实现的问题来自移植时只保留了“清空持久描述中的资源指针”，却删除了 NVRHI BLAS 构建接口中的几何参数。结果是 `GetAccelerationStructureBuildInputs` 读取空指针，D3D12 调试层报告 `VertexBuffer.StartAddress can't be null when VertexCount > 0`，随后发生 `DXGI_ERROR_DEVICE_HUNG`。

## 实施计划

1. 从 RayTracing 示例的普通导出列表移除 `ClosestHit`，并在 D3D12 pipeline 创建处拒绝放错类别的 shader。
2. 将 AS buffer flag 设置移动到虚拟 buffer 的提前返回之前。
3. 为 BLAS 构建接口增加 `std::span<const RT::GeometryDesc>`，让状态转换、资源保活、native geometry desc 和 transform 上传全部使用本次传入的几何。
4. 保留创建描述的资源去引用行为，并按 Triangles/AABBs 的实际 union 成员清空指针。
5. 修正 BLAS/TLAS 对 `PerformUpdate` 与 `AllowUpdate` 的混用。
6. 删除临时 DRED 诊断代码，完成构建、运行和输出图像验证。

## 验证

| 验证项 | 命令/方式 | 预期 |
| --- | --- | --- |
| 光追目标编译 | `xmake build RayTracing`（release） | 编译及链接成功 |
| 光追端到端运行 | `xmake run RayTracing`（release） | 无 pipeline/设备移除错误，退出码 0 |
| 渲染结果 | 检查 `bin/release/RayTracing/raytracing_output.bmp` | 图像不是全零黑图，内容符合三角形光追示例 |
| 仓库 C++ gate | `xmake build PBR`（release） | 编译及链接成功 |
| 补丁完整性 | `git diff --check` | 无空白错误 |

当前 debug 目标被 Visual Studio 调试会话占用，无法覆盖 `bin/debug/RayTracing/RayTracing.exe`。因此先用 release 产物完成等价端到端验证，结束时恢复 xmake 的 debug 配置；不终止用户的 Visual Studio 进程。

## 进展

- [x] 复现 pipeline identifier 获取失败。
- [x] 对照同目录 NVRHI 源码确认 pipeline shader/hit group 契约。
- [x] 通过 D3D12 debug layer 与 DRED 定位 BLAS 空顶点地址和设备挂起。
- [x] 恢复 BLAS 构建接口契约并清理临时诊断。
- [x] 完成 release/debug 验证矩阵并归档计划。

## 意外与发现

- `CreateStateObject` 实际成功，最初的报错来自随后对独立 `ClosestHit` 导出调用 `GetShaderIdentifier`，不是 DXR state object 创建失败。
- 修正导出列表后程序能以退出码 0 结束，但输出 BMP 全零；仅凭退出码不足以证明 GPU 工作成功。
- 虚拟 AS buffer 在设置 `D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE` 之前返回，这与 BLAS 空地址是两处独立的移植缺陷。
- `ConvertGeometryDesc` 未从 DSM 的 `GeometryType` 写入 native `Type`，导致所有几何默认进入三角形分支；AABB 路径此前不可用。
- `PerformUpdate` 未映射到 D3D12 flag，BLAS/TLAS 又错误地用 `AllowUpdate` 判断本次是否更新；两类标志的语义已分离。

## 决策记录

- 不通过取消创建描述的资源去引用来绕开故障。该做法会把资源生命周期绑定回持久描述，并破坏动态更新/重建语义。
- 不保留旧的无几何参数 overload 或 fallback。旧接口无法在资源指针已清理后正确构建，应直接恢复明确、可验证的契约。
- 使用项目已有的 `std::span` 风格表达几何数组，而不是 NVRHI 的裸指针加数量参数。

## 结果与复盘

修复后，用户原始 debug 工作流已直接通过：

- 最终源码快照执行 `xmake build RayTracing`（debug）：退出码 0，`build ok, spent 46.11s`。
- `xmake run RayTracing`（debug）：退出码 0，输出“DXR 示例执行成功”。
- debug 输出 `bin/debug/RayTracing/RayTracingOutput.bmp` 为 512x512，像素区非零字节数 786304，SHA-256 为 `38E07F5C26AB1B1BCAD30CE0B4A7ECF02AD634596234572729B94BFE8083B74C`。
- `xmake build RayTracing` 与 `xmake run RayTracing`（release）：退出码均为 0；输出图像与 debug 的 SHA-256 完全一致。
- 保留 D3D12 调试层的诊断构建运行时，没有再产生 validation、DRED、设备移除或 shader identifier 错误；验证后已删除临时诊断代码。
- `xmake build PBR`（release）：退出码 0，`build ok, spent 181.235s`。首次命令因 120 秒工具上限超时，清理仅属于该命令的孤儿 Xmake 进程后，以 300 秒上限重跑并取得明确成功结果。
- `git diff --check`：退出码 0，仅有仓库行尾转换提示，无空白错误。

最终实现恢复了 NVRHI 的外部契约，而不是在后端重新保存或猜测已被清理的资源指针。这样既修复当前三角形示例，也保留 BLAS 重建/更新所需的资源生命周期语义，并修复 AABB native 类型和 AS 更新标志路径。
