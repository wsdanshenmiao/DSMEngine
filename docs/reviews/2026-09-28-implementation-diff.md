# DSMEngine 当前工作树 Before/After Diff 摘要

日期：2026-09-28
基线：`HEAD` 与当前工作树的差异；其中目录重组等修改来自本轮之前已存在的未提交工作树，不代表全部由本次虚拟路径任务引入。

## 1. Diff 范围

```text
git diff HEAD --find-renames --stat
```

当前统计：

- tracked changed：1007 个路径；
- untracked：393 个路径；
- tracked diff：3096 insertions，71131 deletions；
- 大量删除来自旧目录中的二进制资产和源码移动，不能把删除统计解释为业务代码被全部删除；
- 新增资产和新增 VFS/CMake/Test 文件属于 untracked，标准 `git diff` 不会显示它们，必须结合 `git status --short` 查看。

已尝试在 Codex 打开相对 `main` 的 Review 面板；面板和 `git diff HEAD` 都未必列出未跟踪文件，应同时查看 `git status`。命令行查看已跟踪文件的差异：

```powershell
git diff HEAD --find-renames --stat
git diff HEAD --find-renames --name-status
git diff HEAD --find-renames -- xmake.lua rules.lua .gitmodules
git diff HEAD --find-renames -- DSMEngine/Editor/Project.cpp Engine/Source/Editor/Project.cpp
git diff HEAD --find-renames -- DSMEngine/Editor/Serializer/Serializer.h Engine/Source/Editor/Serializer/Serializer.h
git status --short --untracked-files=all
```

## 2. 目录结构 Diff

```diff
- DSMEngine/Runtime/
- DSMEngine/Editor/
- DSMEngine/Shaders/
- ThirdParty/
- Samples/PBR/
- Samples/RayTracing/
- Projects/Assets/
+ Engine/Source/Runtime/
+ Engine/Source/Editor/
+ Engine/Shaders/
+ Engine/ThirdParty/
+ Engine/Content/
+ Projects/PBR/Source/PBR/
+ Projects/PBR/Content/
+ Projects/PBR/Shaders/
+ Projects/RayTracing/Source/RayTracing/
+ Projects/RayTracing/Content/
+ Projects/RayTracing/Shaders/
+ Tests/VirtualFileSystem/
+ CMakeLists.txt
+ CMakePresets.json
+ tools/dsm.ps1
```

```diff
- Projects/Shared/Content/
- Projects/Assets/Models/
- Projects/Assets/Textures/
+ Projects/PBR/Content/Models/
+ Projects/PBR/Content/Textures/
+ Engine/Content/Textures/DefaultSkybox/
```

## 3. 根 Xmake 入口 Diff

```diff
- includes("ThirdParty")
- includes("DSMEngine")
- includes("Samples/PBR")
- includes("Samples/RayTracing")
+ includes("Engine/ThirdParty")
+ includes("Engine")
+ includes("Projects/PBR")
+ includes("Projects/RayTracing")
+ includes("Tests")
```

