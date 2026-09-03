@@PAGE 1
# ReSTIR 温和导论

## 实时路径复用

Chris Wyman、Markus Kettunen、Daqi Lin、Benedikt Bitterli、Cem Yuksel、Wojciech Jarosz、Pawel Kozlowski、Giovanni De Francesco

2024 年 3 月 4 日

更多信息：https://intro-to-restir.cwyman.org

允许出于个人或课堂用途，免费制作本作品部分或全部内容的数字或纸质副本，前提是这些副本不以营利或商业利益为目的进行制作或分发，并且副本首页须保留本声明和完整引用信息。本作品中第三方组件的版权必须得到尊重。其他用途请联系权利人或作者。

SIGGRAPH '23 Courses，2023 年 8 月 6 至 10 日，美国加利福尼亚州洛杉矶。版权由作者持有。ACM ISBN 979-8-4007-0145-0/23/08。DOI：https://doi.org/10.1145/3587423.3595511

@@PAGE 2
# 摘要

近年来，基于 Reservoir 的时空重要性重采样算法 ReSTIR 仿佛横空出世，迅速席卷了实时渲染领域。通过样本复用，它显著加速了来自数百万动态光源的直接光照、漫反射多次反弹光照、参与介质渲染，乃至复杂的全局光照路径。经过高度优化的变体，相比传统光线追踪与路径追踪方法，效率最高可提升约 100 倍；这正是实现 30 或 60 Hz 帧率的关键。在生产级引擎中，即便每像素仅追踪一条光线或一条路径，也可能只在最高端硬件上可行，因此必须最大化每个样本所能提供的图像质量。

ReSTIR 建立在 Talbot 等人提出的重采样重要性采样 RIS 数学基础上。RIS 过去并未被广泛使用或系统教授，导致许多实践者缺乏必要的直觉和理论根基。扎实理解这些理论非常重要：在将 ReSTIR 集成到引擎时，一些看似显然的“优化”会暗中引入条件概率与依赖关系；若忽略这些关系，结果中就会出现难以控制的偏差。

本课程计划完成四件事：

1. 给出直观、具体的动机，解释 ReSTIR 为什么有效、适用范围、依赖的假设，以及现有理论和实现的局限。
2. 以循序渐进的方式建立理论，面向了解基本蒙特卡洛采样、但从未学习过重采样算法的听众。
3. 给出明确的算法示例与伪代码，并指出实现 ReSTIR 时极易踩中的陷阱。
4. 讨论真实游戏中的集成经验，强调其中的易错点、挑战、边界情况和实际收益。

## 课程形式与预备知识

这是一门时长三小时、中等难度的课程。我们假定听众了解基本光线追踪和微积分，也希望听众接触过渲染方程、蒙特卡洛积分、重要性采样及相关统计知识；不过，在温和引入重采样数学的同时，我们仍会简要回顾这些概念。

## 目标读者

本课程面向对重采样在实时渲染中的效率收益感兴趣的学生、研究人员和渲染工程师。他们可能没有系统跟进近期论文，对重采样数学缺乏直觉，对边界情况存在疑问，希望了解 ReSTIR 集成到生产渲染器时的挑战和收益，或希望寻找尚未解决、值得继续研究的问题。

@@PAGE 3
# 为什么在 2023 年开设 SIGGRAPH ReSTIR 课程？

第一篇 ReSTIR 论文发表后，算法显得非常有吸引力，但当时我们对它的完整能力与约束仍认识有限。后续论文扩展了这些知识，我们也积累了把研究成果带入已发布产品的经验，因此现在有信心给出一个可用、易懂且理论可靠的介绍。

与此同时，社区对 ReSTIR 的兴趣迅速增长：大量社交媒体讨论、解析理论的博客、线上图形学聚会、研究生课程讲义、独立及研发型游戏开发者实验、其他实验室的论文、建模软件插件、学生课程项目，以及大量通过电子邮件提出的技术问题。如此广泛的兴趣说明，社区确实需要一个连贯而温和的重采样理论介绍，以推动进一步研究与落地，并减少大家在常见问题上重复走弯路。

## 三小时课程安排

- 5 分钟：欢迎与介绍 - Chris Wyman。
- 15 分钟：动机；为什么考虑 ReSTIR - Cem Yuksel。
- 20 分钟：重采样重要性采样 RIS - Markus Kettunen。
- 15 分钟：RIS 与直接光照 - Benedikt Bitterli。
- 15 分钟：时空样本复用与 MIS - Benedikt Bitterli。
- 15 分钟：跨域复用样本 - Markus Kettunen。
- 20 分钟：把样本复用扩展到路径 - Daqi Lin。
- 15 分钟：让 ReSTIR 更快：采样器优化 - Daqi Lin。
- 15 分钟：让 ReSTIR 更快：底层优化 - Chris Wyman。
- 25 分钟：《赛博朋克 2077》中的 ReSTIR 集成 - Pawel Kozlowski 与 Giovanni De Francesco。
- 10 分钟：开放问题与未来方向 - 全体讲者。
- 10 分钟：问答 - 全体讲者。

本页边栏列出了课程所引用的 ReSTIR DI、ReSTIR GI、体渲染 ReSTIR、GRIS、生产级重构、相关博客、课程、开源实验与插件等资料；完整书目信息见文末参考文献。

@@PAGE 4
# 作者简介（上）

所有作者都参与了课程方向、观点、日程与讲义的讨论和准备；受其他事务影响，并非所有作者都会在现场授课。

**Chris Wyman（组织者）** 是 NVIDIA 位于雷德蒙德的杰出研究科学家，负责并参与改进样本复用的研究，也协助希望加速渲染器的内部与外部产品团队完成知识转移。他曾参与演讲、合著或组织六门 SIGGRAPH 课程，并曾在爱荷华大学任副教授十年。他拥有犹他大学博士学位和明尼苏达大学学士学位。

**Markus Kettunen（讲者）** 是 NVIDIA 赫尔辛基团队的高级研究科学家，在推动重采样统计理论演进方面发挥了关键作用，使跨积分域借用样本时仍能保持稳健和无偏。在加入 NVIDIA 从事博士后研究之前，他在阿尔托大学取得博士学位、在赫尔辛基大学取得硕士学位；他还曾参与 Weta 的 Manuka 渲染器，并为汽车行业开发图形解决方案。

**Daqi Lin（讲者）** 是 NVIDIA 雷德蒙德团队的研究科学家。研究生期间，他主导了参与介质中复杂路径重采样，以及复杂非漫反射路径重采样的开发与实现。在 NVIDIA，他继续使这些代码更快、更稳健，并推动 ReSTIR 更好地降低复杂路径类型的方差和相关性。他拥有犹他大学博士学位和新加坡国立大学学士学位。

**Benedikt Bitterli（讲者）** 是 NVIDIA 雷德蒙德团队的高级研究科学家，研究方向包括多种渲染问题、采样、新材质模型及深度学习应用。他主导了第一篇面向动态多光源直接光照的 ReSTIR 论文。他拥有达特茅斯学院博士学位，以及苏黎世联邦理工学院的学士和硕士学位，并具有 Disney Research 和 Walt Disney Animation Studios 的研究与生产经验。

**Cem Yuksel（讲者）** 是犹他大学副教授、Roblox Research 高级科学家及 Cyber Radiance LLC 创始人。他的研究横跨图形硬件、物理仿真、几何建模、采样与渲染。他曾共同组织两门 SIGGRAPH 课程，在康奈尔大学从事过博士后研究，并拥有得州农工大学博士学位。

**Wojciech Jarosz（合著者）** 是达特茅斯学院副教授，并共同创立了视觉计算实验室。此前，他曾任 Disney Research Zurich 负责渲染的高级研究科学家，并兼任苏黎世联邦理工学院讲师。他拥有加州大学圣迭戈分校博士和硕士学位，以及伊利诺伊大学厄巴纳-香槟分校学士学位。

@@PAGE 5
# 作者简介（下）

**Pawel Kozlowski（讲者）** 是 NVIDIA 开发者与性能技术团队的首席开发技术工程师，拥有十年以上计算机图形学经验。他专注于把路径追踪集成到现代游戏引擎，其中既涉及采样和降噪算法，也涉及在 GeForce 平台上为玩家榨取尽可能高的 GPU 性能。他最初是在 GE Healthcare 与奥斯陆大学攻读医学成像博士期间，通过三维心脏超声的体渲染工作培养起对实时图形学的兴趣。

**Giovanni De Francesco（讲者）** 是 CD Projekt Red 的高级灯光技术美术师，致力于实现世界一流的视觉效果。他为灯光团队设计并测试工具，确保团队在采用前沿渲染技术时仍遵循最佳实践。在加入 CD Projekt Red 前，他曾在 inVRsion 从事 VR 灯光工作，也参与过广告制作和其他灯光岗位。

@@PAGE 6
# 目录（上）

- 摘要；课程形式与预备知识；为什么在 2023 年开设本课程；课程安排；作者简介。
- 第 1 章 引言：1.1 ReSTIR 的动机。
- 第 2 章 预备知识：2.1 蒙特卡洛积分；2.2 支持集；2.3 多重重要性采样；2.4 无偏贡献权重。
- 第 3 章 重采样重要性采样：3.1 RIS；3.2 MIS 权重；3.3 BSDF 与 NEE 之间的 RIS 示例；3.4 输入 PDF 未知时的处理。
- 第 4 章 ReSTIR：时空 Reservoir 重采样：4.1 加权 Reservoir 采样；4.2 时空复用；4.3 ReSTIR 直接光照示例；4.4 历史长度；4.5 高级主题。
- 第 5 章 跨域复用：5.1 预备知识；5.1.1 Shift Mapping；5.1.2 Jacobian 行列式；5.2 跨域复用样本；5.3 跨域 MIS。
- 第 6 章 ReSTIR 路径追踪：6.1 路径积分；6.2 路径追踪器中的 RIS；6.3 复用路径样本；6.4 什么是好的 Shift Mapping；6.5 常见 Shift Mapping；6.6 面向实时渲染的高效 Shift Mapping；6.7 体渲染。

@@PAGE 7
# 目录（下）

- 第 7 章 让 ReSTIR 更快：7.1 采样器优化；7.1.1 用邻居拒绝近似 MIS 权重；7.1.2 Contribution MIS；7.1.3 Pairwise MIS；7.1.4 有偏 MIS 权重；7.2 底层优化；7.2.1 ReSTIR DI 中的 Sample Tiling；7.2.2 多种解析光类型；7.2.3 加速 Hybrid Shift。
- 第 8 章 游戏集成经验。
- 第 9 章 入门建议。
- 参考文献；缩略语表；符号表。

@@PAGE 8
# 第 1 章 引言

实时路径追踪若要达到电影级效果，会面临极大挑战。离线渲染器可以在渲染农场上花费数小时乃至数天，而实时渲染器通常必须在单块 GPU 上、每帧约 16 ms 的预算内完成工作。在这种环境中，首要目标是最大化效率，这自然促使我们在不同像素之间和不同帧之间交叉复用信息。

