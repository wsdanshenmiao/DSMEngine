# DXR 适配审查：DSMEngine 本地实现 vs NVRHI

> 审查目标：对比 `D:\Code\NVRHI\src\d3d12\d3d12-backend.h`（及 `d3d12-raytracing.cpp`）中的 NVRHI DXR 设计，
> 审查 DSMEngine 本地的 DirectX Raytracing 适配代码，给出具体差异与改进建议。
> 配套实现计划：`docs/exec-plans/completed/2026-07-18-dxr-nvrhi-adaptation.md`（端到端落地，命名空间使用 `DSM::RT`）。

## 0. 结论速览

DSMEngine 本地的 DXR 适配目前是**脚手架 + 桩代码**：特性检测、`BufferDesc` 的 AS 标志、
`ResourceStates` 的 `AccelStruct*`、`ResourceType::RayTracingAccelStruct` 绑定类型、`Shader.h` 的 RT 阶段标志均已就位，
但**三类核心对象（AccelStruct / RayTracingPipeline / ShaderTable）与 Device/CommandList 的 RT 接口均未实现**，
三处 `RayTracingAccelStruct` 绑定写入为 `// TODO` 空实现。

NVRHI 已提供完整的、分层清晰的 DXR 抽象。建议**对齐 NVRHI 的三层对象拆分与堆绑定内存模型**补齐实现，
而非把逻辑塞进单一 `Buffer`。后续实现将按此审查结论落地。

## 1. 本地现状盘点

### 1.1 已具备的脚手架

| 能力 | 位置 | 说明 |
| --- | --- | --- |
| 特性检测 | `D3D12-Device.cpp:314-319` | 已设置 `m_RayTracingSupported`（`D3D12_RAYTRACING_TIER_1_0`）、`m_TraceRayInlineSupported` |
| 缓冲标志 | `Buffer.h:21-23,47-49` | `isAccelStructBuildInput` / `isAccelStructStorage` / `isShaderBindingTable` + Set 方法 |
| 资源状态 | `GraphicsCommon.h:641-644` | `AccelStructRead/Write/BuildInput/BuildBlas` |
| 状态映射 | `D3D12Common.h:201-204` | 已映射到 `D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE` / `NON_PIXEL_SHADER_RESOURCE` |
| 绑定类型 | `ResourceBindings.h:26,73` | `RayTracingAccelStruct` 作为 `BindingLayoutItem` 类型存在 |
| 着色器阶段 | `Shader.h:26-29` | `RayGeneration/Miss/Intersection/Callable/AllRayTracing` 标志 |
| Scratch 对齐 | `D3D12-CommandList.cpp:942-945` | `AllocateGpuBuffer` 已用 `D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT` |
| AS 资源状态 | `D3D12-Buffer.cpp:55`、`D3D12-Device.cpp:671` | 已对 AS 资源状态做特殊处理 |

### 1.2 缺失（均为 `TODO` 桩或完全空缺）

- `IDevice`（`Device.h`）**无任何** RT 创建方法。
- 无 `AccelStruct` / `RayTracingPipeline` / `ShaderTable` 类（搜索 `class RayTracing*` / `TopLevelAS` / `AccelStruct` 无结果）。
- 三处 `RayTracingAccelStruct` 绑定写入为空实现：
  - `D3D12-Device.cpp:1323-1325`
  - `D3D12-CommandList.cpp:1070-1072`
  - `D3D12-ResourceBindings.cpp:433-435`
- `ICommandList`（`CommandList.h`）无 `SetRayTracingState` / `DispatchRays` / `Build*AccelStruct` / `CopyAccelStruct`。

## 2. 方面一：核心对象的创建与管理

### 2.1 NVRHI 设计（对齐参考）

- `AccelStruct`（`d3d12-backend.h:796-826`）：持有 `RefCountPtr<d3d12::Buffer> dataBuffer`、
  `std::vector<rt::AccelStructHandle> bottomLevelASes`、`std::vector<D3D12_RAYTRACING_INSTANCE_DESC> dxrInstances`、
  `rt::AccelStructDesc desc`、`allowUpdate` / `compacted`；方法 `getDeviceAddress()` / `createSRV()` / `getDesc()` / `isCompacted()`。
- `RayTracingPipeline`（`d3d12-backend.h:828-862`）：持有 `rt::PipelineDesc desc`、
  `std::unordered_map<IBindingLayout*, RootSignatureHandle> localRootSignatures`、
  `RefCountPtr<RootSignature> globalRootSignature`、
  `RefCountPtr<ID3D12StateObject> pipelineState`、`RefCountPtr<ID3D12StateObjectProperties> pipelineInfo`、
  `std::unordered_map<std::string, ExportTableEntry> exports`、`maxLocalRootParameters`；
  方法 `getExport()` / `getShaderTableEntrySize()` / `hasLocalResources()` / `createShaderTable()`。
