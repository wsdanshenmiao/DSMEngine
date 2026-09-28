# DSMEngine UE 风格组织、项目资产归属与虚拟路径实施审查

## 状态

- **状态**：completed
- **日期**：2026-09-28
- **审查基线**：当前工作树，保留用户已有未提交改动；未重置、未清理无关改动。
- **第三方边界**：未修改任何第三方子模块源码；只迁移子模块路径并修改第一方构建接入。
- **验证产物**：`D:\Code\DSMEngine\build\verification\virtual-path-2026-09-28\`

## 一页总体判断

本次实施已经把原先“目录搬迁 + 固定物理路径搜索”推进为可运行的项目/引擎资产边界：

- Engine 资产位于 `Engine/Content`，项目资产位于 `Projects/<Name>/Content`；
- `Projects/Shared` 已删除，PBR 场景不再依赖兄弟目录；
- 项目和场景保存 `/Game/...` 等逻辑路径，不保存机器绝对路径；
- Runtime 通过 MountPoint 将 `/Game/`、`/Engine/`、`/GameShaders/`、`/EngineShaders/` 映射到物理目录；
- Xmake 和 CMake 都使用仓库内 `Engine/ThirdParty`，Assimp 不再从系统或用户缓存解析；
- 普通构建不再复制 Content 或 Shader；
- PBR、RayTracing、虚拟路径行为测试和 ReSTIR DI 验证均已实际运行通过。

### 风险排序

1. **P1：Runtime/Editor 的实现边界仍不完全干净**。`RendererDX12.cpp` 仍包含 ImGui/ImGuizmo 后端实现级依赖；这不影响本次构建，但会限制未来真正无 Editor 的 Runtime 部署。
2. **P1：挂载表当前是进程级全局状态**。项目切换和资源缓存生命周期仍需要更明确的 ProjectContext/资源缓存失效协议；当前单项目启动和验证已通过。
3. **P2：未执行 Release、设备丢失、非 RTX 设备和人工 Content Browser 交互验证**。这些不是本次迁移的构建阻塞，但仍是交付前验证项。
4. **P2：本机没有 `clang-tidy`、`cppcheck`、`clang-format`**，因此静态验证使用编译器、CMake/Xmake 配置、结构扫描、`git diff --check` 和专门行为测试替代。

## 修改前后对照

| 范围 | 修改前 | 修改后 | 验证 |
|---|---|---|---|
| 项目资产 | `Projects/Shared/Content` 被多个项目间接引用 | 资产由 PBR 持有；Engine 默认天空盒由 Engine 持有 | 旧引用扫描为 0；PBR 场景资产映射通过 |
| 资源身份 | `../Shared/Content/...`、物理路径和相对路径混用 | `/Game/...`、`/Engine/...`、`/GameShaders/...`、`/EngineShaders/...` | VFS 行为测试、PBR 加载、RayTracing 验证 |
| 路径解析 | `ContentPaths` 固定目录搜索 | `VirtualPath` + `MountPoint` + `VirtualFileSystem` | 读写、Provider、优先级、反向映射、枚举、越界测试 |
| 构建资产 | `AssetsCopy`/ShaderCopy 可能把源资源复制到二进制目录 | 普通构建不复制 Content/Shader；发布 staging 尚未引入 | 构建规则扫描为 0；Xmake/CMake 构建通过 |
| 序列化 | 旧字段和物理/相对资源引用 | `.dsmproj` schema v2，场景使用 `assetPath`/`scenePath` | 3 个项目描述检查通过；旧字段扫描为 0 |
| 写入安全 | 场景和 Content Browser 可直接操作物理路径 | 场景写入必须经过 `/Game/` 可写挂载；Engine 只读 | VFS 写入测试和 PBR `virtualPath=通过` |
| JSON 保存 | 直接截断目标文件 | 同目录临时文件 + Windows 原子替换 | 编译和项目加载验证通过 |
| Shader 缓存 | Shader key 使用解析后的物理路径 | 优先使用虚拟路径作为逻辑 key | PBR/RayTracing Shader 编译及运行通过 |
| Assimp | 构建入口存在远程/缓存解析歧义 | `Engine/ThirdParty/assimp` 本地源码构建 | CMake 配置输出本地 Assimp；子模块干净 |

## 主要实现位置

### 虚拟路径与挂载

- [VirtualPath.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualPath.h)
- [VirtualPath.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualPath.cpp)
- [MountPoint.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/MountPoint.h)
- [VirtualFileSystem.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualFileSystem.h)
- [VirtualFileSystem.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualFileSystem.cpp)
- [ContentPaths.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/ContentPaths.h)
- [ContentPaths.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/ContentPaths.cpp)

`VirtualFileSystem` 的关键行为：

- 解析前拒绝盘符、反斜杠、空路径段、`.`、`..` 和尾部 `/`；
- 读路径按优先级选择实际 Provider；
- 写路径跳过只读挂载；
- 物理边界检查使用 `weakly_canonical`，拒绝通过符号链接越出挂载根；
- 目录枚举拒绝绝对路径和含 `.`/`..` 的相对目录；
- `/Engine/` 在 `ContentPaths` 中以只读挂载注册，`/Game/` 以可写挂载注册。

### 项目和序列化

- [Project.cpp](file:///D:/Code/DSMEngine/Engine/Source/Editor/Project.cpp)
- [Project.h](file:///D:/Code/DSMEngine/Engine/Source/Editor/Project.h)
- [Serializer.h](file:///D:/Code/DSMEngine/Engine/Source/Editor/Serializer/Serializer.h)
- [Engine.dsmengine.json](file:///D:/Code/DSMEngine/Engine/Engine.dsmengine.json)
- [PBR.dsmproj](file:///D:/Code/DSMEngine/Projects/PBR/PBR.dsmproj)
- [RayTracing.dsmproj](file:///D:/Code/DSMEngine/Projects/RayTracing/RayTracing.dsmproj)

项目加载会先反序列化和校验新项目，再加载启动场景；新场景加载失败时不会直接替换当前 Scene。场景保存只接受 `/Game/` 虚拟路径，不能通过 `/Engine/` 绕过只读边界。

### Runtime 资源消费方

- [Model.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Render/Model.cpp)
- [TextureManager.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Render/TextureManager.cpp)
- [Shader.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Render/Shader.cpp)
- [ShaderCompiler.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Render/ShaderCompiler.cpp)
- [SkyboxPass.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Render/Renderer/CommonPass/SkyboxPass.h)
- [RestirDIRenderPipeline.cpp](file:///D:/Code/DSMEngine/Projects/RayTracing/Source/RayTracing/RestirDI/RestirDIRenderPipeline.cpp)

Model 的缓存 key 和场景中的 `assetPath` 使用虚拟路径；Assimp 只在导入边界接收已解析的物理源文件。模型内部的相对纹理引用仍相对于模型物理目录解析，随后转换回挂载下的逻辑路径进入 TextureManager。

### Editor 资源边界

- [EditorContentBrowser.cpp](file:///D:/Code/DSMEngine/Engine/Source/Editor/EditorUI/EditorContentBrowser.cpp)
- [EditorViewport.cpp](file:///D:/Code/DSMEngine/Engine/Source/Editor/EditorUI/EditorViewport.cpp)
- [EditorSceneHierarchy.cpp](file:///D:/Code/DSMEngine/Engine/Source/Editor/EditorUI/EditorSceneHierarchy.cpp)

Content Browser 的拖放 payload 使用 `/Game/...`，删除前先通过 VFS 取得可写物理路径；`/Engine/...` 不会被当作项目资源删除。

### 构建和验证入口

- [根目录 xmake.lua](file:///D:/Code/DSMEngine/xmake.lua)
- [Engine/xmake.lua](file:///D:/Code/DSMEngine/Engine/xmake.lua)
- [Engine/ThirdParty/xmake.lua](file:///D:/Code/DSMEngine/Engine/ThirdParty/xmake.lua)
- [根目录 CMakeLists.txt](file:///D:/Code/DSMEngine/CMakeLists.txt)
- [Engine/CMakeLists.txt](file:///D:/Code/DSMEngine/Engine/CMakeLists.txt)
- [Engine/ThirdParty/CMakeLists.txt](file:///D:/Code/DSMEngine/Engine/ThirdParty/CMakeLists.txt)
- [CMakePresets.json](file:///D:/Code/DSMEngine/CMakePresets.json)
- [tools/dsm.ps1](file:///D:/Code/DSMEngine/tools/dsm.ps1)
- [VFS 行为测试](file:///D:/Code/DSMEngine/Tests/VirtualFileSystem/VirtualFileSystemTests.cpp)

## 实际验证记录

### 静态和结构验证

| 命令/检查 | 结果 |
|---|---|
| `git submodule status` | 通过；顶层第三方路径均为 `Engine/ThirdParty/*` |
| `git -C Engine/ThirdParty/* status --short` | 通过；Assimp、DXC、GLFW、ImGui、ImGuizmo、EnTT、JSON、spdlog 子模块均干净 |
| `git diff --check` | 退出码 0；仅有 Git 关于 LF/CRLF 的提示 |
| `Shared/Content`、`../Shared` 扫描 | 0 个运行时代码/项目引用 |
| `modelFilePath`、`sceneFilePath`、`contentRoots`、`shaderRoots` 扫描 | 0 个项目序列化旧字段 |
| `Projects/Shared` 存在性 | 不存在 |
| `AssetsCopy`、`ShaderCopy`、`EngineShaderCopy` 扫描 | 0 个普通构建引用 |
| `clang-tidy`、`cppcheck`、`clang-format` | 当前环境未安装，未伪称通过 |

资产迁移证据：

- 346 个模型文件逐文件 SHA-256 一致；
- 12 个非 daylight 纹理逐文件 SHA-256 一致；
- 6 个 daylight 文件复制到 Engine 默认天空盒并通过哈希校验；
- PBR 场景 `Sponza.dsmscene` 和 `test0.dsmscene` 的唯一模型虚拟路径均映射到实际文件。

### Xmake

```powershell
xmake
xmake build DSMEngine
xmake build DSMEditor
xmake build PBR
xmake build RayTracing
xmake build VirtualFileSystemTests
```

结果：全部退出 0；最后一次完整 `xmake` 构建耗时约 21.2 秒。

```powershell
.\build\verification\virtual-path-2026-09-28\VirtualFileSystemTests.exe
```

结果：`VirtualFileSystem 行为测试通过`。

### CMake

使用本机 Visual Studio 18 2026 自带 CMake：

```powershell
cmake --preset engine
cmake --preset pbr
cmake --preset raytracing
cmake --build build/cmake/engine --config Debug --target DSMEngine
cmake --build build/cmake/engine --config Debug --target VirtualFileSystemTests
cmake --build build/cmake/pbr --config Debug --target PBR
cmake --build build/cmake/raytracing --config Debug --target RayTracing
```

结果：三个 preset 配置和所有列出的 Debug target 均通过。CMake VFS 测试输出同样为 `VirtualFileSystem 行为测试通过`。

### 工具入口

```powershell
.\tools\dsm.ps1 check Projects\PBR\PBR.dsmproj
.\tools\dsm.ps1 check Projects\PBR\test0.dsmproj
.\tools\dsm.ps1 check Projects\RayTracing\RayTracing.dsmproj
.\tools\dsm.ps1 build Projects\PBR\PBR.dsmproj -BuildSystem xmake -Configuration Debug
.\tools\dsm.ps1 build Projects\PBR\PBR.dsmproj -BuildSystem cmake -Configuration Debug
```

结果：全部通过。

### 运行和项目/资产加载

```powershell
PBR.exe --project Projects\PBR\PBR.dsmproj --validate-project
PBR.exe --project Projects\PBR\test0.dsmproj --validate-project
```

分别从项目目录和 CMake 构建目录启动，退出码均为 0。验证逻辑检查：

- 对象数大于 0；
- 有效 Mesh 数大于 0；
- 场景加载后未被标记为 dirty；
- `/Game/Scenes/Sponza.dsmscene` 可读；
- `/Engine/Textures/DefaultSkybox/daylight0.png` 可读；
- `/Game/Generated/virtual-path-check.tmp` 可写；
- `/Engine/Generated/virtual-path-check.tmp` 不可写。

最后一次可见输出为：`对象=404，有效网格=401，加载后未标脏=是，虚拟路径=通过`。

```powershell
RayTracing.exe --project Projects\RayTracing\RayTracing.dsmproj --validate-render --output build\verification\virtual-path-2026-09-28\render-final-4
RayTracing.exe --project Projects\RayTracing\RayTracing.dsmproj --validate-editor --frames 24 --output build\verification\virtual-path-2026-09-28\editor-final-4
```

两次退出码均为 0。最终 `status.raw.json`：

- Render：`passed=true`、`numeric_passed=true`、`artifacts_ok=true`、`debug_layer_passed=true`；
- Editor：`passed=true`、`rendered_frames=24`、`ui_frames=24`、`camera_moved=true`、`captured=true`、无编辑器前后警告。

Editor 截图已生成于：

`D:\Code\DSMEngine\build\verification\virtual-path-2026-09-28\editor-final-4\editor.bmp`

## 剩余不确定性和后续工作

1. 本次没有做 Release 构建、设备丢失恢复、非 RTX 设备和完整人工 Content Browser 点击/拖放回归；自动 Editor 验证已覆盖项目启动、UI 帧、相机移动、渲染截图和 Debug Layer。
2. `RendererDX12.cpp` 仍有 ImGui/ImGuizmo 后端实现级依赖。下一阶段应定义 Runtime 的 UI/调试绘制桥接接口，再把具体 UI backend 移到 Editor，而不是继续通过目录移动掩盖边界。
3. `VirtualFileSystem` 的挂载表目前是进程级对象，运行中切换 Project 时仍需要统一清理 Model、Texture、Shader 缓存和 Editor 选择状态；本次验证均为单项目启动，未把跨项目热切换宣称为已完成。
4. 还没有实现 UE 的 AssetRegistry、Pak/Cook、动态插件发现和完整 Package Name 系统；本次只实现了计划范围内的虚拟路径、挂载点、读写权限和序列化身份分离。
5. 发布 staging 命令仍待单独设计；普通构建不复制资源，但发布流程不能直接假设二进制目录包含全部 Content。
