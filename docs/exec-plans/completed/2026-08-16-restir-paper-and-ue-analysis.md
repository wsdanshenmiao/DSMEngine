# ReSTIR 原论文译述、UE 实现考证与 DSMEngine 接入分析

**状态**: completed  
**创建日期**: 2026-08-16  
**作者**: Codex

## 摘要 (Summary)

获取 ReSTIR 原始论文的官方 PDF，生成中文逐节译述 PDF，并基于本机 `G:\Works\QSClient` 的 UE 5.8.1 源码与 `D:\Code\DSMEngine` 现状，形成一份可追溯的 UE ReSTIR 实现说明和 DSMEngine 接入分析 PDF。任务只做研究、设计与验证，不修改引擎实现。

最终交付放在 `D:\Notes\ReSTIR`：原论文 PDF、中文译述 PDF、实现分析 PDF，以及用于复核来源和生成过程的简短说明。全文翻译是否可交付以论文许可为准；如无明确再分发/翻译授权，则交付高保真逐节译述而不是完整逐句翻译。

## 背景 (Context)

- 原始 ReSTIR 指 Bitterli 等人在 2020 年发表的动态直接光照时空蓄水池重采样方法；需通过作者、NVIDIA Research、ACM 或 DOI 等一手来源确认版本和许可。
- “UE 的 ReSTIR”必须以本机 UE 源码中的实际模块、控制台变量、着色器和渲染图调用为准，并区分经典 ReSTIR DI、ReSTIR GI、MegaLights 等可能被泛称为 ReSTIR 的功能。
- DSMEngine 是 Xmake/MSVC/C++23/D3D12 引擎；接入建议必须结合现有 Render、Graphics、场景、材质、光源、Ray Tracing、G-Buffer、时序资源和 RenderGraph 能力，不假设不存在的基础设施。
- 用户已有 `DSMEngine/Runtime/Core/Macro.h` 改动与本任务无关，必须保持不动。

## 实施计划 (Implementation Plan)

### 里程碑 1：锁定论文与交付边界

- 从一手来源确认论文标题、作者、发表信息、DOI、官方 PDF 和许可。
- 下载并校验原始 PDF，提取目录、公式、图表和参考文献，记录文件哈希。
- 根据许可选择“完整中文翻译”或“逐节详细译述”，保留公式编号、图表编号和术语表。

成功标准：来源可追溯，PDF 可打开，页数与元数据合理，译述范围不超出版权许可。

### 里程碑 2：UE 源码考证

- 在实际可用的 `G:\Works\QSClient` 搜索 ReSTIR、reservoir、resampling、MegaLights、ray tracing direct lighting 等入口。
- 顺着 C++ pass、RDG 资源、shader permutation、CVar 和 HLSL/USH include 关系还原帧内与跨帧数据流。
- 明确 UE 实现与 2020 ReSTIR DI 在候选生成、时域/空间复用、可见性、MIS 权重、偏差控制、去噪及工程约束方面的异同。

成功标准：关键结论均能落到具体源码路径/符号或官方一手文档，明确区分事实、推断和版本相关内容。

### 里程碑 3：DSMEngine 差距与接入路线

- 阅读当前渲染管线、光源数据、D3D12 ray tracing、G-Buffer、运动矢量、TAA/历史资源和 shader 绑定实现。
- 建立“UE 所需能力 -> DSMEngine 现状 -> 缺口 -> 建议模块/接口 -> 验证方法”映射。
- 给出分阶段实现顺序、算法伪代码、资源布局、同步/屏障、历史失效、调试视图、测试场景和性能指标，但不写实现代码。

成功标准：方案能指导后续单独的实现 ExecPlan，且不依赖无依据的模块假设。

### 里程碑 4：PDF 生成与质量验证

- 生成中文译述 PDF 与详细分析 PDF，统一术语、页眉页脚、目录、代码/公式/表格样式和引用。
- 用 Poppler 渲染全部页面，检查中文字体、分页、表格、公式、图像、链接和页码。
- 用 `pdfinfo`、`pypdf`/`pdfplumber` 检查页数、文本可提取性与文件完整性。

成功标准：最终 PDF 无乱码、裁切、重叠、空白异常或不可读内容，所有输出位于 `D:\Notes\ReSTIR`。

## 验证 (Validation)

