# DSMEngine 工程审查记录（2026-09-24）

## 状态

- **审查类型**：当前工作树分阶段工程审查
- **审查日期**：2026-09-23 至 2026-09-24
- **审查基线**：当前工作树源码、配置、工程文档和验证产物
- **交付状态**：阶段性审查记录；已覆盖主要第一方模块，但不表示所有运行时环境和异常路径都已证明无风险
- **源码变更**：本次审查没有修改源码、配置或工程数据

## 1. 总体判断

DSMEngine 当前可以成功完成默认 Debug 构建、PBR 目标构建和 RayTracing 目标构建；固定 ReSTIR DI 渲染验证和 22 帧编辑器验证也能够通过，D3D12/DXGI Debug Layer 没有报告警告或错误。

但当前第一方代码仍存在几类高风险问题：

1. **数据安全风险**：场景加载失败时可能替换掉当前内存场景；序列化直接覆盖目标文件且没有原子替换；Content Browser 可以无确认递归删除目录。
2. **渲染正确性风险**：BVH 根节点包围盒未初始化；MeshRenderer 世界包围盒可能重复变换且不会随 Transform 自动更新；方向光数量超过上限时写越界；Tile-based lighting 的 groupshared 位掩码未初始化；16 位索引路径判断错误。
3. **错误处理风险**：Shader 编译失败后继续解引用空 COM 对象；D3D12 原生 Buffer 包装没有保存原生资源；部分 GPU 提交和等待失败仍返回成功。
4. **架构契约风险**：EventDispatcher 不使用 `Event::m_Handled`；Play/Stop 状态没有真正切换运行场景；NativeScript 启用状态和生命周期没有接入 Scene 更新；Scene copy/move 契约不成立。
5. **构建部署风险**：RayTracing 目标复制 DXC DLL，但 PBR 目标没有复制；第三方 Assimp 同时存在仓库源码和 xmake 包缓存来源；已有 Visual Studio 生成工程与当前 xmake 目标图存在偏差。

没有发现必须定为 P0 的系统级远程执行问题；但下列 P1 问题已经足以造成场景数据丢失、崩溃、黑屏/漏渲染或发布包无法启动。

## 2. 覆盖范围与调用入口

- **构建与第三方**：`xmake.lua`、`rules.lua`、各 target 的 xmake 文件、ThirdParty 接入规则、DXC/Assimp/GLFW/ImGui/EnTT/JSON 使用边界。
- **Runtime**：Core、Event、Platform、Framework、Math、Utils、Graphics、D3D12、DXR、Render、Forward/Deferred/CommonPass。
- **Editor**：Project、Serializer、Content Browser、Scene Hierarchy、Viewport、Play/Stop、保存/加载入口。
- **Shader 与示例**：`DSMEngine/Shaders`、PBR、RayTracing/ReSTIR DI、GPU ABI、资源寄存器和历史 Reservoir。
- **明确未做**：ThirdParty 逐文件审查、PBR 交互运行、Release 构建、设备丢失恢复、WARP/非 RTX GPU、16 位索引压力测试。

## 3. 具体问题：构建、部署、Core、Event、Platform

### [P1][已确认] PBR 运行目录没有 DXC Runtime DLL

- **位置**：
  - `D:\Code\DSMEngine\DSMEngine\xmake.lua:8-10`
  - `D:\Code\DSMEngine\Samples\PBR\xmake.lua:11-14`
  - `D:\Code\DSMEngine\Samples\RayTracing\xmake.lua:10-14`
  - `D:\Code\DSMEngine\rules.lua:53-83`
- **触发条件**：在没有系统级 DXC DLL 的机器上启动 PBR。
- **证据**：`bin\debug\RayTracing` 中有 `dxcompiler.dll` 和 `dxil.dll`，`bin\debug\PBR` 中没有；`ShaderCompiler` 在 `ShaderCompiler.cpp:97` 以全局静态对象初始化。
- **影响**：PBR 可能在进入编辑器前启动失败。
- **最小修复**：将 `DXCRuntimeCopy` 作为所有使用 ShaderCompiler 的目标的公共规则。
- **验证**：删除 PBR 输出目录后重新构建，并在不依赖 PATH 的环境中启动。

