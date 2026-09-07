# 知识库

本目录应轻量使用。目标是沉淀高频复用的活知识，而不是保存所有调查过程。

## 适合保留

- 高频复用的领域事实。
- 从源码中不容易直接看出的稳定约定。
- 已被多次验证的排查路径。
- 权威外部或内部资料链接。

## 避免放入

- 一次性调查流水账。
- 大段复制源码解释，容易随实现变化而过期。
- 大型原始日志。
- 应归入 `docs/exec-plans/` 的计划和过程记录。

## 维护规则

如果一篇知识记录变得过期或低价值，应删除，或改为引用对应的归档 Exec-Plan。

## ReSTIR DI 资料

- [RestirDI 实现地图（Markdown）](restir-di-sample-implementation.md)：当前 `Samples/RayTracing/RestirDI` 的数据流、数学合同、论文映射、参数和验证证据。
- [RestirDI 实现地图（HTML）](restir-di-sample-implementation.html)：可搜索、可打印的同内容版本。
- [ReSTIR 原始论文中文精读与公式讲解](Bitterli_2020_ReSTIR_Chinese_Guide.md)：面向个人学习的中文解读，不替代原论文。
- [SIGGRAPH 2023 ReSTIR 课程中文学习指南](restir-siggraph-2023-course-chinese-study-guide.md)：课程笔记的原创学习提纲与实现提示。

原始论文请从 [NVIDIA Research 官方页面](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct) 获取。中间翻译 PDF、页面图片、缓存和生成脚本不纳入仓库知识库；本地整理副本位于 `D:\Notes\ReSTIR\archive`。