ReSTIR 是对 RIS 的迭代应用。它不断把多个邻居样本聚合成一个质量更高的样本，因此可以从大量历史帧中进行无偏样本复用。

有经验的实践者经常问：“你怎么保证邻居的样本与当前像素有关？”这是一个非常准确的问题。答案是：必须极其谨慎。正确处理样本的支持集与积分域，正是 ReSTIR 最核心、也最困难的问题。

不过，样本复用本身并不反直觉。现代降噪器和上采样器早已在像素之间复用和过滤颜色；这也可视为在不同积分域之间复用样本。后处理降噪器通常忽略支持集问题，因此常会损失能量或产生其他偏差。

RIS 与 ReSTIR 的巨大优势在于，它们在丢弃任何候选信息之前就完成过滤、重采样和复用。此时我们仍然拥有中间概率、分布和样本，因此可以构造无偏算法；而后处理降噪通常只能访问颜色和少量显式引导缓冲区。

从这个角度看，ReSTIR 是一种对采样分布进行过滤的技术：把多个样本聚合成一个具有更优 PDF 的样本。如果混合邻域颜色能改善图像，那么过滤 PDF 同样可能减少噪声。

Path Guiding 已证明过滤 PDF 有效，它通过历史样本拟合某个 PDF 族。ReSTIR 则跳过显式学习，直接对其他像素和历史帧的既有样本进行加权复用，使 PDF 在反复重采样中得到改善。

ReSTIR 与许多已有采样技术相似。它的一项关键贡献，是借助加权 Reservoir 采样，让这些思想拥有惰性、流式且适合 GPU 的实现。

@@PAGE 9
## 1.1 ReSTIR 的动机

假定读者熟悉光传输的路径积分形式。一个像素接收到的辐射亮度，是从所有发光体到传感器的全部可能路径之和：

```text
I_i = integral_Omega h_i(x) f(x) dx                         (1.1)
```

其中，Omega 包含所有长度的路径，h_i 是像素的图像滤波器，f 是测量贡献函数，dx 是各顶点面积测度的乘积。若使用盒式滤波器，可以定义只包含穿过像素 i 的路径域 Omega_i：

```text
I_i = integral_Omega_i f(x) dx                              (1.2)
```

路径追踪器从相机出发随机采样路径 X，让路径在场景交互点上反弹，并以路径携带的辐射贡献 f(X) 除以其采样概率密度 p(X)：

```text
<I_i> = f(X) / p(X) approximately equals I_i                (1.3)
```

该估计器在有符号误差意义上平均正确，即无偏，但会有噪声。p 与 f 的偏离越大，方差越高。如果 p 与 f 成正比，便得到零方差估计。

也可以平均 N 个样本来降低噪声：

```text
<I_i> = (1/N) * sum_j [ f(X_j) / p(X_j) ]                   (1.4)
```

但这种方式很快变得低效：噪声幅度每减半，样本数约需增加四倍。更好的方向是让 p 更接近 f；然而，在实际光传输中，预判哪些路径承载大量光能本身就很困难。

ReSTIR 的核心前提是：邻近像素看到的路径虽然不同，但重要路径往往相似。像素 a 的优质路径经过少量修改后，通常也能帮助像素 b。因此，像素 b 可以复用像素 a 的好样本来改善估计。

“好路径”意味着 f 值大，但不能随意偏爱高贡献路径，否则会引入偏差。我们的目标不是只输出高贡献样本，而是得到概率密度与 f 成正比的样本。

@@PAGE 10
RIS 正是为此服务。给定输入序列 X_1...X_M，RIS 为每个输入计算权重 w_i，并按这些权重随机选出一个样本，使输出样本的概率密度通常比输入密度更接近 f。简化地说，RIS 是一台聚合机器：接收多个候选，压缩为一个分布更好的样本，从而降低噪声。

一个 RIS 输出当然不会优于把全部 M 个输入都完整求值并平均；它的优势是输出只有一个样本。后续处理一个样本通常远比处理 M 个样本便宜，而当 RIS 发生链式组合时，这一优势会被放大。

假设当前 RIS 从 M_2 个样本中重采样，而每个输入都代表上一帧已聚合的 M_1 个样本。最终输出代表 M_1 x M_2 个底层样本，但当前代价小于 M_1 + M_2：历史中的 M_1 已在过去支付过，借用样本也通常比重新生成便宜。

把这种聚合逐帧迭代：每帧每像素生成一个新样本，再与上一帧聚合样本和空间邻居样本合并。理论上，如果每个像素能近似聚合 1920 x 1080 x 10 个样本，却只需每帧支付约一个新样本的成本，就会获得巨大收益。实践中不可能实现如此理想的全局聚合，但迭代 RIS，也就是 ReSTIR，常能以很低的增量成本获得相当于数百独立 SPP 的图像质量。

@@PAGE 11
# 第 2 章 预备知识

在进入 RIS 与 ReSTIR 的细节前，本章简要回顾随机变量、支持集和蒙特卡洛积分。熟悉这些内容的读者可直接阅读第 3 章。

## 2.1 蒙特卡洛积分

对无法以闭式求解的积分：

```text
I = integral_Omega f(x) dx                                  (2.1)
```

蒙特卡洛积分用 M 个随机样本 X_1...X_M 近似它；函数 f 只需在这些位置求值。本文用大写 X 表示随机变量，用小写 x 表示普通积分变量，这一约定在同时出现两类变量时可以避免混淆。

若 X_i 在 Omega 中均匀分布：

```text
<I> = |Omega| * (1/M) * sum_i f(X_i)                        (2.2)
```

估计值本身是随机变量，单次不一定等于真实积分；但只要样本覆盖完整积分域，其期望就是 I，而且当 M 趋于无穷时会依概率收敛到 I。

非均匀采样通常更有利。连续随机变量 X 的概率密度由 PDF p(X) 描述。使用非均匀分布时，不能再直接平均函数值；采样概率较低的位置必须获得较高权重，才能保证期望正确。

定义：均匀分布意味着所有可能样本值拥有相同概率或概率密度。若无论阈值 epsilon 多小，当 M 增大时，|<I>-I| 大于 epsilon 的概率都趋于零，则称估计器依概率收敛。

@@PAGE 12
一般蒙特卡洛估计器为：

```text
<I> = sum_i [ (1/M) * f(X_i) / p_i(X_i) ]                   (2.3)
```

每个样本都除以自己的概率密度。不同样本可以使用不同 PDF；虽然很多应用让所有样本共享同一 PDF，但组合不同 PDF 正是 ReSTIR 的关键组成部分。

即使 M=1，也可使用：

```text
<I> = f(X) / p(X)                                           (2.4)
```

它的准确度完全取决于 p 与 f 的关系。如果 f(x)/p(x) 在整个积分域中为常数，p 就是完美 PDF，单个样本也足以精确估计积分。然而构造完美 PDF 通常需要预先知道积分值，现实中不可行。ReSTIR 的目标，是让有效 PDF 尽可能接近这个完美 PDF，使极少量样本也能给出良好估计。

估计器的质量通常由方差衡量，即估计值相对期望值的平方偏差的期望。方差越低，噪声越少。降低方差有两种基本方式：增加样本数 M，或让 p 更好地近似 f。ReSTIR 主要解决后者。

估计器期望偏离真实积分称为偏差；没有偏差的算法称为无偏。上述 MC 估计器在较温和条件下无偏，但破坏这些条件会引入偏差。

## 2.2 支持集

函数 f 的支持集 supp(f)，就是 f(x) 不为零的所有 x。随机变量 X 的支持集 supp(X)，是 X 能够取到的所有值。若 X 具有 PDF p，则 supp(X)=supp(p)。

@@PAGE 13
蒙特卡洛积分无偏只需要一个核心条件：随机变量 X 的支持集必须包含 f 的支持集，即 supp(f) 是 supp(X) 的子集。也可以说 X 必须“覆盖”f：

```text
E_X[<I>] = integral_supp(X) [f(x)/p(x)] p(x) dx
         = integral_supp(X) f(x) dx                         (2.5)
```

积分只会发生在 X 的支持集内，落在支持集之外的 f 永远被忽略。传统写法是：只要 f(x)>0，就必须有 p(x)>0。普通算法中这通常不是问题；ReSTIR 会混合来自邻居、历史帧和不同技术的分布，这些分布不一定各自覆盖被积函数，因此支持集会直接影响算法设计。

## 2.3 多重重要性采样

MIS 用于高效组合多个随机变量的样本。朴素公式会把不同技术的方差直接相加，而且只要其中任一技术不覆盖 f，就可能产生偏差。MIS 改为加权组合：

```text
<I> = sum_i m_i(X_i) * f(X_i) / p_i(X_i)                   (2.6)
```

无偏所需条件是：对 f 支持集内任意 x，所有 m_i(x) 之和为 1；当 x 不在 X_i 的支持集时，m_i(x)=0。这样只要求所有输入支持集的并集覆盖 f。

简单平均对应 m_i=1/M，只在每个 X_i 都单独覆盖 f 时无偏。常用 balance heuristic 为：

```text
m_i(x) = p_i(x) / sum_j p_j(x)                             (2.7)
```

在通常假设 MIS 权重非负时，它在方差意义上是“最优”的一种权重方案。

@@PAGE 14
## 2.4 无偏贡献权重

此前假定 p(x) 可闭式求值。但若 X 的生成过程很复杂，例如 Woodcock tracking 或 Photon Mapping，p(X) 可能根本无法实际计算。幸运的是，只要知道随机变量 W_X，并且在给定 X 时满足：

```text
E[W_X | X] = 1 / p(X)
```

就仍可使用修改后的估计器：

```text
<I> = f(X) * W_X                                            (2.8)
```

尽管 p(X) 不可计算，某些图形学过程仍存在公式简单的 W_X。RIS 输出正属于这一类：输出 PDF 是随每次重采样增长的高维积分，实际不可处理，但对应的 W_X 可以廉价求得。在 RIS 语境中，W_X 被称为无偏贡献权重，它是跨域广义复用的关键。

@@PAGE 15
# 第 3 章 重采样重要性采样

重要性采样的效果取决于生成样本所用的 PDF。理想 PDF 常常没有显式表达式；即便知道表达式，也可能无法直接从它采样。RIS 为此提供解决方案：输入候选 X_1...X_M，为每个候选赋予重采样权重 w_i，再按 w_i 的比例随机选出一个候选。输出样本的 PDF 可以不同于生成候选时的 PDF，而我们通过权重控制它。

候选和输出都是连续随机变量。虽然中间执行了离散选择，RIS 仍可与 Path Guiding 类比：它接收若干随机变量，输出具有不同连续分布的随机变量。不同之处在于，RIS 不拟合分布，而是随机保留一个既有样本，让一个样本在分布意义上聚合多个候选。