- `ShaderTable`（`d3d12-backend.h:874-922`）：持有 `RefCountPtr<RayTracingPipeline> pipeline`、
  `Entry rayGenerationShader`、`std::vector<Entry> missShaders/callableShaders/hitGroups`、
  `BufferHandle cache`、`ShaderTableState cacheState`；
  `getUploadSize() = entrySize * numEntries`；`bake()` 将 identifier + 本地绑定写入上传缓冲；
  `ShaderTableState`（`865-872`）缓存 `D3D12_DISPATCH_RAYS_DESC dispatchRaysTemplate` 与描述符堆，**避免每帧重算**。

### 2.2 本地差异

- 本地没有上述任何一类对象，DXR 相关状态散落在 `BufferDesc` / `ResourceStates` 等标量标志上，
  缺少"管线状态对象（StateObject）+ 导出表 + 着色器表"的整体抽象。
- `ShaderTable` 的"缓存 dispatchRaysTemplate"优化在本地完全不存在（无 ShaderTable 概念）。

### 2.3 改进建议（将落地）

1. 新增 `D3D12-RayTracing.h/.cpp`，定义 `DSM::RT::AccelStruct` / `RayTracingPipeline` / `ShaderTable`，
   严格对齐 NVRHI 的"持有原生 COM 指针 + desc + 缓存状态"模式。
2. `RayTracingPipeline` 在创建时即收集 `exports` 映射（export 名 → shader identifier + 绑定布局），
   并提供 `GetShaderIdentifierSize()` / `GetExport(name)`。
3. `ShaderTable` 复用 `Buffer`（`isShaderBindingTable`）作为上传缓存，并缓存 `D3D12_DISPATCH_RAYS_DESC` 模板；
   仅在 `version` 或描述符堆变化时 `bake()` 重算。

## 3. 方面二：资源绑定与内存分配策略

### 3.1 NVRHI 设计（对齐参考）

- 预建信息：`Device::GetAccelStructPreBuildInfo`（`d3d12-backend.h:1325`）调用
  `ID3D12Device5::GetRaytracingAccelerationStructurePrebuildInfo`，得到 `ResultDataMaxSizeInBytes` /
  `ScratchDataSizeInBytes` / `UpdateScratchDataSizeInBytes`。
- 内存需求：`getAccelStructMemoryRequirements` 直接返回 `dataBuffer` 的缓冲需求（即 resultData 对齐后大小）。
- 堆绑定模型：`createAccelStruct` 创建 `dataBuffer`（`isAccelStructStorage=true`，初始状态按 TLAS/BLAS 区分），
  由用户通过 `bindAccelStructMemory(as, heap, offset)` 绑定到具体 Heap（对齐 `D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT=256`）。
- Scratch / Instance 缓冲：`CommandList::build*AccelStruct` 复用命令列表内的 GPU 缓冲分配器
  （`AllocateGpuBuffer` 已含 256 字节对齐），按需从 `ASPreBuildInfo` 取 `ScratchDataSizeInBytes`。
- 绑定写入：AS 作为 SRV 通过 `GetGpuVirtualAddress()` 写入
  `D3D12_SHADER_RESOURCE_VIEW_DESC` 的 `RAYTRACING_ACCELERATION_STRUCTURE` 位置（或作为根参数传 GPU VA）。

### 3.2 本地差异

- 本地 `BufferDesc` 已有 `isAccelStructStorage` / `isAccelStructBuildInput`，`AllocateGpuBuffer` 已含 AS 对齐，
  基础设施基本就绪；但 `GetAccelStructPreBuildInfo` / `getAccelStructMemoryRequirements` / `bindAccelStructMemory` 均未实现。
- 三处 `RayTracingAccelStruct` 绑定写入为空桩，导致 AS 无法作为 SRV 绑定到着色器（本地渲染器无法引用 TLAS）。

### 3.3 改进建议（将落地）

1. 在 `IDevice` 新增 `GetAccelStructPreBuildInfo` / `GetAccelStructMemoryRequirements` / `BindAccelStructMemory`
   （堆绑定模型，本次**不引入 RTXMU 式子分配器**）。
2. 补全三处 `RayTracingAccelStruct` 绑定桩：写入 SRV 描述符（填 `GetGpuVirtualAddress()`），
   与现有 `TypedBuffer_SRV` / `Texture_SRV` 的 SRV 写入路径对齐。
3. `build*AccelStruct` 复用 `AllocateGpuBuffer` 作 scratch/instance 缓冲，并校验 scratch 大小 ≥ `ScratchDataSizeInBytes`。

## 4. 方面三：架构与扩展性

### 4.1 NVRHI 设计（对齐参考）

- 接口下沉：`rt::IAccelStruct` / `rt::IPipeline` / `rt::IShaderTable` 为抽象接口，后端实现；
  `IDevice` 暴露 `createRayTracingPipeline` / `createAccelStruct` / `createShaderTable` 等创建方法；
  `ICommandList` 暴露 `setRayTracingState` / `dispatchRays` / `build*AccelStruct` / `copyRaytracingAccelerationStructure` 等构建/派发方法。
- 导出表与本地根签名：管线创建时统一收集，便于后续接入 OMM、compact、cluster、RayQuery 等特性而不破坏抽象。

