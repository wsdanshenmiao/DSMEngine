// =============================================================================
// VRS_DX12_Example.cpp
// Direct3D 12 可变速率着色（Variable Rate Shading / coarse pixel shading）参考实现
//
// 性质：独立教学示例，**不参与 DSMEngine 的构建**（没有加入 xmake.lua / CMakeLists.txt）。
//       目的是演示 VRS 相关 API 的调用顺序、资源约束与常见坑，可对照移植到
//       Engine/Source/Runtime/Graphics/D3D12/ 下的 Device / CommandList 封装。
//
// 原始来源（官方规范与文档）：
//   [1] DirectX-Specs — Variable Rate Shading
//       https://microsoft.github.io/DirectX-Specs/d3d/VariableRateShading.html
//   [2] MS Learn — Variable-rate shading (VRS)
//       https://learn.microsoft.com/en-us/windows/win32/direct3d12/vrs
//   [3] ID3D12GraphicsCommandList5::RSSetShadingRate
//       https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist5-rssetshadingrate
//   [4] ID3D12GraphicsCommandList5::RSSetShadingRateImage
//       https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist5-rssetshadingrateimage
//   [5] D3D12_FEATURE_DATA_D3D12_OPTIONS6
//       https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options6
//   [6] D3D12_VARIABLE_SHADING_RATE_TIER
//       https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_variable_shading_rate_tier
//   [7] HLSL Shader Model 6.4（SV_ShadingRate）
//       https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-4-features-for-direct3d-12
//   [8] 官方样例 D3D12VariableRateShading
//       https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12VariableRateShading
// =============================================================================

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstdio>

using Microsoft::WRL::ComPtr;

// -----------------------------------------------------------------------------
// 0. 术语与编码速查（[1][2]）
// -----------------------------------------------------------------------------
// coarse pixel（粗像素）：一次像素着色调用覆盖的一块 N×M 像素，是 VRS 的调度单位。
// shading rate（着色率） ：D3D12_SHADING_RATE，编码为 (x << 2) | y：
//     x = 水平方向粗像素宽度指数，y = 垂直方向高度指数，取自 D3D12_AXIS_SHADING_RATE
//     { 1X = 0, 2X = 1, 4X = 2 }。X 轴放 bit[3:2]，Y 轴放 bit[1:0]。
//     枚举值（[2] 明确给出）：1X1=0x0 1X2=0x1 2X1=0x4 2X2=0x5 2X4=0x6 4X2=0x9 4X4=0xa
//     注意 2x4 表示「水平 2x、垂直 4x」，名字里的第一个数字是 X。
// tile（区块）          ：屏空间着色率图像的最小寻址单位，边长 8 或 16 纹素，
//     由 D3D12_FEATURE_DATA_D3D12_OPTIONS6::ShadingRateImageTileSize 查询（[5]）。
//     同一 tile 内、同一图元只能有一个着色率；粗像素不得跨越 tile 边界（[1]）。
//
// 三个着色率来源，按固定顺序用两个组合器串起来（[3][4]）：
//     postRasterizerRate = ApplyCombiner(Combiners[0], 命令列表基础率, 逐图元率)
//     finalRate          = ApplyCombiner(Combiners[1], postRasterizerRate, 屏空间率图[texel])
//     组合器：PASSTHROUGH / OVERRIDE / MIN / MAX / SUM，MIN 取 min(A,B) 即「更高质量」。

namespace vrs_example
{

// =============================================================================
// 1. 能力查询 —— 必须先做，Tier 决定了后面能用哪些机制
// =============================================================================
struct VrsCapabilities
{
    D3D12_VARIABLE_SHADING_RATE_TIER tier = D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
    bool     additionalRates = false;   // 2x4 / 4x2 / 4x4（单采样）是否可用
    uint32_t tileSize        = 0;       // 非 Tier 2 时为 0
    bool     perPrimitiveWithViewportIndexing = false;
    bool     sumCombiner     = false;   // VRS "sum" 组合器（OPTIONS10）