困难是：RIS 输出样本的 PDF 通常不可处理，其求值至少和完整着色一样昂贵。那么没有 p(X)，如何使用 f(X)/p(X)？答案是 RIS 给输出样本附带 W_X。f(X)W_X 是积分的无偏估计；需要的是正确权重，而不是必须显式得到 PDF。

W_X 替代 1/p(X)，但同一个 X 在不同候选集合下可能得到不同 W_X。它不是 X 的确定函数，而是随机变量，故称为“无偏贡献权重”。定义上，不论选择什么被积函数 f，都有：

```text
E[f(X) W_X] = integral_Omega f(x) dx
```

记号写作 W_X 而不是 W(X)，正是为了强调它不是可以在任意 X 上重新求值的函数；不要把它用于普通 MIS 权重。

@@PAGE 16
早期 RIS 与 ReSTIR 文献常写成：

```text
w_i = pHat(X_i) / p(X_i)
W_X = [ (1/M) sum_i w_i ] / pHat(X)                       (3.1)
```

其中 pHat 是目标函数。本文采用 GRIS 的广义写法，把 1/M 放入重采样 MIS 权重：

```text
w_i = (1/M) * pHat(X_i) / p(X_i)
W_X = sum_i w_i / pHat(X)                                 (3.2)
```

两种写法等价，但后一种更清楚：1/M 的职责是 MIS 权重，不是对 weightSum 进行随意平均。当 sum(w_i) 的方差趋于零时，输出 PDF 趋于 pHat 的归一化形式 pBar。若选择 pHat=f，极限情况下 RIS 将趋于零方差估计器。

pHat 常被不准确地称为“目标 PDF”。它其实是未归一化函数，通常直接取被积函数 f 或与 f 接近的代理。真正的目标 PDF 是：

```text
pBar(x) = pHat(x) / integral_Omega pHat(x) dx
```

## 3.1 RIS 的完整过程

1. 在同一积分域 Omega 中取得候选 X_1...X_M。
2. 计算每个候选的重采样 MIS 权重 m_i(X_i)。
3. 计算 w_i = m_i(X_i) pHat(X_i) W_Xi。
4. 按照 w_i 的比例随机选出 X。
5. 令 W_X = sum_j(w_j)/pHat(X)。

输出 X 的分布近似正比于 pHat，候选越多越接近。X 虽然只有一个，却在概率分布中代表多个输入；这一信息反映在 W_X 中。输出至多与所有候选结合后的估计同样好，但后续只需处理一个样本。

@@PAGE 17
## 算法 1：重采样重要性采样

```text
randomIndex(w[1..M]):
    r = rand()
    for s = 1..M:
        if w[s] > 0:
            r -= w[s] / sum(w)
            if r <= 0: return s
    return null

RIS(M):
    for i = 1..M:
        generate X[i]
        w[i] = m_i(X[i]) * pHat(X[i]) * W_Xi
    s = randomIndex(w)
    if s is null: return null, 0
    Y = X[s]
    W_Y = sum(w) / pHat(Y)
    return Y, W_Y
```

W_X 的条件期望等于 1/p(X)。要正确使用输出 X，必须理解其支持集：输入支持集的并集，再去掉 pHat=0 的部分。为了用 X 积分 f，必须保证 f 非零处 pHat 为正，并且输入联合覆盖 f 的支持集。

只要这些条件成立：

```text
<I> = f(X) W_X                                             (3.4)
E[<I>] = integral_Omega f(x) dx = I                        (3.6)
```

若所有 w_i 都为零，应返回 W=0 的空样本。不要反复抽样直到得到非空结果，因为那会改变随机变量的分布并引入偏差。空样本仍是随机变量的合法一次实现；它不意味着整个输入分布失效。

@@PAGE 18
### 示例 3.1.1：简单积分

假设 M 个候选独立同分布，已知 PDF p，且 p 覆盖 f；同时 pHat 在 f 的支持集上为正。输入贡献权重为 1/p(X_i)，相同分布允许使用 m_i=1/M：

```text
w_i = (1/M) * pHat(X_i) / p(X_i)
```

按 w_i 选出 X 后，必须重新设置：

```text
W_X = sum_j(w_j) / pHat(X)
```

不能沿用被选候选原来的 W_Xs；那会忽略选择过程并导致偏差。最终 f(X)W_X 是积分的无偏估计。

如果目的仅是立即积分，把 M 个原始贡献直接平均并不会比 RIS 差。RIS 的真正价值在于：输出 X 的分布聚合了其他候选，之后还能继续追踪一条路径、与邻居共享，或者送入下一轮 RIS。

### 示例 3.1.2：BSDF 重要性采样

把 X_i 视为方向，由 PDF p 的 BSDF sampler 生成；pHat 是完整 BSDF 的廉价代理，并拥有相同支持集。RIS 从 M 个方向中选一个，使输出方向的分布更接近 pHat。随后沿该方向追踪光线，并在蒙特卡洛估计器中用 W_X 替代 1/p(X)。

当候选来自不同分布，例如混合 BSDF sampling 与 light sampling，或复用不同像素时，就需要更一般的 MIS 权重。

@@PAGE 19
## 3.2 MIS 权重

RIS 的直接目标是产生近似服从 pHat 的样本，之后才用它积分 f。若候选支持集的并集覆盖 pHat，且 pHat 覆盖 f，则输出 X 配合 W_X 能无偏积分 f。

若所有输入都各自覆盖 pHat，1/M 在技术上无偏，但只要其中一个输入在某些困难区域表现极差，就可能产生严重离群值。若某输入在 pHat 非零处拥有零 PDF，1/M 甚至会导致有偏结果。

当 PDF 可知时，可以使用 balance heuristic：

```text
m_i(x) = p_i(x) / sum_j p_j(x)                             (3.7)
```

该权重在所有输入分布上对同一个 x 求值，只依赖其他输入的分布，不依赖其他输入本轮实际抽到了什么值。把 X_j 的具体实现值掺进 m_i(X_i) 通常是错误的。

Balance heuristic 的弱点是 M 个样本各需评估 M 个 PDF，总复杂度 O(M^2)。样本较多时应考虑 Pairwise MIS 等高级方法；在基线实现完全正确前，仍建议从 balance heuristic 开始。

工程经验法则：当且仅当全部输入同分布时使用 1/M 权重。

当输入 PDF 不同，只要求它们支持集的并集覆盖 pHat。实践中最稳妥的方法，是加入一个由当前目标直接生成、完整覆盖 pHat 的 canonical sample。

@@PAGE 20
### 示例 3.3：在 BSDF 与 NEE 之间执行 RIS

为了产生可复用的直接光照样本，从 PDF p_1 的 BSDF sampler 取得 M_1 个候选，从 PDF p_2 的灯光 sampler 取得 M_2 个候选。两种 PDF 必须转换到同一测度。

BSDF 候选的 balance heuristic 权重为：

```text
m_i(x) = p_1(x) / [M_1 p_1(x) + M_2 p_2(x)]                (3.9)
```

灯光候选则在分子和自身贡献权重中使用 p_2。实现基线时建议令 pHat=f，即使用完整路径贡献，并在验证正确后再测试更廉价的代理。

按 w_i 选择样本 X，并令：

```text
W_X = sum_j(w_j) / pHat(X)
```

这样得到覆盖 f 完整支持集的直接光照样本，可继续在像素之间共享。

虽然 BSDF 和灯光 sampler 各自可能覆盖所有有效直接光，因此 1/(M_1+M_2) 不一定产生偏差，但其噪声可能退化到仅使用 BSDF sampling 的水平。RIS 中的 MIS 权重和传统蒙特卡洛中的 MIS 同样重要。

面积 PDF 与立体角 PDF 必须统一：面积 PDF 乘几何变换项可转为立体角 PDF，反向转换则除以该项。不要把两种测度下的数值直接相加。

@@PAGE 21
## 3.4 输入 PDF 未知时

现在假设输入 X_i 来自先前的 RIS，只知道 W_Xi，不知道闭式 PDF。MIS 仍是难点，因为不能使用需要真实 PDF 的公式。

RIS 输出样本的分布大致正比于它当时使用的目标函数 pHat_i，因此可把 pHat_i 当作未知 p_i 的代理，得到广义 balance heuristic：

```text
m_i(x) = pHat_i(x) / sum_j pHat_j(x)                       (3.11)
```

每轮都要保证输入随机变量的支持集与各自目标函数支持集一致。如果候选无法联合覆盖当前 pHat，就加入 canonical sample。

这里存在一个细节：pHat_i 没有归一化。如果不同像素的 integral(pHat_i) 差别很大，直接把它们当作 PDF 代理可能扭曲 MIS 权重。

不是由 RIS 产生的样本也可参与 pHat_i-MIS，但需为它指定 pHat_i，保证样本覆盖 pHat_i，并在 pHat_i=0 时把它变成空样本。空样本的 MIS 权重始终为零。

完成这些准备后，我们已经可以在不修改样本、且各样本属于同一积分域的前提下，跨像素和帧复用样本。

@@PAGE 22
# 第 4 章 ReSTIR：时空 Reservoir 重采样

RIS 能改善样本分布，但复杂 pHat 和低质量初始 proposal 可能需要远超预算的候选数。ReSTIR 通过链式调用 RIS，并在空间和时间上复用样本来解决这一问题。本章先介绍 Reservoir Resampling，再讨论时空复用。

## 4.1 加权 Reservoir 采样

朴素 RIS 需要先生成并保存全部候选，再执行第二遍选择；这在 GPU 上很不方便。加权 Reservoir 采样 WRS 可以单遍处理加权样本流，只保存一个当前样本，正好适合实现 RIS。

WRS 依次处理输入。在任意时刻，Reservoir 保存从已处理元素中按目标权重选出的样本。每到一个新元素，以恰当概率替换当前样本；流结束后直接返回 Reservoir。WRS 也可扩展为维护多个样本，但 ReSTIR 常使用单样本 Reservoir。

@@PAGE 23
## 算法 2：Reservoir Resampling

```text
Reservoir:
    Y = null
    W_Y = 0
    weightSum = 0

update(X_i, w_i):
    weightSum += w_i
    if rand() < w_i / weightSum:
        Y = X_i

Resample(M):
    R = empty reservoir
    for i = 1..M:
        generate X_i
        w_i = m_i(X_i) * pHat(X_i) * W_Xi
        R.update(X_i, w_i)
    if R.Y is not null:
        R.W_Y = R.weightSum / pHat(R.Y)
    return R
```

## 4.2 时空复用

每个像素预算内只能生成少量候选，但邻近像素的积分函数和目标分布通常相似。因此，邻居通过 RIS 得到的样本是很有价值的复用候选。

**初始候选。** 每像素从一个或多个独立输入通过 RIS 得到一个近似服从 pHat 的样本。输入同分布时可用 m_i=1/M。

**空间复用。** 每像素在邻域中选择若干像素，再从自身样本与邻居样本执行 RIS。可以重复多轮，分布会继续改善，但样本相关性也会增强。输入分布不同，需要广义 MIS。

