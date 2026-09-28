# DSMEngine：移除 Shared、建立项目资产所有权与 UE 式虚拟路径系统

## 状态

- **状态**：completed
- **日期**：2026-09-28
- **基线**：当前工作树；已有未提交改动全部保留。
- **范围**：删除 `Projects/Shared`，迁移项目/Engine 资产，建立虚拟路径挂载系统，迁移项目和场景序列化，收紧写入边界，接入 Xmake/CMake，并完成 Debug 构建和运行验证。
- **范围外**：UE UObject、AssetRegistry、Pak/Cook、动态插件发现、完整 Package 格式、Release/设备丢失/非 RTX 验证、第三方源码修改。

## 摘要 (Summary)

项目资产现在由项目自身持有，Engine 只持有引擎级默认资源。`Projects/Shared/Content` 已删除；PBR 场景使用 `/Game/...`，Engine 天空盒使用 `/Engine/...`。Shader 使用独立的 `/GameShaders/...` 和 `/EngineShaders/...` 根。

运行时通过 `VirtualPath`、`MountPoint` 和 `VirtualFileSystem` 将逻辑资源身份与物理路径分离。普通构建不再复制 Content 或 Shader，Xmake 和 CMake 均从 `Engine/ThirdParty` 接入本地第三方源码。

## 背景 (Context)

实施前项目通过 `../Shared/Content` 和固定目录搜索加载资源；项目描述文件的 `contentRoots`/`shaderRoots` 只被工具检查，Runtime 不拥有真正的逻辑路径层。PBR 场景中有 401 个模型实例引用 Shared 资产。

最终所有权：

```text
Engine/Content                         Engine 默认资源、编辑器共享资源
Projects/PBR/Content                   PBR 项目资产
Projects/RayTracing/Content            RayTracing 项目资产
Engine/Shaders                         Engine Shader
Projects/<Name>/Shaders                项目 Shader
```

最终虚拟根：

```text
/Game/             → 当前项目 Content，可写
/Engine/           → Engine Content，只读
/GameShaders/      → 当前项目 Shaders，可写
/EngineShaders/    → Engine Shaders，只读
```

## 实施内容

### 资产迁移

1. 346 个模型从旧 Shared 资产复制到 `Projects/PBR/Content/Models`，逐文件 SHA-256 校验通过。
2. 12 个非 daylight 纹理复制到 `Projects/PBR/Content/Textures`，逐文件 SHA-256 校验通过。
3. `daylight0..5.png` 复制到 `Engine/Content/Textures/DefaultSkybox`，供 Engine SkyboxPass 和 RayTracing 默认环境使用。
4. PBR 场景的 `modelFilePath` 改为 `assetPath`，所有引用改为 `/Game/Models/...`。
5. 场景的 `sceneFilePath` 改为 `scenePath`，项目启动场景改为 `/Game/Scenes/...`。
6. 迁移完成后删除 `Projects/Shared`；运行时代码和项目数据中不再保留 Shared fallback。

### VFS 核心

新增：

```text
Engine/Source/Runtime/Core/VirtualFileSystem/
├─ VirtualPath.h/.cpp
├─ MountPoint.h/.cpp
└─ VirtualFileSystem.h/.cpp
```

行为：

- 统一 `/` 分隔符；拒绝反斜杠、盘符、空段、`.`、`..` 和尾部 `/`；
- MountPoint 支持 owner、priority、readOnly、recursive；
- 读路径按 Provider 优先级查找；
- 写路径跳过只读 Provider；
- 使用 `weakly_canonical` 检查物理路径，阻止符号链接越界；
- 支持物理路径反向转换为虚拟路径；
- 目录枚举拒绝绝对路径和 `.`/`..` 越界目录。

### 项目/序列化

- 新增 `Engine/Engine.dsmengine.json`；
- `.dsmproj` 升级 schema v2，包含 `engine`、`startupTarget`、`startupScene`、`modules`、`targets`、`plugins`；
- 项目加载前先校验 schema 和 Engine 名称；
- 项目或场景加载失败时不直接替换当前有效状态；
- 场景保存只能经过 `/Game/` 可写挂载；
- Serializer 使用同目录临时文件和 Windows 原子替换，避免直接截断有效 JSON；
- Content Browser 拖放 payload 使用虚拟路径，删除操作必须通过可写项目挂载。

### 构建系统

- Xmake Runtime/Editor 目标分离为 `DSMEngine` 和 `DSMEditor`；
- CMake 增加根、Engine、ThirdParty、PBR、RayTracing 和 Presets；
- Assimp 的 Xmake 包使用 `Engine/ThirdParty/assimp` 本地源码；
- CMake 使用本地 `add_subdirectory`，不调用 `find_package(assimp)` 或 `find_package(glfw3)`；
- 删除 `AssetsCopy`、`ShaderCopy` 和 `EngineShaderCopy` 的普通构建引用及规则；
- 新增 `VirtualFileSystemTests`，同时接入 Xmake 和 CMake。

## 验证 (Validation)

