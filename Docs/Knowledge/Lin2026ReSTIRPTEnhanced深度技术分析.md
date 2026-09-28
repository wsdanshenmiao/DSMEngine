# ReSTIR PT Enhanced：深度技术分析、公式推导与 DSMEngine 落地建议

## 0. 文档定位与证据边界

本文针对 Daqi Lin、Markus Kettunen、Chris Wyman 的论文 *ReSTIR PT Enhanced: Algorithmic Advances for Faster and More Robust ReSTIR Path Tracing*（PACM CGIT 9(1), Article 13, 2026）进行二次技术分析。论文正文共 19 页，实验基于 Falcor、RTX 5880 Ada、1920×1080，并以 Lin et al. 2022 的 ReSTIR PT 公开实现为基线。

下文区分三类内容：

1. **论文事实**：论文正文明确给出的公式、算法、参数、测量和结论。
2. **数学解释**：对论文公式作等价改写、推导或用简化例子解释；这不是论文原文的额外定理。
3. **DSMEngine 建议**：结合已有的 DSMEngine / UE 5.8 审阅结果提出的工程路线，必须在当前代码分支和目标 GPU 上重新验证。

论文把统一 DI/GI 后的 PSS MIS 与 source PDF 细节放在 supplemental；本报告不假设未提供的细节，而是给出实现时应满足的契约和验证方法。论文也没有声称所有增强都无偏：duplication map 会主动修改置信上限并引入偏差。

## 1. 一页结论

ReSTIR PT Enhanced 不是一个全新的 estimator，而是在 ReSTIR PT 的 GRIS 链和 hybrid shift 上做“工程化收敛”：

- 用**互惠配对空间复用**复用一对像素之间已经算过的双向移位，把空间移位的主要成本摊销掉；
- 用**双射线足迹 + 单顶点粗糙度**判据替代依赖场景尺寸的“距离 + 双顶点粗糙度”判据，尽早且更可靠地找到可连接顶点；
- 用**sample duplication map**识别同一初始候选扩散成的相关团块，自适应降低时间 `c_Cap`，以小偏差换取显著较少的 firefly 传播；
- 把直接光和间接光放入**统一路径空间 reservoir**，既省掉一套 pass/存储，也让直接光受益于 PT 的移位映射；
- 通过条件移动、流压缩、强制 NEE 重连接、初始阶段 Russian roulette、向量色彩权重、双运动向量等手段降低 GPU 分歧和重建噪声。

论文报告的核心数字是：成本削减链平均 `2.74×` 加速；包含全部质量改进后相对原始 ReSTIR PT 仍 `2.30×` 更快，单场景总体为 `2.08×–3.05×`；reservoir 相关存储从 `431 MB` 降到 `265 MB`；配对空间复用平均 `1.63×` 加速；默认去相关在最困难 Kitchen 场景的平均绝对相对偏差为 `3.25%`。

最重要的工程判断是：

> PT Enhanced 的“快”主要来自减少移位、重放和分歧；它的“稳”部分来自更好的移位可连接性，部分来自有意引入的偏差控制。两者必须以独立开关、独立 reference 和独立 GPU timestamp 验证。

## 2. 记号与 estimator 契约：避免最常见的混淆

| 记号 | 含义 | 不能替代为 |
|---|---|---|
| `p(x)` | 候选的 source PDF / proposal PDF | 目标函数 `p̂` |
| `p̂(x)` | 未归一化目标函数，通常近似标量贡献或 `abs(F)` | 概率密度 `p` |
| `p̄(x)` | 归一化目标 PDF，`p̄ = p̂ / ∫p̂` | 任何单个 source PDF |
| `F(x)` / `f(x)` | 带颜色或完整路径项的 integrand / PSS integrand | 亮度 target `p̂` |
| `mᵢ` | GRIS 的 resampling MIS 权重，负责不同 proposal/domain 的覆盖归一化 | path MIS `ωₜ` |
| `ωₜ` | 路径采样技术（NEE、BSDF 等）之间的 MIS 权重 | reservoir 的 `W` |
| `X` | reservoir 当前持有的样本（路径或灯） | 样本数 `M` |
| `W_X` | 样本的无偏贡献权重，期望为 `1/p_X(X)` | `wSum` |
| `c` | 论文称 confidence weight / effective sample count | 一定等于 `M` 的工程字段 |
| `M` | 工程实现常用的候选/置信计数（需按所用 GRIS 契约解释） | `W` |
| `wSum` | 候选权重和的累积量 | PDF 的倒数本身 |
| `W` | 最终样本归一化权重，如 `wSum/(M p̂(X))` | RGB radiance |

对一个普通 RIS reservoir，最安全的实现关系是：候选权重 `w_i = p̂(X_i)/p(X_i)`（若论文采用了 `1/M`，则把该因子一致地放入 `wSum`）；选中样本 `Y` 的估计为 `f(Y)W_Y`，其中 `W_Y = wSum/p̂(Y)`，或在按平均权重定义时写成 `wSum/(M p̂(Y))`。关键不是某个字段名，而是：**同一份 `wSum`、`M`、`p̂`、source PDF 定义必须贯穿 initial、temporal、spatial、shade 四个阶段，不能重复除以亮度或样本数。**

## 3. 从 RIS 到 GRIS：为什么 reservoir 可以跨像素、跨帧复用

### 3.1 RIS 的基本推导

目标是估计

```text
I = ∫Ω f(x) dx。
```

从易采样但不理想的 `p(x)` 取 `M` 个候选。令 `p̂(x) ≈ |f(x)|`，对每个候选计算

```text
w_i = (1/M) · p̂(X_i) / p(X_i)。
```

按 `w_i` 选出 `Y`。在 `M` 足够大时，选择概率趋近于 `p̄(y) = p̂(y)/Z`，其中 `Z = ∫Ω p̂(x)dx`。若 `w_i` 已含 `1/M`，则 `wSum = Σ_i w_i` 的期望是 `Z`，选中样本的 `W_Y=wSum/p̂(Y)` 估计 `1/p_Y(Y)`；若实现把 `1/M` 留在最后，则使用 `W_Y=wSum/(M p̂(Y))`。这里的 unbiasedness 依赖候选覆盖目标支撑、权重与选择概率使用同一 convention，以及没有事后按样本内容删改候选。

### 3.2 GRIS 的新增因素

跨像素复用时，邻居样本 `X_i` 不在当前像素的同一随机变量域。用双射 `T_i` 把它映射到当前域，`Y_i = T_i(X_i)`。若候选原本来自不同域/不同技术/相关分布，就需要：

```text
w_i = m_i(Y_i) · p̂(Y_i) · W_Xi · |∂T_i/∂X_i|。                    (1)
```