**时间复用。** 用运动矢量找到上一帧对应像素，把历史样本与当前样本进行 RIS。逐帧执行时，样本可以无限向前传播；之后再进行空间复用，还能让历史好样本快速扩散。

一种自然的帧内顺序是：初始候选、Temporal Reuse、Spatial Reuse，最后用选中样本计算 f(X)W_X。

重要警告：不能根据邻居 Reservoir 中具体保存的随机样本来决定是否选择这个邻居，否则会对样本进行条件化并产生偏差。可以依据与随机样本无关的 G-buffer 几何属性选择邻居。

@@PAGE 24
## 4.3 示例：ReSTIR 直接光照

直接光照包含所有长度为 3 的路径：光从发光体出发，在一个表面或粒子上反射，然后到达传感器。按传感器向外编号，路径写成 [x0, x1, x2]：x0 位于像平面，x1 是主可见性命中点，x2 位于发光表面。令 A 表示所有发光表面点的集合，则 x2 属于 A。

x0 与 x1 可以是确定的，也可以由随机镜头坐标决定；一旦确定本像素本帧的主射线，就把它们视为常量，只把 x2 当作自由变量：

```text
xBar = [x0, x1, x2]                                        (4.1)
```

为完成像素着色，需要对发光表面上的 x2 积分：

```text
L(x1 -> x0) = integral_A fs(x2 -> x1 -> x0)
              * G(x1 <-> x2) * V(x1 <-> x2)
              * Le(x2 -> x1) dx2                          (4.2)
```

fs 是 x1 处 BSDF，Le 是 x2 朝 x1 的发射辐射亮度，G 是几何项，V 是可见性。固定 x0、x1 后，可简写为：

```text
L(x1 -> x0) = integral_A f(x2) dx2                         (4.3)
```

不同像素 i 拥有不同的 x0、x1，因而拥有不同 f_i，但它们都在相同的发光表面空间 A 上积分。ReSTIR 在当前像素的 canonical sample 与空间/时间借来的样本之间重采样 x2，逐步改善 x2 的分布。

图 4.1：一条直接光照路径，依次包含像平面点、主命中点和发光表面点。

@@PAGE 25
直接光照被积函数为：

```text
f(x) = fs(x) * G(x) * V(x) * Le(x)
```

最易验证的基线直接选择完整被积函数作为目标：

```text
pHat(x) = fs(x) * G(x) * V(x) * Le(x)                      (4.4)
```

为减少候选阶段的阴影光线，也可从 pHat 中去掉 V，只保留 fs、G、Le。这会使极限采样分布变差，并需要额外条件保证正确，但实践中可能更高效。作者强烈建议先把可见性包含在 pHat 中，建立正确基线；修复一个已经叠加多种优化的错误实现，会比在正确实现上逐项优化困难得多。

**初始候选。** 可用标准 light sampler 在发光表面上生成 M 个样本，以 1/M 的 MIS 权重做 RIS。先实现最简单、正确的版本。

**时空复用。** 对来自像素 j 的输入，要用该像素自己的传感器点与主命中点评估 pHat_j。广义 balance heuristic 为：

```text
m_i(x) = pHat_i(x) / sum_j pHat_j(x)                       (4.5)
```

完整贡献作为目标时，就是在所有输入像素的路径 x -> x_j,1 -> x_j,0 上分别评估 fs、G、V、Le。

**空间邻居。** 可在当前像素附近的方形或圆盘区域随机选邻居。用 G-buffer 法线、深度等属性启发式筛选相似像素通常是安全的，只要决定不依赖 Reservoir 中的具体样本。对不同像素直接使用 1/M 权重通常不会收敛到正确目标分布，并会产生偏差。

@@PAGE 26
**时间复用。** 运动矢量必须准确描述表面点从上一帧到当前帧的像素位置。当前像素与上一帧匹配像素进行 RIS。严格的无偏 MIS 可能要求在上一帧的场景中评估上一帧 pHat，进而需要保留上一帧加速结构；这在生产实现中通常过于昂贵，因而常采用受控近似。

## 4.4 历史长度与置信度

如果 Temporal Reuse 总是把历史样本与新样本等权合并，每帧会丢失约一半已经积累的历史。为解决这一问题，可为每个 Reservoir 存储置信度 c，并用加权 MIS：

```text
m_i(x) = c_i pHat_i(x) / sum_j [c_j pHat_j(x)]              (4.7)
```

c 可近似理解为该 Reservoir 聚合过的有效样本数。一个输入代表 7 个近似独立样本、另一个代表 2 个样本时，前者应获得更高权重。合并 Reservoir 时，输出置信度通常取输入置信度之和。

但这个和只是有效样本数的上界。空间复用会反复传播同一祖先样本，若每轮都机械累加 c，置信度会指数增长，而真正新增的独立样本很少。新样本权重将指数衰减，算法可能收敛到错误结果。

实践中必须把 c 截断到固定上限，在噪声与相关性之间取得平衡。常用上限约为 5 至 30，20 是很好的起点。历史原因使许多实现把置信度字段命名为 M，把截断称为 M-capping。

建议验证顺序：只做初始候选；加入 Spatial；再加入无运动 Temporal；最后才加入运动。每一步都应在静止场景中累计大量独立帧，并验证是否收敛到路径追踪参考结果。

@@PAGE 27
## 算法 3：带置信度的重采样

```text
Reservoir:
    Y = null
    W_Y = 0
    weightSum = 0
    c = 0

update(X_i, w_i, c_i):
    weightSum += w_i
    c += c_i
    if rand() < w_i / weightSum:
        Y = X_i

Resample(inputs):
    for each input i:
        w_i = m_i(X_i) * pHat(X_i) * W_Xi
        update(X_i, w_i, c_i)
    if Y is not null:
        W_Y = weightSum / pHat(Y)
    c = min(c, cCap)
```

一个新独立样本的置信度为 1；从 M 个新样本中执行 RIS 后，输出置信度可设为 M。相机移动时新进入画面的像素没有历史前驱，应把 c 重置为 0；遮挡或显露检测也可触发重置。重置条件只能依赖 G-buffer 或其变化，若依赖 Reservoir 样本内容会引入偏差。

## 4.5 高级主题：更好的初始采样

直接光照可以按光源功率选择一个光源，再在其表面均匀采样。原始 ReSTIR DI 每像素生成 32 个灯光候选并通过 WRS 选一个。还可把候选预生成到由屏幕块共享的 light tiles 中，以提高缓存局部性。

@@PAGE 28
按功率采光不考虑当前着色点属性，对高光材质可能很差。高光表面可增加 BSDF sampling，并与 light sampling 通过 MIS 混合。必须先把两者 PDF 转换到同一测度。

作者仍建议先完成不含 BSDF sampling 的基础版本，因为同时调试多个系统非常困难。加入 BSDF 候选后，为了正确处理高光和镜面路径，往往还需要下一章的 shift mapping。

此前介绍的 RIS 默认所有复用样本属于同一个积分域。当物体运动、帧间场景域发生变化时，直接复用旧顶点并不完整。样本需要通过一个确定映射进行修改，再在新积分域中使用；概率密度也必须按映射的 Jacobian 修正。

# 第 5 章 跨域复用

更高级的样本复用通常需要在借用时修改样本。场景会随帧变化，不同像素看到的路径空间也不同。若不修改路径顶点，就无法有效复用穿过镜面或玻璃的路径，因为新路径必须继续满足理想反射或折射约束。本章通过 shift mapping 把 RIS 推广到不同积分域之间。

@@PAGE 29
## 5.1 Shift Mapping

Shift mapping 这一名称来自梯度域渲染。在梯度域方法中，为估计相邻像素之间的离散图像梯度，路径样本要从一个像素“平移”到另一个像素。路径含多个顶点，其中一些必须修改，才能在目标像素中仍构成有意义的路径，并满足金属、玻璃等材质约束。

映射 T 把域 A 中的路径 x 映到域 B 中的路径 y=T(x)。简单的 reconnection shift 会保留从第二个表面顶点开始的自由顶点，只把路径开头重新连接到目标像素：

```text
T_i->j([x_i,0, x_i,1, x2, x3, ...])
    = [x_j,0, x_j,1, x2, x3, ...]                           (5.1)
```

它适合漫反射和粗糙表面，却不适合光滑或理想镜面，因为新的连接方向不一定满足反射定律。其他映射包括 half-vector shift、random replay shift 及其混合形式。

形式上，shift mapping 是从 A 的某个子集 D(T) 到 B 中像集 I(T) 的双射。它必须是确定的；一条路径最多映到一条目标路径；两条路径不能映到同一路径；必须存在逆映射；并非所有路径都必须可映射。

实践中常通过对称性保证可逆：若 y=T_i->j(x)，则 x 必须等于 T_j->i(y)。若正向映射因遮挡等条件失败，对应逆向也必须视为未定义。

首个实现建议使用 reconnection shift，并先验证同一像素到自身的恒等性质：路径不变、贡献不变、Jacobian 为 1。即便只出现很小差异也要报告，因为 shift mapping 常暴露路径追踪器中原本隐藏的问题。

@@PAGE 30
## 5.1.2 Jacobian 行列式

普通函数会改变相邻数值间的距离，因此也会改变样本密度；一维缩放由导数给出，多维映射的局部缩放由 Jacobian 行列式给出。Shift mapping 也会改变路径密度。

若 Y=T(X)：

```text
p_Y(Y) = p_X(X) / |T'(X)|                                  (5.2)
W_Y    = W_X * |T'(X)|                                      (5.3)
```

Jacobian 必须出现在重采样权重和 MIS 权重的正确位置，既用于保持无偏，也能帮助抑制离群值。具体渲染映射通常拥有可计算的几何公式。

## 5.2 跨域 RIS

1. 取得输入 X_i，每个来自自己的域 Omega_i。
2. 用 Y_i=T_i(X_i) 把它们映到目标域 Omega。
3. 计算重采样 MIS 权重 m_i(Y_i)。
4. 计算：

```text
w_i = m_i(Y_i) * pHat(Y_i) * W_Xi * |T_i'(X_i)|
```

5. 按 w_i 比例选择 Y。
6. 设置 W_Y=sum(w_i)/pHat(Y)。

输出 Y 位于目标域，可用于积分或继续进入下一轮 RIS；输入越多，输出分布越接近 pHat。

@@PAGE 31
## 算法 4：跨域 RIS

```text
Resample(inputs):
    R = empty reservoir
    for each i:
        X_i = obtain input, e.g. from a reservoir
        Y_i = T_i(X_i)
        if shift failed:
            w_i = 0
        else:
            w_i = m_i(Y_i) * pHat(Y_i) * W_Xi * absJacobian
        R.update(Y_i, w_i, c_i)
    if R.Y is valid:
        R.W_Y = R.weightSum / pHat(R.Y)
    R.c = min(R.c, cCap)
```