### [P2][已确认] AssetsCopy 只在目标目录不存在时复制

- **位置**：`D:\Code\DSMEngine\rules.lua:42-50`
- **触发条件**：目标 `Assets` 目录已经存在，源模型或纹理发生更新。
- **影响**：运行目录继续使用旧资源，源码和运行结果不一致。
- **建议**：改为按文件同步、时间戳复制或显式清理后复制。

### [P2][已确认] Assimp 来源和 IDE 工程图不统一

- **位置**：`xmake.lua:26`、`DSMEngine\xmake.lua:8`、`ThirdParty\xmake.lua`、`vsxmake2022\DSMEngine.sln`
- **证据**：当前构建使用 xmake 包缓存中的 Assimp v6.0.5，仓库同时存在 `ThirdParty\assimp`；现有 solution 还包含独立 `RestirDI` 工程，而当前 xmake 文件只声明 PBR 和 RayTracing。
- **影响**：离线新环境、IDE 构建和 xmake 构建可能使用不同依赖和不同目标集合。
- **建议**：确定唯一依赖来源，固定版本/lock；构建图变化后重新生成 VS 工程。

### [P2][已确认] 文档路由存在失效引用

- **位置**：`Documents\README.md:14,40,155,294`、`docs\knowledge\README.md:28`
- **问题**：旧 README 使用错误的 `Runtime/*.md` 相对路径；知识库仍链接已经删除的 ReSTIR 指南文件。
- **建议**：增加 Markdown 路径检查，删除失效链接。

### [P1][已确认] GLFW 初始化/窗口创建失败后继续使用窗口指针

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Core\Window.cpp:18-31`
- **触发条件**：`glfwInit()` 或 `glfwCreateWindow()` 失败。
- **影响**：继续设置回调、切换 context 或析构未初始化窗口，可能崩溃。
- **建议**：窗口创建使用显式失败返回；`m_Window` 初始化为 `nullptr`，失败后立即停止。

### [P2][已确认] Engine 生命周期不可重启，关闭后继续更新可能空指针

- **位置**：`Runtime\DSMEngine.h:39-60`、`Runtime\DSMEngine.cpp:12-42`
- **问题**：`StartEngine` 不重置 `m_Running`；`ShutDownEngine` 后 `Update` 没有状态保护。
- **建议**：引入显式生命周期状态，拒绝非法状态转换。
- **验证**：Start→Close→Start、Update-before-Start、Update-after-Shutdown。

### [P2][已确认] 滚轮回调错误构造 MouseMovedEvent

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Core\Window.cpp:108-116`
- **影响**：`MouseScrolledEvent` 永远不会从 GLFW 滚轮输入路径产生。
- **修复**：构造 `MouseScrolledEvent(xOffset, yOffset)`。

### [P2][已确认] EventDispatcher 丢弃回调返回值且不设置 m_Handled

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Event\Event.h:52-67`
- **影响**：事件无法阻止后续传播；`Event::m_Handled` 当前没有有效语义。
- **修复**：将回调返回值合并到 `m_Handled`，调用方尊重已处理状态。

### [P2][已确认] Windows 文件对话框使用 A 版 API，过滤器和默认扩展名构造不可靠

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Platform\Windows\PlatformUtils.cpp:100-140`
- **问题**：NUL 嵌入字符串、`lpstrDefExt` 传入完整 pattern、UTF-8 路径通过 ANSI API。
- **影响**：中文路径、过滤器和自动扩展名可能错误。
- **修复**：改用 `OPENFILENAMEW`，显式构造双 NUL filter，默认扩展名只传 `.dsmproj` 等。
## 4. 具体问题：Framework、项目、序列化、Math、Utils

### [P1][已确认] LoadScene 在新场景验证成功前替换当前场景