其中 `W_Xi` 不是“邻居 reservoir 的最终颜色”，而是对邻居样本原始抽样 PDF 倒数的无偏估计；Jacobian 修正变量变换；`m_i` 保证多种 proposal 的覆盖不重复计权。若 `Var[Σw_i] → 0`，重采样分布收敛到目标分布。论文强调，通常要保留一个完整覆盖目标支撑的 canonical sample，才能维持无偏积分。

### 3.3 temporal/spatial 的置信上限为何既有用又危险

时间 reservoir 的置信度被截为 `min(c_Cap, c_temp)+1`，可防止动态场景无限积累旧样本，也可限制相关性传播。但只要 `c_Cap` 依赖样本内容、空间位置或历史相关性，原本用于 MIS 归一化的 partition of unity 就不再严格成立。PT Enhanced 明确接受这个 trade-off：默认 duplication map 是 biased mode；关闭该功能时恢复无偏性。

## 4. 原始 ReSTIR PT 基线：它为什么比 DI/GI 难

### 4.1 路径空间和技术分裂

ReSTIR DI 的随机变量通常是“当前接收点到灯光的一个灯样本”，而 ReSTIR PT 的随机变量是不同长度、不同采样技术产生的整条路径。对长度 `d` 的路径 `x̄=[x₀,…,x_d]`，最终顶点可能由 NEE 连接到光源，也可能由 BSDF continuation 得到。两种技术的路径空间 PDF、MIS 权重和可移位方式不同，因此需要在路径样本上携带 technique index，并在 PSS 中拆分 integrand：

```text
F(ū) = ω_t(x̄) · f(x̄) / p_t(x̄)。
```

这也是不能把 ReSTIR GI 的 Jacobian 直接套到 DSMEngine 解析灯 DI 的原因：DI 的样本域和路径顶点测度不同。

### 4.2 hybrid shift 的工作方式

如果相邻像素都粗糙，梯度域式重连接可以直接把前缀保持不变并在某顶点重连；镜面顶点会让小屏幕位移造成巨大的方向/PDF 变化。原始 ReSTIR PT 因而预先寻找第一个满足

```text
min(α_x(k-1), α_xk) ≥ α_min
and
||x_k - x_(k-1)|| ≥ d_min
```

的顶点对，之前的顶点使用相同随机数 replay，之后从 `y_(k-1)` 连接到 `x_k`。这是一个预计算重连接位置的 GPU 友好折衷：不用存整条 base path，也不用每次搜索路径长度；但距离与双顶点粗糙度阈值会随观察尺度、材质和几何变化而失配。

### 4.3 PSS Jacobian 的含义

论文给出的 hybrid shift Jacobian（发生重连接时）是：

```text
|∂T/∂ū| = [ p^y_(k-1)(ω′_(k-1)) G(y_(k-1)→x_k) p^y_k(ω_k) ]
             / [ p^x_(k-1)(ω_(k-1)) G(x_(k-1)→x_k) p^x_k(ω_k) ].       (2)
```

第一比值是重连接顶点的面积密度变化，第二比值是下一方向的立体角密度变化。`G(a→b)=cosθ/||a-b||²` 是单侧几何项；当 `k=d`，末端方向 PDF 取 1。若没有重连接，Jacobian 为 1。理想情况是目标 PDF 的变化抵消 Jacobian，使移位样本仍有相近的重要性；实际中错误重连接会把样本推入高能量峰，形成 firefly。

## 5. 贡献一：互惠配对空间复用

### 5.1 朴素空间复用为什么要算“两次移位”

设当前像素为 A，邻居为 B。为了让 B 的样本成为 A 域中的候选，需要算 `T_(B→A)`；为了计算 GRIS 的 pairwise MIS 权重，又要评估 A 的样本在 B 域中的对应量 `T_(A→B)`。对 `N` 个随机邻居，朴素做法大约需要 `2N` 次 shift mapping。

论文观察到移位具有互惠关系：A 从 B 复用时，已经计算了 A↔B 这一对所需的两条映射；若随后让 B 从 A 复用，同一对映射可以直接重用。因此把像素组织成自逆（self-inverting）的配对纹理：A 的纹理偏移指向 B，B 的偏移指向 A。空间 pass 可分为“先把所有路径移位到配对邻居”的 pre-pass 和“再做 reservoir 合并”的 resampling pass。

### 5.2 为什么选择高斯而不是均匀圆盘

原始 ReSTIR PT 以固定半径 `R` 的圆盘随机取邻居。PT Enhanced 的配对纹理天然产生近似二维高斯偏移，因此令两者平均半径相同：

```text
E[distance]_Gaussian = σ √(π/2)
E[distance]_disk     = 2R/3

σ = √(8/(9π)) R。
```

在 `R=30` 时 `σ≈16.0`。高斯分布在中心附近更密集，通常更容易通过深度/法线兼容性测试，所以论文观察到 FLIP 也略有改善；代价是远邻覆盖概率不同，不能把它当作与均匀圆盘完全相同的 proposal。

### 5.3 纹理构造与统计性质

论文以 `254×254` 等偶数尺寸纹理填充连续 link index，再反复执行平铺的 `2×2` 随机洗牌。每次洗牌让链接位置近似作标准差 `σ/√2` 的随机移动；两个端点的差合成标准差 `σ`：

```text
n_σ = floor(σ²/2 + 1.46σ⁻¹ + 1.76σ⁻² + 0.656σ⁻³ + 0.5)
    ≈ floor(σ²/2 + 0.5)。
```

纹理边界通过 wrap 后的短路径处理：差值大于半宽度则减宽度，小于负半宽度则加宽度。对于论文的 `254×254` 纹理，可用两个有符号 8 位通道（一个 16 位 texel）存储 `Δx,Δy∈[-127,127]`；若使用 `256×256`，则需要 16 位通道表示 `128`、`-128` 及更大的差值。查找配对端点时使用 `N/2×2` index table：第一次并行写入 `(linkIndex,0)`，第二个端点写入 `(linkIndex,1)`。第一次写入存在数据竞争，但每个 link 恰好需要两个位置，因而可用无锁的“谁先写谁占槽”策略。

### 5.4 去结构化与逐帧随机化

单张自逆纹理在固定帧中重复使用会形成可见周期。论文为不同邻居使用不同尺寸（示例 `254、230、210`），并逐帧随机翻转、镜像、转置和偏移。多尺寸叠加可避免 Bekaert et al. 的 `16×16` N-rook 块边界和低样本结构伪影。这个思想也可以用于经典路径复用，而不只用于 ReSTIR。

### 5.5 成本与风险判断