### 4.2 本地差异

- `IDevice` / `ICommandList` 接口中完全没有 RT 方法，无法在抽象层表达 DXR 工作流。
- 抽象层与 D3D12 后端之间缺少 `DSM::RT` 命名空间的描述结构（`AccelStructDesc` / `PipelineDesc` / `ShaderTableDesc` / `InstanceDesc` / `GeometryDesc` / `DispatchRaysArguments` / `State`）。

### 4.3 改进建议（将落地）

1. 在独立的 `RayTracing.h` 新增 `DSM::RT` 命名空间，定义上述描述结构与 `RayTracingPipelineHandle` /
   `AccelStructHandle` / `ShaderTableHandle`（`RefPtr` 别名）；复用已有的 `CreateShaderLibrary` 产物（DXR 用 Shader Library + Export 名）。
2. 在 `IDevice` / `ICommandList` **接口末尾**追加 RT 虚方法（不改动既有方法签名，保证向后兼容）。
3. 后续特性（OMM / RayQuery / compact / cluster）可在同一抽象上增量接入。

## 5. 命名空间约定

- 本实现统一使用 **`DSM::RT`**（大写），对应 NVRHI 的 `nvrhi::rt`。
- 接口/句柄命名沿用 DSM 既有风格：`RefPtr` 引用计数、`StaticVector`、`Context`、`Handle` 别名、PascalCase 方法名、`m_` 成员前缀、中文注释。

## 6. 具体改进清单（映射到实现计划）

| 项 | 文件 | 动作 |
| --- | --- | --- |
| RT 描述结构 + 句柄 | `RayTracing.h` | 新增 `DSM::RT` 命名空间 |
| Device RT 接口 | `Device.h` | 追加 `CreateRayTracingPipeline` / `CreateAccelStruct` / `GetAccelStructMemoryRequirements` / `BindAccelStructMemory` / `GetAccelStructPreBuildInfo` / `CreateShaderTable` |
| CommandList RT 接口 | `CommandList.h` | 追加 `SetRayTracingState` / `DispatchRays` / `BuildBottomLevelAccelStruct` / `BuildTopLevelAccelStruct` / `CopyAccelStruct` |
| 三类核心对象 | `D3D12-RayTracing.h/.cpp`（新增） | 实现 `AccelStruct` / `RayTracingPipeline` / `ShaderTable` + `GetAccelStructPreBuildInfo` |
| Device 后端 | `D3D12-Device.h/.cpp` | 实现上述 Device 方法；补全 `1323` 行桩 |
| CommandList 后端 | `D3D12-CommandList.h/.cpp` | 实现 build/dispatch；补全 `1070` 行桩；复用 `AllocateGpuBuffer` |
| 绑定桩 | `D3D12-ResourceBindings.cpp` | 补全 `433` 行 SRV 写入 |
| 端到端示例 | `Samples/RayTracing/`（新增） | HLSL + 渲染通道（BLAS/TLAS/ShaderTable/DispatchRays）+ xmake 目标 |
| 验证 | — | 特性门禁 + `xmake build RayTracing` + `xmake run RayTracing` |

## 7. 实现与验证证据

### 7.1 与 NVRHI 已对齐的关键行为

- 堆绑定 AS 采用 `CreateAccelStruct` → `GetAccelStructMemoryRequirements` → `BindAccelStructMemory`；内存需求复用底层结果缓冲的实际 allocation info，且 placed resource 绑定后回填 GPU 虚拟地址。
- AS 结果缓冲保持初始状态；BLAS/TLAS 更新路径校验 `AllowUpdate`、自动携带该标志、选择 `UpdateScratchDataSizeInBytes` 并将自身作为 source AS。
- BLAS 输入资源、AS 目标与 TLAS 实例缓冲均在构建前完成状态提交和命令生命周期引用保持；构建后写入 UAV 屏障。
- SBT 条目大小按 `D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT` 对齐；单条 miss、hit-group 与 callable 表使用 NVRHI 相同的零 stride 表示。

### 7.2 当前边界

- 当前版本仅支持全局根签名；`PipelineShaderDesc::bindingLayout`、`HitGroupDesc::bindingLayout` 与 ShaderTable 的 local binding 数据尚未编码为 local root signature / export association。该边界需在后续补齐 NVRHI 的 `LOCAL_ROOT_SIGNATURE`、`SUBOBJECT_TO_EXPORTS_ASSOCIATION` 与 SBT 根参数写入后再开放。
- SBT 当前按命令列表上传缓冲烘焙，尚未实现 NVRHI 的 version / descriptor heap 失效缓存与默认堆 cache buffer。

### 7.3 验证结果

- `xmake build RayTracing`：通过（2026-07-18）。
- `xmake run RayTracing`：在 xmake 运行环境中以退出码 0 完成。
- 直接运行 `bin/debug/RayTracing/RayTracing.exe`：当前 shell 缺少运行时 DLL，返回 `0xC0000135`；应通过 `xmake run RayTracing` 或补齐本机 DXC 运行时环境启动。