    bool SupportsPerDraw()    const { return tier >= D3D12_VARIABLE_SHADING_RATE_TIER_1; }
    bool SupportsImageBased() const { return tier >= D3D12_VARIABLE_SHADING_RATE_TIER_2; }
};

inline VrsCapabilities QueryVrsCapabilities(ID3D12Device* device)
{
    VrsCapabilities caps;

    D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
    {
        caps.tier             = options6.VariableShadingRateTier;
        caps.additionalRates  = options6.AdditionalShadingRatesSupported != FALSE;
        caps.tileSize         = options6.ShadingRateImageTileSize;
        caps.perPrimitiveWithViewportIndexing =
            options6.PerPrimitiveShadingRateSupportedWithViewportIndexing != FALSE;
    }

    // SUM 组合器与 mesh shader 写 SV_ShadingRate 的 cap 是 VRS 首发之后追加的，
    // 需要较新的 Windows SDK / DirectX-Headers 才有 D3D12_FEATURE_DATA_D3D12_OPTIONS10。
    D3D12_FEATURE_DATA_D3D12_OPTIONS10 options10{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS10, &options10, sizeof(options10))))
    {
        caps.sumCombiner = options10.VariableRateShadingSumCombinerSupported != FALSE;
    }
    return caps;
}

// =============================================================================
// 通用工具
// =============================================================================
inline D3D12_RESOURCE_BARRIER MakeTransition(ID3D12Resource* resource,
                                            D3D12_RESOURCE_STATES before,
                                            D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter  = after;
    return barrier;
}

// 有些渲染后端只持有 ID3D12GraphicsCommandList，需要按需 QI 出 -5 接口。
// VRS 的两个方法都在 ID3D12GraphicsCommandList5 上（[3][4]）。
inline ID3D12GraphicsCommandList5* AsCommandList5(ID3D12GraphicsCommandList* cmdList)
{
    ID3D12GraphicsCommandList5* cmdList5 = nullptr;
    // 注意：不需要依次 QI -4、-5，直接从基础接口 QI -5 即可，加引用计数由调用方管理。
    if (FAILED(cmdList->QueryInterface(IID_PPV_ARGS(&cmdList5))))
        return nullptr;
    return cmdList5;
}

// =============================================================================
// 2. 来源一：逐 draw 基础率（Tier 1 唯一可用的机制）
// =============================================================================
// RSSetShadingRate 是**命令列表状态**：设置后一直生效，直到再次调用或被 ClearState 重置。
// 因此「用完必须还原」是硬性要求，否则后续 UI / 角色的 draw 也会被降速率。
// combiners 传 nullptr 等价于 { PASSTHROUGH, PASSTHROUGH }（默认值，[3]）。
inline void SetPerDrawRate(ID3D12GraphicsCommandList5* cmdList5, D3D12_SHADING_RATE rate)
{
    cmdList5->RSSetShadingRate(rate, nullptr);
}

// 典型用法（Tier 1）：
//   SetPerDrawRate(cmd5, D3D12_SHADING_RATE_2X2);
//   /* 远景 / 半透明 / 模糊 / 景深 等画面区域 */ Draw(...);
//   SetPerDrawRate(cmd5, D3D12_SHADING_RATE_1X1);   // 还原
//
// Tier 1 的已知限制（[1][2]）：
//   - 只能「逐 draw」，无法在屏幕空间上做局部变化；
//   - 请求 1x2、开启 programmable sample positions、开启 conservative rasterization
//     时硬件可能回退为精细着色；
//   - SampleMask 非全掩码会禁用粗像素着色；
//   - 使用 SV_Coverage、输出 SV_Depth/SV_StencilRef 等也会退回 fine rate。