旧版内容可用 `git show HEAD:xmake.lua` 查看；当前文件：[xmake.lua](file:///D:/Code/DSMEngine/xmake.lua)。

## 4. 构建后复制规则 Diff

```diff
 rule("Imguiini")
     ...
 rule_end()

- rule("ShaderCopy")
-     ...
- rule_end()
-
- rule("EngineShaderCopy")
-     ...
- rule_end()
-
- rule("AssetsCopy")
-     after_build(function(target)
-         local assetsFiles = path.join(os.projectdir(), "Samples", "Assets")
-         os.cp(assetsFiles, target:targetdir(), {copy_if_different = true})
-     end)
- rule_end()
+ rule("DXCRuntimeCopy")
+     ...
+ rule_end()
```

影响：普通构建不再把项目 Content/Shader 复制到二进制目录；运行时从项目锚点和 Engine descriptor 建立挂载。

文件：[rules.lua](file:///D:/Code/DSMEngine/rules.lua)。

## 5. 项目描述 Diff

```diff
- {
-     "filePath": "D:\\Code\\DSMEngine\\Projects\\Sponza.dsmproj",
-     "name": "",
-     "sceneFilePath": "D:\\Code\\DSMEngine\\Projects\\Assets\\Scene\\Sponza.dsmscene"
- }
+ {
+     "schemaVersion": 2,
+     "name": "PBR",
+     "engine": "DSMEngine",
+     "startupTarget": "PBR",
+     "startupScene": "/Game/Scenes/Sponza.dsmscene",
+     "modules": ["PBR"],
+     "targets": ["PBRGame", "PBREditor", "PBRValidation"],
+     "plugins": []
+ }
```

当前文件：[Projects/PBR/PBR.dsmproj](file:///D:/Code/DSMEngine/Projects/PBR/PBR.dsmproj)。

## 6. 场景资产引用 Diff

```diff
- "modelFilePath": "D:\\Code\\DSMEngine\\Projects\\Assets\\Models\\Sponza\\pbr\\sponza2.gltf"
+ "assetPath": "/Game/Models/Sponza/pbr/sponza2.gltf"
```

```diff
- "sceneFilePath": "D:\\Code\\DSMEngine\\Projects\\Assets\\Scene\\Sponza.dsmscene"
+ "scenePath": "/Game/Scenes/Sponza.dsmscene"
```

当前场景：[Sponza.dsmscene](file:///D:/Code/DSMEngine/Projects/PBR/Content/Scenes/Sponza.dsmscene)。

## 7. ContentPaths / VFS Diff

本轮之前的工作树中间态（**不是 `HEAD`**）使用固定目录搜索：

```text
Project/Content → Projects/Shared/Content → Engine/Content
```

`HEAD` 版本则主要把磁盘绝对路径直接写入项目和场景文件；没有 VFS。

当前行为：

```diff
+ /Game/             -> <ProjectRoot>/Content
+ /Engine/           -> <EngineRoot>/Content       [read-only]
+ /GameShaders/      -> <ProjectRoot>/Shaders
+ /EngineShaders/    -> <EngineRoot>/Shaders       [read-only]
```

新增关键接口：

```cpp
std::expected<std::filesystem::path, PathError>
ResolveForRead(const VirtualPath& path) const;

std::expected<std::filesystem::path, PathError>
ResolveForWrite(const VirtualPath& path) const;

std::expected<VirtualPath, PathError>
ToVirtualPath(const std::filesystem::path& physicalPath) const;
```

文件：

- [ContentPaths.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/ContentPaths.cpp)
- [VirtualPath.h](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualPath.h)
- [VirtualFileSystem.cpp](file:///D:/Code/DSMEngine/Engine/Source/Runtime/Core/VirtualFileSystem/VirtualFileSystem.cpp)

## 8. 场景写入安全 Diff

```diff
- Serializer::SerializeToFile(filepath, *scene);
+ auto physicalPath = ContentPaths::ResolveVirtualForWrite(virtualPath);
+ if (!physicalPath) {
+     return;
+ }
+ Serializer::SerializeToFile(physicalPath->string(), *scene);
```

现在 `/Engine/...` 写入会失败，场景只能保存到 `/Game/...`。

文件：[Project.cpp](file:///D:/Code/DSMEngine/Engine/Source/Editor/Project.cpp)。

## 9. Serializer 写入 Diff

```diff
- std::ofstream file{path};
- file << json.dump(4);
+ std::ofstream file{path.string() + ".tmp", std::ios::binary | std::ios::trunc};
+ file << json.dump(4) << '\n';
+ file.flush();
+ MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
```

并且反序列化失败会捕获 JSON/文件异常并返回 `false`。

文件：[Serializer.h](file:///D:/Code/DSMEngine/Engine/Source/Editor/Serializer/Serializer.h)。

## 10. 第三方依赖 Diff

```diff
- ThirdParty/assimp
- ThirdParty/glfw
- ThirdParty/dxc
- ThirdParty/imgui
+ Engine/ThirdParty/assimp
+ Engine/ThirdParty/glfw
+ Engine/ThirdParty/dxc
+ Engine/ThirdParty/imgui
```

Xmake：`HEAD` 已通过本地包构建仓库中的 Assimp；此次修改的是路径和包解析限定，并非首次引入本地构建：

```diff
- set_sourcedir(path.join(os.projectdir(), "ThirdParty", "assimp"))
- add_requires("assimp")
+ set_sourcedir(path.join(os.scriptdir(), "assimp"))
+ add_requires("assimp", {system = false})
```

CMake：`HEAD` 中不存在 CMake 工程，因此这里是新增内容，不存在“删除 `find_package`”这一步：

```diff
+ add_subdirectory(assimp EXCLUDE_FROM_ALL)
+ add_subdirectory(glfw EXCLUDE_FROM_ALL)
```

文件：

- [Engine/ThirdParty/xmake.lua](file:///D:/Code/DSMEngine/Engine/ThirdParty/xmake.lua)
- [Engine/ThirdParty/CMakeLists.txt](file:///D:/Code/DSMEngine/Engine/ThirdParty/CMakeLists.txt)

## 11. 测试 Diff

新增：

```text
Tests/VirtualFileSystem/VirtualFileSystemTests.cpp
Tests/CMakeLists.txt
Tests/xmake.lua
```

覆盖非法虚拟路径、Provider 优先级、读写权限、物理反向映射、目录枚举、`..` 越界和符号链接越界。

文件：[VirtualFileSystemTests.cpp](file:///D:/Code/DSMEngine/Tests/VirtualFileSystem/VirtualFileSystemTests.cpp)。

## 12. 重要说明

完整差异包含大量二进制资产的移动/删除/新增，因此不适合全部内嵌在聊天中。已跟踪文件的差异可通过 Codex Review 面板和以下命令查看；未跟踪文件需要另外查看：

```powershell
git diff HEAD --find-renames --stat
git diff HEAD --find-renames --name-status
git status --short --untracked-files=all
```

`git diff HEAD` 不包含 untracked 文件；本次新增的 VFS、CMake、Tests、Engine descriptor 和迁移后的项目资产必须结合 `git status --short --untracked-files=all` 查看。
