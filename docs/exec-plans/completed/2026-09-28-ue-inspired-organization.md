# DSMEngine：简化版 UE 风格工程组织方案

## 状态

- **状态**：completed（设计稿，实施证据已由 `2026-09-28-virtual-path-and-asset-ownership.md` 接续）
- **日期**：2026-09-28
- **范围**：工程目录、项目锚点、构建目标、Runtime/Editor 边界、Content/Shader 资产边界、第三方依赖边界、验证入口。
- **本阶段不做**：不修改第三方源码；不引入 UE 的反射/UHT、动态模块加载、蓝图系统或完整插件生态；不以一次性大搬迁作为首个交付。

---

## 摘要（Summary）

目标不是复制 Unreal Engine 的目录数量，而是提取 UE 中最有价值的四个概念：

1. **Engine 与 Project 分离**：引擎代码和项目代码不再混在 `DSMEngine/`、`Samples/`、`Projects/` 三个并列目录中。
2. **项目描述文件是唯一锚点**：以 `<ProjectName>.dsmproj` 所在目录作为项目根目录，项目、场景、Content、Shader 和生成产物都从这个根目录解析；不再把绝对路径写入项目文件，也不再依赖构建后复制一份资源来“模拟项目运行目录”。
3. **按生命周期而不是按文件数量拆模块**：第一阶段只保留 `DSMEngine`、`DSMEditor`、`DSMThirdParty` 三个主要构建边界，不把每个 Pass 或每个小工具都拆成独立库。
4. **Engine Content、Project Content、Generated Data 分层**：源资产、派生缓存、用户保存数据和可执行产物分别有稳定位置。

最终建议的简化结构如下：

```text
D:\Code\DSMEngine\
├─ Engine/                         # 可复用引擎
│  ├─ Source/
│  │  ├─ Runtime/
│  │  │  ├─ Core/
│  │  │  ├─ Platform/
│  │  │  ├─ Event/
│  │  │  ├─ Math/
│  │  │  ├─ Framework/
│  │  │  ├─ Graphics/
│  │  │  └─ Render/
│  │  └─ Editor/
│  ├─ Shaders/
│  ├─ Content/                    # 仅放引擎/编辑器共享内容
│  └─ Config/
├─ Projects/                      # 每个可启动项目一个目录
│  ├─ PBR/
│  │  ├─ PBR.dsmproj
│  │  ├─ Source/PBR/
│  │  ├─ Content/
│  │  ├─ Shaders/
│  │  ├─ Config/
│  │  ├─ Binaries/                # 忽略的生成目录
│  │  ├─ Intermediate/             # 忽略的生成目录
│  │  └─ Saved/                    # 日志、自动保存、验证输出
│  ├─ RayTracing/
│  │  ├─ RayTracing.dsmproj
│  │  ├─ Source/RayTracing/
│  │  ├─ Content/
│  │  ├─ Shaders/
│  │  ├─ Config/
│  │  └─ Validation/
│  └─ Shared/Content/              # 多个示例共享且不属于引擎的内容
├─ ThirdParty/                     # 供应商源码和本地依赖构建定义
├─ Tools/                          # 项目发现、构建、验证、资产检查工具
├─ Tests/                          # 跨项目的自动化验证
├─ docs/
└─ xmake.lua
```

### 一页结论

- **现在最值得先改的不是目录名称，而是锚点和边界**：`.dsmproj` 应先变成可搬迁的相对路径描述；构建和运行时应能以它的目录为工作目录；Runtime 不应再包含 Editor 实现。
- **建议先做“逻辑分层 + 小范围目标拆分”，再做物理搬迁**。这样每个阶段都能单独构建和回归，不会因为一次移动几百个 include 而无法判断问题来源。
- **不建议立刻把所有 Runtime 子目录拆成十几个库**。对于当前规模，`DSMEngine`、`DSMEditor`、`DSMThirdParty` 三个边界足够清晰；只有在编译时间或依赖闭环确实成为瓶颈时，再把 `Core`、`Graphics`、`Render` 拆成独立目标。

---

## 背景（Context）

### 当前工作树的关键事实