论文实测空间复用平均加速 `1.63×`，不是理论上的 `2×`：pre-pass、配对纹理地址计算和第二 pass 都有开销。它的收益成立的前提是 shift mapping 占空间 pass 的主导成本；如果目标引擎的邻居筛选、资源同步或光照重算更贵，实际收益会小得多。

配对会改变邻居联合分布。工程实现必须确认：

- 每条无向配对边只调度一次，同时产出 A→B 与 B→A 两个有向候选；避免整对被重复调度和累计；
- source 与 destination reservoir 分离，不能在 UAV 原地覆盖导致顺序依赖；
- 纹理逐帧随机化且多尺寸周期不重合；
- 邻居拒绝后，`M`/`c` 与 `wSum` 的计数保持一致；
- 需要对比“同平均距离的均匀圆盘”而不是只看相同 `σ`。

## 6. 贡献二：双射线足迹重连接判据

### 6.1 从“Jacobian 接近 1”到两个密度比

论文在 `p̄_j(T(ū))≈p̄_i(ū)` 的近似下，把目标变换质量的主要风险归因于 `|∂T/∂ū|`。由式（2）可拆成：

```text
A = p^y_(k-1)(ω′_(k-1)) G(y_(k-1)→x_k)
    / [p^x_(k-1)(ω_(k-1)) G(x_(k-1)→x_k)]     (area-density ratio)

B = p^y_k(ω_k) / p^x_k(ω_k)                  (solid-angle ratio)
```

好的重连接应使 `A≈1`、`B≈1`。原始“世界距离 + 两顶点粗糙度”是间接地控制这两个比值；它没有随像素投影尺度、光源距离和局部材质自动缩放。

### 6.2 足迹的几何解释

面积 PDF 的倒数是一个样本代表的表面积，也就是 ray footprint。更长的连接距离或更粗糙的 BSDF 往往产生更大的足迹；在足迹内密度变化较平滑时，从邻居移位过来的样本仍具有相近权重。反之，低粗糙度微表面或几何尖角会制造窄密度峰，极小的位置变化就会使 `A` 巨变。

论文定义正向和反向两个足迹：

```text
R_forward  = [p^x_(k-1)(ω_(k-1)) G(x_(k-1)→x_k)]⁻¹
R_inverse  = [p^x_k(ω_k) G(x_k→x_(k-1))]⁻¹。
```

`R_forward` 约束重连接顶点 `x_k` 的面积密度变化；`R_inverse` 通过 PDF reciprocity 约束改变入射方向后 `x_k` 处 BSDF 采样 PDF 的变化。仅有前者会漏掉低粗糙度 glossy 材质的方向密度尖峰。

### 6.3 论文的双足迹判据

可实施形式是：

```text
min(R_forward, R_inverse)
  ≥ (c/100) · ||x₀−x₁||²
    / ( ⟨n_x1, d_x1x0⟩ / (4π) )。                     (5)
```

右侧是主射线足迹的常数倍；`c=0.02` 是论文跨场景经验值，即实际乘子 `c/100=0.0002`。同时在 `x_(k-1)` 使用单顶点粗糙度阈值 `α_(x_(k-1))≥α_min`。注意它不是把两个顶点的粗糙度取最小值，而是只检查尚未重连的前一顶点；因此允许在足迹足够大的远距离 glossy 终点重连。

### 6.4 充分条件的推导直觉

论文要求两种相对密度变化均小于 `ε`：

```text
|A−1| < ε，|B−1| < ε。                                  (6a, 6b)
```

于是 `(1−ε)² < |∂T/∂ū| < (1+ε)²`。假设随机 replay 保持局部面积密度，并令 `Δx_k=y_k−x_k` 被主射线足迹的 `c₁` 倍限制：

```text
||Δx_k|| < c₁ R̄_x^pri,
R̄_x^pri = √[ ||x₀−x₁||² / (⟨n_x1,d_x1x0⟩/(4π)) ]。
```

若半径 `√[c₂/(p^x_(k-1)G)]` 的足迹内相对密度变化受 `ε` 限制，则足迹大于 `(c₁²/c₂)(R̄_x^pri)²` 是式（6a）的充分条件。论文把未知常数折叠为 `c`，让 `c` 越大越保守。对式（6b），利用 glossy BSDF 的近似 PDF reciprocity，把反向射线足迹作为同样的充分条件。

### 6.5 适用边界

论文的 reciprocity 假设对微表面 NDF 采样是精确的，对 VNDF 采样是近似的；对 diffuse 或 emissive 的 `x_k`，重连接不改变 `p^x_k(ω_k)`，可以跳过 inverse footprint 测试。环境光重连接的 `Δx_k` 可能无界，因此仍保留前一顶点粗糙度阈值。视差、曲率、极低粗糙度以及薄几何都可能让足迹局部平滑假设失效，工程上应记录每条拒绝原因。

### 6.6 相对原判据的真正改进

原判据的距离阈值以世界单位固定，镜头拉远时可能过于保守，拉近时又过于激进；粗糙度阈值把材质和距离耦合成需手调的常数。新判据以主光线足迹归一化距离，并显式观察正向/反向 PDF，因而能随屏幕尺度、法线、距离和 BSDF 同时变化。图 10–12 的证据是跨观察距离、材质粗糙度和光照变化，重采样质量更一致；不是“任何场景都保证 Jacobian=1”。

## 7. 贡献三：duplication map 与可解释的有偏去相关

### 7.1 相关团块的来源

低概率高能量 initial sample 本来就是 Monte Carlo 重尾的一部分；不完美 shift 又可能把普通样本人为映射成高权重样本。空间复用把它复制到邻域，时间复用再以较高 `c` 保存很多帧，最终形成圆盘、条纹或随相机运动拖曳的 correlation blob。去噪器常把这种跨像素、跨帧一致的结构误判为真实信号。

### 7.2 duplication score 的定义

reservoir 已保存随机种子以支持 replay。若两个 reservoir 的路径来自同一 initial candidate，它们共享种子。每帧结束后，对每个像素在 `17×17` 邻域统计相同种子的 reservoir 数，并除以 288：

```text
D = duplicateCount / 288，D∈[0,1]。
```

时间重投影后，在历史 reservoir 的位置读取 `D`，再设：

```text
c_Cap = lerp(c_DefaultCap, c_minCap, D^α)。
```

论文默认 `c_DefaultCap=20`、`c_minCap=1`、`α=0.1`。因为 `α` 很小，只要 `D` 略大于零，`D^α` 就会迅速升高，形成对早期相关性的敏感抑制。

### 7.3 偏差从哪里来

