# 架构说明

源码是最终权威。本目录用于帮助 Agent 快速定位子系统和入口，避免重复加载大量上下文。

## 既有模块分析

现有项目文档位于 `Documents/`：

- `Documents/README.md`
- `Documents/Core_模块分析文档.md`
- `Documents/Framework_模块分析文档.md`
- `Documents/Graphics_模块分析文档.md`
- `Documents/Render_模块分析文档.md`

对相应模块做大范围修改前，先阅读这些文档，再用当前源码校验是否仍然准确。

## 系统地图

- `DSMEngine/Runtime/Core/`：窗口、输入、日志、计时和性能采样。
- `DSMEngine/Runtime/Framework/`：Scene、GameObject、组件和 ECS 集成。
- `DSMEngine/Runtime/Graphics/`：图形 API 抽象和 D3D12 后端。
- `DSMEngine/Runtime/Render/`：渲染管线、pass、渲染资源和模型加载。
- `DSMEngine/Runtime/Math/`：数学基础类型和空间查询。
- `DSMEngine/Editor/`：编辑器外壳和 ImGui 集成。
- `DSMEngine/Shaders/`：HLSL 着色器代码。

## 记录原则

- 只有当架构说明能帮助后续 Agent 找入口或避坑时才新增。
- 临时调查过程放入 Exec-Plan。
- 如果文档过期，应明确标记或删除，避免误导。
