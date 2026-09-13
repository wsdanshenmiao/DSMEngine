# DSMEngine ReSTIR DI：实现原理、论文映射与工程边界

> 事实基线：2026-09-13。本文描述 `Samples/RayTracing/RestirDI` 当前源码，不泛指所有 ReSTIR 实现。

## 一页结论

当前 Sample 是一套不复用 Forward/Deferred 的纯光追直接光管线：DXR 生成主表面和最终可见性，Compute 完成 Initial RIS、Temporal 与 Spatial Reservoir Resampling，最后只用全屏光栅 Pass 做曝光和 ACES 显示。

算法上，它已经具备 ReSTIR DI 的核心闭环：

- 使用 `w = pHat / q` 构建初始 Reservoir。
- 使用加权 Reservoir Sampling 保存一个代表样本及 `weightSum、M、W、normalizationM`。
- Temporal 和 Spatial 阶段从空 Reservoir 开始，按 `currentPHat * source.W * source.M` 原子合并每个来源。
- 选定最终样本后重放同一来源集合，计算 Algorithm 6 的支持质量 `Z`，并使用 `W = weightSum / (Z * pHat)`。
- 最终估计量为 `unshadowedContribution * W * visibility`。
- 正常渲染固定为 1 SPP：每像素只有一个 Reservoir 和一条最终可见性射线。

更准确地说，它现在是原论文第 4.4 节、Algorithm 6 的**简单无偏 ReSTIR DI**：使用 uniform MIS 支持域修正；没有启用 visibility reuse，因此计算 `Z` 时只需重新评价各来源表面的未遮挡 target，不需要额外阴影射线。Temporal 仍按论文第 5 节验证重投影并把上一帧 `M` 限制为当前 `M` 的 20 倍。

| 维度 | 当前实现 |
|---|---|
| 主可见性 | DXR，每像素一条主射线 |
| 候选域 | 解析灯、自发光三角形、环境贴图 |
| Initial RIS | Compute，每像素默认 32 个候选 |
| Temporal | Motion Vector 重投影 + 稳定 ID/材质/法线/深度校验 |
| Spatial | 固定 1 Pass，默认 3 个随机邻居，半径 30 px；两遍重放求 `Z` |
| 最终阴影 | DXR，每个有效像素最多一条阴影射线 |
| SPP | 固定 1 SPP，不提供多 lane 扩展 |
| 显示 | HDR StructuredBuffer → 全屏 ACES Pass |
| 算法性质 | 原论文 Algorithm 6 uniform MIS 无偏版本；不复用 visibility |

## 1. 一帧的完整数据流

```text
Scene / Editor Settings
        │
        ▼
场景同步：Mesh / Material / Light / Environment
        │  BLAS 重建或复用；TLAS 重建或 refit
        ▼
Primary DXR
        │  position / normal / material / motion / stable ID
        ▼
Initial RIS Compute ───── 每像素生成一个 Reservoir
        ▼
Temporal Reuse Compute ─ 重投影上一帧的单个 Reservoir
        ▼
Spatial Reuse Compute ── 0 或 1 次；选择后重放来源并计算 Z
        ▼
Visibility DXR ───────── 只测试最终被选中的候选
        │  contribution × W × visibility
        ▼
HDR Buffer → ACES Present → Editor Viewport
```

CPU 调度入口是 [`RenderPipeline::Render`](../../Samples/RayTracing/RestirDI/RestirDIRenderPipeline.cpp)。所有 DXR、Compute 和资源上传都记录在 Graphics Queue；稳态帧不调用 `WaitForIdle()`。Surface 历史与 Reservoir 历史通过句柄轮换，不执行整屏复制。

## 2. 数学合同：`q`、`pHat`、`M` 与 `W`

### 2.1 直接光目标

原论文从直接光积分开始：

\[
L = \int_A f(x)\,dx, \qquad f(x)=\rho(x)L_e(x)G(x)V(x)
\]

