# DXR 适配对齐 NVRHI 审查与端到端实现

**状态**: completed  
**创建日期**: 2026-07-18  
**完成日期**: 2026-07-18  
**整合来源**: `.codebuddy/plans/DXR适配对齐NVRHI审查与端到端实现_113bee13.md`

## 摘要 (Summary)

对比 NVRHI 的 D3D12 DXR 后端，审查并端到端实现 DSMEngine 本地的 DirectX Raytracing 适配。交付内容包括：`DSM::RT` 抽象、`AccelStruct` / `RayTracingPipeline` / `ShaderTable` 三类核心对象、D3D12 的构建与派发后端、TLAS SRV 资源绑定、Heap 绑定的加速结构内存模型，以及可导出图像的 `Samples/RayTracing` 示例。

用户可运行一次光追派发并查看 `bin/debug/RayTracing/RayTracingOutput.bmp`。本计划不包含窗口、交换链、连续帧渲染、本地根签名、带本地参数的 SBT、OMM、compact、cluster 或 RayQuery。

## 背景 (Context)

### 用户需求

对比 `D:\Code\NVRHI\src\d3d12\d3d12-backend.h` 及其 DXR 实现，重点审查并改进：

1. 加速结构、管线状态和着色器表的创建与管理；
2. 资源绑定与内存分配；
3. 代码架构与后续扩展性；
4. 提供具体差异、改进建议和可运行的端到端示例。

### 实施前状态

项目已有部分 DXR 脚手架：资源状态、`BufferDesc` 的 AS 标志、`RayTracingAccelStruct` 绑定类型和部分特性检测；但没有 `AccelStruct`、`RayTracingPipeline`、`ShaderTable` 对象，也没有抽象层的创建、构建和派发接口。`RayTracingAccelStruct` 的描述符写入仍是空桩。

NVRHI 将 DXR 拆分为加速结构、管线和着色器表三类对象，采用“创建虚拟 AS 结果资源 → 查询内存需求 → 绑定 Heap”的显式内存模型。本实现以该行为模型为准，而非把 DXR 状态塞入单一 `Buffer`。

### 文档与代码位置

- 审查记录：`docs/reviews/dxr-nvrhi-review.md`
- 公共 DXR API：`DSMEngine/Runtime/Graphics/RayTracing.h`
- D3D12 DXR 后端：`DSMEngine/Runtime/Graphics/D3D12/D3D12-RayTracing.h/.cpp`
- Device 和 CommandList 后端：`D3D12-Device.cpp`、`D3D12-CommandList.cpp`
- 资源绑定：`D3D12-ResourceBindings.cpp`
- 示例：`Samples/RayTracing/main.cpp`
- CodeBuddy 原始计划：`.codebuddy/plans/DXR适配对齐NVRHI审查与端到端实现_113bee13.md`

## 实施计划 (Implementation Plan)

### 里程碑 1：审查 NVRHI 与定义接口契约

- 对比 NVRHI 的 `AccelStruct`、`RayTracingPipeline`、`ShaderTable`、AS 预建信息、Heap 绑定、SBT bake 和 `DispatchRays` 调用路径。
- 在独立 `RayTracing.h` 中定义 `DSM::RT` 的 `AccelStructDesc`、`GeometryDesc`、`InstanceDesc`、`PipelineDesc`、`ShaderTableDesc`、`DispatchRaysArguments`、`State`、资源接口和 `RefPtr` 句柄。
- 在 `IDevice` 追加 `CreateAccelStruct`、内存查询/绑定、预建信息查询、RT pipeline 与 shader table 创建接口。
- 在 `ICommandList` 追加 BLAS/TLAS 构建、AS copy、RT state 与 `DispatchRays` 接口。

**完成标准**：上层不访问 D3D12 原生接口，也能描述和调用完整的 DXR 工作流。

**结果**：完成。原始计划曾拟将类型放入 `GraphicsCommon.h` 并使用 `DSM::rt`；实施时按用户要求改为独立 `RayTracing.h` 和大写 `DSM::RT`。