// =============================================================================
// 3. 来源三：屏空间着色率图像（Tier 2）
// =============================================================================
// 资源约束（[4] 明确列出，逐条都是硬性校验）：
//   - DXGI_FORMAT_R8_UINT、TEXTURE2D、单 mip、非数组、sample count 1；
//   - D3D12_TEXTURE_LAYOUT_UNKNOWN；
//   - 不能是 render-target / depth-stencil / simultaneous-access / cross-adapter 资源；
//   - 尺寸 = { ceil(rtWidth / tileSize), ceil(rtHeight / tileSize) }；
//   - 每个字节 = 一个 D3D12_SHADING_RATE 值；
//   - 使用前必须处于 D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE（只读状态）；
//   - **被设为 shading rate source 期间，任何 shader 阶段都不能读写它**（[1]）→
//     所以「同一帧内既当率图又当查询纹理」是不可能的，要调试可视化就另开一张纹理。
inline ComPtr<ID3D12Resource> CreateShadingRateImage(ID3D12Device* device,
                                                    uint32_t rtWidth,
                                                    uint32_t rtHeight,
                                                    uint32_t tileSize)
{
    if (tileSize == 0) return nullptr;   // 不支持 Tier 2

    const uint32_t imageWidth  = (rtWidth  + tileSize - 1) / tileSize;
    const uint32_t imageHeight = (rtHeight + tileSize - 1) / tileSize;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    heap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask     = 1;
    heap.VisibleNodeMask      = 1;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment          = 0;
    desc.Width              = imageWidth;
    desc.Height             = imageHeight;
    desc.DepthOrArraySize   = 1;                          // 必须非数组
    desc.MipLevels          = 1;                          // 必须单 mip
    desc.Format             = DXGI_FORMAT_R8_UINT;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    // 允许的 flags：NONE / ALLOW_UNORDERED_ACCESS / DENY_SHADER_RESOURCE（[1]）
    desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ComPtr<ID3D12Resource> image;
    // 初始状态选择：本示例先让 compute 写入，故用 UNORDERED_ACCESS；
    // 若想用 CPU 上传一张预计算好的率图（初始全 1x1），改用 COPY_DEST。
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr, IID_PPV_ARGS(image.GetAddressOf()));
    if (FAILED(hr)) return nullptr;

    return image;
}

// =============================================================================
// 4. 生成率图：用 compute 做「内容自适应」（Tier 2 的典型玩法）
// =============================================================================
// 思路：把已经渲染好 / 降采样好的亮度缓冲按 tile 分辨率采样，检测局部对比度：
//   高对比（文字、硬边缘、发光物体）→ 1x1；中等 → 2x2；平坦（天空、雾、虚化区域）→ 4x4。
// 其他常见策略（同一套 API）：
//   - 运动自适应：用上一帧 motion vector，运动模糊区域内降速率；
//   - 注视点渲染（VR）：按眼动追踪结果做同心圆/环形 LOD 掩码；
//   - 材质/深度掩码：远处几何、半透明、DOF 区域降速率。
inline constexpr const char* kShadingRateImageCS = R"(
// 输入：已降采样到率的图像分辨率的亮度缓冲（每 texel 对应一个 tile）
Texture2D<float>   g_Luminance        : register(t0);
// 输出：每 texel 一个字节的 D3D12_SHADING_RATE 值
RWTexture2D<uint>  g_ShadingRateImage : register(u0);

#define VRS_RATE_1X1 0x0u
#define VRS_RATE_2X2 0x5u
#define VRS_RATE_4X4 0xau

[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    uint w, h;
    g_ShadingRateImage.GetDimensions(w, h);
    if (tid.x >= w || tid.y >= h)
        return;

    // 3x3 邻域最大亮度差 → 粗略的「细节密度」估计
    float center = g_Luminance[int2(tid.xy)];
    float maxDelta = 0.0f;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int2 p = clamp(int2(tid.xy) + int2(dx, dy), int2(0, 0), int2(w - 1, h - 1));
            maxDelta = max(maxDelta, abs(g_Luminance[p] - center));
        }
    }

    // 满足 AdditionalShadingRatesSupported 为 false 的设备请把 VRS_RATE_4X4 换成 VRS_RATE_2X2，
    // 否则输出会被驱动 sanitize 到设备支持的档位（结果不是错误，但行为不可预期）。
    uint rate = (maxDelta > 0.10f) ? VRS_RATE_1X1
              : (maxDelta > 0.02f) ? VRS_RATE_2X2
                                   : VRS_RATE_4X4;

    g_ShadingRateImage[tid.xy] = rate;
}
)";

