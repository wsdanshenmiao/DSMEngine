# 验证工作流

本文维护 DSMEngine 当前可用的常用验证 workflow。仓库级验证协议见 `docs/verification.md`。

## 文档与 Harness 改动

适用范围：

- `AGENTS.md`
- `CLAUDE.md`
- `PLANS.md`
- `docs/`
- `.agents/`
- `tools/README.md`

检查方式：

1. 阅读受影响文档，确认入口、路径和目录描述一致。
2. 从仓库根目录运行：

   ```powershell
   git diff --check
   ```

3. 如涉及新增路径，确认目录实际存在。

成功标准：

- Markdown 内容可读。
- 路径引用能在仓库中找到，或明确说明是未来预留路径。
- `git diff --check` 没有空白错误。

## C++ 构建验证

适用范围：

- `DSMEngine/Runtime/`
- `DSMEngine/Editor/`
- `Samples/PBR/`
- 影响编译的头文件、源文件或构建规则。

默认工作目录：仓库根目录。

命令：

```powershell
xmake
```

或只验证 PBR：

```powershell
xmake build PBR
```

成功标准：

- 命令退出码为 0。
- 没有新增编译或链接错误。
- 如果只构建 `PBR`，需要说明为什么足以覆盖本次改动。

## 渲染与编辑器人工验证

适用范围：

- 渲染管线。
- HLSL 着色器。
- GPU 资源创建、绑定、生命周期。
- 场景加载、材质、灯光、阴影、后处理。
- 编辑器 viewport 或 ImGui 面板。

默认工作目录：仓库根目录。

命令：

```powershell
xmake build PBR
xmake run PBR
```

记录内容：

- 使用的 project 或 scene。
- 相机视角或编辑器操作路径。
- 预期视觉结果。
- 实际观察结果。
- 截图、录像或 RenderDoc capture 路径，如有。

成功标准：

- `PBR` 能构建并启动。
- 目标路径无明显崩溃、黑屏、资源缺失或断言。
- 用户可见行为符合本次任务定义。

## 构建图或工程生成验证

适用范围：

- `xmake.lua`
- `rules.lua`
- 新增源文件或 shader 文件的构建集成。
- Visual Studio 工程生成规则。

默认工作目录：仓库根目录。

命令：

```powershell
xmake
xmake project -k vsxmake2022
```

成功标准：

- 构建通过。
- 需要 IDE 工程时，生成步骤通过。
- 新增文件被正确纳入目标，或明确说明不应纳入构建。

## Release 或性能敏感验证

适用范围：

- 优化路径。
- 条件编译。
- 资源生命周期。
- 只在 release 暴露的问题。

默认工作目录：仓库根目录。

命令：

```powershell
xmake f -m release
xmake
```

成功标准：

- release 构建通过。
- 如任务要求性能结论，必须记录测试场景、指标、采样方式和对比基线。

完成后如需继续 debug 开发，显式切回：

```powershell
xmake f -m debug
```