### 里程碑 2：实现 D3D12 核心对象和加速结构内存模型

- 实现 `AccelStruct`：保存描述、预建信息和底层结果 `Buffer`，支持取得 GPU 虚拟地址。
- 实现 `RayTracingPipeline`：组装 DXIL library、hit group、shader config、pipeline config 与全局根签名的 StateObject，并保存 `ID3D12StateObjectProperties` 用于查询 shader identifier。
- 实现 `ShaderTable`：保存 ray-generation、miss、hit-group、callable 条目，在命令列表上传缓冲中写入 shader identifier，并构造 `D3D12_DISPATCH_RAYS_DESC`。
- 实现 NVRHI 风格 Heap 路径：`CreateAccelStruct` → `GetAccelStructMemoryRequirements` → `BindAccelStructMemory`。绑定 placed resource 后更新其 GPU 虚拟地址。
- 构建 BLAS/TLAS 时使用命令列表的 scratch/upload 分配器；构建前提交资源状态，构建后插入 UAV barrier，并保持输入、实例和结果资源的 GPU 生命周期。

**完成标准**：BLAS、TLAS 可被构建、绑定并作为 shader 可访问的 AS 使用。

**结果**：完成。更新路径校验 `AllowUpdate`、使用 `UpdateScratchDataSizeInBytes`，并将目标 AS 作为 source AS。

### 里程碑 3：补齐 AS 资源绑定与 RT 派发

- 为 `RayTracingAccelStruct` 生成 `D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE` 描述符，将 TLAS GPU 虚拟地址写入 SRV。
- 在 `SetRayTracingState` 绑定全局 root signature、pipeline、binding sets 和 descriptor heaps。
- 在 `DispatchRays` 烘焙 SBT 并调用 D3D12 `DispatchRays`。

**完成标准**：HLSL 中的 `RaytracingAccelerationStructure` 可追踪 TLAS，且 `RWStructuredBuffer<float4>` 获得光追结果。

**结果**：完成。

### 里程碑 4：实现端到端示例与可观察输出

- 新建 `Samples/RayTracing` 目标并接入根 `xmake.lua`。
- 创建三角形顶点缓冲、BLAS、单实例 TLAS、DXIL library、全局 binding layout、RT pipeline 和 shader table。
- 光追 shader 使用 miss shader 输出背景色，closest-hit shader 用重心坐标为三角形着色。
- 将 `RWStructuredBuffer<float4>` 的输出复制到 `CpuAccessMode::Read` 的 readback buffer，CPU 映射后导出 `512×512`、24-bit BMP。

**完成标准**：运行后生成可直接查看、包含背景和命中三角形的图片文件。

**结果**：完成。输出文件为 `bin/debug/RayTracing/RayTracingOutput.bmp`。

## 架构与关键决策

```text
CreateAccelStruct
  → GetAccelStructMemoryRequirements
  → CreateHeap
  → BindAccelStructMemory
  → BuildBottomLevelAccelStruct / BuildTopLevelAccelStruct
  → CreateRayTracingPipeline + CreateShaderTable
  → SetRayTracingState
  → DispatchRays
  → CopyBuffer 到 Readback Buffer
  → MapBuffer 并导出 BMP
```

| 决策 | 选择 | 原因 |
| --- | --- | --- |
| 命名空间 | `DSM::RT` | 用户明确要求使用大写 `RT`，且独立于通用图形定义。 |
| 三层对象 | `AccelStruct` / `RayTracingPipeline` / `ShaderTable` | 对齐 NVRHI 的职责拆分，避免把 pipeline/SBT 逻辑混入 Buffer。 |
| AS 内存 | 显式 Heap 绑定 | 对齐 NVRHI 的创建、查询、绑定流程，保留后续子分配和别名扩展空间。 |
| 根签名范围 | 仅全局根签名 | 首先闭环可运行路径；local-root 数据编码尚未具备完整设计。 |
| SBT 存储 | 命令列表上传缓冲 | 支持当前最小闭环；NVRHI 的持久 cache/version 策略列为后续优化。 |
| 可视化 | 一次性 BMP 导出 | 复用现有 buffer/readback 抽象，避免本次引入窗口与交换链系统。 |

