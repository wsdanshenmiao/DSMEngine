# 实现延迟渲染管线 (Deferred Render Pipeline)

**状态**: completed  
**创建日期**: 2026-06-27  
**完成日期**: 2026-06-27  
**作者**: Codex

## 摘要 (Summary)

基于正向渲染管线实现了延迟渲染管线。G-Buffer 通过 MRT 输出 albedo+metallic、view-space normal、roughness+occlusion，全屏 DeferredLightingPass 读取 G-Buffer 和 tile-based 光照数据进行着色。

## 背景 (Context)

见原始设计。

## 实施计划 (Implementation Plan)

### 里程碑 1: GBuffer 着色器

**文件**: DSMEngine/Shaders/DeferredShader/Passes/GBufferPass.hlsl

- GBufferPassVS: 从 gMeshBuffer 读取 world/worldIT，输出 posCS, normalWS, uv, posWS
- GBufferPassPS: 采样 bindless 纹理，计算法线贴图（可选 tangent 分支），输出 view-space 法线
- 输出 3 个 RT: RT0=albedo+metallic, RT1=encoded normal, RT2=roughness+occlusion

### 里程碑 2: GBufferPass C++

**文件**: DSMEngine/Runtime/Render/Renderer/DeferredRenderer/GBufferPass.h

- 完整重写：支持 tangent/no-tangent 变体、反向 Z、双面材质
- PSO 缓存系统（按 HasTangent × ReverseZ 索引）
- 绑定布局：MeshBuffer(t0) + MaterialBuffer(t1) + PassCB(b0) + PushConstants(b1) + Bindless(space1)
- 遍历 Opaque 物体渲染 G-Buffer
- 应用 TAA jitter

### 里程碑 3: DeferredLightingPass 修复

**文件**: DSMEngine/Runtime/Render/Renderer/DeferredRenderer/DeferredLightingPass.h

- 添加 TAA jitter 到投影矩阵（与 GBuffer pass 一致）
- 添加 TaaPass.h 引用
- 验证寄存器绑定正确性

### 里程碑 4: 验证

- 编译通过 ✓
- 运行启动正常 ✓

## 验证 (Validation)

| 验证项 | 结果 |
|--------|------|
| xmake 编译 | 通过 |
| xmake run PBR 启动 | 正常启动，GPU 选择正确 |

## 进展 (Progress)

- [x] 阅读并理解现有 Forward 管线架构
- [x] 分析延迟管线骨架代码
- [x] 创建 ExecPlan
- [x] 里程碑 1: GBuffer 着色器
- [x] 里程碑 2: GBufferPass C++
- [x] 里程碑 3: DeferredLightingPass 修复
- [x] 里程碑 4: 编译验证

## 意外与发现 (Surprises & Discoveries)

1. SSAO 着色器期望**视图空间**法线，初始 G-Buffer 实现输出世界空间法线。通过 transform normalWS→view-space 修复。
2. DeferredRenderPipeline.h 和 DeferredLightingPass.h 已事先编写了大部分结构，减少了实现量。
3. GBufferPass.h 原骨架仅有测试三角形渲染，需完全重写为 Scene Object 渲染。
4. TAA jitter 必须在 GBufferPass 和 DeferredLightingPass 中使用一致的 jittered projection。

## 决策记录 (Decision Log)

| 决策 | 选择 |
|------|------|
| 法线编码空间 | View-space（SSAO 依赖） |
| Normal 纹理格式 | RG32_FLOAT（保持向前兼容） |
| Tangent 支持 | USE_TANGENT 条件编译（与 LitPass 一致） |
| 透明物体 | Forward LitPass Transparent |

## 结果与复盘 (Outcomes & Retrospective)

### 结果
- 延迟管线完整实现，Pass 顺序: GBuffer → MotionVector → SSAO → Lighting(Compute) → DeferredLighting → Skybox → LitPass(Transparent) → TAA → PostEffect → Final
- 所有公共 Pass（SSAO, Skybox, TAA, MotionVector, Lighting, PostEffect, Final）无需修改
- 共享资源路径不变（RenderResource::CommonTextureSlot）
- 示例 Samples/PBR 已在 main() 中使用 DeferredRenderPipeline

### 后续建议
- 可以在渲染视口中添加 G-Buffer 可视化调试模式
- 考虑后续添加 Subsurface Scattering、Screen Space Reflection 等基于 G-Buffer 的后处理
- 透明物体的延迟渲染（Order-Independent Transparency）可作为后续优化方向