1. `D:\Code\DSMEngine\DSMEngine\xmake.lua` 使用 `add_files("**.cpp")`，因此 `D:\Code\DSMEngine\DSMEngine\Runtime` 和 `D:\Code\DSMEngine\DSMEngine\Editor` 的实现目前都进入同一个 `DSMEngine` 静态库。
2. `D:\Code\DSMEngine\ThirdParty\xmake.lua` 将 GLFW、ImGui、DDSTextureLoader 等实现放入一个 `ThirdParty` 目标，并把多个第三方 include 目录以 public 方式向上游传播。
3. `D:\Code\DSMEngine\Samples\PBR` 和 `D:\Code\DSMEngine\Samples\RayTracing` 是可执行目标，但没有自己的项目根、配置目录、Content 根和 Saved 根。
4. `D:\Code\DSMEngine\Projects\Sponza.dsmproj` 和 `D:\Code\DSMEngine\Projects\test0.dsmproj` 目前记录 `D:\Code\DSMEngine\...` 形式的绝对路径；换机器、换工作区或复制项目后，描述文件不具备可搬迁性。
5. `D:\Code\DSMEngine\Projects\Assets` 与 `D:\Code\DSMEngine\Samples\Assets` 分别约有 348 和 407 个文件，合计约 1.79 GB；当前 `AssetsCopy` 还会在构建后把 `Samples\Assets` 投递到二进制目录。
6. `D:\Code\DSMEngine\DSMEngine\Editor\Project.cpp` 既负责项目/场景文件读写，又直接替换全局 Scene；`D:\Code\DSMEngine\DSMEngine\Editor\Serializer\Serializer.h` 同时依赖 Runtime 类型和 Editor 的 Project 类型，项目描述、场景序列化和编辑器工作流尚未分层。

### 从 UE 借鉴什么

UE 的目录模型把引擎和项目放在同一工作区的不同根目录中；Engine 和项目都可以拥有 `Source`、`Content`、`Config`、`Binaries`、`Intermediate`、`Saved` 等边界。引擎侧再按 `Runtime`、`Editor`、`Developer`、`Programs` 分区，项目侧按模块组织源代码。

UE 的构建入口以项目描述、Target 和 Module 描述为事实来源，而不是把 IDE 解决方案当成构建真相；模块通过 `Build.cs` 声明依赖，目标通过 `Target.cs` 声明构建形态。

UE 的 Content Browser 之所以稳定，是因为项目资产有明确的 Content 根和可发现的目录层级；引擎 Content 与项目 Content 可以分别存在，项目只访问自身以及允许共享的引擎内容。

### 明确不照搬什么

- 不引入 `Developer/`、`Programs/`、`Templates/` 等目录，除非 DSMEngine 真的出现对应生命周期。
- 不为每个 Render Pass 建一个独立动态模块；Pass 仍归属 `Render` 或具体 Renderer 目录。
- 不把 `dsmproj` 做成需要代码生成器才能打开的复杂二进制描述；采用 JSON 文本即可。
- 不建立完整 UE 插件扫描和依赖分层系统。只有可独立启用、拥有独立资源和验证闭环的功能，才考虑放进 `Plugins/`。
- 不保留旧路径的长期 fallback。迁移完成后，include、资源路径和构建入口只保留新契约。

**对应链接：**