如果 `c_Cap` 只由预先固定的 pass 参数决定，可以把它纳入一致的 GRIS/MIS 设计；现在它由“这个具体样本是否被复制”决定，因而候选技术权重的单位分割不再成立。直观地说，同一个高能量样本被观察到复制后，会在未来被系统性降权；其负误差不会由其他样本严格补偿，所以总体呈能量损失。

论文的 Kitchen 压力场景报告平均绝对相对偏差 `mean(|bias|/reference)=3.25%`，且 glossy 表面更明显。长时间积累时，有偏版本的误差曲线趋于平台；关闭去相关时，无偏性恢复。默认版本在实时窗口早期可能有更低 MSE，因为它先消除了扩散的 outlier。

### 7.4 与 RTXDI boiling filter 的差别

RTXDI boiling filter 在 warp 内求平均权重 `w̄`，清除 `w_j > a w̄` 的 reservoir，`a=-9+10/s`。它按权重异常检测，不关心样本谱系；duplication map 按“同一来源的复制比例”检测，不必把一个孤立但合法的大权重立即杀掉。论文图 9 显示 boiling filter 为清除大多数伪影会产生更强的暗化。两者都属于产品化 bias control，不应出现在 unbiased reference 路径中。

### 7.5 可改进点

17×17 全邻域统计若朴素实现会很贵。可尝试同种子二值图的 separable box filter、分块 shared-memory histogram、低分辨率 duplication map，或多尺度近似；但必须比较 false positive。随机种子还必须足够稳定地区分“同一 initial lineage”，不能逐 pass 重写或压缩到高碰撞率。建议输出 `D`、修改前后 `c_Cap`、seed hash 和受影响能量，单独量化 bias。

## 8. 统一直接光与间接光：不是简单“共用一个结构体”

原始 ReSTIR PT 跳过 `d=2` 的直接光路径，假定另有 ReSTIR DI。PT Enhanced 在路径树生成时从 `x₁` 增加一条 NEE 候选，让 initial resampling 可以选中 `d=2` 或 `d≥3` 的路径。于是 reservoir 的随机变量域扩为“完整路径空间 + technique index”。这带来两种收益：

- 删除独立 ReSTIR DI 的 candidate、temporal、spatial、history 和 storage；
- 直接光高光也能使用 PT 的 hybrid shift 与 path MIS，而不是只按灯样本重连。

但统一成立必须满足以下数学契约：

1. `d=2` 与更长路径的 source PDF 都要写成同一 PSS 下的密度。
2. NEE 与 BSDF-hit-light 技术使用 path MIS `ω_t` 分割相同路径。
3. GRIS 的 resampling MIS `m_i` 仍处理不同像素/帧 proposal；不能拿 `ω_t` 代替。
4. 发生 shift 后，path length 与 technique index 的支撑必须可逆或由 canonical sample 覆盖。
5. visibility 只对 RIS 选中的直接光候选测试时，`p̂` 必须明确排除 visibility。

论文把具体 PSS source PDF / MIS 修改放在 supplemental，因此复现者不能只把 DI sample 塞进旧 reservoir 就宣称等价。最可靠的验证是把 `d=2`、`d=3+` 分成 debug channel，分别做归一化、选中频率和长期平均 reference。

多灯场景中，论文在主命中点从预采样 light tile 取 32 个 NEE 候选；每帧预计算 128 个、每个 1024 灯的 tile，每个 `8×8` 屏幕 tile 选一个。深层反弹使用 `max(1,32/B²)` 候选。这个布局主要服务缓存相干性与固定预算，不等于每个灯的真实 PMF 自动正确；tile 构造、灯功率分布和条件 PDF 必须一起记录。

## 9. GPU 优化链为何能带来 2–3 倍加速

### 9.1 条件移动与 reservoir 压缩

reservoir 更新、路径类型和重连接条件包含大量分支。论文用 conditional move 替换多数分支并代数化简，在不改变 sampler 行为的前提下降低 warp divergence；同时删除字段、对选定量有损压缩，将单 reservoir 从 88 B 降到 64 B。加上统一 DI/GI，双历史缓冲从 `2×(88+16)` B/px 降为 `2×64` B/px；1080p 报告从 431 MB 降到 265 MB。

压缩不是无风险优化：随机种子、重连接顶点、方向/PDF 和 contribution weight 的量化误差可能改变 shift 可逆性和极端权重。应先用 float32 reference 建范围直方图，再决定每字段格式；不要仅因论文达到 64 B 就照抄位宽。

### 9.2 replay stream compaction

许多 pixel-neighbor pair 不需要随机重放；若每个屏幕 pixel thread 内直接执行，warp 仍等待少数长路径。论文将工作重排为 pair stream，先 compact 掉无需 replay 的 pair，再对紧凑列表并行。收益来自同时减少 active warp 数和 warp 内路径长度差异。代价包括计数、prefix sum/allocator、间接 dispatch 和临时 buffer；只有 replay 稀疏时才划算。

### 9.3 强制 NEE light reconnect

replay 到 NEE light vertex 时重新做 light sampling 很贵，而且 power sampling 常再次选到同一灯。若此前没有找到重连接，论文强制连接到原光源顶点，省掉 replay 内的选灯。该近似可能增加方差，但 glossy case 的 path MIS 常已将其降权。实现必须保留 technique PDF，不能把“常选同一灯”误当成 PDF=1。

### 9.4 Russian roulette 的位置变化

论文只在 initial path sampling 用 Russian roulette，在 replay 中移除。等价地，roulette 不再属于被 shift 的 PSS 维度，而是 initial proposal 外部的路径终止随机变量；对应 survival probability 必须进入 initial source PDF。这样不会在 replay 中随机杀掉本来有效的邻居路径，也消除了 roulette 导致的 shift failure。代价是 initial 样本更噪，但时空复用通常能吸收。

## 10. 色彩噪声与去遮挡：sampling history 不等于 lighting history

### 10.1 向量重采样权重

若 `p̂=|F|` 是 luminance 等标量，reservoir 只按亮度选索引，RGB 色度残差仍有高方差。论文在 spatial reuse 中已经评估 `F(Y_i)`，于是同时累积：

```text
scalar w_i = m_i(Y_i) · p̂(Y_i) · W_Xi · |J_i|     (用于选择/未来 reuse)

vector w_i = m_i(Y_i) · F(Y_i) · W_Xi · |J_i|     (用于当前 shading)
```

最终 shading 使用 `Σ vector w_i`，相当于对随机“选中哪个候选索引”做 Rao-Blackwell 式边缘化；只要空间邻居的色度噪声不是同源相关，色彩会自然平均。它并没有把 RGB reservoir 变成三套独立 sampling distribution，也不允许用 vector weight 驱动抽样。