- **位置**：`D:\Code\DSMEngine\DSMEngine\Editor\Project.cpp:103-131`
- **触发条件**：当前场景存在，目标场景不存在、损坏或反序列化失败。
- **证据**：旧场景保存发生在 `109-119`；新空场景替换发生在 `127`；反序列化失败后没有恢复旧场景。
- **影响**：旧场景可能已保存，内存中又被空场景替换，编辑状态丢失。
- **修复**：先反序列化临时 Scene，成功后再交换全局 Scene。
- **验证**：加载不存在文件、损坏 JSON、缺字段 JSON，失败后旧对象数和路径必须不变。

### [P1][已确认] Serializer 没有异常边界和原子写入

- **位置**：`D:\Code\DSMEngine\DSMEngine\Editor\Serializer\Serializer.h:42-72,316-350`
- **问题**：JSON 解析/字段访问异常会越过编辑器命令边界；目标文件直接 truncate；写失败仍可能返回成功。
- **影响**：坏文件导致编辑器崩溃；中断或磁盘不足可能损坏原文件。
- **修复**：schema 校验、结构化错误、临时文件+flush+原子替换。

### [P1][已确认] 序列化使用绝对路径和 typeid(T).name()

- **位置**：`Serializer.h:280-285,320-325`、`Project.cpp:42-56`
- **证据**：当前 `.dsmproj` 保存 `D:\Code\DSMEngine\...` 绝对路径；组件 key 是 `class DSM::MeshRenderer` 等编译器相关名称。
- **影响**：项目移动到另一目录或另一机器后无法可靠加载；编译器、ABI、命名空间变化会破坏旧场景。
- **修复**：保存 project-relative 规范化路径；使用稳定字符串类型名；添加 `schemaVersion`。

### [P2][已确认] Scene 拷贝赋值会追加对象，移动操作实际是深拷贝

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Framework\Scene.cpp:22-43,134-251`
- **问题**：`operator=` 没有清空目标；移动构造/移动赋值同样调用 `CopyScene`。
- **影响**：非空目标赋值后对象重复；move 仍然复制全部 ECS 对象。
- **修复**：明确 copy=完全克隆、move=转移所有权，然后分别实现。

### [P2][已确认] Scene 更新忽略 GameObject 和 NativeScript 的启用状态

- **位置**：`Runtime\Framework\Scene.cpp:45-65`、`GameObject.h:29-30`、`NativeScript.h:18-26`
- **问题**：只检查脚本指针，不检查对象和组件是否 enabled。
- **影响**：禁用对象或脚本后仍执行 `OnUpdate`/`OnGUI`。
- **修复**：更新条件必须同时满足 object enabled、component enabled、script 非空。

### [P1][已确认] Content Browser 可以无确认递归删除项目目录

- **位置**：`D:\Code\DSMEngine\DSMEngine\Editor\EditorUI\EditorContentBrowser.cpp:134-151`
- **问题**：Delete 直接对目录执行 `remove_all`，没有确认、回收站、undo，也没有保护 Assets、Library、项目根目录。
- **影响**：误操作可删除整个资源目录。
- **修复**：保护特殊路径、二次确认、优先移动到回收站或实现可恢复删除。

### [P2][已确认] Play/Stop 按钮没有真正切换运行场景

- **位置**：`EditorMenuBar.cpp:211-217`、`EditorUI.cpp:203-213`
- **问题**：Toolbar 只修改 `m_SceneState`；`OnScenePlay`/`OnSceneStop` 没有调用方。
- **影响**：Play 状态下不会创建运行时 Scene 副本，运行时可能直接修改编辑场景。
- **修复**：toggle 时显式调用 `OnScenePlay`/`OnSceneStop`，处理重复点击、加载场景和退出清理。

### [P1][已确认] BVH 新父节点没有包围盒

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Math\Collision\BVH.h:117-148`、`RenderResource.cpp:93-104,167-184`
- **问题**：新建 `newParent` 后没有设置 `Union(sibling->bounds, newNode->bounds)`；`newNode->UpdateBounds()` 不会更新父节点。
- **影响**：多个物体插入后根节点可能保持无效范围，视锥裁剪结果错误。
- **修复**：插入、旋转、删除后统一向根回溯更新 bounds 和 height。