- 原论文：`Get-FileHash`、`pdfinfo`、逐页渲染抽查，记录来源 URL、下载日期、页数和 SHA-256。
- 源码证据：搜索命令与关键文件/符号清单写入分析附录；对版本/分支作明确说明。
- DSMEngine：仅进行只读源码考证，不运行构建；以路径、接口和数据流交叉核验分析结论。
- 生成 PDF：全页渲染为 PNG，自动检查页面尺寸与渲染成功，再人工检查封面、目录、代表性正文、表格密集页和末页。
- 仓库文档：从 `D:\Code\DSMEngine` 运行 `git diff --check`，并确认未触碰用户已有代码改动。

## 进展 (Progress)

- [x] 读取 `AGENTS.md`、`PLANS.md`、文档与验证协议
- [x] 确认用户已有改动并划定保护边界
- [x] 确认论文版本、许可并下载原 PDF
- [x] 完成论文提取、术语表和中文逐节译述
- [x] 完成 UE 源码数据流考证
- [x] 完成 DSMEngine 现状与差距分析
- [x] 生成并验证全部 PDF
- [x] 更新计划、归档并交付

## 意外与发现 (Surprises & Discoveries)

- `AGENTS.md` 记录的 UE 根路径当前不可用，实际源码位于 `G:\Works\QSClient`；`Build.version` 为 UE 5.8.1、兼容 CL 55116800，并包含 QQSpeed 定制注释。
- UE 中的 Lumen ReSTIR Gather 是 ReSTIR GI 原型，不是原论文的 ReSTIR DI；直接光产品功能 MegaLights 使用向量化 WRS 和历史引导，但不保存或合并历史 reservoir。
- DSMEngine 已有底层 DXR 抽象和独立 sample 记录，但 DeferredRenderer 尚无场景级 BLAS/TLAS 生命周期；RayQuery 也没有主渲染器集成证据。
- 当前 tile light mask shader 未看到 `gsTileInfo` 位掩码的可靠显式清零；未来把它作为 ReSTIR proposal 前必须先修正或验证。
- 论文首页许可允许个人/课堂复制和带署名摘要，但没有给出完整翻译再分发授权，因此交付逐节详细中文译述而非逐句全文翻译。

## 决策记录 (Decision Log)

- 2026-08-16：把原论文、中文译述和工程分析拆成独立文件，便于保留原件、阅读与后续更新。
- 2026-08-16：不把“UE ReSTIR”预设为单一功能；先从本机 UE 源码确认实际对应的 renderer feature。
- 2026-08-16：不修改 DSMEngine 代码或第三方依赖；本任务只交付分析材料。
- 2026-08-16：DSMEngine 首版应采用解析点光/聚光灯的 ReSTIR DI，不复制依赖 Lumen surface cache 的 ReSTIR GI。
- 2026-08-16：先保留 float32/full-resolution/reference path，再按实测吸收 MegaLights 的压缩、clamp、history guiding 和 denoiser。

## 结果与复盘 (Outcomes & Retrospective)

已交付到 `D:\Notes\ReSTIR`：

- `Bitterli_2020_ReSTIR_Original.pdf`：官方原论文，17 页，50,176,579 bytes，SHA-256 `A1AE1233EECD4F82CC9502900DE08012E9A0C233FB8014F53F20C2E9814F243D`；
- `Bitterli_2020_ReSTIR_Chinese_Guide.pdf`：逐节中文译述、公式与实现检查清单，11 页；
- `ReSTIR_UE5.8_DSMEngine_Analysis.pdf`：UE 两条路径源码分析和 DSMEngine 分阶段方案，24 页；
- 两份同名 Markdown 源稿，便于搜索和后续修订。

验证证据：三个 PDF 均通过 `pdfinfo`；原论文 SHA-256 与下载后记录一致；两份生成 PDF 可分别提取 11,862 和 38,605 个字符；52 页全部通过 Poppler 渲染，并用全页 contact sheet 加代表性原尺寸页面检查，无乱码、裁切、空白异常或表格越界。未构建 DSMEngine，因为任务没有代码变更。

最终建议是先补 stable light identity、tile list 正确性、renderer TLAS 和 brute-force reference，再实现 full-resolution ReSTIR DI；MegaLights 式产品优化应在数学参考路径通过后逐项引入。当前 128 局部灯上限下，收益需以固定预算 RT 阴影和真实 GPU 计时证明。