### 10.2 双运动向量

新显露区域没有合法 temporal history。dual motion vectors 用遮挡/被遮挡表面相对运动假设提供备选重投影，可把附近历史 reservoir 带入 disocclusion。普通色彩 history 直接复制会产生图案粘贴；ReSTIR PT 仍在当前域重算并无偏重采样路径，因而论文认为不出现同类 pattern cloning。这个结论依赖 shift、surface validation 和 current shading 正确，不意味着可无条件接收替代运动向量。

### 10.3 与去噪器的边界

reservoir history 改善采样分布；vector marginalization 改善当前 estimator；dual motion vector 改善历史候选可用性；最终时域/空域去噪又是另一层 reconstruction。DSMEngine 若还有 TAA，应分别开关四层，否则 bias、ghosting 和响应迟滞会混在一起。

## 11. 端到端算法伪代码（根据论文重构，非原文代码）

### 11.1 离线：生成自逆配对纹理

```text
function BuildPairingTexture(width, sigma, seed):
    assert width is even and sigma >= 0.8
    tex = MakeConsecutivePairLabels(width, width)
    n = floor(sigma*sigma/2
              + 1.46/sigma + 1.76/(sigma*sigma)
              + 0.656/(sigma*sigma*sigma) + 0.5)

    for iteration in 0 .. n-1:
        offset = (iteration & 1) ? (1, 1) : (0, 0)
        for each tiled 2x2 block in parallel, with wrap(offset):
            RandomPermuteFourLabels(block, Hash(seed, iteration, block))

    endpoints[label][0..1] = ParallelLocateBothOccurrences(tex)
    for each pixel p:
        q = OtherEndpoint(endpoints[tex[p]], p)
        delta = WrapToShortestTiledOffset(q - p, width)
        out[p] = PackSigned16x2(delta)
    return out
```

### 11.2 每帧主流程

```text
function RenderFrame(frame):
    surfaces = GBufferAndMotion()

    initial = InitialPathTreeRIS(surfaces)
        // include x1 NEE so d=2 direct paths and d>=3 paths share path domain
        // Russian roulette only here; survival probability enters source PDF

    temporal = TemporalGRIS(initial, historyReservoir, historyDuplication)
        // reproject; validate surface/path support; use adaptive cCap in biased mode

    pairWork = BuildPixelNeighborPairs(pairingTextures, frameTransform)
    shifted = ShiftPrepass(temporal, pairWork)
        // compute A->B and B->A once per undirected pair
    replayList = CompactPairsRequiringReplay(shifted)
    ReplayAndReconnect(replayList, dualFootprintCriterion)
    spatial = PairwiseGRIS(temporal, shifted)

    color = ShadeWithVectorWeightSum(spatial)
    duplication = CountSameSeedIn17x17(spatial)

    historyReservoir = spatial
    historyDuplication = duplication
    return Reconstruct(color, surfaces)
```

### 11.3 时间 GRIS 与 duplication control

```text
function TemporalGRIS(current, history, duplicationMap):
    H = ReprojectAndValidate(history)
    if invalid(H): return current

    if EnableBiasedDecorrelation:
        D = duplicationMap[H.pixel]
        cap = lerp(DefaultCap=20, MinCap=1, pow(D, alpha=0.1))
    else:
        cap = DefaultCap

    H.confidence = min(H.confidence, cap)
    return GRISMerge(canonical=current, candidate=Shift(H))
```

### 11.4 选择重连接顶点

```text
function FindReconnectVertex(basePath):
    primaryArea = LengthSquared(x0-x1)
                  / (Dot(n1, Direction(x1,x0)) / (4*pi))
    threshold = (c/100) * primaryArea       // c = 0.02 paper default

    for k in 2 .. pathLength:
        if Roughness(x[k-1]) < alphaMin: continue

        forward = 1 / (PdfAt(x[k-1], omega[k-1]) * G(x[k-1],x[k]))
        if x[k] is diffuse or emissive:
            inverse = +INF
        else:
            inverse = 1 / (PdfAt(x[k],omega[k]) * G(x[k],x[k-1]))

        if min(forward, inverse) >= threshold:
            return k
    return noReconnect
```

数值实现应对 `cosθ≤0`、距离平方接近零、PDF 为零/非有限、delta lobe、environment endpoint 分支显式处理；不要用一个很小 epsilon 悄悄把非法路径变成巨大足迹。

## 12. 与相关工作的逐项比较

| 维度 | ReSTIR DI (2020) | ReSTIR GI (2021) | ReSTIR PT (2022) | ReSTIR PT Enhanced (2026) | 现代产品式实现 |
|---|---|---|---|---|---|
| 样本域 | 当前点的灯样本 | 二次顶点/间接路径尾部 | PSS 中一般路径 + technique | 同 PT，加入 d=2 统一 DI | 常按产品拆成灯、方向、cache hit 等域 |
| 复用范围 | 直接光 | 漫反射/中光泽 GI | 多反弹、镜面、焦散 | 同上但更快、更稳 | 常牺牲通用性换固定预算 |
| shift | 接收点重评估/重连灯 | reconnection | random replay + reconnection hybrid | footprint 控制 hybrid | 可能只做 cheap reconnection 或近似 |
| Jacobian | 灯样本域常无需 PT Jacobian | 需要 GI 映射 Jacobian | PSS hybrid Jacobian | 同式（2），判据更合理 | 常有 clamp、简化或域专用公式 |
| DI/GI 组织 | DI reservoir | GI reservoir | PT 排除 DI，另跑 DI | 单 reservoir 覆盖完整路径空间 | Lumen/MegaLights 等通常仍按产品子系统分开 |
| 空间邻居 | 随机圆盘等 | 随机邻居 | 每邻居双向 shift | 自逆配对，摊销双向 shift | 常用 blue-noise、tile、wave 操作 |
| 相关性控制 | M-cap、boiling 等 | history cap | cap + 后续 CRIS/MCMC 等 | duplication-aware cCap | 感知 clamp、history reset、hash/visibility guide |
| 色彩 | 标量 target 后着色 | 同类问题 | RGB integrand / scalar selection | vector weight marginalization | 常 diffuse/specular 分离、clamp、denoise |
| unbiasedness | 有无偏与偏置变体 | 取决于 reuse/MIS | 理论 GRIS 无偏条件 | 去相关关闭时保持；默认有小偏差 | 通常明确偏置换稳定和预算 |
| 核心瓶颈 | 候选与 visibility | 二次命中和 shading | replay、shift、分歧、88B reservoir | 降到 64B，compaction、paired reuse | AS、带宽、wave occupancy、去噪 |