### [P1][已确认] MeshRenderer 世界包围盒重复变换且不会随 Transform 同步

- **位置**：`MeshRenderer.h:20-29`、`BVH.h:101-104`、`TransformComponent.h:43-47`
- **问题**：`SetMesh` 已将 local bounds 变换为 world bounds，BVH 插入又乘一次 Transform；Transform 修改只标记 dirty，不更新 renderer bounds。
- **影响**：非单位变换下包围盒错误，物体可能被错误裁剪。
- **修复**：Renderer 保存 local bounds，BVH 每次按当前 Transform 计算 world bounds；或建立明确 dirty propagation。

### [P2][已确认] Renderer/GetMaterial 和空 MeshRenderer 的边界契约不一致

- **位置**：`Renderer.h:22`、`MeshRenderer.h:37-49`、`RenderResource.cpp:114-121`
- **问题**：渲染路径使用严格的 `GetMaterialIndex`/`GetMaterial`，但新建 MeshRenderer 或材质数组不完整时可能越界；编辑器也直接 `GetMaterial(0)`。
- **影响**：空组件、部分导入失败或材质缺失时可能断言或越界。
- **修复**：统一定义缺省材质/材质索引行为；公共 API 返回 optional 或安全默认值。

### [P1][已确认] 16 位索引 Mesh 上传判断错误枚举

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Render\Mesh.cpp:196-203`
- **问题**：判断的是 `Format::R16_FLOAT`，索引格式应为 `Format::R16_UINT`。
- **影响**：16 位索引走 32 位路径，产生断言、错误字节数或错误索引数据。
- **修复**：判断 `R16_UINT`，同时验证索引范围。

### [P2][已确认] Windows DirectXMath Normalize 没有写回结果

- **位置**：`XMVector.h:222-224,353-354`；调用点 `Geometry.cpp:339-344`
- **问题**：`XMVector3::Normalize`/`XMVector4::Normalize` 调用了 DirectXMath 但没有把返回值赋回 `m_Vector`。
- **影响**：几何生成切线不会归一化，法线贴图/高光可能受模型尺寸影响。
- **修复**：将返回值赋给 `v.m_Vector`。

### [P1][已确认] ThreadPool 的 thread_local 队列不绑定具体 ThreadPool

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Utils\ThreadPool.h:41-54,103-110`
- **触发条件**：Pool A 的 worker 中向 Pool B 调用 `Submit`。
- **影响**：任务可能被放进 Pool A 的本地队列，Pool B 的 future 长时间不完成。
- **修复**：worker context 必须带 `ThreadPool*`，只有 context.pool == this 时才能投递本地队列。
- **验证**：两个池相互提交任务，设置有限超时检查 future。

### [P2][已确认但当前 Windows 未覆盖] 通用 Math fallback 存在确定错误

- **位置**：`Runtime\Math\Matrix.h:38,133-139,163-207,240-249`
- **问题**：矩阵除法实际调用乘法；4×4 乘法只处理前三行；旋转矩阵写入 `Set(4, ...)`；Z 轴矩阵第三行错误；Transpose 返回原矩阵而非结果。
- **建议**：如果项目明确只支持 Windows，删除未维护 fallback；否则先对齐外部数学契约再修复。

### [P2][已确认] Scene.DestroyObject 会把被删除子对象留在 RootObjects

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Framework\Scene.cpp:103-131`
- **问题**：销毁层级时对子节点调用 `SetParent(nullptr)`，子节点会被插入 `m_RootObjects`，之后只从 `m_Objects` 删除，没有从 root set 删除。
- **影响**：根列表残留已销毁对象，后续层级绘制和序列化可能访问失效实体。
- **修复**：删除节点前从 root set 清除；删除父对象时统一维护层级索引。

## 5. 具体问题：Graphics、D3D12、DXR、Render、HLSL

### [P1][已确认] DXC 编译失败路径继续解引用空 COM 对象

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Render\ShaderCompiler.cpp:61-89,130-140`
- **问题**：`AssertShaderCompiler` 只记录错误，不改变控制流；编译失败后仍访问 `shaderByteCode->GetBufferSize()`。
- **影响**：缺失 shader、语法错误或 include 错误可能直接崩溃。
- **修复**：每一步检查 HRESULT 和输出对象，失败时返回无效 `ShaderByteCode`，保留文件/entry/target/defines 诊断。