inline void GenerateShadingRateImage(ID3D12GraphicsCommandList* cmdList,
                                     ID3D12PipelineState* computePso,
                                     ID3D12RootSignature* computeRootSig,
                                     uint32_t imageWidth,
                                     uint32_t imageHeight,
                                     D3D12_GPU_VIRTUAL_ADDRESS luminanceSrv,
                                     D3D12_GPU_VIRTUAL_ADDRESS rateImageUav)
{
    cmdList->SetComputeRootSignature(computeRootSig);
    cmdList->SetPipelineState(computePso);
    // 用 root SRV / root UAV 直接传 GPU 地址，省掉示例里的描述符堆样板代码。
    // 生产代码请走引擎的描述符表（DSMEngine: RootSignature / DescriptorTable 那套）。
    cmdList->SetComputeRootShaderResourceView(0, luminanceSrv);
    cmdList->SetComputeRootUnorderedAccessView(1, rateImageUav);
    cmdList->Dispatch((imageWidth + 7) / 8, (imageHeight + 7) / 8, 1);
}

// =============================================================================
// 5. Tier 2 绘制：屏障 → 绑定率图 → 设置组合器
// =============================================================================
inline void BeginAdaptiveVrsPass(ID3D12GraphicsCommandList* cmdList,
                                 ID3D12GraphicsCommandList5* cmdList5,
                                 ID3D12Resource* rateImage,
                                 const VrsCapabilities& caps)
{
    if (!caps.SupportsImageBased() || rateImage == nullptr)
        return;   // Tier 1：只能走第 2 节的逐 draw 路径

    // 5.1 compute 已写好率图 → 转成只读的 SHADING_RATE_SOURCE。
    //     从 UAV 状态转出会附带必要的写可见性处理；保守一点还可以再补一个 UAV barrier。
    D3D12_RESOURCE_BARRIER toSource = MakeTransition(
        rateImage,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);
    cmdList->ResourceBarrier(1, &toSource);

    // 5.2 绑定率图
    cmdList5->RSSetShadingRateImage(rateImage);

    // 5.3 设置组合器 —— 【最容易踩的坑】：
    //     不调用 RSSetShadingRate 设置组合器时，两个组合器都是 PASSTHROUGH，
    //     屏空间率图会被**完全忽略**（[4] 的 Remarks 原文如此）。
    const D3D12_SHADING_RATE_COMBINER combiners[D3D12_RS_SET_SHADING_RATE_COMBINER_COUNT] = {
        // [0] 命令列表基础率 × 逐图元率：MIN = min(A,B)，保留「更高质量」的一方
        D3D12_SHADING_RATE_COMBINER_MIN,
        // [1] 上一步结果 × 率图 texel：这里让率图拥有最终决定权
        D3D12_SHADING_RATE_COMBINER_OVERRIDE
    };
    cmdList5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, combiners);

    // 之后正常设置 PSO / 描述符堆 / DrawIndexedInstanced —— VRS 不需要改 PSO。
    // 也可把基础率设为 2X2 并让 [1] 用 MIN：这样率图只能「变细」不能「变粗」，
    // 相当于给整帧设一个「最粗不劣于 2x2」的保底。
}

