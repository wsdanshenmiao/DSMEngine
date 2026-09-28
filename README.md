# DSMEngine

DSMEngine 是一个面向 Windows/MSVC 的 C++23、Direct3D 12 实时渲染引擎实验项目。项目使用 Xmake 作为默认构建入口，同时提供 CMake 构建方式，包含 Runtime、Editor、PBR 示例、RayTracing/ReSTIR DI 示例，以及一套基于虚拟路径的 Engine/Project 资源组织方案。

## 项目特点

- **Runtime / Editor 分离**：Runtime 位于 `Engine/Source/Runtime/`，Editor 位于 `Engine/Source/Editor/`。
- **Direct3D 12 渲染**：包含 D3D12 资源、命令列表、管线、Shader 编译和 RayTracing 支持。
- **Engine / Project 分离**：Engine 资源位于 `Engine/Content/`，项目资源由各自的 `Projects/<Name>/Content/` 持有。
- **虚拟资源路径**：运行时使用 `/Game/`、`/Engine/`、`/GameShaders/` 和 `/EngineShaders/`，避免在场景和项目文件中保存机器绝对路径。
- **本地第三方依赖**：第三方源码位于 `Engine/ThirdParty/`，包括 Assimp、GLFW、DXC、ImGui、ImGuizmo、EnTT、JSON、spdlog 等。
- **PBR 示例**：位于 `Projects/PBR/`，用于验证模型、材质、纹理、Forward Renderer 和项目资源加载。
- **RayTracing 示例**：位于 `Projects/RayTracing/`，包含 ReSTIR DI、DXR、环境光和自动化渲染验证。
- **行为验证**：`Tests/VirtualFileSystem/` 提供虚拟路径的读写、挂载、优先级、反向映射和越界行为测试。

## 当前边界

当前 Engine 和 Editor 主要构建为静态库；PBR、RayTracing 和验证测试目标提供可执行入口。Engine 目前还没有类似 Unreal Editor 的独立 `EditorHost.exe`，因此不能直接运行 `DSMEngine` 本身；需要通过 PBR、RayTracing 或测试目标启动进程。

当前阶段也没有实现 UE 的 UObject、AssetRegistry、Pak/Cook、动态插件发现和完整 Package 系统。虚拟路径系统目前负责挂载、解析、读写权限和物理路径反向映射。

## 目录结构

```text
D:\Code\DSMEngine\
├─ Engine/
│  ├─ Source/
│  │  ├─ Runtime/
│  │  └─ Editor/
│  ├─ Shaders/
│  ├─ Content/
│  ├─ ThirdParty/
│  ├─ Engine.dsmengine.json
│  ├─ xmake.lua
│  └─ CMakeLists.txt
├─ Projects/
│  ├─ PBR/
│  │  ├─ PBR.dsmproj
│  │  ├─ Source/PBR/
│  │  ├─ Content/
│  │  ├─ Shaders/
│  │  ├─ Binaries/
│  │  └─ Intermediate/
│  └─ RayTracing/
│     ├─ RayTracing.dsmproj
│     ├─ Source/RayTracing/
│     ├─ Content/
│     ├─ Shaders/
│     ├─ Binaries/
│     └─ Intermediate/
├─ Tests/
├─ Tools/
├─ Docs/
├─ xmake.lua
├─ CMakeLists.txt
└─ CMakePresets.json
```

## 资源路径

项目和场景文件使用逻辑虚拟路径，不保存本机绝对路径：

```text
/Game/Models/...
/Game/Scenes/...
/GameShaders/RestirDI/...
/Engine/Textures/DefaultSkybox/...
/EngineShaders/ForwardShader/...
```

默认挂载关系：

```text
/Game/          → Projects/<ProjectName>/Content       可写
/Engine/        → Engine/Content                        只读
/GameShaders/   → Projects/<ProjectName>/Shaders       可写
/EngineShaders/ → Engine/Shaders                        只读
```

项目描述文件使用 schema v2，例如：

```json
{
    "schemaVersion": 2,
    "name": "PBR",
    "engine": "DSMEngine",
    "startupTarget": "PBR",
    "startupScene": "/Game/Scenes/Sponza.dsmscene",
    "modules": ["PBR"],
    "plugins": []
}
```

相关实现位于：

- `Engine/Source/Runtime/Core/ContentPaths.h`
- `Engine/Source/Runtime/Core/ContentPaths.cpp`
- `Engine/Source/Runtime/Core/VirtualFileSystem/`

## 环境要求

- Windows 10/11
- Visual Studio/MSVC，支持 C++23
- Windows SDK，包含 D3D12、DXGI 和 DXC 运行库
- Git，且需要初始化仓库子模块
- Xmake 3.x，默认构建方式
- CMake 3.22 或更高版本，CMake 构建方式
- 支持 DirectX 12 的 GPU
- RayTracing 示例需要支持 DXR 的 GPU

初始化第三方子模块：

```powershell
git submodule update --init --recursive
```

## 使用 Xmake 构建

默认在仓库根目录执行：

```powershell
Set-Location D:\Code\DSMEngine
```

### 构建全部 Debug 目标

```powershell
xmake f -m debug
xmake
```

根目录 Xmake 会构建：

```text
EngineThirdParty
DSMEngine
DSMEditor
PBR
RayTracing
VirtualFileSystemTests
```

