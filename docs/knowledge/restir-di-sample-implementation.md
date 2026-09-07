# DSMEngine ReSTIR DI：实现原理、论文映射与工程边界

> 事实基线：2026-09-07。本文描述 `Samples/RayTracing/RestirDI` 当前源码，不泛指所有 ReSTIR 实现。

## 一页结论

当前 Sample 是一套不复用 Forward/Deferred 的纯光追直接光管线：DXR 生成主表面和最终可见性，Compute 完成 Initial RIS、Temporal 与 Spatial Reservoir Resampling，最后只用全屏光栅 Pass 做曝光和 ACES 显示。

算法上，它已经具备 ReSTIR DI 的核心闭环：

- 使用 `w = pHat / q` 构建初始 Reservoir。
- 使用加权 Reservoir Sampling 保存一个代表样本及 `weightSum、M、W、selectedPHat`。
- Temporal 和 Spatial 阶段按 `currentPHat * source.W * source.M` 合并上游 Reservoir。
- 最终估计量为 `unshadowedContribution * W * visibility`。
- 每个 SPP 是一条独立的完整 ReSTIR lane，而不是重复使用同一个 Reservoir。

更准确地说，它是原论文 Algorithm 5 的**实用有偏 ReSTIR DI**：包含论文第 5 节的表面相似性拒绝和历史 M 限制，但没有实现第 4.3/4.4 节的 MIS 修正与 Algorithm 6 无偏合并，也没有实现初始可见性复用。这个边界必须保留，否则会把“标准 ReSTIR 核心流程”和“严格无偏估计器”混为一谈。

| 维度 | 当前实现 |
|---|---|
| 主可见性 | DXR，每像素一条主射线 |
| 候选域 | 解析灯、自发光三角形、环境贴图 |
| Initial RIS | Compute，默认每 lane 32 个候选 |
| Temporal | Motion Vector 重投影 + 稳定 ID/材质/法线/深度校验 |
| Spatial | 默认 2 Pass，每 Pass 5 个随机邻居，半径 30 px |
| 最终阴影 | DXR，每个有效 lane 最多一条阴影射线 |
| SPP | 1～8 条独立 Reservoir lane，最终平均 |
| 显示 | HDR StructuredBuffer → 全屏 ACES Pass |
| 算法性质 | 实用有偏，不是严格无偏 MIS 版本 |

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
Initial RIS Compute ───── 每个 SPP lane 独立生成 Reservoir
        ▼
Temporal Reuse Compute ─ 重投影上一帧同 lane 的 Reservoir
        ▼
Spatial Reuse Compute ── 0～2 次邻域 Reservoir 合并
        ▼
Visibility DXR ───────── 只测试最终被选中的候选
        │  Σ contribution × W × visibility / SPP
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

[`InitialRISCS`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICompute.hlsl) 默认为每个像素、每条 lane 生成 32 个候选，然后把每个候选以 `candidateM = 1` 送入 `ReservoirUpdate`。

### 2.3 加权 Reservoir Sampling