### 12.1 相对 Conditional RIS / MCMC mutation / Area ReSTIR

- Conditional RIS 通过只复用路径部分、保持前缀独立来降低某些相关性，理论更细但成本高；duplication map 更便宜、覆盖多种相关团块，却以 bias 为代价。
- MCMC mutations 通过突变已有样本恢复多样性，能处理 sample impoverishment，但要额外评估/接受步骤；duplication map 不创造新样本，只限制旧谱系扩散。
- Area ReSTIR 把像素滤波/散焦/抗锯齿考虑进采样域，对高频运动问题更自然；PT Enhanced 仍以像素/路径复用为主，论文把二者统一列为未来方向。

### 12.2 相对 UE Lumen / MegaLights 的现实定位

Lumen ReSTIR Gather 更接近 ReSTIR GI：采样一次间接反弹方向，依赖 surface cache/hit lighting；它不是本文完整路径空间 PT。MegaLights 面向大量直接光，使用向量化 WRS、历史可见性引导、压缩和时空去噪，但不等价于经典“历史 reservoir 作为 proposal 的时空 ReSTIR DI”。PT Enhanced 的价值是展示更通用路径 estimator 如何被压到更可生产的范围，而不是给出可逐行复制的 UE 模块。

## 13. 论文结果的数值解读

### 13.1 性能链

| 版本 | 总帧 ms | 相对基线 | 解释 |
|---|---:|---:|---|
| Baseline | 35.73 | 1.00× | Lin et al. 2022 公开实现 |
| + 代码微优化 | 32.98 | 1.08× | 分支/计算简化 |
| + 强制 NEE 重连接 | 29.75 | 1.20× | replay 删除光源采样 |
| + replay compaction | 26.81 | 1.33× | 只执行需要 replay 的 pair |
| + paired spatial reuse | 25.02 | 1.43× | 空间 shift 摊销 |
| + Russian roulette | 16.52 | 2.16× | 初始路径平均长度下降 |
| + unify DI & GI | 13.04 | 2.74× | 删除独立 DI pass/存储 |
| + new thresholds | 14.51 | 2.46× | 判据额外计算 |
| + all improvements | 15.53 | 2.30× | 去相关、色彩、去遮挡质量项 |

从 35.73 到 13.04 ms 的累计收益不能简单相加：后续步骤改变了前面阶段的工作量和缓存行为。尤其 Russian roulette 使 initial sampling 从 11.79 降至 5.21 ms，unify DI/GI 又把 “ReSTIR DI and others” 从 5.24 降到 1.00 ms；这解释了为何它们比单纯的 shader 微优化更关键。

### 13.2 GPU occupancy 与显存

Opera House profiling 显示：SM occupancy `22.4%→31.1%`，active threads/warp `15.3→19.9`，warp latency `347k→241k` cycles；加 roulette 后分别为 `34.9%`、`20.6`、`82k`。这些是特定 RTX 5880 Ada / Falcor 实现的硬件指标，不应直接当作 DSMEngine 的保证。

存储由每 pixel 两套 `(88+16)` B reservoir（PT + DI）降为两套 `64` B：

```text
baseline ≈ 2 × 104 B × 1920 × 1080 ≈ 431 MiB
enhanced ≈ 2 × 64 B × 1920 × 1080 ≈ 265 MiB。
```

这是约 `38.5%` 的显存下降；还未计 duplication map、pairing texture、replay compaction buffer、motion/depth history 和 denoiser history。产品预算不能只按表中 reservoir 计算。

### 13.3 FLIP、MSE 与 bias 的读法

论文同时使用 FLIP（更贴近 HDR 感知差异）和 MSE。FLIP 对孤立 firefly 不如 MSE 敏感，却仍能看出大区域暗化。默认有偏版本短期可能优于无偏版本，因为它阻止 outlier 扩散；若做 1024 次独立运行平均，系统性暗化就会显现。正确报告应至少给：

- 单帧/短时窗口的感知误差；
- 长期多 seed 平均的 signed bias 与 absolute relative bias；
- 固定时间预算下的 MSE/FLIP 收敛曲线；
- 开关 duplication map 前后的 variance 和能量变化。

## 14. DSMEngine 的边界对齐

已有 DSMEngine 审阅显示：当前是 deferred renderer，有 GBuffer、motion vector、tiled local-light list、DXR 底层抽象和 TAA，但 local light 上限约 128，renderer 级 BLAS/TLAS 生命周期、稳定 light identity 与 ReSTIR history 契约仍需补齐。因此不建议第一步直接移植完整 PT Enhanced，而应把论文思想拆成可证伪的层次。

### 14.1 第一版外部契约

- 仅 deferred opaque surface；
- 仅 point/spot local lights；directional light 保留现有 raster shadow path；
- 不含透明、alpha mask、area light、emissive mesh、环境光和 GI；
- 每个最终 selected light 追踪一条 boolean visibility ray；
- DXR 不可用时完整回退现有 DeferredLighting；
- 提供 unshadowed 和 visible brute-force reference；
- full-resolution reservoir、float32 字段、无 clamp 的 correctness mode。

当前 128 灯规模可能让 tiled raster 比 ReSTIR 更便宜，尤其没有昂贵阴影时。第一版价值目标应是“固定少量 RT 阴影光线下的质量/扩展性”，而不是预设一定比 raster 快。

### 14.2 四层模块

```text
Scene RT Layer
  BLAS cache / instances / TLAS build-refit / visibility query
        ↓
Light Sampling Layer
  stable light table / tile candidates / source PMF / pHat
        ↓
ReSTIR DI Layer
  initial → temporal → spatial → visibility → shade
        ↓
Reconstruction Layer
  lighting history → moments/confidence → spatial filter → TAA
```

建议的职责边界（路径为建议，不代表现有文件已经存在）：

```text
Runtime/Render/RayTracing/RayTracingScene.*
Runtime/Render/DeferredRenderer/ReSTIRDirectLightingPass.*
Shaders/DeferredShader/ReSTIR/{Initial,Temporal,Spatial,Visibility,Shade}.usf
```

采样器只输出“从当前表面到 selected light 的可见性查询”，不绑定某个唯一 RT backend；这样可以同时保留 inline RayQuery spike 和 DispatchRays fallback。

### 14.3 稳定 light identity

CPU culling 会重排 packed light index，reservoir 不能只保存 index。每个 scene light 应有跨帧稳定的 32 位 `stableLightId`；光强、位置、方向、范围、形状或采样参数的语义变化增加 `revision`。GPU 每帧上传 dense light data、stable→dense 映射、active/revision 和 tile candidate list。点/聚光灯 reservoir 只保存 stable ID 时，灯移动通常可在当前位置重算 target；若以后保存 area-light UV，则形状/拓扑变化必须使 history 失效。