### 单独构建目标

```powershell
xmake build DSMEngine
xmake build DSMEditor
xmake build PBR
xmake build RayTracing
xmake build VirtualFileSystemTests
```

构建产物主要位于：

```text
build/engine/Debug/DSMEngine.lib
build/engine/Debug/DSMEditor.lib
Projects/PBR/Binaries/Debug/PBR.exe
Projects/RayTracing/Binaries/Debug/RayTracing.exe
build/verification/virtual-path-2026-09-28/VirtualFileSystemTests.exe
```

### Release 构建

```powershell
xmake f -m release
xmake
```

切回 Debug：

```powershell
xmake f -m debug
```

### 生成 Visual Studio 工程

当目标图或 Xmake 配置发生变化时：

```powershell
xmake project -k vsxmake2022
```

当前仓库的 Visual Studio 工程生成器以本机 Xmake/Visual Studio 安装为准。

## 使用 CMake 构建

CMake preset 位于 [CMakePresets.json](CMakePresets.json)。当前 preset 使用 Visual Studio generator；如果本机 Visual Studio 版本不同，需要调整 preset 中的 `generator`。

### 只构建 Engine Runtime

该 preset 构建 `DSMEngine` 静态库和验证目标，不构建 PBR/RayTracing 项目，也不构建 Editor：

```powershell
cmake --preset engine
cmake --build build\cmake\engine --config Debug --target DSMEngine
cmake --build build\cmake\engine --config Debug --target VirtualFileSystemTests
```

### 构建 PBR

```powershell
cmake --preset pbr
cmake --build build\cmake\pbr --config Debug --target PBR
```

### 构建 RayTracing

```powershell
cmake --preset raytracing
cmake --build build\cmake\raytracing --config Debug --target RayTracing
```

### CMake Release 构建

```powershell
cmake --build build\cmake\pbr --config Release --target PBR
cmake --build build\cmake\raytracing --config Release --target RayTracing
```

如果 `cmake` 不在 `PATH`，可以使用 Visual Studio 自带的 CMake，例如：

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --preset pbr
& $cmake --build build\cmake\pbr --config Debug --target PBR
```

## 项目检查、构建和运行

统一项目入口位于 `Tools/dsm.ps1`。

### 检查项目描述和资源根

```powershell
.\Tools\dsm.ps1 check Projects\PBR\PBR.dsmproj
.\Tools\dsm.ps1 check Projects\PBR\test0.dsmproj
.\Tools\dsm.ps1 check Projects\RayTracing\RayTracing.dsmproj
```

### 通过 Xmake 构建并运行 PBR

```powershell
.\Tools\dsm.ps1 build Projects\PBR\PBR.dsmproj -BuildSystem xmake -Configuration Debug
.\Tools\dsm.ps1 run Projects\PBR\PBR.dsmproj -BuildSystem xmake -Configuration Debug
```

也可以使用项目可执行文件的验证入口：

```powershell
.\Projects\PBR\Binaries\Debug\PBR.exe `
    --project .\Projects\PBR\PBR.dsmproj `
    --validate-project
```

该验证会检查项目场景、模型、网格、Engine 默认天空盒和虚拟路径读写权限。

### 通过 CMake 构建并运行 PBR

```powershell
.\Tools\dsm.ps1 build Projects\PBR\PBR.dsmproj -BuildSystem cmake -Configuration Debug
.\Tools\dsm.ps1 run Projects\PBR\PBR.dsmproj -BuildSystem cmake -Configuration Debug
```

### 运行 RayTracing

```powershell
.\Tools\dsm.ps1 build Projects\RayTracing\RayTracing.dsmproj -BuildSystem xmake -Configuration Debug
.\Tools\dsm.ps1 run Projects\RayTracing\RayTracing.dsmproj -BuildSystem xmake -Configuration Debug
```

RayTracing 自动验证入口：

```powershell
.\Projects\RayTracing\Binaries\Debug\RayTracing.exe `
    --project .\Projects\RayTracing\RayTracing.dsmproj `
    --validate-render `
    --output .\build\verification\restir-di\render

.\Projects\RayTracing\Binaries\Debug\RayTracing.exe `
    --project .\Projects\RayTracing\RayTracing.dsmproj `
    --validate-editor `
    --frames 24 `
    --output .\build\verification\restir-di\editor
```

`status.raw.json`、截图、metrics 和 Debug Layer 诊断文件会写入指定输出目录。

## 当前已知限制

- Engine 当前是静态库，没有独立的 Editor Host 可执行程序；不能直接运行 `xmake run DSMEngine`。
- RayTracing 的 `Untitled.dsmscene` 中包含程序化验证几何体，目前普通序列化路径不能完整恢复这些 Mesh；使用 `--validate-render` 和 `--validate-editor` 时会由验证入口重新生成内存场景。
- 当前 CMake preset 使用固定 Visual Studio generator，其他 Visual Studio 版本需要修改 `CMakePresets.json`。
- Release、设备丢失恢复和非 DXR GPU 验证不属于默认 Debug 构建流程。

## 验证入口

更完整的验证协议和失败排查路线见：

- `Docs/Verification.md`
- `Docs/Guides/VerificationWorkflows.md`
- `Docs/Reviews/`
- `Docs/ExecPlans/`