- [UE 官方：虚幻引擎目录结构](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/unreal-engine-directory-structure)
- [UE 官方：虚幻引擎模块](https://dev.epicgames.com/documentation/zh-cn/unreal-engine/unreal-engine-modules)
- [UE 官方：Unreal Build Tool Target Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-build-tool-target-reference)
- [UE 官方：Content Browser](https://dev.epicgames.com/documentation/en-us/unreal-engine/content-browser-in-unreal-engine)
- [UE 官方：Plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
- [本机 UE 项目描述源码](file:///G:/Works/QSClient/Engine/Source/Runtime/Projects/Public/ProjectDescriptor.h)
- [本机 UE 项目管理源码](file:///G:/Works/QSClient/Engine/Source/Runtime/Projects/Private/ProjectManager.cpp)
- [本机 UBT 项目描述源码](file:///G:/Works/QSClient/Engine/Source/Programs/UnrealBuildTool/Configuration/Descriptors/ProjectDescriptor.cs)
- [本机 UBT 模块规则源码](file:///G:/Works/QSClient/Engine/Source/Programs/UnrealBuildTool/Configuration/Rules/ModuleRules.cs)
- [本机 QQSpeed 项目描述](file:///G:/Works/QSClient/QQSpeed/QQSpeed.uproject)
- [本机 QQSpeed 模块构建规则](file:///G:/Works/QSClient/QQSpeed/Source/QQSpeed/QQSpeed.Build.cs)

---

## 目标结构与边界契约

### 1. Engine 边界

`Engine/` 是可复用代码和共享运行时内容的唯一根。它不能依赖任何 `Projects/<Name>/` 内容。

```text
Engine/Source/Runtime/     # 运行时可执行程序需要的代码
Engine/Source/Editor/      # 仅编辑器需要的代码
Engine/Shaders/            # 引擎通用 shader 和 include
Engine/Content/            # 编辑器字体、图标、默认材质等共享内容
Engine/Config/             # 引擎默认配置
```

**契约：**

- Runtime 可以依赖 ThirdParty 和 Engine 内部模块，不能 include `Editor/` 或项目源代码。
- Editor 可以依赖 Runtime、ImGui 和项目描述/序列化服务，但 Runtime 不依赖 Editor。
- Engine 的 shader 和 content 不得通过每次构建复制到随机工作目录才能访问；运行时由 `ProjectContext` 提供有序搜索根。

### 2. Project 边界

每个项目目录包含一个与目录同名的 `.dsmproj`。项目根通过描述文件确定，不通过当前进程工作目录猜测。

推荐的 `PBR.dsmproj` v1：

```json
{
  "schemaVersion": 1,
  "name": "PBR",
  "startupScene": "Content/Scenes/Sponza.dsmscene",
  "sourceModules": ["PBR"],
  "contentRoots": ["Content", "../Shared/Content"],
  "shaderRoots": ["Shaders"],
  "startupTarget": "PBR"
}
```

约束：

- 所有路径均相对于 `.dsmproj` 所在目录，统一使用 `/` 写入 JSON。
- 读取时把路径解析到项目根；保存时再转回相对路径。
- 禁止持久化 `D:\...`、`E:\...` 等机器绝对路径。
- 允许的路径必须经过根目录校验，禁止 `..` 逃出项目根、共享 Content 根或 Engine 根。
- `sceneFilePath` 不再是全局场景对象随手保存的绝对路径，而是项目描述或场景引用中的规范化相对路径。
- `.dsmproj` 只描述项目结构和入口，不把运行期缓存、窗口布局、日志、编译中间文件写进去。

### 3. Runtime / Editor 边界

第一阶段只建立三类构建目标：

| 目标 | 包含 | 不包含 | 依赖 |
|---|---|---|---|
| `DSMThirdParty` | GLFW、ImGui、DDSTextureLoader 等仓库内接入代码；外部包的构建定义 | Runtime/Editor 业务代码 | 本地子模块、系统库 |
| `DSMEngine` | Core、Platform、Event、Math、Framework、Graphics、Render、引擎主循环 | 编辑器 UI、项目菜单、Content Browser、ImGui 编辑器逻辑 | `DSMThirdParty`、Assimp、D3D12/DXGI |
| `DSMEditor` | `DSMEditor`、Project 工作流、Serializer、EditorUI、编辑器专用启动代码 | Runtime 的基础设备和渲染实现 | `DSMEngine`、ImGui |
| 项目可执行目标 | `Projects/<Name>/Source/<Name>` | 引擎内部实现 | `DSMEngine`；编辑器目标按需依赖 `DSMEditor` |

这比 UE 的模块数量少，但保留了最重要的生命周期边界：Runtime 可以单独编译、测试和被非编辑器程序使用；Editor 是可选层。

### 4. 逻辑模块边界

初始阶段仍保留当前 Runtime 的八个目录，不立即拆成八个库：

```text
Runtime/Core       # 生命周期基础、日志、时间、输入接口
Runtime/Platform   # Windows 文件对话框、路径、平台实现
Runtime/Event      # 事件类型和分发
Runtime/Math       # 数学、几何、碰撞
Runtime/Framework  # Scene、Object、Component、脚本生命周期
Runtime/Graphics   # RHI 风格接口和 D3D12 后端
Runtime/Render     # 资源导入、渲染管线、Pass、Renderer
Runtime/Utils      # 与引擎生命周期无关的通用工具
```

必须先修正依赖方向，再考虑更多目标：

```text
Utils / Math
      ↓
Core / Event / Platform
      ↓
Graphics
      ↓
Framework ───────┐
      ↓          │
RenderCore       │
      ↓          │
Renderer / Pass ←┘
      ↓
DSMEngine 主循环

DSMEditor → DSMEngine
项目目标   → DSMEngine (+ DSMEditor)
```

其中 `Framework` 不应直接依赖具体 `GraphicsRenderer`；渲染器消费由场景导出的只读 `RenderWorld` 或 `SceneView`。这项解耦属于后续重构，不应在第一次目录迁移时同时完成。

### 5. Content、Shader、Generated Data 边界

| 类型 | 归属 | 例子 | 运行时处理 |
|---|---|---|---|
| 引擎共享 Content | `Engine/Content` | 编辑器字体、默认图标 | `EngineContentRoot` |
| 项目源资产 | `Projects/<Name>/Content` | FBX、DDS、场景、材质描述 | `ProjectContentRoot` |
| 多项目共享示例资产 | `Projects/Shared/Content` | Sponza 等公共样例 | `SharedContentRoot` |
| 引擎 shader | `Engine/Shaders` | Common、Forward/Deferred 通用 include | `EngineShaderRoot` |
| 项目 shader | `Projects/<Name>/Shaders` | ReSTIR DI 项目 shader | `ProjectShaderRoot` |
| 派生缓存 | `Projects/<Name>/Intermediate` | 导入缓存、shader 编译缓存 | 可删除、可重建 |
| 用户数据 | `Projects/<Name>/Saved` | imgui.ini、日志、自动保存、验证结果 | 不参与源码构建 |
| 可执行/运行时 DLL | `Projects/<Name>/Binaries/Win64` | exe、DXC DLL | 构建产物 |

核心原则：**源资产不复制到 `bin` 才能运行**。程序启动时从 `.dsmproj` 推导项目根，并把上述根目录传给资源系统。发布打包时可以进行 staging，但 staging 是发布步骤，不是普通增量构建的资源真相。

---

## `.dsmproj` 项目锚点方案

### 外部接口

增加一个轻量 `ProjectDescriptor` / `ProjectContext`，不要求引入复杂反射系统：

```cpp
struct ProjectDescriptor {
    uint32_t schemaVersion;
    std::string name;
    std::filesystem::path descriptorPath;
    std::filesystem::path projectRoot;
    std::filesystem::path startupScene;
    std::vector<std::filesystem::path> contentRoots;
    std::vector<std::filesystem::path> shaderRoots;
    std::string startupTarget;
};

class ProjectContext {
public:
    static std::expected<ProjectDescriptor, ProjectError>
    Load(const std::filesystem::path& descriptorPath);

    std::filesystem::path ResolveContent(std::filesystem::path relative) const;
    std::filesystem::path ResolveShader(std::filesystem::path relative) const;
};
```

这只是目标接口，不要求第一阶段马上按该精确类型实现；但行为必须先固定：

- `Load` 的输入是 `.dsmproj` 文件路径；
- `projectRoot == descriptorPath.parent_path()`；
- 相对路径按项目根解析；
- 越界路径和不存在的必需入口返回带路径信息的错误；
- 项目切换完成后，旧 ProjectContext、Scene 和资源句柄不能继续被新项目使用。

### 构建和运行入口

推荐提供一个薄封装工具，而不是要求每个人手写 xmake 参数：

```text
Tools/dsm.ps1 build Projects/PBR/PBR.dsmproj
Tools/dsm.ps1 run   Projects/PBR/PBR.dsmproj
Tools/dsm.ps1 check Projects/PBR/PBR.dsmproj
```

该工具只负责：

1. 规范化并校验 `.dsmproj` 路径；
2. 将项目路径传给 xmake 的 project option；
3. 选择目标和配置；
4. 把当前工作目录设为项目根或显式传递项目根；
5. 把验证摘要写入 `Projects/<Name>/Saved/Verification/`。

不建议让用户直接把当前目录切到 `bin/<target>` 再启动。`xmake run PBR` 可以作为底层兼容入口，但文档和 CI 应以 `.dsmproj` 命令为主。

### 项目切换

`Project::LoadProject` 的目标行为：

1. 先校验新描述文件和启动场景，校验失败不改变当前项目；
2. 在确认新项目可加载后，再保存或放弃当前编辑状态；
3. 建立新的 `ProjectContext` 和资源搜索路径；
4. 在新 Scene 加载成功后，原子替换当前 Scene；
5. 清理旧项目的资源缓存和编辑器选择状态。

这会直接修正当前 `D:\Code\DSMEngine\DSMEngine\Editor\Project.cpp` 中“先保存/替换全局对象，再验证完整输入”的脆弱边界。

---

## 文件迁移映射

迁移时优先使用版本控制的移动操作，避免复制两套源码长期并存。

| 当前路径 | 目标路径 | 说明 |
|---|---|---|
| `D:\Code\DSMEngine\DSMEngine\Runtime` | `D:\Code\DSMEngine\Engine\Source\Runtime` | 引擎 Runtime 根 |
| `D:\Code\DSMEngine\DSMEngine\Editor` | `D:\Code\DSMEngine\Engine\Source\Editor` | 编辑器根；从 Runtime target 移出 |
| `D:\Code\DSMEngine\DSMEngine\Shaders` | `D:\Code\DSMEngine\Engine\Shaders` | 引擎 shader 根 |
| `D:\Code\DSMEngine\Samples\PBR\PBR.cpp` | `D:\Code\DSMEngine\Projects\PBR\Source\PBR\Private\PBRMain.cpp` | 项目入口 |
| `D:\Code\DSMEngine\Samples\RayTracing\*` | `D:\Code\DSMEngine\Projects\RayTracing\Source\RayTracing\Private\*` | 项目代码和验证入口分开 |
| `D:\Code\DSMEngine\Samples\RayTracing\RestirDI\Shaders` | `D:\Code\DSMEngine\Projects\RayTracing\Shaders\RestirDI` | 项目 shader |
| `D:\Code\DSMEngine\Projects\Assets` | `D:\Code\DSMEngine\Projects\Shared\Content` 或具体项目 `Content` | 先做清单/哈希确认，再迁移 |
| `D:\Code\DSMEngine\Samples\Assets` | `D:\Code\DSMEngine\Projects\PBR\Content` 或共享 Content | 消除重复源；不得盲删 |
| `D:\Code\DSMEngine\Projects\Sponza.dsmproj` | `D:\Code\DSMEngine\Projects\Sponza\Sponza.dsmproj` | 描述文件与项目根同目录 |
| `D:\Code\DSMEngine\Projects\test0.dsmproj` | `D:\Code\DSMEngine\Projects\test0\test0.dsmproj` | 同上 |
| `D:\Code\DSMEngine\DSMEngine\Editor\Serializer` | 第一阶段保留在 `Engine/Source/Editor/Serialization`；稳定后移至 `Engine/Source/Runtime/Serialization` | 先拆依赖，再决定最终归属 |

迁移中不做“旧目录指向新目录”的永久兼容层。若需要分批提交，可以在一个短生命周期分支中使用临时重定向，但合并前必须删掉。

---

## 构建目标和 Xmake 方案

### 第一阶段的 target 图

```text
DSMThirdParty
      ↓
DSMEngine  (Runtime only)
      ├── PBR
      └── RayTracing

DSMEditor  → DSMEngine
      ├── PBR-Editor style executable（如果保留编辑器启动）
      └── RayTracing-Editor style executable（如果需要）
```

PBR 当前直接使用 `DSMEditor`，因此它的目标依赖应明确写成 `DSMEngine + DSMEditor`；RayTracing 的纯渲染验证目标应尽量只依赖 `DSMEngine`，编辑器模式再额外依赖 `DSMEditor`。这样可以验证 Editor 不是 Runtime 的隐式依赖。

### Xmake 组织原则

- 根 `D:\Code\DSMEngine\xmake.lua` 只负责全局选项、公共系统库和 include 子目录。
- `Engine/xmake.lua` 负责 `DSMEngine` 与 `DSMEditor` 目标。
- `Projects/<Name>/xmake.lua` 负责项目目标、源代码、shader 根和项目特有验证。
- `ThirdParty/xmake.lua` 负责供应商构建定义；Assimp 继续使用仓库子模块源码包，不让项目目标直接知道其 CMake 细节。
- 复制规则改为“部署/打包规则”，普通构建只更新必要的生成文件，不把整个 Content 目录作为每次构建的副作用。
- 项目文件的存在应由项目描述或明确的 `include` 发现，不用根目录的 `add_files("**.cpp")` 把 Engine、Editor、Project 混成一个库。

### 何时才拆成更多库

满足以下任一条件再拆 `DSMCore`、`DSMGraphics`、`DSMRender`：

- 改动一个 UI 文件却触发全部 D3D12/Renderer 重编译；
- 需要无窗口、无 D3D12 的 Scene/Math 单元测试；
- 多个项目只需要 Core/Math，却被迫链接完整 Renderer；
- 模块之间可以形成无环依赖并且公共头文件边界已经稳定。

在这些条件满足前，多个静态库只会增加 xmake 配置、链接顺序和接口维护成本，不会自动带来更清晰的架构。

---

## 分阶段实施计划（Implementation Plan）

### M0：冻结契约和迁移清单

**目标**：先把新结构写成可验证的规则，不移动源码。

**工作项：**

- 定义 `.dsmproj` schema v1 和相对路径规则；
- 列出 `Projects/Assets` 与 `Samples/Assets` 的文件清单、SHA-256 和引用者；
- 生成当前 include、target、shader、Content 的依赖地图；
- 明确 PBR、RayTracing 的运行模式：纯 Runtime、Editor、Validation；
- 将本方案标为实施 ExecPlan，后续每个里程碑记录实际命令和差异。

**完成判据：**

- 可以在不读聊天记录的情况下说明每个资产根、shader 根和 target 的唯一来源；
- `.dsmproj` 中没有必须保存的绝对路径；
- 所有待迁移文件都有唯一目标路径。

### M1：引入项目锚点和路径解析

**目标**：让项目描述文件真正成为运行时锚点，但暂不物理搬迁所有目录。

**工作项：**

- 新增项目描述加载和路径解析服务；
- 将现有 `D:\Code\DSMEngine\Projects\*.dsmproj` 改为相对路径格式；
- 让 Project/Scene 加载先验证新项目，再替换当前状态；
- 增加 `Tools/dsm.ps1 check`，检查路径越界、缺失入口和 schema 版本；
- 运行时从 ProjectContext 获取 Content/Shader 根，不再依赖进程当前目录或复制后的 bin 目录。

**完成判据：**

- 将仓库复制到另一个目录后，PBR 和 Sponza 项目仍能加载同一个场景；
- 从仓库根目录、项目目录和 `Binaries/Win64` 启动，资源搜索结果一致；
- 故意破坏 startupScene 时，当前已打开项目不被替换。

### M2：Runtime / Editor 物理分离

**目标**：让 `DSMEngine` target 只包含 Runtime，让 Editor 成为可选目标。

**工作项：**

- 将 `D:\Code\DSMEngine\DSMEngine\Editor` 移至 `D:\Code\DSMEngine\Engine\Source\Editor`；
- 将 `D:\Code\DSMEngine\DSMEngine\Runtime` 移至 `D:\Code\DSMEngine\Engine\Source\Runtime`；
- 更新 include 根和 xmake `add_files` 范围；
- 新增 `DSMEditor` target；
- 把 Serializer 的 Editor 依赖收敛到明确的 Project/Serialization 模块；
- PBR 显式依赖 `DSMEditor`，RayTracing 纯验证目标不依赖 `DSMEditor`。

**完成判据：**

- `xmake build DSMEngine` 不编译任何 Editor `.cpp`；
- `xmake build PBR` 和 RayTracing 验证目标都通过；
- 删除/隐藏 `Engine/Source/Editor` 后，纯 Runtime 示例仍能编译；
- 用链接器或 xmake target 信息确认 Runtime 没有反向依赖 Editor。

### M3：项目目录和 Content/Shader 归属

**目标**：把 Sample 变成真正的 Project，停止用 `Samples/Assets` 作为全局内容根。

**工作项：**

- 将 PBR 和 RayTracing 放入各自 `Projects/<Name>`；
- 把项目入口、项目 shader、验证代码归入对应项目；
- 根据清单和哈希将资产放入项目 Content 或 `Projects/Shared/Content`；
- 删除 `AssetsCopy` 作为普通构建必经步骤，改为运行时搜索根和发布 staging；
- 将 imgui.ini、日志、验证输出改到项目 `Saved`。

**完成判据：**

- 不运行资产全量复制也能启动 PBR、加载模型和 shader；
- 修改一个项目 Content 不影响其他项目；
- 共享资产只保留一份源文件；
- `git diff --check`、构建和运行期资源加载均通过。

### M4：统一构建入口和生成目录

**目标**：让 `.dsmproj`、xmake target 和输出目录建立稳定对应关系。

**工作项：**

- `Tools/dsm.ps1 build/run/check` 统一项目选择、配置和工作目录；
- 约定 `Projects/<Name>/Binaries/Win64`、`Intermediate`、`Saved` 的产物协议；
- 根 xmake 增加 project option 与目标发现逻辑；
- 重新生成 Visual Studio 工程，并把“生成工程”定位为 IDE 视图而非构建真相；
- 在 `docs/verification.md` 或 workflow 文档中记录新命令和退出码。

**完成判据：**

- 新开发者只需给出 `.dsmproj` 路径即可完成 build/run/check；
- 删除 `Intermediate` 后可以重新生成；
- 从不同 cwd 调用同一 project 命令得到相同资源和 shader 结果。

### M5：稳定 Runtime 数据边界（后续重构）

**目标**：减少 Editor 直接操作 Runtime 全局状态，为后续工具和测试留出空间。

**工作项：**

- 将 Scene/Project 序列化 schema 从 UI 代码中抽离；
- 用 `ProjectContext`、`SceneManager` 或明确的 Engine service 替代 `DSMEngine::sm_GlobalContext.scene` 的跨层直接写入；
- 定义 `RenderWorld`/`SceneView` 快照，让 Renderer 不直接依赖 Editor/GameObject 的可变对象图；
- 给资源句柄、shader include、GPU 资源生命周期定义错误返回和所有权边界。

**完成判据：**

- 无窗口场景加载测试可以链接 Runtime 而不链接 ImGui；
- 切换项目、加载失败、保存失败都不会破坏当前活动项目；
- Renderer 能消费一致的只读场景视图，而不是从 UI 线程对象图任意读取。

---

## 验证（Validation）

本方案本身不修改 C++，因此本轮不需要为了计划文档运行完整构建；已执行的结构检查如下：

| 命令 | 工作目录 | 结果 | 证据 |
|---|---|---|---|
| `Get-Content -Raw AGENTS.md` | `D:\Code\DSMEngine` | 通过 | 已读取工程约束和文档路由 |
| `Get-Content -Raw PLANS.md` | `D:\Code\DSMEngine` | 通过 | 已确认 ExecPlan 必填章节 |
| `Get-Content -Raw docs\README.md` | `D:\Code\DSMEngine` | 通过 | 已确认架构/计划/Review 分工 |
| `Get-Content -Raw docs\verification.md` | `D:\Code\DSMEngine` | 通过 | 已确认验证入口和产物协议 |
| `git status --short --branch` | `D:\Code\DSMEngine` | 通过 | 创建本方案前工作树为 `main` 且无未提交改动；本次新增本计划文档 |
| `xmake show -t DSMEngine --verbose` | `D:\Code\DSMEngine` | 通过 | 确认当前 DSMEngine 是单一静态库且包含 `DSMEngine\**.cpp` |
| `xmake show -t PBR --verbose` | `D:\Code\DSMEngine` | 通过 | 确认 PBR 仅通过 DSMEngine 间接获取 Editor/ThirdParty 边界 |
| PowerShell 文件/目录统计 | `D:\Code\DSMEngine` | 通过 | 确认 Projects/Samples 资产重复和当前模块规模 |

实施 M1 后的最小验证矩阵：

```text
xmake build DSMEngine
xmake build PBR
xmake build RayTracing
Tools/dsm.ps1 check Projects/PBR/PBR.dsmproj
Tools/dsm.ps1 check Projects/RayTracing/RayTracing.dsmproj
Tools/dsm.ps1 run Projects/PBR/PBR.dsmproj
Tools/dsm.ps1 run Projects/RayTracing/RayTracing.dsmproj -- --validate-render
```

实施 M2/M3 后还必须验证：

- `xmake show -t DSMEngine --verbose` 的 files 列表不包含 `Engine/Source/Editor`；
- 从三个不同当前目录启动同一 `.dsmproj`，比较最终解析出的 project/content/shader 根；
- 删除项目 `Intermediate` 和 `Saved` 后重建，确认它们能自动恢复；
- 场景加载失败时，当前 Scene、项目路径、编辑器选择状态保持不变；
- 资产源文件只存在一份，构建不再产生全量 Content 复制；
- 通过 `git diff --check` 和新旧项目描述文件的 schema 校验。

---

## 进展（Progress）

### 已完成

- [x] 读取 `D:\Code\DSMEngine\AGENTS.md`、`PLANS.md`、`docs\README.md`、`docs\verification.md`。
- [x] 检查当前 git 状态和顶层目录。
- [x] 检查根、Runtime、ThirdParty、PBR、RayTracing 的 Xmake 目标边界。
- [x] 检查项目描述文件、Serializer、Editor/Runtime 依赖和 Content 重复情况。
- [x] 结合本机 UE 项目及官方文档确定借鉴范围。
- [x] 形成“不照搬 UE、只保留 Engine/Project/Module/Content 四个核心概念”的方案。

### 下一步

- [ ] 用户确认目标树、`.dsmproj` v1 schema 和三目标边界。
- [ ] 经确认后创建实施型 ExecPlan 或直接进入 M1。
- [ ] M1 完成后更新本文件的验证证据和决策记录。

### 当前阻塞

无。当前只是方案阶段；本次仅新增本计划文档，尚未修改源码、配置、项目资产或第三方目录。

---

## 意外与发现（Surprises & Discoveries）

1. 当前 git 工作树没有未提交改动，因此本方案基于当前提交状态，不需要为既有 diff 保留兼容分支。
2. `.dsmproj` 文件已经存在，说明项目锚点概念并非从零引入；主要问题是它目前只保存序列化字段，没有承担项目根、内容根和构建入口的职责。
3. `xmake show` 显示 `DSMEngine` 当前仍以 `DSMEngine\**.cpp` 收集文件，Editor 与 Runtime 的边界是构建配置问题，不只是目录命名问题。
4. `Projects/Assets` 与 `Samples/Assets` 都是较大目录，不能用简单 `Move-Item` 解决；必须先生成哈希/引用清单，再决定共享资产和项目资产的归属。
5. 本机 UE 项目规模远大于 DSMEngine；照搬 UE 的模块/插件数量会降低可读性。因此本方案把“模块作为构建边界”的思想保留，但把第一阶段目标数限制在三个主要库和两个项目可执行目标。

---

## 决策记录（Decision Log）

### D1：以 `.dsmproj` 所在目录作为项目根

**决定**：是。项目文件路径是唯一锚点，所有相对路径从其父目录解析。

**原因**：符合用户希望“路由到 `.proj` 目录下，以这个文件为锚点构建工程”的目标；可脱离当前工作目录；便于多项目并存。

### D2：第一阶段不拆成十几个 Runtime 静态库

**决定**：否。先按 Runtime/Editor/ThirdParty 生命周期拆三类目标。

**原因**：当前规模尚未证明细粒度链接边界能带来收益；过早拆分会放大 include、链接顺序和循环依赖的维护成本。

### D3：普通构建不再全量复制 Content

**决定**：是。运行时使用搜索根；发布流程才做 staging。

**原因**：避免构建时间随资产量增长；消除“源资产和运行副本不一致”的风险；项目根锚点已经可以提供稳定路径。

### D4：Serializer 先留在 Editor，随后下沉为稳定服务

**决定**：分两步。M2 先隔离 Editor target，M5 再把与 UI 无关的 schema/读写能力放到 Runtime/Serialization 或独立服务。

**原因**：直接在目录搬迁、target 拆分和序列化契约重写中同时改动，会让失败定位困难；但长期不能让 Runtime 场景加载依赖 ImGui/Editor。

### D5：插件不是第一阶段的默认容器

**决定**：否。只有具有独立启用开关、资源、依赖和验证闭环的功能才进入 `Plugins/`。

**原因**：当前 PBR、RayTracing、ReSTIR DI 更像项目/示例内容，不需要引入插件发现和生命周期系统。

---

## 结果与复盘（Outcomes & Retrospective）

当前结果是一个待确认的架构方案，不是已实施的代码重构。确认后预期得到的用户可见变化是：

1. 可以直接从 `Projects/<Name>/<Name>.dsmproj` 构建和运行，不依赖仓库根目录或构建后资源副本。
2. Runtime 和 Editor 的构建边界可由 Xmake target 直接观察，不再通过一个全量 glob 隐式绑定。
3. 项目资产、引擎资产、shader、缓存、日志和二进制产物各有唯一归属。
4. 项目描述文件可以复制到另一工作区而不携带当前机器的绝对路径。
5. 复杂的 RenderWorld、资源管线和更多静态库拆分被推迟到有验证证据时再做，不把结构重构变成一次不可回滚的大爆炸。

实施后需要重点复盘：

- `.dsmproj` 是否真的能成为构建、运行、编辑器和验证的共同入口；
- 资源搜索根是否比复制资产更快且更容易诊断；
- `DSMEditor` 分离后，Runtime 是否还隐藏依赖 ImGui、Windows UI 或项目对象；
- 共享 Content 是否确实减少了重复，而不是形成新的“公共垃圾场”；
- 何时有足够证据把 `DSMEngine` 再拆为 `DSMCore`、`DSMGraphics` 和 `DSMRender`。

---

## 实施更新（2026-09-28）

### 已完成

- [x] 将 Runtime、Editor、Shaders、ThirdParty、PBR、RayTracing 和 Content 按 Engine/Project 结构迁移。
- [x] 将顶层 Git 子模块路径统一到 `Engine/ThirdParty/*`，并修复当前工作树中子模块及 DXC 嵌套子模块的本地 Git 元数据路径。
- [x] 新增 `DSMEngine`、`DSMEditor`、`EngineThirdParty` 的 Xmake/CMake 目标边界。
- [x] Assimp 在 Xmake 和 CMake 中均从仓库子模块源码构建；没有调用系统 Assimp 包。
- [x] 新增 CMake 根入口、Engine/ThirdParty/Project CMake 文件和 `CMakePresets.json`。
- [x] 将项目描述文件改为 schema v1 相对路径格式，并新增 `ContentPaths` 与 `tools/dsm.ps1`。
- [x] 合并共享 Content，验证并删除 345 个重复模型文件；普通构建移除全量 `AssetsCopy`。
- [x] 完成 Xmake/CMake 的 Engine、PBR、RayTracing Debug 构建验证。

### 当前剩余

- [ ] 执行 PBR/RayTracing 的交互运行和人工渲染观察。
- [ ] 将 `RendererDX12` 中的 ImGui/ImGuizmo 后端进一步移动到 Editor 侧渲染桥接；当前仍可构建，但 Runtime 对 UI 后端存在实现级依赖。
- [ ] 执行 Release 构建、完整场景加载、设备丢失和非 RTX 设备验证。

### 关键验证证据

详细命令和结果见：

- `docs/reviews/2026-09-28-ue-organization-implementation.md`

### 决策修订

- 第三方供应商源码统一放在 `Engine/ThirdParty`；第一阶段不创建 `Engine/Source/ThirdParty`。
- 由于当前 DXC 子模块不包含可直接链接的 `dxcompiler.lib`/`dxcompiler.dll`，CMake/Xmake 使用 Windows SDK 的 `dxcompiler` 导入库；该限制已记录为外部依赖，而不是伪造为完全自包含。