## 与原始计划的实现差异

原始 CodeBuddy 计划是实施前设计，其中部分内容在落地时被纠正或明确延期：

- `GraphicsCommon.h` 中的 `DSM::rt` 方案改为独立 `RayTracing.h` 中的 `DSM::RT`。
- 原计划中的 local root signature、export association、local SBT 参数和 SBT cache/version 机制未开放；当前接口仅支持全局根签名。
- 原计划拟定多个独立 HLSL 文件；实际示例将最小 DXIL library 以内嵌 HLSL 在运行时写出并编译，便于独立运行。
- `ID3D12StateObjectProperties` 没有 `GetShaderIdentifierSize()`；实现采用 D3D12 规范常量 `D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES`。

这些调整均记录在审查文档中，并以 NVRHI 的实际 D3D12 行为和项目现有抽象为依据。

## 验证 (Validation)

在 `d:/Code/DSMEngine` 执行：

```powershell
xmake build RayTracing
xmake run RayTracing
```

结果：

- `xmake build RayTracing` 通过。
- `xmake run RayTracing` 退出码为 0。
- 成功生成 `d:/Code/DSMEngine/bin/debug/RayTracing/RayTracingOutput.bmp`。
- BMP 文件签名为 `BM`、尺寸为 `512×512`、文件大小为 `786486` 字节，检查到 `8193` 种 BGR 颜色，确认不是空白图。
- 直接运行 `RayTracing.exe` 时，当前 shell 可能缺少 DXC 运行时 DLL；应通过 `xmake run RayTracing` 启动。

## 进展 (Progress)

- [x] 完成 NVRHI D3D12 DXR 对比审查。
- [x] 完成 `DSM::RT` 的描述、资源接口和句柄。
- [x] 完成 D3D12 的 AccelStruct、RT pipeline、SBT、AS 构建与派发。
- [x] 完成 AS SRV 资源绑定与 Heap 绑定内存路径。
- [x] 完成 DXIL library 编译支持。
- [x] 完成离线 BMP 导出示例与构建、运行验证。
- [x] 将 CodeBuddy 原始计划与仓库 ExecPlan 整合为本文件。

## 意外与发现 (Surprises & Discoveries)

1. 初始示例仅向 GPU output buffer 写入结果，没有交换链、渲染目标或 `Present`，因此运行成功时没有可见画面。
2. D3D12 readback buffer 必须使用 `CpuAccessMode::Read`；后端将其创建在 `D3D12_HEAP_TYPE_READBACK` 中，状态为 `COPY_DEST`。
3. SBT 与 TLAS 实例数据不能使用不可映射的 GPU allocator；应使用可上传内存。
4. placed AS 资源绑定后需刷新 GPU 虚拟地址，否则 TLAS SRV 与构建输入地址会保持为零。
5. StateObject 子对象描述所引用的 vector 容器需预留容量，避免 reallocation 造成 D3D12 描述指针失效。

## 结果与复盘 (Outcomes & Retrospective)

DSMEngine 已具备最小可用的 D3D12 DXR 主路径：构建 BLAS/TLAS、将 TLAS 作为 SRV 绑定、创建 StateObject 和 SBT、派发光线，并导出可检查的图像。`docs/reviews/dxr-nvrhi-review.md` 保留设计差异和 NVRHI 对齐证据；本文件同时保留原始 CodeBuddy 计划的需求、方案和最终执行结果，作为仓库内归档版本。

后续若扩展，应优先按 NVRHI 设计补齐 local root signature、`SUBOBJECT_TO_EXPORTS_ASSOCIATION`、local SBT 参数，以及 version/descriptor-heap 驱动的 SBT 缓存；窗口和交换链呈现可在该基础上独立接入。