利用 W_Yi=W_Xi|T_i'|，权重仍可看成普通形式 w_i=m_i(Y_i)pHat(Y_i)W_Yi。为了链式重采样或积分，所有输入映到目标域后的支持集必须联合覆盖 pHat。常让一个输入直接由当前目标 sampler 生成，并使用 Jacobian=1 的恒等映射，作为 canonical sample。

## 5.3 跨域 MIS

单域广义 balance heuristic 用各输入 pHat_i 作为未知 PDF 的代理。跨域时，pHat_i 定义在源域 Omega_i，无法直接在目标域样本 y 上求值。若真实 PDF 已知，理想权重应使用每个映射后随机变量 Y_i 的 PDF：

```text
m_i(y) = p_Yi(y) / sum_j p_Yj(y)                            (5.7)
```

要得到 p_Yi(y)，先用逆映射把 y 移回 x_i=T_i^-1(y)，再乘逆映射的 Jacobian。

@@PAGE 32
映射后 PDF 可写成：

```text
p_Yi(y) = p_Xi(T_i^-1(y)) * |(T_i^-1)'(y)|                 (5.8)
```

若 y 无法逆向映回源域，则该项为零。真实 p_Xi 不可知时，用 pHat_i 代理，定义“来自 i 的目标密度”：

```text
pHatFrom_i(y) = pHat_i(T_i^-1(y)) * |(T_i^-1)'(y)|         (5.9)
```

映射失败或逆向样本不在 X_i 支持集时取零。于是跨域广义 balance heuristic 为：

```text
m_i(y) = pHatFrom_i(y) / sum_j pHatFrom_j(y)                (5.10)
```

加入置信度：

```text
m_i(y) = c_i pHatFrom_i(y) / sum_j[c_j pHatFrom_j(y)]       (5.11)
```

这些权重在所有能覆盖 y 的输入之间总和为 1。图 5.1 表明：多个源域经 shift mapping 后可能在目标域中不均匀重叠，MIS 的作用就是保证每个目标点的总覆盖权重恰好为 1。

Balance heuristic 对少量候选很稳健，但总成本为 O(M^2)。后文会介绍更廉价的替代方案。作者再次强调：先实现正确、原理清楚但较慢的版本，往往能显著缩短总开发时间。

@@PAGE 33
## 算法 5：广义 Balance Heuristic

```text
pHatFrom(j, y):
    x_j, Jinv = inverseShift(j, y)
    if shift succeeded:
        return pHat_j(x_j) * Jinv
    return 0

pHatFromOptimized(j, x, Jforward):
    return pHat_j(x) / Jforward

GeneralizedBalance(i, y, sourceX, sourceJ):
    numerator = c_i * pHatFromOptimized(i, sourceX, sourceJ)
    denominator = numerator
    for j != i:
        denominator += c_j * pHatFrom(j, y)
    if denominator == 0: return 0
    return numerator / denominator
```

对空样本必须返回零，避免 0/0。正向已知来自输入 i 时，可以用 pHat_i(x)/|T_i'(x)| 代替一次逆映射。

@@PAGE 34
# 第 6 章 ReSTIR 路径追踪

本章依据 ReSTIR PT，把广义 RIS 应用于一般全局光照路径采样。首先形式化路径积分，然后对路径树应用 RIS，讨论 shift mapping 的设计，并介绍适合实时 GPU 的高效 hybrid shift；最后简述对参与介质的扩展。

## 6.1 路径积分

光路可以包含任意次反弹。若 A 为全部场景表面的集合，全局光照积分跨越所有路径长度和面积乘积空间：

```text
L(x1 -> x0) = sum_D integral_A^(D-1)
    product_j [ fs(x_j+1 -> x_j -> x_j-1)
                * G(x_j <-> x_j+1)
                * V(x_j <-> x_j+1) ]
    * Le(x_D -> x_D-1) dx2...dxD                           (6.1)
```

一条包含 D-1 次反弹的面积测度路径写成：

```text
[x0, x1, x2, x3, ..., xD]                                  (6.2)
```

## 6.2 在路径追踪器中使用 RIS

带有每顶点 NEE 的路径追踪器会生成一棵路径树。我们希望用 RIS 从整棵树中选出一条路径，再送入 ReSTIR。

把同一棵路径树中的 1 至 k 次反弹路径记为 x_1...x_k，仍使用 w_i=m_i(x_i)pHat(x_i)W_xi。不同长度路径属于互不相交的子空间，因此此处 m_i=1。通常令 pHat(x_i)=f(x_i)，路径 PDF 可由路径追踪器直接获得，因此 W_xi=1/p(x_i)。

@@PAGE 35
路径追踪器通常通过传统 MIS 合并 NEE 与 BSDF 命中光源两种技术。放入 RIS 时，两类灯光样本都作为候选路径；同一长度的两种技术会互相竞争，所以 m_i 需包含另一种灯光采样技术的 PDF，而不再恒等于 1。

## 6.3 复用路径样本

最简单的方法是在面积测度下使用恒等式式重连接：

```text
T([x0, x1, x2, ..., xD]) = [y0, y1, x2, ..., xD]           (6.3)
```

它保留整个自由顶点序列 [x2...xD]，类似直接光照。ReSTIR GI 使用了这种思路。严格实现需要把 y1 重新连接到 x2，重新评估两个 BSDF、一个几何项，并追踪一条可见性光线。

为提高速度，ReSTIR GI 会预先保存沿 x2->x1 的出射辐射，并在复用到 x2->y1 时假定它不变。该假设只对 Lambertian 等有限材质成立，因此属于有偏近似；若 x2 是镜面顶点，就无法得到忠实结果。严格无偏版本可收敛到参考结果，但复用含镜面顶点的路径有时反而增大方差。

## 6.4 什么是好的 Shift Mapping

假设各像素已有质量相近的 sampler，从像素 i 映到像素 j 时，理想映射应让映射后样本的分布接近像素 j 自身的分布：

```text
pBar_j(T(x)) * |dT/dx| approximately equals pBar_i(x)       (6.4)
```

面积测度恒等重连接对远处漫反射 x2 通常有效；如果 x1、y1 或 x2 是镜面/低粗糙度表面，目标贡献可能差异巨大。重连接距离若从很短变得很长，几何项比值也可能爆炸。好的映射应避免这些情况，同时尽量保留更多原路径顶点，因为重合段具有单位 Jacobian 且贡献项相同。

@@PAGE 36
图 6.1 展示重连接失败：源路径在高光表面满足接近镜面方向的高贡献条件，但目标像素重新连接后偏离高光瓣，BSDF 接近零。

## 6.5 常见 Shift Mapping

若相邻像素亮度变化平滑，并令 pHat 近似 f，可用贡献函数检查映射质量：

```text
f_j(T(x)) * |dT/dx| approximately equals f_i(x)             (6.5)
```

这与梯度域渲染使用的条件一致，因此梯度域开发的映射也可用于路径重采样。

Gradient-Domain Path Tracing 的一种映射会逐顶点构建偏移路径：在每个源顶点复制切线空间 half-vector，再按对应的反射或折射方向追踪下一个偏移顶点。遇到源路径中连续两个被判定为漫反射的顶点后，将偏移路径重新连接回源路径。为了可逆，偏移路径对应顶点也必须满足漫反射条件。

Half-vector shift 能较好保持近镜面顶点的路径吞吐，因为重要性采样的高光反射方向集中在完美镜面方向附近。但反弹增加时，偏移顶点会逐渐偏离源路径，所以仍应在材质允许时尽早重连接。

更复杂的全局方法如 manifold exploration 能在两个漫反射顶点之间求解整段镜面链，更好保持贡献，但计算成本也更高。

@@PAGE 37
## 6.6 面向实时渲染的高效 Hybrid Shift

ReSTIR PT 提出的 hybrid shift 面向 GPU，和梯度域路径追踪一样延迟重连接，但有三点区别：

1. 在源路径上预先确定重连接顶点；偏移时使用 random replay 重新追踪前半段，因此无需保存完整源路径。Random replay 复制源路径每次反弹使用的随机数，通常等价于复制 half-vector、方向或 NEE 光源位置。
2. 增加距离条件，避免产生过短的重连接线段。
3. 只根据实际采样到的材质 lobe 粗糙度分类，适合多层材质。

每像素只需在普通 Reservoir 数据外保存一个重连接顶点和 RNG seed，内存为常量。相比简单重连接，它能显著改善高光和折射材质。

为保证可逆，源路径候选重连接顶点 x_k 必须满足距离条件：

```text
min(|x_k-x_k-1|, |x_k-y_k-1|) >= dMin                    (6.6)
```

以及粗糙度条件：

```text
min(alpha_xk-1(l_k-1), alpha_yk-1(l'_k-1),
    alpha_xk(l_k)) >= alphaMin                              (6.7)
```

alpha 衡量本次采样 lobe 的粗糙度；漫反射可设为很大值。多 lobe 顶点可选择粗糙度最大的相关 lobe。图 6.2 展示 hybrid shift：偏移路径先通过 random replay 生成，在最早满足距离和粗糙度条件的位置重新连接到源路径。

@@PAGE 38
初次追踪源路径时，保存最小的 k>=2，使：

```text
|x_k-x_k-1| >= dMin                                        (6.8)
min(alpha_xk-1(l_k-1), alpha_xk(l_k)) >= alphaMin           (6.9)
```

偏移时 random replay 生成到 y_k-1，再连接 x_k。目标侧同样必须满足距离和粗糙度条件：

```text
|x_k-y_k-1| >= dMin                                        (6.10)
min(alpha_yk-1(l_k-1), alpha_xk(l_k)) >= alphaMin           (6.11)
```

还必须确认目标路径不存在更早的 k' 也满足同样条件；否则当目标路径反过来作为源路径时会选择不同重连接点，破坏可逆性。不可逆样本在 RIS 中权重为零。

### 带 Lobe/Technique Tag 的扩展路径

为了让路径空间与生成它的随机数序列一一对应，ReSTIR PT 给路径样本附加 lobe 和灯光采样技术标签：

```text
xBar = [x0, (x1,l1), (x2,l2), ..., (xD-1,lD-1), xD]       (6.12)
```

l_j 标识顶点使用的采样 lobe。若光源顶点来自 NEE，则用特殊标签表示，并在前一顶点包含全部 lobe。Random replay 依靠这些标签精确重建同一类子路径。重连接时必须复制对应 lobe 索引；目标顶点不存在该 lobe 时映射失败。

@@PAGE 39
扩展路径把路径积分拆成所有路径长度以及所有 lobe/technique 序列之和。扩展路径的 integrand 包含传统灯光采样 MIS 权重，并且只评估被选 lobe 的部分路径贡献。

### Primary Sample Space

ReSTIR PT 用 Primary Sample Space（PSS）参数化路径，带来两个优势：

1. 路径 integrand 可写成每次反弹 f/p 的乘积，也就是路径追踪器直接维护的 throughput；初始候选在 PSS 中 PDF 恒为 1，避免单独追踪 f 和 p 导致浮点上溢。
2. Random replay 部分在 PSS 中具有单位 Jacobian，只需为最终重连接计算 Jacobian；若使用路径空间参数化，每个重放顶点都需计算 Jacobian 项。

