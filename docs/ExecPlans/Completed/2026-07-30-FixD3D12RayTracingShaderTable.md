# D3D12 ShaderTable 修复计划

状态：completed

## 摘要 (Summary)

恢复 D3D12 光线追踪 ShaderTable 的可编译性，并完成缓存 Shader Binding Table（SBT）的创建、烘焙、复制和状态跟踪。完成后，`xmake build PBR` 与 `xmake build RayTracing` 必须通过；随后运行 `xmake run RayTracing` 并检查导出的 BMP 是否包含蓝色背景和居中的彩色三角形。

## 背景 (Context)

当前 `D3D12-RayTracing.h/.cpp`、`D3D12-CommandList.h/.cpp` 和 `RayTracing.h` 存在未完成的 NVRHI 移植：`ShaderTableDesc` 的命名空间错误、`ShaderTableState` 在定义前按值使用、缓存描述未传入创建接口、命令列表在声明状态前访问它，且加速结构实现仍引用已改名字段。参考实现位于 `D:\Code\NVRHI\src\d3d12\d3d12-raytracing.cpp` 与 `d3d12-backend.h`。不修改 `ThirdParty/`。

## 实施计划 (Implementation Plan)

1. 对齐 `RT::IPipeline::CreateShaderTable`、`RT::ShaderTableDesc` 与 D3D12 实现的外部契约，确保缓存容量和名称能进入后端；修正 fluent setter 的赋值目标。
2. 在 D3D12 后端定义完整的 ShaderTable 状态、缓存缓冲区创建和 SBT 烘焙逻辑；使用现有上传分配器、描述符堆和资源状态追踪接口。
3. 在 CommandList 中按缓存/非缓存语义取得状态、重建 SBT、复制缓存并保留资源寿命；修复加速结构字段不一致和签名失配。
4. 实现 D3D12 DXR 状态对象创建，包含 DXIL 库、命中组、全局/局部根签名、配置和导出标识符；将 RayTracing 示例迁移到当前抽象接口。
5. 逐轮执行 `xmake build PBR`、`xmake build RayTracing` 和 `xmake run RayTracing`；检查生成的 BMP 颜色和三角形形状。

## 验证 (Validation)

在 `D:\Code\DSMEngine` 执行：

- `xmake build PBR`：编译 D3D12 运行时和 PBR 示例。
- `xmake build RayTracing`：验证 ShaderTable 公共接口、DXR 状态对象和样例调用。
- `xmake run RayTracing`：生成并检查 `RayTracingOutput.bmp`。
- `git diff --check`：验证修改没有空白错误。

构建命令、退出码和无法验证的原因将记录在本计划的“进展”和“结果与复盘”中。

## 进展 (Progress)

- 已完成：对齐 `RayTracing.h`、D3D12 ShaderTable、CommandList 和加速结构接口；缓存/非缓存 SBT 均可创建、烘焙并绑定。
- 已完成：实现 `Device::CreateRayTracingPipeline` 的 DXIL 库、命中组、根签名、状态对象和导出表创建；RayTracing 示例已迁移至当前抽象接口。
- 已完成：`xmake build RayTracing` 通过；`xmake run RayTracing` 于 2026-07-30 22:42:57 成功生成 `bin/debug/RayTracing/RayTracingOutput.bmp`。
- 已完成：最终 `xmake build PBR` 通过（23.954 秒）；仅保留工程既有的 C4834 `[[nodiscard]]` 警告。

## 意外与发现 (Surprises & Discoveries)

- 当前目录是 Git 工作区而非可识别的 Perforce 工作区；P4 检测未找到 `p4config.txt`，因此不会创建 changelist。
- 用户明确要求运行和查看 RayTracing 图像；因此 `Device::CreateRayTracingPipeline` 从原定范围外项提升为本计划的必要项。
- 非索引三角形的空 `indexBuffer` 被无条件交给状态跟踪器，触发断言并阻塞运行；D3D12 构建路径现在仅跟踪有效输入缓冲区。
- 线性分配器此前只对分配大小对齐，未对起始偏移对齐，导致 TLAS 实例描述符和 SBT 地址违反 DXR 对齐要求；已修正为对齐起始偏移并保留前后空闲区间。

## 决策记录 (Decision Log)

- 采用 NVRHI 的缓存语义：缓存表在创建时按 `maxEntries` 分配 GPU 缓冲区，非缓存表按命令列表保存临时状态。
- 公共接口使用带默认值的 `CreateShaderTable(const ShaderTableDesc&)`，保留无参数调用的源兼容性。
- RayTracing 示例按当前 `GeometryTriangles`、`PipelineShaderDesc` 和 `IShaderTable` API 迁移，而不恢复已废弃的聚合字段和 Device 级 ShaderTable 创建入口。
- ClosestHit、AnyHit 和 Intersection 只作为命中组导入时不请求独立 shader identifier，因为 DXR 仅为 RayGeneration、Miss、Callable 和 HitGroup 提供该标识符。

## 结果与复盘 (Outcomes & Retrospective)

已完成。`xmake build RayTracing`、`xmake run RayTracing` 和 `xmake build PBR` 均通过；`RayTracingOutput.bmp` 已人工检查，显示预期的蓝色背景、居中三角形和红/绿/蓝重心坐标渐变。未修改 `ThirdParty/`。

本次未覆盖多几何体、局部根签名、缓存 SBT 或更新式 AS 构建的实际画面验证；这些路径的实现已对齐 NVRHI 的状态和缓存语义，但应在后续场景中单独覆盖。