### [P1][已确认] Debug Layer 初始化失败时可能解引用空错误回调

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Graphics\D3D12\D3D12-Device.cpp:213-225`
- **触发条件**：直接使用默认 `DeviceDesc`，且系统没有 D3D12 Debug Layer。
- **影响**：`m_Context.messageCallback` 为空时可能崩溃。
- **修复**：内置 no-op/default callback，所有回调调用前检查为空。

### [P1][已确认] 原生 Buffer 包装接口没有保存传入资源

- **位置**：`D3D12-Buffer.cpp:103-118`、`D3D12-Device.cpp:699-708`
- **问题**：`resource = resource` 是参数自赋值；`CreateHandleForNativeBuffer` 创建 wrapper 后也没有绑定资源。
- **影响**：返回的 Buffer 没有有效 ID3D12Resource、GPU VA、SRV 或 Map 语义。
- **修复**：参数改名为 `nativeResource`，赋给成员 `this->resource`，并完整注册状态追踪。

### [P1][已确认] Tiled resource API 有空指针和越界错误

- **位置**：`D3D12-Device.cpp:500-529,531-597`
- **问题**：`GetTextureTiling` 使用固定 16 项数组却按 `*numTiles` 写入；`UpdateTextureTileMappings` 的 heap 判定条件反了。
- **影响**：tiled texture 路径可能越界写或直接崩溃。
- **修复**：按真实数量分配 tiling 数组，修正 heap 判定并校验 region/offset。

### [P2][已确认] Shader-visible descriptor heap 扩容一次申请到上限

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Graphics\D3D12\DescriptorHeap.cpp:131-148`
- **问题**：使用 `newSize = max(newSize, maxSize)`，第一次扩容就可能分配 Tier 1 最大堆。
- **影响**：描述符数量稍微增长就可能产生不必要的显存/驱动压力。
- **修复**：使用 `min(nextPowerOfTwo, maxSize)`，超过上限显式失败。

### [P2][已确认] GPU 提交和等待失败仍可能返回成功

- **位置**：`D3D12-Device.cpp:1402-1413,1421-1429`
- **问题**：`ExecuteCommandLists` 只记录设备移除原因仍返回 fence；`WaitForIdle` 无论 Signal/Wait 结果都返回 true。
- **影响**：上层验证会把设备丢失或 Fence 失败误判为成功。
- **修复**：检查 Signal、SetEventOnCompletion、WaitForSingleObject 和设备移除原因，返回结构化错误。

### [P1][已确认] 方向光数量超过 4 时写越界

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Render\Renderer\CommonPass\LightingPass.h:101-123`
- **问题**：方向光数组固定为 4，但方向光分支没有上限判断。
- **影响**：第五个方向光开始写越界。
- **修复**：超过上限时跳过并告警，或改为动态容量。
- **验证**：0、1、4、5、16 个方向光。

### [P1][已确认] Tile-based lighting 的 groupshared 位掩码未初始化

- **位置**：`D:\Code\DSMEngine\DSMEngine\Shaders\ForwardShader\TileBasedLighting.hlsl:17-20,29-33,70-90`
- **问题**：`gsTileInfo.lightMask[]` 没有在线程组初始化阶段清零，后续直接 `InterlockedOr`。
- **影响**：Tile 可能包含随机光源 bit，产生闪烁、错误光照或跨帧残留。
- **修复**：线程组协作清零所有 `MAX_LIGHT_COUNT_PER_TILE / 32` 元素，再同步。

### [P2][已确认] 空场景时 RenderResource 不清理旧 BVH

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Render\Renderer\CommonPass\RenderResource.cpp:61-80`
- **问题**：`objView.size_hint() == 0` 直接返回，旧 BVH leaf 不删除。
- **影响**：旧 GameObject 仍被 BVH 强引用，加载空场景后仍可能参与遍历。
- **修复**：空视图也走 BVH 删除和资源清空路径。