PSS 路径积分写成：

```text
I = sum_D integral_U_D F(uBar) duBar                        (6.14)
```

uBar 是生成对应长度路径的随机数序列，U_D 是单位超立方体。F(uBar) 等于扩展路径贡献除以路径空间 PDF。

固体角参数化下，重连接的局部 Jacobian 包含目标和源顶点余弦比，以及两条连接线段长度平方比：

```text
|d omega_y / d omega_x|
  = |cosTheta_y / cosTheta_x|
    * |x_k-x_k-1|^2 / |x_k-y_k-1|^2                       (6.15)
```

@@PAGE 40
PSS 中 hybrid shift 的完整 Jacobian 等于重连接阶段的局部 Jacobian。映射会改变 x_k-1 和 x_k 对应的随机数，其行列式可分解为两个局部项（若 x_k 本身是光源顶点，则不存在后一个项）：

```text
|du_y/du_x| = |du_y,k-1 / du_x,k-1| * |du_y,k / du_x,k|   (6.16)
```

每个局部项由源/目标路径上 lobe 与方向的联合 PDF 比值以及几何映射 Jacobian 构成。虽然重连接后某段方向相同，其采样 PDF 仍可能不同，因为前一个出射方向不同。

### ReSTIR PT Reservoir 中需要保存的路径信息

Reservoir 不保存整条路径，而保存足以重放和重连的紧凑描述：

- 重连接顶点，包括入射方向 omega、沿该方向的辐射估计 L。
- 三角形 ID 与重心坐标，用于在静态或动画几何上恢复顶点。
- 重连接两侧的 lobe 标签。
- 两个 RNG seed：一个重放前半路径，一个恢复后半路径或相关采样。
- 重连接深度 k 与可复用的源侧 Jacobian 部分 J。
- 选中样本 Y、无偏贡献权重 W_Y、weightSum 与置信度 c。

用第一个 seed 重放偏移子路径并得到 throughput beta；恢复重连接顶点后，重新评估连接处 BSDF、方向 PDF、灯光采样 MIS 和辐射 L，得到目标函数与路径贡献。预存源侧 J 可避免下一轮复用时重复计算。

@@PAGE 41
## 算法 6：ReSTIR PT Reservoir

```text
Reservoir:
    Y:
        RcVertex:
            omega, radianceL
            triangleId, barycentrics
            lobeBefore, lobeAfter
        seedBefore, seedAfter
        reconnectDepthK
        cachedJacobianPartJ
    W_Y = 0
    weightSum = 0
    confidence = 0
```

本页随后转入体渲染扩展。

## 6.7 体渲染

参与介质使每次反弹的积分域从表面 A 扩展到表面与体积的并集 M=A union V。路径贡献除表面 BSDF、几何项、可见性和发光外，还包含介质中的透射率、散射、吸收和体发射。

固定针孔相机的子像素方向后，沿主射线的积分既包含最近不透明表面的贡献，也包含从相机到该表面之间各个介质位置的吸收发光和入射散射。碰撞距离 z_1 决定首个体积或表面顶点 x_1。

@@PAGE 42
介质中的透射率为：

```text
T(x <-> y) = exp(- integral_0^z sigma_t(x + s omega) ds)    (6.22)
```

其中消光系数 sigma_t=sigma_s+sigma_a。一般非均匀介质中，这个积分没有廉价闭式表达式；Delta Tracking 能采碰撞距离，却可能没有可计算的结果 PDF。

Volumetric ReSTIR 使用简化 pHat：通过 ray marching 近似透射率，步长在速度与采样方差间取舍；候选阶段可使用低分辨率体数据节省内存，并让分段常量介质的透射分布可以解析反演。精确透射率留到最终着色阶段求值。

跨像素复用时，可复制碰撞距离 z_1 来构造目标像素的 x_1。后续路径有两种方式：

- **Vertex reuse**：直接复制顶点序列 [x2...xD]，速度快，但几何奇异性会产生大量 firefly。
- **Direction reuse**：复制各段散射方向与碰撞距离，重新追踪偏移路径，最后重新连接光源；更慢，但默认质量更稳健。

可把 ReSTIR PT 的 hybrid shift 思想用于体积：当重连接线段足够长时尽早重连，并用 random replay 替代机械的距离/方向复制。

@@PAGE 43
体渲染章节在此收束。距离样本通常只需要一个随机数，因此很适合用 random replay 重建；关键仍是保持映射可逆、正确处理透射率近似，并避免几何或介质密度比值形成离群权重。

@@PAGE 44
# 第 7 章 让 ReSTIR 更快

ReSTIR 的主要吸引力之一，是为实时渲染提供高质量采样，因此高性能实现极为重要。但优化前首先要问：究竟优化什么？

ReSTIR 本质上是一类通用采样技术，通常用“采样效率”综合衡量样本成本和质量。作为重采样方法，它的效率取决于选择哪些邻居、如何设置 MIS 权重，也取决于缓存、分支、内存带宽等底层实现。因此本章把优化分为采样器优化和底层优化。

## 7.1 采样器优化

RIS/ReSTIR 可视为使用 MIS 组合多个估计器。每个被借用的像素，都是当前像素可以采样的一个不同估计器。人们常通过 BSDF sampling 和 light sampling 学习 MIS，但 MIS 几乎可以组合任何估计器，包括这些看似奇怪的“邻居 Reservoir 估计器”。

重要但容易忽视的一点：MIS 组合估计器并不保证质量一定提高。BSDF 与 light sampling 的组合几乎总有帮助，容易让人误以为邻居复用也必然有益。实际上，邻居可能是当前像素极差的估计器。

图 7.1 的海葵具有细长结构，相邻像素的表面法线可能近乎相反，两者共有的高贡献路径集合接近空集。复用这类邻居往往增加而非降低噪声。

可以根据主命中点的法线、深度和材质属性拒绝明显无关的邻居。只要决定不查看 Reservoir 中具体随机样本或权重，这类基于积分域属性的拒绝通常不会引入偏差。

@@PAGE 45
## 7.1.1 把邻居拒绝看成近似 MIS

严格无偏要求准确理解每个样本可由哪些像素或技术生成。若跟踪不正确，很容易在积分域的某些部分重复计数或漏计，表现为异常变亮或变暗。候选来自不同像素时，通常需要正确 MIS 权重。

Balance heuristic 的 O(M^2) 成本会快速增长。一种性能优化是使用不严格正确的常量 1/M 权重，再通过邻居拒绝减少偏差。它近似 Veach 的 cutoff heuristic：假定两个不兼容像素在对方积分域中的 PDF 极低，直接丢弃对应技术。

邻居拒绝能形成廉价但有偏的近似权重。若需要完全无偏，还可使用后续的 Contribution MIS 或 Pairwise MIS。

## 7.1.2 Contribution MIS

GRIS 允许只对最终被选样本计算一个修正权重。设重采样用的权重为：

```text
w_i = m_i(T_i(X_i)) * pHat(T_i(X_i))
      * W_i * |dT_i/dX_i|
```

被选索引为 s，输出 Y=T_s(X_s)。只要 correction weight c_i(y) 在所有能覆盖 y 的输入间总和为 1，就可使用：

```text
W_Y = [c_s(Y)/m_s(Y)] * sum_j(w_j) / pHat(Y)               (7.1)
```

这样 m_i 可以选得很廉价，而只为最终选中样本支付更精确的 c_s 计算。

@@PAGE 46
原始 ReSTIR DI 的一种做法是重采样阶段使用常量 m_i=1/M，最终使用广义 balance heuristic 计算 c_s。因为只需为一个选中样本求 correction，复杂度从 O(M^2) 降为 O(M)。

这种方法在直接光照中通常可用，但当积分域差异很大时，会给选中样本的最终贡献增加显著噪声；参与介质中尤其明显。此外，只有使用正确的重采样 MIS 权重 m_i，样本分布本身才会收敛到目标 PDF。

## 7.1.3 Pairwise MIS

Pairwise MIS 假设 M 个技术中有一个 canonical 技术，它覆盖完整积分域且质量相对可靠。空间重采样中，canonical 技术就是当前像素，其他技术是邻居像素。

核心思想是：每个非 canonical 技术只与 canonical 技术两两比较。若 canonical 索引为 c，基本形式把 p_i 与 p_c 放进二技术 balance heuristic，然后在所有配对间平均。

朴素配对会让 canonical 样本权重过大；当所有技术完全相同时，canonical 权重会是其他技术的 M-1 倍。需要把 p_c 按 M-1 下调，使相同技术最终得到相同权重。

## 7.1.3 的公式含义

对非 canonical 输入，权重只需比较“该邻居生成 y 的能力”与“当前像素生成 y 的能力”；对 canonical 输入，则把它与每个邻居的配对结果累加。这样把 O(M^2) 降到 O(M)。

@@PAGE 47
真实 PDF 不可知时，使用 pHat 代理，就得到广义 Pairwise MIS。为防止近似 pHat 让差邻居获得过高权重，可给 canonical 样本一个固定的防御份额，形成 defensive pairwise MIS。

直观上，defensive 版本是在两种策略之间插值：

- 一部分权重无条件留给当前像素 canonical sample，保证稳健性。
- 剩余权重根据每个邻居与 canonical 的成对 PDF 比较进行分配。

置信度 c_i 也可以代替显式样本数 M。若所有 pHatFrom_i 都相同，非 defensive 形式退化为按置信度比例分配；defensive 形式则额外为 canonical 保留 c_c/sum(c) 的基础份额。

带 shift mapping 时，公式中的 pHat_i(y) 应替换成 pHatFrom_i(y)，即把 y 逆映到源域并乘逆 Jacobian 后的目标函数。

ReSTIR PT 观察到，O(M) 的 Pairwise MIS 在收敛行为上可接近 O(M^2) balance heuristic，因此把 defensive 形式作为 GRIS 空间重采样的默认选择。

@@PAGE 48
## 算法 7：广义 Defensive Pairwise MIS

算法首先定义 pHatFrom(j,y)：逆向 shift 到源域 j，成功则返回 pHat_j(x_j) 乘逆 Jacobian，否则返回 0。已知 y 由输入 i 正向映射而来时，可用 pHat_i(x)/Jforward 快速计算。

对于 canonical 样本：

```text
cTotal = sum_k c_k
mCanonical = cCanonical / cTotal
for each neighbor j:
    numerator = cCanonical * pHatCanonical(y)
    denominator = numerator
                + (cTotal-cCanonical) * pHatFrom(j,y)
    mCanonical += (c_j/cTotal) * numerator/denominator
```

对于非 canonical 样本 i：

```text
numerator = (cTotal-cCanonical) * pHatFromOptimized(i,...)
denominator = numerator + cCanonical * pHatCanonical(y)
m_i = (c_i/cTotal) * numerator/denominator
```

空样本返回 0，避免 0/0。

## 7.1.4 有偏 MIS 权重