[`ReservoirUpdate`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 对应论文 Algorithm 2：

```text
weightSum += candidateWeight
M         += candidateM

以 candidateWeight / weightSum 的概率：
    selectedSample = candidate
    selectedPHat   = candidatePHat
```

因此 Reservoir 无论代表 32 个、几百个还是更多历史候选，GPU 都只需保存一个候选和固定大小的统计量。

### 2.4 最终 Reservoir 权重

[`ReservoirFinalize`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 对应论文 Eq. (6) / Algorithm 3：

\[
W = \frac{weightSum}{M\,\hat p(y)}
\]

其中 `y` 是被选中的候选。字段映射如下：

| 论文符号 | GPU 字段 | 含义 |
|---|---|---|
| `y` | `GpuReservoirSample` | 被选候选的源类型、稳定 ID、索引和随机种子 |
| `Σw` | `weightSum` | 所有输入候选的累计权重 |
| `M` | `M` | 当前 Reservoir 代表的候选数量 |
| `pHat(y)` | `selectedPHat` | 被选候选在当前表面的目标值 |
| `W` | `W` | 把单个代表样本还原为 RIS 估计量的归一化权重 |

无效 PDF、零目标、NaN/Inf 或非正 `W` 都会清空 Reservoir，防止坏值进入时空历史。

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

Temporal 阶段根据 Primary 输出的 motion vector 把当前像素重投影到上一帧，并检查：命中有效、稳定 ID 相容、法线夹角低于阈值、相对深度差低于阈值，以及重投影坐标在屏幕内。通过后，历史 Reservoir 被视为一个包含 `history.M` 个候选的上游 Reservoir，与当前 Reservoir 合并。

Spatial 阶段在屏幕半径内随机选择邻居，按同样的表面兼容条件拒绝跨物体、跨深度或跨法线的邻居。每个邻居只贡献其代表样本，但通过 `source.W * source.M` 恢复它所代表的上游候选质量。最终合并权重为：

\[
w_{merge}=\hat p_{current}(y_{source})\;W_{source}\;M_{source}
\]

这就是论文 Algorithm 4/5 的工程化合并形式。每次 pass 都重新 Finalize，并把 `M` 限制在 `initialCandidates × historyMCapMultiplier` 以内。这个 M 上限是防止历史无限增长、拖影和数值爆炸的实用有偏措施；它不是论文严格无偏版本中的等价替换。Spatial 默认 2 pass、每 pass 5 个邻居、半径 30 px，UI 允许关闭或缩小这些开销。

## 6. 最终可见性、SPP 与性能含义

在 [`RestirDITrace.hlsl`](../../Samples/RayTracing/RestirDI/Shaders/RestirDITrace.hlsl) 中，最终直接光近似为：

\[
\widehat L_{direct}=f_{unshadowed}(y)\;W\;V(y)\,.
\]

`V(y)` 由 DXR Shadow Ray 求得，结果写入 HDR `StructuredBuffer<float4>`，最后由全屏 Pass 做曝光和 ACES 色调映射。

`samplesPerPixel` 的语义是 **1～8 条独立完整 ReSTIR lane**：每条 lane 都有自己的初始随机序列、Reservoir 历史、Temporal/Spatial 合并和最终 Visibility；只有 lane 结果在最终 HDR 阶段平均。因此增加 SPP 不会把同一个 Reservoir 错误放大，但会近似线性增加 Reservoir 计算、历史显存和最终阴影射线成本。Primary 表面通常仍是一条共享主射线，主要新增成本来自每 lane 的候选与 Visibility。

这也解释了“提高 SPP 但画质提升不明显”的常见现象：ReSTIR 已经把许多候选压缩成一个代表样本，剩余误差可能来自 proposal 不匹配、极亮长尾、时空相关、没有额外去噪以及色调映射。当前实现的自动验证记录了 1/2/4/8 SPP 相对参考图的逐步下降误差，因此 SPP 是有效的，但不是把低质量 proposal 变成无噪声结果的开关。

## 7. 模式、调试视图与可调参数

| 模式 | 用途 | 是否时空复用 | 适合场景 |
|---|---|---:|---|
| `ReSTIR` | 完整 Initial + Temporal + Spatial + Visibility | 是 | 日常质量/性能 |
| `Independent RIS` | 每帧独立 RIS，不使用历史和邻居 | 否 | 判断噪声来自复用还是 proposal |
| `Reference` | 每像素高数量独立候选并逐样本可见性 | 否 | 质量回归基线，成本最高 |

Debug View 覆盖 Surface、Normal、Albedo、Source Type/ID、`pHat`、`M`、`W`、Temporal/Spatial Accept 和 Visibility。建议排查顺序是先看 Surface/Normal，再看 Source ID 和 `pHat`，最后看接受率与 Visibility；如果 Surface 正确但 Source ID 全无效，问题在候选域或 PDF；如果 Source 正确而 Visibility 全黑，问题在 TLAS mask、法线偏移或阴影射线。

常用调参路径：

- 先用 `Independent RIS` 调整解析灯、自发光、环境三域权重和初始候选数。
- 再打开 Temporal，观察静止画面的接受率和运动后的拒绝区域。
- 最后打开 Spatial；逐步增加邻居数或 pass，避免一开始就把空间相关和高 SPP 叠加。
- `normalBias` 只用于消除自遮挡，不要用大偏移掩盖错误法线；`alphaCutoff` 必须与材质裁剪语义一致。
- SPP 适合用于降低独立阴影样本噪声；若高亮点仍有火花，应先检查 Alias PMF、环境立体角和 `pHat/q`，而不是只继续增加 SPP。

## 8. 与原论文的对应关系

| 当前代码/概念 | 原论文位置 | 说明 |
|---|---|---|
| `ReservoirUpdate` | Algorithm 2 | 加权 Reservoir Sampling |
| `ReservoirFinalize` | Eq. (6)、Algorithm 3 | `W = sum(w)/(M pHat(y))` |
| `InitialRISCS` | 第 3 节、Algorithm 1/2 | 每像素初始候选与 RIS |
| `TemporalReuseCS` | Algorithm 4/5 | 历史 Reservoir 的重投影合并 |
| `SpatialReuseCS` | Algorithm 5 | 邻域 Reservoir 合并与表面相似性 |
| `VisibilityRayGen` | 第 5 节最终 shade | 对代表候选补做可见性 |
| `historyMCapMultiplier` | 工程扩展 | 限制历史置信度，带来偏差 |
| 三域 Alias proposal | 论文通用 `q(x)` 的工程扩展 | 便于解析灯、发光几何和环境共存 |

推荐同时阅读官方论文页面：[Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct)。论文给出的是通用 ReSTIR DI 框架；本 Sample 的三域候选、Alpha any-hit、Stable ID 和 SPP lane 是面向 DSMEngine 的落地实现。

## 9. 当前实现明确没有做什么

以下内容不能从当前代码推断为已实现：

- 没有实现论文第 4.3/4.4 节的 MIS 校正或 Algorithm 6 的严格无偏合并。
- 没有在 Initial RIS 阶段复用上一帧 Visibility；`enableVisibilityReuse` 是保留字段，保持关闭。
- 没有把跨帧 Reservoir 当作“免费无限 SPP”；历史 M 有上限，且兼容性拒绝会丢弃样本。
- 没有复用 Forward/Deferred 的 GBuffer、Lighting 或 Shadow Pass；只有最终显示沿用全屏光栅写回 Viewport。
- 没有新增 RHI 接口或修改 `DSMEngine/Runtime/Graphics/**`。

因此，若目标是论文中的严格无偏参考实现，需要另行加入 MIS 权重、可见性历史和相应的方差/偏差验证；不能只把当前 UI 的 `historyMCapMultiplier` 改大。

## 10. 验证证据与维护入口

验证代码位于 [`RestirDIValidation.cpp`](../../Samples/RayTracing/RestirDI/RestirDIValidation.cpp)，覆盖有限值、Reservoir 有效率、三候选域非零能量、Temporal/Spatial 接受拒绝、Resize、Alpha、`CastShadow=false`、HDR 加载和 Reference 对比。最近一次通过记录：

`build/verification/restir-di-paper-annotations/2026-09-07/attempt-3/`

关键指标：

| 指标 | 结果 |
|---|---:|
| 验证退出码 | 0 |
| HDR 有限像素 | 1.0 |
| 有效 Reservoir 比例 | 0.999964 |
| 平均亮度相对误差 | 0.031412 |
| 16×16 分块归一化 RMSE | 0.062496 |
| Editor 自动帧 | 120 / 120 |
| D3D12/DXGI Debug 消息 | 0 条 |

对应的标准化、论文注释和执行计划分别见：

- [`docs/exec-plans/completed/2026-09-07-restir-di-paper-annotations.md`](../exec-plans/completed/2026-09-07-restir-di-paper-annotations.md)
- [`docs/exec-plans/completed/2026-09-07-restir-di-standardization.md`](../exec-plans/completed/2026-09-07-restir-di-standardization.md)

修改候选分布、Reservoir 布局或 HLSL 常数时，应同时检查 [`RestirDIShared.h`](../../Samples/RayTracing/RestirDI/RestirDIShared.h) 的 `static_assert`、[`RestirDICommon.hlsli`](../../Samples/RayTracing/RestirDI/Shaders/RestirDICommon.hlsli) 的 enum 序号，以及 Debug/Release 和 DebugLayer 验证。