### 14.4 建议 reservoir layout

```text
struct ReservoirSample {
    uint stableLightId;
    uint lightRevision;
    uint flags;
    uint reserved;             // future shape/technique tag
};

struct ReservoirStats {
    float wSum;
    float M;                   // first implementation: float32/uint32
    float W;                   // wSum / (M * pHat(selected))
    float selectedPHat;        // debug only; recompute on reuse/shade
};
```

不要缓存邻居最终 RGB 作为 truth；每次 reuse/shade 都用当前 surface、当前 light table 重新计算 unshadowed RGB 与 `pHat`。若 `pHat` 采用 luminance，`W` 只负责 target/PDF 归一化，最终不能再除一次 luminance。

## 15. DSMEngine 的逐 pass 实现建议

### Pass 0：reference

先实现两种 debug reference：

1. unshadowed：遍历 tile 内所有 local light，计算完整 RGB，不追踪 RT；
2. visible：每灯追踪一条 shadow query。

若 ReSTIR 与 unshadowed reference 的多帧平均不一致，不要先调 denoiser、normal bias 或 TAA。

### Pass 1：initial RIS

正确性优先时，从 tile candidate 中均匀抽 `M₀` 个候选：

```text
pSource(i) = 1 / lightCount
pHat_i    = luminance(max(unshadowedDiffuse_i + unshadowedSpecular_i, 0))
w_i       = pHat_i / pSource(i)
```

流式 weighted reservoir sampling 后计算 `W`。target 不含 visibility，因此 initial 不应为每个候选发阴影线。无灯或 `pHat=0` 时写空 reservoir，并显式处理 `wSum=0`。

若枚举最多 128 盏灯并按 `pHat` 直接选，选中概率为 `q_i=pHat_i/ΣpHat`；若把它作为一个 source draw 的 reservoir，应设 `M=1`、`wSum=ΣpHat`、`W=1/q_i`。不要把 `M` 误设为 lightCount，除非完整采用对应的 deterministic-candidate GRIS 推导。

### Pass 2：temporal GRIS

用明确定义的 `previousUV = currentUV - motionVector`，验证历史 UV、depth、normal、stable ID/revision、scene epoch、reservoir resolution、jitter phase 和 camera-cut 标志。先做 unbiased cap mode，再可选 duplication-aware cap：

```text
H.M     = min(H.M, cap)
pHat_H  = EvaluateTarget(currentSurface, Resolve(H.lightId))
w_H     = pHat_H * H.W * H.M
Merge(R, H.sample, w_H, H.M)
FinalizeW(R, EvaluateTarget(currentSurface, R.sample))
```

对 DSMEngine 解析灯 DI 不使用 Lumen GI Jacobian。灯移动后按当前 light data 重算 target；遮挡移动不能依赖历史 visibility 判真，最终仍需 trace。

### Pass 3：spatial GRIS

第一版一轮、4 个邻居、full-res、source/destination ping-pong。若采用论文 pairing texture，先执行配对 shift pre-pass，再 compact 需要 replay 的 pair，最后一次性合并。邻居权重的核心形式是：

```text
w_neighbor = pHat_current(neighbor.sample)
              * neighbor.W * neighbor.M
```

不加 GI Jacobian。深度/法线/plane-distance rejection 必须在 merge 前完成，并记录 rejection reason；薄墙两侧应有专门回归场景。

### Pass 4：visibility 与 shade

对最终 reservoir 解析当前 stable light，重算完整 RGB direct contribution，然后只为 selected light 发 visibility query：

```text
origin    = worldPosition + geometricNormal * normalBias
direction = normalize(lightPosition - origin)
TMin      = epsilon
TMax      = lightDistance - endpointEpsilon

directRGB = visible
          ? FullUnshadowedRGB(currentSurface, selectedLight) * R.W
          : 0。
```

第一版只纳入静态、不透明、双面策略明确的实例；可用 accept-first-hit/end-search 的最小 payload。若 inline RayQuery 尚未在主 renderer 验证，使用最小 DXR raygen/miss/any-hit fallback。不要让历史 visibility 代替当前遮挡测试，也不要把 `R.W` 再除以 `pHat` 或 luminance。

### Pass 5：重建与 TAA 协调

先 full-resolution 验证 sampling，再探索 half-resolution。低分辨率 reservoir 上采样时，在 full-res 当前表面重算 BSDF，按 depth、normal、plane distance 和 roughness 加权；不要直接复制邻居最终颜色。diffuse/specular 可分开输出 moments、variance、confidence。

ReSTIR lighting history 与全画面 TAA 是两套 history。建议提供四种诊断模式：

```text
0. reservoir history off + lighting history off + TAA off
1. reservoir history on  + lighting history off + TAA off
2. reservoir history on  + lighting history on  + TAA off
3. reservoir history on  + lighting history on  + TAA on
```

这样可以区分 sample correlation、lighting filter ghosting 和最终 TAA 拖影。

### Pass 6：按收益逐项吸收产品优化

只有 correctness mode 与 reference 稳定后，才按以下顺序试验：

1. clustered candidate / alias table / power proposal；
2. paired spatial reuse；
3. stream compaction；
4. initial-only roulette；
5. reservoir packing；
6. duplication-aware `c_Cap`；
7. vector color weights、dual motion vector、lighting denoise；
8. half-resolution 和多 slot。

每项都记录 GPU 时间、rays/pixel、显存、variance、signed bias 和视觉回归；不能因为 UE 或论文采用某项就默认适合 128 灯的 DSMEngine。

## 16. 分阶段实施路线与完成标准

### 阶段 A：契约与诊断基础

定义 local-light 替换边界，确保现有 DeferredLighting 不重复贡献；增加 stable ID/revision、tile mask 清零验证、camera cut/resize/scene epoch reset；建立 selected light、source PDF、`pHat`、`wSum/M/W`、history validity、visibility/rejection debug view。完成标准：灯光重排、相机运动和 resize 后 stable ID 仍指向同一语义灯。

### 阶段 B：renderer 级 RT 场景

完成静态 mesh BLAS cache、instance transform、TLAS build/refit、资源 barrier/fence、normal bias、TMin/TMax、双面与 RT fallback。完成标准：简单遮挡场景与 raster shadow reference 对齐，无自阴影、穿透或上一帧 AS 延迟。

### 阶段 C：单帧 initial-only

均匀 source、float32 reservoir、full-res、无 history、无 spatial reuse。用 unshadowed/visible 两个 reference 做 0/1/128 灯、极端功率、彩色灯的多 seed 平均。完成标准：平均 RGB 接近 reference，无 NaN/Inf，selected histogram 符合 PMF。