理解偏差来源后，可以为了效率有意识地近似 MIS。像素 i 与 j 复用时，balance heuristic 不仅需要 p_i(X_i)、p_j(X_j)，还需要在“并未生成该样本”的另一像素中评估 p_i(X_j) 或 p_j(X_i)。

@@PAGE 49
若样本是一条路径，在另一像素中重新评估它通常意味着新增光线追踪。Temporal Reuse 更麻烦：在上一帧像素 j 中评估当前帧样本 X_i，可能需要上一帧完整 BVH，这在工程上很不理想。

可选近似包括：用当前帧 BVH 代替上一帧 BVH；假设 p_j(X_i)=0；使用上一帧材质数据但假定可见性未变化等。

对二技术 balance heuristic：

```text
m_i(X_i) = p_i(X_i) / [p_i(X_i) + p_j(X_i)]                (7.10)
```

若用近似 pTilde_j 替换真实 p_j：

- pTilde_j > p_j 时，m_i 被压低，产生变暗偏差。
- pTilde_j = p_j 时，该样本无额外偏差。
- pTilde_j < p_j 时，m_i 被抬高，产生变亮偏差。

近似误差可能随样本改变，因此同一图像不同区域会同时出现偏亮和偏暗。若简单假定 p_j(X_i)=0，就相当于声称任何本帧新样本都不可能在上一帧被选中；静态场景中显然不成立，会让本帧样本被重复计数，从而产生变亮偏差。

## 7.2 底层优化

优化必须先定义硬件相关目标，而不是笼统地追求“更快”。

@@PAGE 50
可优化的指标包括：

- 最小化每像素阴影光线数。
- 最小化追踪的完整路径数量。
- 最大化昂贵路径样本的复用次数。
- 降低最终着色的跨像素相关性，使降噪器更有效。
- 最大化并行度和流式复用，充分利用 GPU。
- 减小 Reservoir 与中间缓冲区。
- 降低显存带宽。
- 降低执行分歧，保持 warp/wave 中线程活跃。
- 降低内存访问分歧，避免缓存抖动。
- 直接降低帧时间。Temporal Reuse 依赖帧率，因此有时降低单帧质量、换取更快帧率和更多时间复用，最终质量反而更好。
- 降低寄存器压力及其他传统 GPU 成本。

部分优化无偏，部分天然有偏，还有一些只有借助更复杂数学才能无偏。应根据应用目标选择。

把 Reservoir 稀疏存到世界空间网格，可以近似线性地减少射线和内存，但每像素 Reservoir 本身并不算大；网格复用会引入难以控制的偏差和相关性，未必是最划算的优化。

### 7.2.1 ReSTIR DI 的 Sample Tiling

Amusement Park 场景拥有三百多万个自发光三角形。朴素实现中，仅随机挑选灯光候选就可能花费约 25 ms，因为每次随机选择都访问完全不同的缓存行，导致缓存抖动。

@@PAGE 51
在 1920x1080 图像中约有两百万像素；每像素最终只选一个灯光，本帧实际用于着色的灯光最多也只占全部灯光的一部分。若能把本帧相关灯光整理到更紧凑的内存区域，就能减少缓存抖动，但直接预测哪些灯光相关很难。

一个简单思路是分层抽样：每帧只从不同的灯光子集采样。若始终固定同一子集，其他灯永远不会发光；若逐帧轮换子集，长期仍可覆盖全部灯光，而且重要灯光可通过 ReSTIR 历史继续影响后续帧。

Sample Tiling 把该思想限制到屏幕小块：一个 16x16 tile 只有 256 个像素，即使每像素最终选不同灯，也最多需要 256 个灯。可以先为 tile 准备一个 1024 或 2048 灯的候选子集，然后所有像素都只访问这个紧凑集合。

算法：

1. 每帧生成多个灯光子集 S，每个子集按照原始灯光 proposal（如按功率）从全场景采样。
2. 每个 8x8 或 16x16 屏幕 tile 选择一个本帧灯光子集。
3. tile 中各像素需要候选时，从该子集内均匀挑选；若子集有 N 个元素，条件概率为 1/N，同时完整 PDF 必须保留生成子集时的概率。

经验上，128 个子集、每个包含 1024 个灯光样本，已能覆盖广泛场景。灯光总数远小于约 128000 时，构建 tile 的不足 0.1 ms 开销可能超过缓存收益。

@@PAGE 52
这种预计算 Sample Tiling 可视为一种退化 RIS：目标函数就是原本的灯光 proposal p。它的本质是用 RIS 重新组织采样过程，以换取更一致的缓存访问。这一思想不只适用于直接光照。

## 7.2.2 多种解析光类型

生产应用常同时拥有球形灯、矩形灯、圆柱灯、三角灯、环境贴图、线光源、点光源、聚光灯和网格灯等。每种灯都可能有不同且昂贵的采样代码。

若同一 warp 中每个像素随机选择不同灯型，GPU 可能串行执行多种采样分支，并访问分散的资源。可以先按灯型以一致方式预采样位置，再把这些预采样点送入 Sample Tiling。内层像素循环只处理结构完全相同的“点状候选”；每类几何灯的复杂采样过程只在每帧预处理阶段成批执行。

## 7.2.3 加速 Hybrid Shift

Hybrid shift 同时包含 random replay、路径追踪、可见性光线和 BSDF 重评估。把它们全部放进一个大 kernel，容易产生：

- 复杂嵌套逻辑带来的高寄存器占用、低 occupancy 和寄存器溢出。
- 严重执行分歧；即便只有少量像素需要继续追踪，也可能拖慢整个 warp。

建议拆成更小 kernel：专用 random replay kernel 只做路径追踪；专用 reconnection kernel 只做 BSDF 重评估和可见性。

@@PAGE 53
第二项关键优化是 stream compaction：只为真正存在追踪任务的像素分配线程。许多路径样本不需要 random replay 即可重连，压紧任务后可以避免大量线程在 warp 中空转。

拆 kernel 会增加少量全局内存读写，因为 random replay 的中间 throughput 要写回供 reconnection 读取。但通常这一代价远小于 shader 时间下降。课程在 Veach Ajar 场景中观察到：首次拆分使相关时空重采样 shader 时间减少约 40%，stream compaction 又在此基础上减少约 40%。

@@PAGE 54
# 第 8 章 游戏集成经验

关于把 ReSTIR 集成到 CD Projekt Red《赛博朋克 2077》的 RT Overdrive 模式和《往日之影》资料片中的关键经验与结论，请参阅课程网站上的 Pawel Kozlowski 与 Giovanni De Francesco 演示文稿。讲义正文在本章仅提供这一指引。

@@PAGE 55
# 第 9 章 入门建议（上）

课程作者多年研究 ReSTIR，反复编写过原型、演示、SDK，并将算法集成到大型代码库。所有经验归结为一句话：从简单实现开始。

现有 ReSTIR 实现几乎都有初学者一时难懂的部分，类似每个人的第一个路径追踪器都可能出现差一个 pi 的错误。研究者仍在探索怎样最清晰地组织这类代码，因此最早的论文也未必是最佳入门材料。

**先建立同场景 Ground Truth。** 实现一个简单的蒙特卡洛路径追踪器，不必拥有高级重要性采样，但必须与 ReSTIR 使用同一套代码和场景。频繁比较参考结果，才能在偏差刚被引入时发现，而不是数月后才知道基础实现本身有错。

**先实现基本 RIS。** Talbot 的 RIS 相对直接且可无偏实现。如果基本 RIS 都不能收敛到参考结果，加入时空复用只会让错误更复杂。

**认真管理偏差。** 实时工程师常习惯接受近似，但时空样本复用会让偏差极快扩散，多反弹路径中的偏差还会以非常反常的方式出现。即使在游戏中，也必须主动量化和管理偏差。

**先 Spatial，后 Temporal。** 静态空间复用没有帧间场景变化，更容易验证，也应明显改善图像。确认后尽快加入 Temporal，因为二者交错带来的质量远高于只做 Spatial。

@@PAGE 56
**不要过早叠加高级选项。** RTXDI 中存在 checkerboard、sample permutation、boiling suppression 等大量选项，其中一些从未以无偏为目标，不同组合也未必都被充分验证。先证明基础链路正确，再只添加确实需要的优化。

**一个 Reservoir 通常只在一个点上有效。** 把同一 Reservoir 当作整个 voxel 的有效分布虽然可以节省资源，却很难避免偏差，也会放大 voxel 内部相关性。

**谨慎复用可见性。** 减少阴影光线是 ReSTIR 早期的重要吸引力，但可见性复用也是最难调试的偏差来源之一。建议在目标函数和 MIS 中先完整包含可见性，验证收敛后，再逐步复用或近似 ray query。

**ReSTIR 的加速来源不只一个。** 即使最终仍为选中样本重新追踪可见性，跨像素摊销候选和路径生成成本的收益依然存在。

**把 ReSTIR 看成积分域的稀疏采样。** 环境光探针可通过降低纹理分辨率来减少采样域复杂度；面对高维路径空间时，无法简单“降低分辨率”，但可以减少新建独立路径的次数，更多复用已有路径。

@@PAGE 57
# 参考文献（按引用顺序，上）

1. Benedikt Bitterli、Chris Wyman、Matt Pharr、Peter Shirley、Aaron Lefohn、Wojciech Jarosz，2020。Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting（用于动态直接光照实时光追的时空 Reservoir 重采样）。ACM Transactions on Graphics，SIGGRAPH 论文集，39(4)。DOI: 10/gg8xc7。
2. Yaobin Ouyang、Shiqiu Liu、Markus Kettunen、Matt Pharr、Jacopo Pantaleoni，2021。ReSTIR GI: Path Resampling for Real-Time Path Tracing（用于实时路径追踪的路径重采样）。Computer Graphics Forum 40(8)，17-29。DOI: 10/gqwmdx。
3. Daqi Lin、Chris Wyman、Cem Yuksel，2021。Fast Volume Rendering with Spatiotemporal Reservoir Resampling（使用时空 Reservoir 重采样的快速体渲染）。ACM TOG，SIGGRAPH Asia，40(6)，279:1-279:18。DOI: 10/grrjd6。
4. Daqi Lin、Markus Kettunen、Benedikt Bitterli、Jacopo Pantaleoni、Cem Yuksel、Chris Wyman，2022。Generalized Resampled Importance Sampling: Foundations of ReSTIR（广义重采样重要性采样：ReSTIR 的基础）。ACM TOG，SIGGRAPH，41(4)，75:1-75:23。DOI: 10/gqjn7b。
5. Chris Wyman、Alexey Panteleev，2021。Rearchitecting Spatiotemporal Resampling for Production（面向生产环境重构时空重采样）。High-Performance Graphics。DOI: 10/grrjkk。
6. Justin F. Talbot、David Cline、Parris Egbert，2005。Importance Resampling for Global Illumination（用于全局光照的重要性重采样）。Eurographics Symposium on Rendering，139-146。DOI: 10/gfzsm2。
7. Benedikt Bitterli，2022。Correlations and Reuse for Fast and Accurate Physically Based Light Transport（面向快速精确物理光传输的相关性与复用）。达特茅斯学院博士论文。
8. Jakub Boksansky、Paula Jukarainen、Chris Wyman，2021。Rendering Many Lights with Grid-Based Reservoirs（使用网格 Reservoir 渲染大量光源）。Ray Tracing Gems II，351-365。DOI: 10.1007/978-1-4842-7185-8_23。
9. NVIDIA，2021。NVIDIA RTX Direct Illumination。https://developer.nvidia.com/rtxdi
10. Alex Battaglia，2021。Sword and Fairy 7 is the cutting-edge PC exclusive nobody's talking about（《仙剑奇侠传七》：鲜有人讨论的前沿 PC 独占作品）。Eurogamer。
11. Andrew Burnes，2022。Portal with RTX Out Now: A Breathtaking Reimagining Of Valve's Classic With Full Ray Tracing & DLSS 3（《Portal with RTX》发布：用完整光追与 DLSS 3 重塑 Valve 经典）。NVIDIA。
12. Jiayin Cao，2022。Understanding The Math Behind ReSTIR DI（理解 ReSTIR DI 背后的数学）。
13. Julien Guertault，2022。Reading list on ReSTIR（ReSTIR 阅读清单）。
14. Shubham Sachdeva，2021。Spatiotemporal Reservoir Resampling (ReSTIR) - Theory and Basic Implementation（ReSTIR 理论与基础实现）。