当前实现把 `EvaluateBRDF * emission/radiance * geometry term` 放入未遮挡贡献，把可见性 `V` 延迟到最终 DXR 阴影射线。Reservoir 的标量目标函数是：

\[
\hat p(x)=\operatorname{luminance}(f_{unshadowed}(x))
\]

这样 Initial、Temporal 和 Spatial 阶段都不需要为每个候选发阴影射线；只有最后幸存的候选才计算可见性。这是 ReSTIR 在大量灯光下能够保持低光线预算的关键，而不是“先算很多完整光照再随机保留一个”。

### 2.2 初始 RIS 权重

候选由 proposal distribution `q(x)` 生成，RIS 权重对应论文 Eq. (5)：

\[
w_i = \frac{\hat p(x_i)}{q(x_i)}
\]

[`InitialRISCS`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICompute.hlsl) 默认为每个像素生成 32 个候选，然后把每个候选以 `candidateM = 1` 送入 `ReservoirUpdate`。

### 2.3 加权 Reservoir Sampling

[`ReservoirUpdate`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 对应论文 Algorithm 2：

```text
weightSum += candidateWeight
M         += candidateM

以 candidateWeight / weightSum 的概率：
    selectedSample = candidate
```

因此 Reservoir 无论代表 32 个、几百个还是更多历史候选，GPU 都只需保存一个候选和固定大小的统计量。

### 2.4 初始与复用后的 Reservoir 权重

初始 RIS 只有一个来源流，支持质量就是 `Z=M`。此时 [`ReservoirFinalize`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 退化为论文 Eq. (6) / Algorithm 3：

\[
W = \frac{weightSum}{M\,\hat p(y)}
\]

时空复用合并多个来源时，不同表面可能对同一光源样本具有不同支持域。Algorithm 6 改用：

\[
Z(y)=\sum_i M_i\,[\hat p_{q_i}(y)>0],\qquad
W = \frac{weightSum}{Z(y)\,\hat p_q(y)}
\]

`Z` 只累计能够产生最终样本 `y` 的来源流。若一部分邻居因法线朝向不同而对 `y` 的 target 为零，则 `Z<M`；仍用总 `M` 会产生论文第 4 节分析的暗偏。

其中 `y` 是被选中的候选。字段映射如下：

| 论文符号 | GPU 字段 | 含义 |
|---|---|---|
| `y` | `GpuReservoirSample` | 被选候选的源类型、稳定 ID、索引和随机种子 |
| `Σw` | `weightSum` | 所有输入候选的累计权重 |
| `M` | `M` | 当前 Reservoir 代表的候选数量 |
| `Z(y)` | `normalizationM` | 对最终样本具有非零 target 支持的来源候选质量 |
| `W` | `W` | 把单个代表样本还原为 RIS 估计量的归一化权重 |

`pHat(y)` 在需要时由当前 `GpuSurface` 和样本重新评价，不再冗余存储。无效 PDF、零目标、NaN/Inf 或非正 `W` 会使代表样本失效；但只要输入流本身有效，`M` 会保留，因为 Algorithm 6 的支持计数不能把“这一次没有选到有效代表样本”误当成“该来源没有生成候选”。

## 3. 三个候选域与完整 proposal PDF

当前 Sample 不是把所有光源简单平均，而是把候选空间拆成三个可独立开关、可独立调权的域。CPU 在 [`RestirDIRenderPipeline.cpp`](../../Samples/RayTracing/RestirDI/RestirDIRenderPipeline.cpp) 中根据各域的总功率计算混合概率，GPU 再乘以域内 Alias PMF：

\[
q(x)=P_D\,q_D(x),\qquad D\in\{A,E,Env\}
\]

| 域 | 候选 | 域内采样 | `q_D(x)` 的主要组成 |
|---|---|---|---|
| `A` | Directional、Point、Spot | 解析灯 Alias Table | 灯的离散 PMF |
| `E` | 自发光材质的三角形 | 三角形 Alias Table + 三角形内连续采样 | 三角形 PMF / 世界空间面积 |
| `Env` | 环境经纬度纹理像素 | 亮度 × 纬度立体角 Alias Table + 像素内抖动 | 像素 PMF / 立体角 |

三张 Alias Table 都由 Walker Alias 方法构建，采样复杂度为 O(1)。`GpuAliasEntry` 同时保存 `probability`、`alias` 和归一化 `pmf`；后者不能省略，因为 Reservoir 权重必须使用真实的完整 proposal PDF，而不是只使用 Alias 的分桶概率。候选的 `sampleSeed` 保留了域内连续随机状态，使历史重投影后能够重新评价同一个稳定候选。

环境贴图先统一为线性经纬度浮点像素。纬度越高，单个经纬度像素对应的立体角越小，因此环境 Alias 权重包含纬度余弦项；环境旋转和强度只影响评价，不改变资源布局。无效或无法解析的 HDR 会被拒绝，旧环境继续生效。

## 4. 场景转译与 DXR 可见性

场景同步模块 [`RestirDIScene.cpp`](../../Samples/RayTracing/RestirDI/RestirDIScene.cpp) 把当前 `Scene` 转译为 Sample 专属 GPU 数据：

1. 每个唯一 Mesh 缓存统一顶点和 32 位索引；每个 Submesh 变成一个 DXR Geometry。
2. 每个唯一 Mesh 建立或复用 BLAS；实例按稳定 `ObjectID` 排序，另存稠密 `InstanceID` 到稳定 ID 的映射。
3. TLAS 使用可更新、偏向快速追踪的构建方式。拓扑、材质集合或实例集合变化时重建并清除历史；只有变换变化时执行 update/refit。
4. Primary 和 Shadow 使用不同 Instance Mask。`CastShadow=false` 的对象仍可被 Primary 命中，但不会阻挡最终阴影射线。
5. 不透明材质使用 `ForceOpaque`；透明材质进入 any-hit，依据基础色纹理 Alpha 和 `alphaCutoff` 进行裁剪。双面、法线、粗糙度、金属度、自发光和纹理索引都来自 Sample 自己的材质缓冲。

Primary RayGen 输出 `GpuSurface`：位置、设备深度、法线、粗糙度、反照率、金属度、自发光、当前/上一帧运动信息以及稳定实例 ID。背景不会创建 Reservoir，只返回环境颜色。最终 Visibility RayGen 只为幸存候选发一条阴影射线；Reference RayGen 则用高数量独立候选作为验证基线。

历史清除条件包括 Resize、场景切换、拓扑/材质集合变化、光源分布变化、环境变化、算法参数变化和显式 Reset。连续相机、对象和灯光运动不主动清除历史，而是交给重投影兼容性检查。

## 5. Temporal 与 Spatial Reservoir Reuse

Temporal 阶段根据 Primary 输出的 motion vector 把当前像素重投影到上一帧，并检查命中有效、稳定 ID/材质相容、法线和深度连续性以及屏幕边界。这些检查用于确认历史仍属于当前表面；它们不是无偏修正本身。通过后，当前与历史 Reservoir 都作为独立来源，从空输出 Reservoir 重新合并。上一帧来源质量按论文第 5 节裁剪为：

\[
M_{history}'=\min(M_{history},20M_{current})
\]

Spatial 阶段在 30 px 半径内随机选择 3 个有效表面。无偏版不再使用有偏实现的法线/材质邻居拒绝，因为跨支持域正是 `Z` 要校正的情况。每个来源只贡献其代表样本，但通过 `source.W * source.M` 恢复它所代表的候选质量：

\[
w_{merge}=\hat p_{current}(y_{source})\;W_{source}\;M_{source}
\]

合并分两遍完成：

1. 第一遍将中心和邻居来源逐一执行上述原子更新，选出最终代表样本 `y`。
2. 第二遍从相同随机种子重放完全相同的邻居序列，在每个来源表面重新评价 `pHat_qi(y)`，把非零来源的 `M_i` 累加为 `Z`。
3. 最后使用 `W=weightSum/(Z*pHat_current(y))` 完成 Algorithm 6。

邻居选择和 reservoir replacement 使用相互独立的随机状态，否则第一遍的随机替换会改变第二遍邻居序列，`Z` 将对应错误的来源集合。实现固定一次 spatial pass，既对齐论文无偏配置，也避免多 pass 增加来源谱系和方差。历史上限只作用于进入 temporal 的上一帧来源，不再在每个 spatial pass 后全局缩放 `weightSum/M`。

## 6. 最终可见性与固定 1 SPP

在 [`RestirDITrace.hlsl`](../../Samples/RayTracing/RestirDI/Shaders/RestirDITrace.hlsl) 中，最终直接光近似为：

\[
\widehat L_{direct}=f_{unshadowed}(y)\;W\;V(y)\,.
\]

`V(y)` 由 DXR Shadow Ray 求得，结果写入 HDR `StructuredBuffer<float4>`，最后由全屏 Pass 做曝光和 ACES 色调映射。

正式渲染路径固定为论文交互式配置使用的 1 SPP。CPU 每帧只调度一条 Initial、Temporal、Spatial、Visibility 链；GPU 每像素只保存一份 Reservoir 历史，最后最多追踪一条阴影射线。代码中不再存在多 lane 资源、逐 lane 随机种子、HDR 累加或 SPP 调节项。

这并不表示每个像素只考察一个灯光候选：默认 Initial RIS 仍会生成 32 个候选，时空复用还会把历史与邻居 Reservoir 所代表的候选质量带入当前像素。1 SPP 指最终只输出一个代表样本并只对它求一次可见性。若仍有噪声，应改进 proposal、候选数或另加去噪，而不是复制整条 ReSTIR 链。

## 7. 调试视图与可调参数

Editor 只暴露 `Unbiased ReSTIR DI` 正式模式。高样本 Reference RayGen 仅供自动验证内部使用，不是可切换的生产渲染变体。

Debug View 覆盖 Surface、Normal、Albedo、Source Type/ID、`pHat`、`M`、`Z/M`、`W`、Temporal/Spatial Accept 和 Visibility。`Support ratio Z/M` 中绿色表示所有来源都支持最终样本，偏红表示 Algorithm 6 排除了一部分来源；背景为红色，因为它没有 Reservoir。建议排查顺序是先看 Surface/Normal，再看 Source ID、`pHat` 和 `Z/M`，最后看接受率与 Visibility。

常用调参路径：

- 先调整解析灯、自发光、环境三域权重和初始候选数。
- 再观察 Temporal 的静止接受率和运动后的拒绝区域。
- 最后逐步增加 Spatial 邻居数，检查空间复用是否跨越几何边界。
- `normalBias` 只用于消除自遮挡，不要用大偏移掩盖错误法线；`alphaCutoff` 必须与材质裁剪语义一致。
- 若高亮点仍有火花，应先检查 Alias PMF、环境立体角和 `pHat/q`。

## 8. 与原论文的对应关系

| 当前代码/概念 | 原论文位置 | 说明 |
|---|---|---|
| `ReservoirUpdate` | Algorithm 2 | 加权 Reservoir Sampling |
| `ReservoirFinalize` | Eq. (6)、Eq. (20)、Algorithm 6 | 初始用 `M`，复用用支持质量 `Z` 归一化 |
| `InitialRISCS` | 第 3 节、Algorithm 1/2 | 每像素初始候选与 RIS |
| `ReservoirMergeSource` / `ReservoirSupportM` | Algorithm 6 第 4、7–11 行 | 原子合并来源并计算 uniform MIS 支持质量 |
| `TemporalReuseCS` | Algorithm 5/6、第 5 节 | 历史重投影、20× M 上限与无偏合并 |
| `SpatialReuseCS` | Algorithm 6、第 5 节 | 3 个随机邻居、两遍重放、一次 spatial pass |
| `VisibilityRayGen` | 第 5 节最终 shade | 对代表候选补做可见性 |
| `temporalHistoryMCapMultiplier` | 论文第 5 节 | 上一帧 `M` 最多为当前 `M` 的 20 倍 |
| 三域 Alias proposal | 论文通用 `q(x)` 的工程扩展 | 便于解析灯、发光几何和环境共存 |

推荐同时阅读官方论文页面：[Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct)。论文给出通用 ReSTIR DI 框架；本 Sample 的解析灯域、Alpha any-hit 和 Stable ID 是面向 DSMEngine 的场景接入，正式估计器固定为论文采用的 1 SPP Algorithm 6 路径。

## 9. 当前实现明确没有做什么

以下内容不能从当前代码推断为已实现：

- 没有实现论文 Eq. (22) 的 balance heuristic；当前选择的是 Algorithm 6 更简单的 uniform MIS。
- 没有在 Initial RIS 阶段复用上一帧 Visibility。由于 visibility 不属于 reservoir target，支持域测试无需额外阴影射线。
- 没有把跨帧 Reservoir 当作“免费无限 SPP”；历史 M 有上限，且兼容性拒绝会丢弃样本。
- 没有复用 Forward/Deferred 的 GBuffer、Lighting 或 Shadow Pass；只有最终显示沿用全屏光栅写回 Viewport。
- 没有新增 RHI 接口或修改 `DSMEngine/Runtime/Graphics/**`。

因此，“无偏”只描述当前直接光估计器的期望值，不代表低方差、无 firefly、包含间接光或已经去噪。若将来加入 visibility reuse，必须把可见性同时纳入 Algorithm 6 的来源支持测试并承担额外阴影射线；否则会重新引入偏差。

## 10. 验证证据与维护入口

验证代码位于 [`RestirDIValidation.cpp`](../../Samples/RayTracing/RestirDI/RestirDIValidation.cpp)，覆盖有限值、Reservoir 有效率、三候选域非零能量、Temporal/Spatial 接受拒绝、Resize、Alpha、`CastShadow=false`、HDR 加载和 Reference 对比。最近一次通过记录：

`build/verification/restir-di-one-spp/2026-09-13/attempt-2/`

关键指标：

| 指标 | 结果 |
|---|---:|
| 验证退出码 | 0 |
| 正式路径 SPP / 最大 Visibility 计数 | 1 / 1 |
| HDR 有限像素 | 1.0 |
| 有效 Reservoir 比例 | 1.0 |
| `0 < Z <= M` 合法比例 | 1.0 |
| 实际发生 `Z < M` 的比例 | 12.33% |
| 最小 `Z/M` | 0.25 |
| 平均亮度相对误差 | 0.003270 |
| 16×16 分块归一化 RMSE | 0.039933 |
| Editor 自动帧 | 120 / 120（相机移动通过） |
| D3D12/DXGI Debug 消息 | 0 条 |

对应的执行计划和论文注释见：

- [`docs/exec-plans/completed/2026-09-08-restir-di-unbiased.md`](../exec-plans/completed/2026-09-08-restir-di-unbiased.md)
- [`docs/exec-plans/completed/2026-09-07-restir-di-paper-annotations.md`](../exec-plans/completed/2026-09-07-restir-di-paper-annotations.md)

修改候选分布、Reservoir 布局或 HLSL 常数时，应同时检查 [`RestirDIShared.h`](../../Samples/RayTracing/RestirDI/RestirDIShared.h) 的 `static_assert`、[`RestirDICommon.hlsli`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 的 enum 序号，以及 Debug/Release 和 DebugLayer 验证。