inline void EndAdaptiveVrsPass(ID3D12GraphicsCommandList* cmdList,
                               ID3D12GraphicsCommandList5* cmdList5,
                               ID3D12Resource* rateImage)
{
    // 解绑：nullptr 等价于「率图恒为 1x1」（[4]）
    cmdList5->RSSetShadingRateImage(nullptr);
    // 转回 UAV，供下一帧的 compute 覆写（同一帧后续若还要用，别忘了这个依赖）
    D3D12_RESOURCE_BARRIER toUav = MakeTransition(
        rateImage,
        D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cmdList->ResourceBarrier(1, &toUav);
}

// =============================================================================
// 6. 来源二：逐图元率 SV_ShadingRate（Tier 2 + Shader Model 6.4，[1][7]）
// =============================================================================
// 规则：
//   - 只能由 VS / GS / MS 之一写出（不允许 DS 等其他阶段）；从 MS 写出还需要
//     MeshShaderPerPrimitiveShadingRateSupported cap；VS 写出的值取 provoking vertex。
//   - 该语义是 flat（nointerpolation），整个图元共用同一个值。
//   - PS 可以把它作为输入读回，得到硬件最终选定的率（可写进 GBuffer，供延迟着色按
//     粗粒度派发计算）。
//   - PS 若同时输入 SV_ShadingRate 又使用 sample-based execution（如输入 SV_SampleIndex
//     或使用 sample 插值关键字），会**编译失败**（[2]）。
//   - 若 VS/GS/MS 写了 SV_ShadingRate 但没有通过组合器启用「逐图元」这一源，
//     该写法不产生任何效果（[1]）。
inline constexpr const char* kPerPrimitiveRateHLSL = R"(
struct VSInput  { float3 position : POSITION; float2 uv : TEXCOORD0; };

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    // Tier 2 + SM 6.4；flat 语义，不能插值，编译时用 nointerpolation 显式声明
    nointerpolation uint shadingRate : SV_ShadingRate;
};

cbuffer SceneCB : register(b0) { matrix g_ViewProj; uint g_FlatBackgroundInstanceID; };

VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    VSOutput o;
    o.position    = mul(float4(input.position, 1.0f), g_ViewProj);
    o.uv          = input.uv;
    // 背景/装饰性图元降到 2x2，主角与近景保持 1x1（0x5 = 2X2, 0x0 = 1X1）
    o.shadingRate = (instanceID == g_FlatBackgroundInstanceID) ? 0x5u : 0x0u;
    return o;
}

float4 PSMain(VSOutput input) : SV_Target
{
    // 读回本图元最终生效的率：数值按 D3D12_SHADING_RATE 解释；
    // 未使用粗像素着色时为 1x1（0x0）。
    uint appliedRate = input.shadingRate;

    // 例：按率缩放 mip 偏置，补偿粗像素导致 screen-space derivative 变大、
    //     mip 选得偏细的问题（[2] 提醒导数与 mip 选择都会受影响）。
    float mipBias = (appliedRate == 0x0u) ? 0.0f : -1.0f;
    float4 color = SampleScene(input.uv, mipBias);
    return color;
}
)";

// =============================================================================
// 7. 每帧调用顺序（把上面各段串起来）
// =============================================================================
//  1) 渲染 / 降采样得到 tile 分辨率的亮度缓冲；
//  2) compute 写率图            —— 率图状态：UNORDERED_ACCESS
//  3) 屏障                      —— UNORDERED_ACCESS → SHADING_RATE_SOURCE
//  4) BeginAdaptiveVrsPass()    —— 绑定率图 + 设置组合器（别忘了组合器！）
//  5) 正常 Draw                 —— PSO 无需为 VRS 做任何改动
//  6) EndAdaptiveVrsPass()      —— 解绑率图 + SHADING_RATE_SOURCE → UNORDERED_ACCESS
//  7) 需要全速率的 pass（UI、角色特写）：RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr)
//
// 若同时使用逐 draw 与率图，请务必确认组合器顺序与语义：
//     final = Combiner[1]( Combiner[0](基础率, 逐图元率), 率图 )
// 例如 (MIN, OVERRIDE) 表示「率图完全说了算」；(MIN, MAX) 表示「率图只能更粗」，
// 想保证某处一定不低于全速率，可用 (MIN, MIN)。