### 阶段 D：时间复用

加入 history depth/normal/reservoir、motion reprojection、stable resolve、`M` cap、camera cut 和 disocclusion。完成标准：静态噪声下降；移动相机、物体、灯后有限帧内恢复；reset 首帧等价 initial-only。

### 阶段 E：一轮空间复用

先随机邻居，再 paired texture；使用 ping-pong、depth/normal/plane rejection、可调半径/邻居数。完成标准：平面方差下降，薄墙和轮廓无明显跨面漏光，长期 bias 有报告。

### 阶段 F：产品折中

加入 vector color weight、dual motion vector、lighting history、moments/confidence、compaction、packing、half-res；每个近似保留开关。完成标准：目标 GPU/场景中，同画质优于 raster shadow、全灯 RT reference 和 initial-only 基线。

### 阶段 G：扩展路径域

只有前述契约稳定后才考虑 area/sphere/disk light、emissive mesh、environment proposal、alpha mask、skinned BLAS、多 reservoir 和 GI。GI 不是解析灯 DI 的“小扩展”：它需要路径样本、二次点着色、Jacobian、世界空间可见性和不同的去噪契约，应单独立项。

## 17. 验证矩阵

| 场景 | 主要验证 | 失败征兆 |
|---|---|---|
| 0 / 1 灯 | 空 reservoir、权重基线 | NaN、无灯却有能量 |
| 128 等强灯 | source PMF、selected histogram | 某些 ID 永不被选 |
| 一盏极亮 + 多暗灯 | 重尾、wSum、clamp | firefly、系统丢能量 |
| 红/绿/蓝灯 | luminance target → RGB | 色偏、重复除 target |
| 灯添加/删除/重排 | stable ID/revision | 历史变成另一盏灯 |
| 移动灯/遮挡物 | current target/visibility | 阴影滞留、假可见 |
| 薄墙两侧强灯 | spatial rejection | 跨墙漏光 |
| camera cut/resize | epoch reset | 满屏旧 reservoir |
| disocclusion | motion/history validation | 拖尾、黑洞 |
| 光滑/粗糙材质 | specular target/footprint | 高光丢失或扩散 |

数值上至少报告：多帧平均 signed bias、absolute relative bias、MSE/PSNR、HDR FLIP、variance、`M/W/wSum/pHat` 直方图、selected-light frequency、history accept/reject 原因、NaN/Inf 和极端权重数量。性能上分开测 initial、temporal、spatial、replay、trace、shade、denoise、TLAS build/refit；同时记录 rays/pixel、any-hit invocation、显存和分辨率缩放。

## 18. 主要风险与决策门

### 风险 1：128 灯时没有收益

现有 tiled raster loop 可能比 ReSTIR 更便宜，特别是无阴影 local light。应把第一价值目标定义为固定 RT 阴影预算和高灯数可扩展性，并保留 raster fallback。

### 风险 2：TLAS 成本吞掉采样收益

场景级 AS 是比 sampler 更基础的缺口。先计静态 BLAS + TLAS refit/build，再决定动态物体范围；不能只报 trace 时间。

### 风险 3：历史灯索引失配

这是最难复现的爆亮来源。stable ID/revision 必须先于 reservoir history，并有灯添加/删除/排序回归。

### 风险 4：多重历史和偏差无法解释

spatial 多 pass、duplication cap、visibility guidance、lighting denoise、TAA 都可能改变能量。严格的逐项开关和 reference 是唯一可靠定位手段。

### 风险 5：镜面重尾与低分辨率漏样

specular `pHat` 对视角/法线极敏感；邻居样本在当前点可能重要性突变。第一版 full-res，后续按 roughness 缩小 upsample 核，必要时给 specular 更多 slot。

### 风险 6：误用 GI Jacobian

DSMEngine 第一阶段是解析灯 DI。设计文档和 shader 注释应明确“直接光灯域复用不使用 Lumen GI Jacobian”，避免距离/余弦重复校正。

### 风险 7：RayQuery 只通过 feature query

API tier 支持不代表主 renderer 的 shader 编译、descriptor、TLAS lifetime 已打通。先做 inline spike，再保留 DispatchRays fallback。

进入实现前应明确：local-light 路径是否替换/混合/仅 fallback；目标硬件是否强制 DXR 1.1；是否接受 bias mode 为产品默认及允许的长期能量误差；stable ID 由 scene 还是 render proxy 拥有；动态 mesh/alpha mask 是否为首版硬需求。

## 19. 最终建议

对 DSMEngine 最短而可靠的路线是：

1. 先实现 stable light identity、tile list 正确性和 renderer TLAS；
2. 用 full-res、float32、均匀 source 完成 initial-only ReSTIR DI；
3. 用 unshadowed/visible brute-force reference 分别证明 estimator 和 RT；
4. 再加入 temporal 与单轮 spatial merge，并保持 DI light-domain 数学；
5. 只有性能曲线支持时，才引入 paired reuse、compaction、roulette、packing、vector weights、dual motion 和 duplication cap；
6. 最后再考虑 half-res、area light、emissive mesh 或 GI。

不要逐行仿照 UE。应采用“论文提供 estimator 数学 + Lumen 提供 pass/history 组织 + MegaLights 提供产品化直接光经验 + DSMEngine 自己的 light/DXR/TAA 契约”的分层组合，并把每个工程近似标成可开关 bias feature。

## 20. 关键公式速查

```text
RIS candidate weight:
    w_i = (1/M) pHat(X_i) / p(X_i)

GRIS weight:
    w_i = m_i(Y_i) pHat(Y_i) W_Xi |∂T_i/∂X_i|

PSS integrand:
    F(ū) = ω_t(x̄) f(x̄) / p_t(x̄)

Hybrid-shift Jacobian:
    |∂T/∂ū| = [p^y_(k-1) G_y p^y_k] / [p^x_(k-1) G_x p^x_k]

Dual footprint criterion:
    min([p^x_(k-1)G(x_(k-1)→x_k)]⁻¹,
        [p^x_kG(x_k→x_(k-1))]⁻¹)
      ≥ (c/100) · primaryFootprint

Pairing Gaussian match:
    σ = √(8/(9π)) R

Duplication-aware cap:
    c_Cap = lerp(c_DefaultCap, c_minCap, D^α)

RIS reservoir final weight (one common convention):
    W = wSum / (M pHat(selected))
```

最后一行只是常见 convention；实现必须先固定 `w_i` 是否已含 `1/M`，再统一 `wSum` 与 `W`，绝不能混用两种定义。