### [P2][已确认] Model boundingBox 的 Union 结果被丢弃

- **位置**：`D:\Code\DSMEngine\DSMEngine\Runtime\Render\Model.cpp:150-156`
- **问题**：`AxisAlignedBox::Union(model.boundingBox, mesh->bounds)` 返回值没有赋回。
- **影响**：多 Mesh 模型的 `Model::boundingBox` 可能保持默认无效值。
- **修复**：赋值回 `model.boundingBox`。

### [P2][已确认] 默认 Cube 纹理只写入第一个 array slice

- **位置**：`TextureManager.cpp:72-77`、`D3D12-CommandList.cpp:392-439`
- **问题**：Cube texture 设置 6 个 slice，但只调用一次 `WriteTexture(arraySlice=0)`。
- **影响**：默认 Cube 其他面内容未初始化。
- **修复**：对六个 slice 分别上传，或提供完整 subresource 上传接口。

### [P2][已确认] 绑定/资源状态契约依赖队列类型分支，容易形成隐性同步缺陷

- **位置**：
  - `D3D12-CommandList.cpp:1362-1403`
  - `D3D12-CommandList.cpp:1620-1624`
  - `D3D12-CommandList.cpp:1416-1515`
- **问题**：自动绑定状态追踪只在 Graphics queue 分支启用；Compute queue 通过 `SetComputeState` 时不会统一为 binding set 资源建立状态要求。与此同时，状态追踪在 Compute queue 中还会剥离若干状态位。
- **影响**：新增 Compute pass 或修改资源用途时，可能出现状态依赖只在某些队列成立的情况。
- **最小修复**：把资源读写声明从“队列类型特判”改为 pass/绑定契约；Compute/Graphics 都生成合法状态转换，跨队列依赖由 fence 明确表达。
- **验证**：用同一资源在 Copy→Compute→Graphics 间轮转，并启用 D3D12 Debug Layer。

### [P2][已确认] ThreadPool、序列化和 D3D12 公共 API 的 noexcept/失败契约不一致

- **位置示例**：`GameObject.h:40-54`、`MeshRenderer.h:20-31`、`Renderer.h:22-31`、`D3D12-Device.cpp` 多处。
- **问题**：标记 `noexcept` 的接口内部仍可能分配、调用 EnTT、解引用空资源或触发异常；失败时又常返回空句柄、默认值或继续执行。
- **影响**：异常被 terminate，调用方无法区分“空资源”“合法默认值”和“设备失败”。
- **建议**：先定义返回契约，再统一使用 `expected`/错误对象/显式状态；不要用 `noexcept` 掩盖资源创建失败。

## 6. ReSTIR/DXR 验证结果

### 已通过

- `xmake build RayTracing`：通过。
- ReSTIR render validation：
  - `passed=true`
  - `numeric_passed=true`
  - `fixed_one_spp_passed=true`
  - `debug_layer_passed=true`
  - D3D12/DXGI message 数组为空。
- ReSTIR editor validation，22 帧：
  - `executed_frames=22`
  - `rendered_frames=22`
  - `ui_frames=22`
  - `camera_moved=true`
  - `debug_layer_passed=true`
  - `passed=true`
- 输出图像人工检查：固定场景的墙体、地面、物体和环境存在；1 SPP 噪声明显，但没有整帧黑屏或主要几何体消失。

### 解释和边界

- ReSTIR validation 是独立固定场景，不覆盖用户当前 `Projects\Assets\Scene\test0.dsmscene`。
- 1 SPP 图像噪声是当前算法设置的预期现象，不应单凭视觉噪声判定为 GPU 同步错误。
- 当前未验证多 GPU、WARP、设备移除、极端光源数量、超大材质纹理表和真实生产场景。

## 7. 验证命令与结果

