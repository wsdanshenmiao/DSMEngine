# 持久化程序化 MeshRenderer 场景数据

**状态**: completed  
**创建日期**: 2026-09-29  
**作者**: Codex

## 摘要 (Summary)

修复 `Projects/RayTracing/Content/Scenes/Untitled.dsmscene` 中由 ReSTIR DI 验证场景生成器创建的程序化 `MeshRenderer` 被保存为 JSON `null`、重新加载后丢失可渲染网格的问题。目标是明确区分模型文件引用和内嵌程序化网格数据，使现有启动场景加载后至少恢复其几何、材质绑定所需的运行时状态，并保持模型资产场景的现有格式。

## 背景 (Context)

`Engine/Source/Editor/Serializer/Serializer.h` 的 `MeshRenderer` 序列化器仅在 `GetModel()` 非空时写入 `assetPath` 与 `meshIndex`。`Projects/RayTracing/Source/RayTracing/RestirDI/RestirDIValidationScene.cpp` 通过 `SetMesh()` 创建内存网格而不绑定 `Model`，因此场景保存时组件值保持 `null`；加载时组件虽被创建，但没有网格，`SceneAdapter` 会过滤掉这些对象。当前 RayTracing 启动场景包含 10 个这种对象。

范围内：序列化/反序列化程序化网格所需的 CPU 几何和 MeshRenderer 的最小渲染属性、更新仓库内 RayTracing 启动场景、构建和运行验证。范围外：第三方代码、通用材质资产系统、旧场景格式的大规模迁移。

## 实施计划 (Implementation Plan)

1. **建立数据契约**：确认模型引用路径与程序化网格的互斥表示；内嵌网格至少包含名称、索引格式、顶点属性、索引数据和子网格描述，并拒绝缺字段/越界数据。
2. **实现序列化**：在 `Serializer.h` 中为程序化网格写入稳定的 `mesh` 对象；保留模型引用分支；反序列化后上传 GPU buffer，并恢复 MeshRenderer 的渲染开关、材质索引和必要包围盒。
3. **修复仓库场景**：用修复后的序列化逻辑重新生成或定向更新 `Projects/RayTracing/Content/Scenes/Untitled.dsmscene`，不得覆盖无关用户资源。
4. **验证**：运行 `xmake build RayTracing`；运行 RayTracing 的固定渲染/编辑器验证；用 JSON 检查确认 10 个 MeshRenderer 不再为 `null`，并记录产物和剩余限制。

## 验证 (Validation)

- 工作目录：`D:\Code\DSMEngine`。
- 静态检查：解析 `Projects/RayTracing/Content/Scenes/Untitled.dsmscene`，确认每个 `class DSM::MeshRenderer` 为有效对象且包含网格数据。
- 构建：`xmake build RayTracing`。
- 运行：`xmake run RayTracing --validate-render --output <任务专属目录>`；必要时 `xmake run RayTracing --validate-editor --output <任务专属目录> --frames 22`。
- 失败时优先读取 `status.raw.json`、主日志和截图；不删除现有验证产物。

实际结果：

- `cmake --preset raytracing`：通过，生成 `build/cmake/raytracing`。
- `cmake --build build/cmake/pbr --config Debug --target DSMEditor -- /m:1`：通过，最终生成 `build/cmake/pbr/engine/Debug/DSMEditor.lib`。
- JSON 静态检查：通过；场景包含 140 个对象、10 个 `MeshRenderer`，`NullMesh=0`、`EmbeddedMesh=10`，`scenePath=/Game/Scenes/Untitled.dsmscene`。
- `xmake build RayTracing`：未通过，失败发生在现有 xmake Assimp include 配置（`Model.cpp` 找不到 `assimp/Importer.hpp`），不是本次序列化代码诊断。
- `xmake run RayTracing --validate-render --output .tmp/verification/20260929-restir-render`：通过；`status.raw.json` 为 `exit_code=0`、`passed=true`、D3D12/DXGI debug layer 通过。
- `xmake run RayTracing --validate-editor --output .tmp/verification/20260929-restir-editor --frames 22`：通过；22 帧渲染和 UI 均完成、相机移动检查通过、无 debug layer 警告。
- 用当前启动场景启动 `Projects/RayTracing/Binaries/Debug/RayTracing.exe --project Projects/RayTracing/RayTracing.dsmproj`：进程保持运行并未在 8 秒观察窗口内崩溃；未执行交互式截图验收。
- CMake RayTracing 最终重链：编译阶段完成，但链接器长时间占用约 500 MB 内存且无输出，未等待到退出；已有 Debug 可执行文件可启动，故该项记为环境/链接耗时限制，不掩盖静态和运行验证结果。

## 进展 (Progress)

- [x] 读取 `AGENTS.md`、`PLANS.md` 和 `Docs/Verification.md`。
- [x] 定位根因：程序化网格只调用 `SetMesh()`，MeshRenderer 序列化器只支持 Model 引用。
- [x] 完成程序化网格序列化契约和实现。
- [x] 更新 RayTracing 启动场景。
- [x] 完成可执行的构建与运行验证，并记录 xmake/CMake 最终链接限制。

## 意外与发现 (Surprises & Discoveries)

- `Untitled.dsmscene` 的对象名称和灯光数量与 `CreateValidationScene()` 一致，说明它是固定验证场景被保存后的产物，而不是 Assimp 模型场景。
- `Model::LoadModelFromGeometry` 在头文件中有声明但当前实现中没有定义；本次不依赖该未完成 API，避免扩大任务范围。
- 当前 `MeshRenderer` 序列化器对模型场景也没有保存完整 Material 对象；本次先保证程序化几何恢复和渲染器有效，材质资产系统另列后续债务。

## 决策记录 (Decision Log)

| 决策 | 选择 | 原因 |
|------|------|------|
| 程序化网格存储 | 场景内嵌 CPU 网格数据 | 没有现成的网格资产格式，且目标场景本身没有可引用的文件资产 |
| 模型场景格式 | 保留 `assetPath`/`meshIndex` 分支 | 避免改变现有模型引用契约 |
| 场景文件更新 | 只更新 RayTracing 启动场景 | 该文件是已确认的回归输入，其他用户场景不应被批量重写 |

## 结果与复盘 (Outcomes & Retrospective)

已完成：`MeshRenderer` 现在支持两种互斥持久化形式：模型引用（`assetPath` + `meshIndex`）和内嵌程序化网格（顶点属性、子网格索引、包围盒、材质基础参数）。RayTracing 启动场景已重新生成，10 个程序化网格均可由 JSON 恢复，不再是 `null`。

剩余限制：内嵌材质当前只保存基础颜色、金属度、粗糙度、自发光和透明/双面标志，不保存 GPU 纹理像素；因此 `Alpha Checker` 的程序化纹理在重新加载后会回退到纹理缺省值。若需要完全复现编辑器验证场景，应后续增加纹理资产/内嵌纹理契约。`xmake build RayTracing` 的 Assimp 头文件路径问题和 CMake RayTracing 重链耗时也应作为构建环境债务单独处理。