// =============================================================================
// 8. 移植到 DSMEngine 的落点（基于当前源码状态的实测结论）
// =============================================================================
// 现状（grep 实测）：
//   - Engine/Source/Runtime/Graphics/D3D12/D3D12-Device.cpp:314 已查询 OPTIONS6，
//     328 行已算出 m_VariableRateShadingSupported，但 D3D12-Device.h:260 这个成员
//     没有被任何地方读取；
//   - D3D12-Device.cpp:1471 `case Feature::VariableRateShading: return false;` 是硬编码 false；
//   - 通用层骨架齐备但无人消费：GraphicsCommon.h:645 ResourceStates::ShadingRateSurface、
//     Texture.h:92 TextureDesc::isShadingRateSurface、FrameBuffer.h:30 shadingRateAttachment；
//   - D3D12Common.h:205 已把 ResourceStates::ShadingRateSurface 映射到
//     D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE（这一层是通的）；
//   - 但 D3D12 后端**完全没有** RSSetShadingRate / RSSetShadingRateImage 的调用，
//     即「有声明、无实现」。
//
// 最小落地路径：
//   a) D3D12-Device.h 增加 VrsCapabilities（或直接复用 m_Options6）并让
//      Feature::VariableRateShading 返回真实能力（tier >= TIER_1，别只判 TIER_2）；
//   b) D3D12-CommandList.h:62-64 目前只有 cmdList / cmdList4 / cmdList6，
//      补一个 RefPtr<ID3D12GraphicsCommandList5>（或在需要处 QI），否则拿不到 VRS 方法；
//   c) CreateFramebuffer 里消费 FramebufferDesc::shadingRateAttachment →
//      绘制前 RSSetShadingRateImage + RSSetShadingRate(基础率, 组合器)；
//   d) 率图资源走 TextureDesc（R8_UINT、1 mip、非数组）并把 isShadingRateSurface
//      映射到 ResourceStates::ShadingRateSurface，d 步的枚举已经就绪；
//   e) PSO 侧零改动 —— VRS 的组合器是命令列表状态，D3D12-Device.cpp:1008 的
//      CreateGraphicsPipeline 不用动；只有用 SV_ShadingRate 时才需要在 VS/GS 里加语义
//      （并把着色器编译目标提到 SM 6.4）。

// =============================================================================
// 9. 约束 / 坑清单（每条都能在 [1]-[7] 中找到依据）
// =============================================================================
//  [状态] RSSetShadingRate 与 RSSetShadingRateImage 都是命令列表状态，Draw 时才校验
//         率图资源状态；ClearState 会把基础率与率图都复位。
//  [组合器] 不设置组合器 → 率图被完全忽略（默认双 PASSTHROUGH）。
//         组合器按 X/Y 轴分别作用，不是对整个枚举值比较：
//         MIN(1X2, 2X1) = 1x1（[1] 的原话举例）。
//         SUM 组合器需要 VariableRateShadingSumCombinerSupported，否则行为未定义。
//  [深度] 深度/模板/覆盖率**始终按完整采样分辨率**计算；PS 输出 SV_Depth / SV_StencilRef
//         会退回精细着色。
//  [导数] 粗像素让 screen-space derivative 成倍增大（2x2 → 梯度 2 倍），
//         进而导致 mip 选择偏向更粗的 mip；shader 里常需要补偿。
//  [丢弃] discard 丢弃的是整个粗像素，不是单个像素 —— 与 alpha test 组合时要小心。
//  [MSAA] 保守光栅化与粗像素着色在 Tier 2 上正交；Tier 1 上可能直接禁用粗着色。
//  [文档坑] MS Learn 的 D3D12_SHADING_RATE_COMBINER 枚举页把 MIN/MAX 的描述写反了
//         （页面说 MIN = max(A,B)）。以 [1] 规范与 [3] 的伪代码为准：
//         MIN = min(A,B) 即更高质量。参见
//         https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_shading_rate_combiner
//  [跨平台] 粗像素网格与 tile 网格的对齐方式「可能因平台而异」（[1]），
//         不要依赖精确到像素的率图内容。
//  [版本] VRS Tier 2 是 DirectX 12 Ultimate 四大特性之一（另三个是 DXR 1.1、Mesh Shader、
//         Sampler Feedback），在 Feature Level 12_2 上为强制项；
//         https://devblogs.microsoft.com/directx/announcing-directx-12-ultimate/

} // namespace vrs_example