| 命令 | 结果 |
|---|---|
| `xmake --version` | 通过，xmake v3.0.9+HEAD.2b184e178 |
| `xmake` | 通过，约 44.39 秒 |
| `xmake build PBR` | 通过，约 32.875 秒 |
| `xmake build RayTracing` | 通过，约 4.5 秒 |
| `xmake require --info assimp` | 确认使用 xmake 包缓存 Assimp v6.0.5 |
| `xmake run RayTracing --validate-render --output .tmp/review-20260923-restir` | 通过，生成验证图像、metrics、status.raw.json |
| `xmake run RayTracing --validate-editor --output D:\Code\DSMEngine\.tmp\review-20260923-restir-editor --frames 3` | 按验证逻辑失败，3 帧不足以触发第 20 帧的相机移动检查 |
| `xmake run RayTracing --validate-editor --output D:\Code\DSMEngine\.tmp\review-20260923-restir-editor-22 --frames 22` | 通过，exit_code=0 |
| `git diff --check HEAD` | 用户已有 PDF 新增内容产生大量 trailing whitespace 诊断；未修改这些文件 |
| `xmake run PBR` | 未执行，避免编辑器退出自动保存当前用户场景 |
| `xmake f -m release && xmake` | 未执行 |
| `xmake project -k vsxmake2022` | 未执行 |

## 8. 跨模块改进路线图

### 立即修正确性和数据安全

1. 修复 `LoadScene` 的临时加载后交换。
2. Serializer 增加 schema 校验、异常边界、临时文件和原子替换。
3. 项目路径改为 project-relative，组件 key 改为稳定字符串。
4. 修复 BVH 父节点 bounds 与 MeshRenderer local/world bounds 契约。
5. 限制方向光数量，初始化 TileInfo。
6. 修复 16 位索引和 DirectXMath Normalize。
7. 为 PBR 复制 DXC DLL。
8. 修复 ShaderCompiler 空结果处理、原生 Buffer wrapper 和 tiled resource API。
9. 禁止 Content Browser 递归删除受保护目录。

### 对齐外部契约

- Scene：copy 必须完全克隆，move 必须转移所有权；load 失败不能改变当前场景。
- Serializer：带 schema version，路径规范化，组件类型稳定，坏输入返回诊断。
- IDevice：提交、等待、设备移除、资源创建必须返回可判定的错误状态。
- IRenderPipeline：pass 显式声明资源读写和队列依赖，不把 fence 依赖散落在各 Pass 中。
- Editor：Play/Stop 真正切换运行场景；删除具有确认和可恢复语义。

### 后续重构

- 用稳定句柄或 GUID 减少渲染索引对 `shared_ptr<GameObject>` 的依赖。
- 统一 RenderResource、BVH、Material/Texture table 的资源同步阶段。
- 为 ThreadPool 引入绑定实例的 worker context。
- 将 D3D12 descriptor heap、resource state、GPU lifetime 统一为可审计的资源图/帧图。
- 为 ShaderCompiler 增加编译缓存和完整诊断。
- 将 PBR、DXR、Editor validation 统一为可重放 fixture。

## 9. 剩余不确定性

1. PBR Forward/Deferred 实际窗口运行、当前用户场景加载和 ShaderCompiler 启动路径未在本轮运行入口中验证。
2. 当前用户场景 `D:\Code\DSMEngine\Projects\Assets\Scene\test0.dsmscene` 的最新未提交内容没有被 PBR 入口覆盖验证。
3. Release 优化下的生命周期、未定义行为和异常路径未验证。
4. 设备丢失、Resize/Minimize、WARP、其他 GPU 型号未验证。
5. ThirdParty 本身未逐文件审查。
6. 线程池、异步模型加载、TextureManager 并发加载尚未做压力测试。
7. 非 Windows Math fallback 已由源码确认存在问题，但未进行跨平台编译。

## 10. 记录说明

本文件是对 2026-09-23/24 工程审查结果的持久记录，不替代后续修复计划。修复重要问题时，应在对应 ExecPlan 或新的 review 记录中补充：目标接口、行为边界、实际验证命令、产物路径和剩余风险。