验证根目录：`D:\Code\DSMEngine`。
验证产物：`D:\Code\DSMEngine\build\verification\virtual-path-2026-09-28\`。

### 静态验证

```powershell
git submodule status
git -C Engine/ThirdParty/assimp status --short
git diff --check
rg "Shared/Content|../Shared|modelFilePath|sceneFilePath|contentRoots|shaderRoots" Engine Projects tools Tests
rg "AssetsCopy|ShaderCopy|EngineShaderCopy|Samples/Assets|GetSharedContentRoot" .
```

结果：

- 8 个顶层第三方子模块均干净；
- 旧 Shared 运行时引用为 0；
- 旧序列化字段为 0；
- `Projects/Shared` 不存在；
- 普通构建复制规则引用为 0；
- `git diff --check` 退出码 0，仅有 LF/CRLF 提示；
- `clang-tidy`、`cppcheck`、`clang-format` 在本机不可用，未伪称通过。

### 资产和项目检查

```powershell
.\tools\dsm.ps1 check Projects\PBR\PBR.dsmproj
.\tools\dsm.ps1 check Projects\PBR\test0.dsmproj
.\tools\dsm.ps1 check Projects\RayTracing\RayTracing.dsmproj
```

3 个项目描述全部通过 schema、Engine、启动场景和 Content/Shaders 根检查。PBR 三个场景中所有唯一 MeshRenderer 模型虚拟路径均可映射到实际文件。

### VFS 行为测试

```powershell
xmake build VirtualFileSystemTests
.\build\verification\virtual-path-2026-09-28\VirtualFileSystemTests.exe

cmake --build build/cmake/engine --config Debug --target VirtualFileSystemTests
.\build\cmake\engine\verification\Debug\VirtualFileSystemTests.exe
```

两套构建的测试均输出 `VirtualFileSystem 行为测试通过`，覆盖非法路径、优先级 Provider、只读/可写、反向映射、枚举、`..` 越界和符号链接越界（若当前权限允许创建符号链接）。

### 构建测试

```powershell
xmake
xmake build DSMEngine
xmake build DSMEditor
xmake build PBR
xmake build RayTracing
xmake build VirtualFileSystemTests

cmake --preset engine
cmake --preset pbr
cmake --preset raytracing
cmake --build build/cmake/engine --config Debug --target DSMEngine
cmake --build build/cmake/engine --config Debug --target VirtualFileSystemTests
cmake --build build/cmake/pbr --config Debug --target PBR
cmake --build build/cmake/raytracing --config Debug --target RayTracing
```

所有命令最终退出 0。最后一次 Xmake 完整构建耗时约 21.2 秒。

### 运行与资产加载

```powershell
PBR.exe --project Projects\PBR\PBR.dsmproj --validate-project
PBR.exe --project Projects\PBR\test0.dsmproj --validate-project
RayTracing.exe --project Projects\RayTracing\RayTracing.dsmproj --validate-render --output build\verification\virtual-path-2026-09-28\render-final-4
RayTracing.exe --project Projects\RayTracing\RayTracing.dsmproj --validate-editor --frames 24 --output build\verification\virtual-path-2026-09-28\editor-final-4
```

PBR 两个项目描述从不同工作目录启动均退出 0；最终可见输出包含：

```text
对象=404，有效网格=401，加载后未标脏=是，虚拟路径=通过
```

RayTracing 最终状态：

```text
render: passed=true, numeric_passed=true, artifacts_ok=true, debug_layer_passed=true
editor: passed=true, rendered_frames=24, ui_frames=24, camera_moved=true, captured=true
```

## 进展 (Progress)

- [x] 阅读 AGENTS.md、PLANS.md 和项目文档地图。
- [x] 记录并校验迁移前资产清单。
- [x] 迁移 346 个模型、12 个非 daylight 纹理和 6 个 Engine 默认天空盒纹理。
- [x] 删除 `Projects/Shared` 并清除旧运行时引用。
- [x] 实现 VFS、挂载表、读写权限、Provider、反向映射和越界检查。
- [x] 升级 Engine/Project/Scene 序列化契约。
- [x] 迁移 Model、Texture、Shader、Skybox、ReSTIR DI 和 Editor Content Browser。
- [x] 删除普通构建的全量资源/Shader 复制规则。
- [x] 接入 Xmake/CMake VFS 行为测试。
- [x] 完成静态、Debug 编译、PBR/RayTracing 运行和资产加载验证。

## 意外与发现 (Surprises & Discoveries)

- 首次 PowerShell 复制模型时产生了临时 `Models/Models` 嵌套目录，已清理并重新完成 346 个文件哈希校验。
- ReSTIR DI 环境加载器原先会在传入目录下再次拼接 `Textures/`，迁移 daylight 资源后已改为直接使用 Engine 默认天空盒目录。
- Editor 验证曾因把临时验证目录误设为活动 `/Game` 导致空帧，修正为真实项目根后 24 帧验证通过。
- CMake VFS 测试第一次配置发现源文件路径错误，已修正为 `Tests/VirtualFileSystem/VirtualFileSystemTests.cpp`，最终 preset 和构建均通过。

## 决策记录 (Decision Log)

- 使用 `/Game`、`/Engine`、`/GameShaders`、`/EngineShaders`，不提供模糊的 `/Content` 覆盖别名。
- Shared 中的 daylight 归 Engine，其余样例资产归 PBR；迁移是一次性实体复制，不在运行时复制。
- 基础第三方库不插件化；插件目录只为未来真正可选的 Runtime/Editor 功能预留。
- Runtime 的物理路径只作为导入/编译边界内部值，持久化身份和缓存 key 使用虚拟路径。
- 不在本阶段实现 Pak/Cook/AssetRegistry/UObject 包系统。

## 结果与复盘 (Outcomes & Retrospective)

本计划的目标已经完成：项目不再依赖 Shared 目录，资产身份与物理存储分离，Engine/Project/Shader 的挂载边界可验证，Xmake/CMake 和运行时加载均通过。

遗留项：

1. Runtime/Editor UI backend 仍有实现级耦合；
2. VFS 是进程级全局状态，跨项目热切换的缓存失效协议尚需单独重构；
3. Release、设备丢失、非 RTX 和人工 Content Browser 交互验证另列为后续验证任务；
4. 发布 staging、AssetRegistry、Pak/Cook、动态插件发现不属于本阶段交付。

详细修改前后对照、代码链接和验证证据见：

`Docs/Reviews/2026-09-28-UEOrganizationImplementation.md`