@@PAGE 58
# 参考文献（中）

15. Jacco Bikker，2023。Lecture 14 - TAA & ReSTIR（第 14 讲：TAA 与 ReSTIR）。
16. Tomasz Stachowiak，2022。Global Illumination in 'kajiya' Renderer（kajiya 渲染器中的全局光照）。
17. Mr. Zyanide，2023。Jedi Outcast 集成效果分享。
18. Adam Badke，2021。Next Event Estimation via Reservoir-Based Spatio-Temporal Importance Resampling（通过基于 Reservoir 的时空重要性重采样进行下一事件估计）。Simon Fraser University 硕士论文。
19. Ege Ciklabakkal、Adrien Gruson、Iliyan Georgiev、Derek Nowrouzezahrai、Toshiya Hachisuka，2022。Single-Pass Stratified Importance Resampling（单遍分层重要性重采样）。Computer Graphics Forum 41(4)。DOI: 10.1111/cgf.14585。
20. Guillaume Boisse，2021。World-Space Spatiotemporal Reservoir Reuse for Ray-Traced Global Illumination（用于光追全局光照的世界空间时空 Reservoir 复用）。SIGGRAPH Asia Technical Communications，1-4。DOI: 10/grrjbg。
21. Xander Hermans，2022。The Effectiveness of the ReSTIR Technique When Ray Tracing a Voxel World（ReSTIR 在体素世界光线追踪中的有效性）。Utrecht University 硕士论文。
22. Fuyan Liu、Junwen Gan，2023。Light Subpath Reservoir for Interactive Ray-Traced Global Illumination（用于交互式光追全局光照的光源子路径 Reservoir）。Computers & Graphics 111，37-46。DOI: 10/grrjgw。
23. Shinji Ogaki，2021。Vectorized Reservoir Sampling（向量化 Reservoir 采样）。SIGGRAPH Asia Technical Communications，1-4。DOI: 10/grrjhq。
24. Stefan Krake，2021。hdRstr：基于 ReSTIR/RTXDI 的 Hydra Render Delegate。
25. Stefan Krake，2022。blRstr：基于 ReSTIR/RTXDI 的 Blender 渲染引擎。
26. Christoph Schied 等，2017。Spatiotemporal Variance-Guided Filtering: Real-Time Reconstruction for Path-Traced Global Illumination（时空方差引导过滤：路径追踪全局光照的实时重建）。High Performance Graphics。DOI: 10/ggd8dg。
27. Christoph Schied、Christoph Peters、Carsten Dachsbacher，2018。Gradient Estimation for Real-Time Adaptive Temporal Filtering（用于实时自适应时间滤波的梯度估计）。PACMCGIT 1(2)，24:1-24:16。DOI: 10/ggd8dh。
28. Takafumi Saito、Tokiichiro Takahashi，1990。Comprehensible Rendering of 3-D Shapes（三维形状的可理解渲染）。Computer Graphics 24(4)，197-206。DOI: 10/fp3t53。
29. Jiri Vorba、Johannes Hanika、Sebastian Herholz、Thomas Muller、Jaroslav Krivanek、Alexander Keller，2019。Path Guiding in Production（生产环境中的路径引导）。SIGGRAPH Courses。DOI: 10.1145/3305366.3328091。
30. Min-Te Chao，1982。A General Purpose Unequal Probability Sampling Plan（通用非等概率采样方案）。Biometrika 69(3)，653-656。DOI: 10/fd87zs。
31. Eric Veach，1997。Robust Monte Carlo Methods for Light Transport Simulation（用于光传输仿真的稳健蒙特卡洛方法）。斯坦福大学博士论文。
32. Eric Veach、Leonidas J. Guibas，1995。Optimally Combining Sampling Techniques for Monte Carlo Rendering（为蒙特卡洛渲染最优组合采样技术）。SIGGRAPH，419-428。DOI: 10/d7b6n4。

@@PAGE 59
# 参考文献（下）

33. Ivo Kondapaneni、Petr Vevoda、Pascal Grittmann、Tomas Skrivan、Philipp Slusallek、Jaroslav Krivanek，2019。Optimal Multiple Importance Sampling（最优多重重要性采样）。ACM TOG，SIGGRAPH，38(4)，37:1-37:14。DOI: 10/gf5jbj。
34. E. R. Woodcock、T. Murphy、P. J. Hemmings、T. C. Longworth，1965。Techniques Used in the GEM Code for Monte Carlo Neutronics Calculations in Reactors and Other Systems of Complex Geometry（GEM 代码中用于反应堆及其他复杂几何系统蒙特卡洛中子学计算的技术）。Argonne National Laboratory。
35. Henrik Wann Jensen，1996。Global Illumination Using Photon Maps（使用 Photon Map 的全局光照）。Eurographics Workshop on Rendering，21-30。DOI: 10/fzc6t9。
36. Hao Qin、Xin Sun、Qiming Hou、Baining Guo、Kun Zhou，2015。Unbiased Photon Gathering for Light Transport Simulation（用于光传输仿真的无偏 Photon Gathering）。ACM TOG 34(6)。DOI: 10/f7wrc6。
37. Tizian Zeltner、Iliyan Georgiev、Wenzel Jakob，2020。Specular Manifold Sampling for Rendering High-Frequency Caustics and Glints（用于渲染高频焦散和闪光的镜面流形采样）。ACM TOG，SIGGRAPH，39(4)。DOI: 10/gg8xc8。
38. Jaakko Lehtinen、Tero Karras、Samuli Laine、Miika Aittala、Fredo Durand、Timo Aila，2013。Gradient-Domain Metropolis Light Transport（梯度域 Metropolis 光传输）。ACM TOG，SIGGRAPH，32(4)，95:1-95:12。DOI: 10/gbdghd。
39. Markus Kettunen、Marco Manzi、Miika Aittala、Jaakko Lehtinen、Fredo Durand、Matthias Zwicker，2015。Gradient-Domain Path Tracing（梯度域路径追踪）。ACM TOG，SIGGRAPH，34(4)，123。DOI: 10/gfzrhn。
40. Binh-Son Hua、Adrien Gruson、Victor Petitjean、Matthias Zwicker、Derek Nowrouzezahrai、Elmar Eisemann、Toshiya Hachisuka，2019。A Survey on Gradient-Domain Rendering（梯度域渲染综述）。Computer Graphics Forum 38(2)，455-472。
41. Yusuke Tokuyoshi，2023。Efficient Spatial Resampling Using the PDF Similarity（利用 PDF 相似性的高效空间重采样）。PACMCGIT 6(1)，1-19。
42. Marco Manzi、Fabrice Rousselle、Markus Kettunen、Jaakko Lehtinen、Matthias Zwicker，2014。Improved Sampling for Gradient-Domain Metropolis Light Transport（梯度域 Metropolis 光传输的改进采样）。ACM TOG，SIGGRAPH Asia，33(6)。DOI: 10/f6r2hp。
43. Anton S. Kaplanyan、Johannes Hanika、Carsten Dachsbacher，2014。The Natural-Constraint Representation of the Path Space for Efficient Light Transport Simulation（面向高效光传输仿真的路径空间自然约束表示）。ACM TOG，SIGGRAPH，33(4)，102:1-102:13。DOI: 10/f6cz85。
44. Wenzel Jakob、Steve Marschner，2012。Manifold Exploration: A Markov Chain Monte Carlo Technique for Rendering Scenes with Difficult Specular Transport（流形探索：渲染困难镜面传输场景的 MCMC 技术）。ACM TOG，SIGGRAPH，31(4)，58:1-58:13。DOI: 10/gfzq4p。

@@PAGE 60
# 缩略语表

- **GPU**：Graphics Processing Unit，图形处理器。
- **MC**：Monte Carlo，蒙特卡洛。
- **MIS**：Multiple Importance Sampling，多重重要性采样。
- **NEE**：Next-Event Estimation，下一事件估计。
- **PDF**：Probability Density Function，概率密度函数。
- **ReSTIR**：Reservoir-Based Spatiotemporal Importance Resampling，基于 Reservoir 的时空重要性重采样。
- **RIS**：Resampled Importance Sampling，重采样重要性采样。
- **RNG**：Random Number Generator，随机数生成器。
- **WRS**：Weighted Reservoir Sampling，加权 Reservoir 采样。

@@PAGE 61
# 符号表

- **m_i(X)**：随机变量 X 的 MIS 权重。
- **w_i**：从候选列表 X_1...X_M 中选择第 i 个候选时使用的重采样权重；实际选择概率为 w_i/sum(w)。
- **T**：Shift Mapping，把一个积分域中的样本确定地映到另一个积分域。
- **supp(X)、supp(f)**：随机变量 X 或函数 f 的支持集。
- **pHat(x)**：x 处的未归一化目标函数。
- **pBar(x)**：由 pHat 归一化得到的目标 PDF。
- **W_X**：随机变量 X 的无偏贡献权重。已知 X 的 PDF p(X) 时，可以使用 1/p(X)。

## 译名约定

为方便与论文、RTXDI 源码和 Shader 变量对照，本译本保留 Reservoir、Shift Mapping、Jacobian、Pairwise MIS、Contribution MIS、Sample Tiling、Random Replay、Hybrid Shift、Path Guiding、Firefly 等常用英文术语；首次出现时给出中文解释。pHat 对应论文中的目标函数，不应误称为已归一化目标 PDF；W_X 对应 unbiased contribution weight，不是可以在任意样本上重新求值的 PDF 倒数函数。
