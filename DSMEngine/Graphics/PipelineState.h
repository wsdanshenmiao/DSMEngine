#pragma once
#ifndef __PIPELINESTATE_H__
#define __PIPELINESTATE_H__

#include "FrameBuffer.h"
#include "ResourceBindings.h"

namespace DSM {

    enum class BlendFactor : uint8_t
    {
        Zero,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DstAlpha,
        InvDstAlpha,
        DstColor,
        InvDstColor,
        SrcAlphaSaturate,
        ConstantColor,
        InvConstantColor,
        Src1Color,
        InvSrc1Color,
        Src1Alpha,
        InvSrc1Alpha
    };

        
    enum class BlendOp : uint8_t
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class ColorMask : uint8_t
    {
        // These values are equal to their counterparts in DX11, DX12, and Vulkan.
        Red = 1,
        Green = 2,
        Blue = 4,
        Alpha = 8,
        All = 0xF
    };
    
    struct BlendState
    {
        struct RenderTarget
        {
            bool        blendEnable = false;
            BlendFactor srcBlend = BlendFactor::One;
            BlendFactor destBlend = BlendFactor::Zero;
            BlendOp     blendOp = BlendOp::Add;
            BlendFactor srcBlendAlpha = BlendFactor::One;
            BlendFactor destBlendAlpha = BlendFactor::Zero;
            BlendOp     blendOpAlpha = BlendOp::Add;
            ColorMask   colorWriteMask = ColorMask::All;

            constexpr RenderTarget& SetBlendEnable(bool enable) { blendEnable = enable; return *this; }
            constexpr RenderTarget& EnableBlend() { blendEnable = true; return *this; }
            constexpr RenderTarget& DisableBlend() { blendEnable = false; return *this; }
            constexpr RenderTarget& SetSrcBlend(BlendFactor value) { srcBlend = value; return *this; }
            constexpr RenderTarget& SetDestBlend(BlendFactor value) { destBlend = value; return *this; }
            constexpr RenderTarget& SetBlendOp(BlendOp value) { blendOp = value; return *this; }
            constexpr RenderTarget& SetSrcBlendAlpha(BlendFactor value) { srcBlendAlpha = value; return *this; }
            constexpr RenderTarget& SetDestBlendAlpha(BlendFactor value) { destBlendAlpha = value; return *this; }
            constexpr RenderTarget& SetBlendOpAlpha(BlendOp value) { blendOpAlpha = value; return *this; }
            constexpr RenderTarget& SetColorWriteMask(ColorMask value) { colorWriteMask = value; return *this; }

            constexpr bool operator ==(const RenderTarget& other) const
            {
                return blendEnable == other.blendEnable
                    && srcBlend == other.srcBlend
                    && destBlend == other.destBlend
                    && blendOp == other.blendOp
                    && srcBlendAlpha == other.srcBlendAlpha
                    && destBlendAlpha == other.destBlendAlpha
                    && blendOpAlpha == other.blendOpAlpha
                    && colorWriteMask == other.colorWriteMask;
            }
        };

        std::array<RenderTarget, c_MaxRenderTargets> targets;
        bool alphaToCoverageEnable = false;

        constexpr BlendState& SetRenderTarget(uint32_t index, const RenderTarget& target) { targets[index] = target; return *this; }
        constexpr BlendState& SetAlphaToCoverageEnable(bool enable) { alphaToCoverageEnable = enable; return *this; }
        constexpr BlendState& EnableAlphaToCoverage() { alphaToCoverageEnable = true; return *this; }
        constexpr BlendState& DisableAlphaToCoverage() { alphaToCoverageEnable = false; return *this; }

        constexpr bool operator ==(const BlendState& other) const
        {
            if (alphaToCoverageEnable != other.alphaToCoverageEnable)
                return false;

            for (uint32_t i = 0; i < c_MaxRenderTargets; ++i)
            {
                if (targets[i] != other.targets[i])
                    return false;
            }

            return true;
        }
    };




    enum class RasterFillMode : uint8_t
    {
        Solid,
        Wireframe,
    };

    enum class RasterCullMode : uint8_t
    {
        Back,
        Front,
        None
    };

    struct RasterState
    {
        RasterFillMode fillMode = RasterFillMode::Solid;
        RasterCullMode cullMode = RasterCullMode::Back;
        bool frontCounterClockwise = false;
        bool depthClipEnable = false;
        bool scissorEnable = false;
        bool multisampleEnable = false;
        bool antialiasedLineEnable = false;
        int depthBias = 0;
        float depthBiasClamp = 0.f;
        float slopeScaledDepthBias = 0.f;
        uint8_t forcedSampleCount = 0;
        bool conservativeRasterEnable = false;
        
        constexpr RasterState& SetFillMode(RasterFillMode value) { fillMode = value; return *this; }
        constexpr RasterState& SetFillSolid() { fillMode = RasterFillMode::Solid; return *this; }
        constexpr RasterState& SetFillWireframe() { fillMode = RasterFillMode::Wireframe; return *this; }
        constexpr RasterState& SetCullMode(RasterCullMode value) { cullMode = value; return *this; }
        constexpr RasterState& SetCullBack() { cullMode = RasterCullMode::Back; return *this; }
        constexpr RasterState& SetCullFront() { cullMode = RasterCullMode::Front; return *this; }
        constexpr RasterState& SetCullNone() { cullMode = RasterCullMode::None; return *this; }
        constexpr RasterState& SetFrontCounterClockwise(bool value) { frontCounterClockwise = value; return *this; }
        constexpr RasterState& SetDepthClipEnable(bool value) { depthClipEnable = value; return *this; }
        constexpr RasterState& EnableDepthClip() { depthClipEnable = true; return *this; }
        constexpr RasterState& DisableDepthClip() { depthClipEnable = false; return *this; }
        constexpr RasterState& SetScissorEnable(bool value) { scissorEnable = value; return *this; }
        constexpr RasterState& EnableScissor() { scissorEnable = true; return *this; }
        constexpr RasterState& DisableScissor() { scissorEnable = false; return *this; }
        constexpr RasterState& SetMultisampleEnable(bool value) { multisampleEnable = value; return *this; }
        constexpr RasterState& EnableMultisample() { multisampleEnable = true; return *this; }
        constexpr RasterState& DisableMultisample() { multisampleEnable = false; return *this; }
        constexpr RasterState& SetAntialiasedLineEnable(bool value) { antialiasedLineEnable = value; return *this; }
        constexpr RasterState& EnableAntialiasedLine() { antialiasedLineEnable = true; return *this; }
        constexpr RasterState& DisableAntialiasedLine() { antialiasedLineEnable = false; return *this; }
        constexpr RasterState& SetDepthBias(int value) { depthBias = value; return *this; }
        constexpr RasterState& SetDepthBiasClamp(float value) { depthBiasClamp = value; return *this; }
        constexpr RasterState& SetSlopeScaleDepthBias(float value) { slopeScaledDepthBias = value; return *this; }
        constexpr RasterState& SetForcedSampleCount(uint8_t value) { forcedSampleCount = value; return *this; }
        constexpr RasterState& SetConservativeRasterEnable(bool value) { conservativeRasterEnable = value; return *this; }
        constexpr RasterState& EnableConservativeRaster() { conservativeRasterEnable = true; return *this; }
        constexpr RasterState& DisableConservativeRaster() { conservativeRasterEnable = false; return *this; }
    };




   enum class StencilOp : uint8_t
   {
       Keep = 1,
       Zero = 2,
       Replace = 3,
       IncrementAndClamp = 4,
       DecrementAndClamp = 5,
       Invert = 6,
       IncrementAndWrap = 7,
       DecrementAndWrap = 8
   };

   enum class ComparisonFunc : uint8_t
   {
       Never = 1,
       Less = 2,
       Equal = 3,
       LessOrEqual = 4,
       Greater = 5,
       NotEqual = 6,
       GreaterOrEqual = 7,
       Always = 8
   };

   struct DepthStencilState
   {
       struct StencilOpDesc
       {
           StencilOp failOp = StencilOp::Keep;
           StencilOp depthFailOp = StencilOp::Keep;
           StencilOp passOp = StencilOp::Keep;
           ComparisonFunc stencilFunc = ComparisonFunc::Always;

           constexpr StencilOpDesc& SetFailOp(StencilOp value) { failOp = value; return *this; }
           constexpr StencilOpDesc& SetDepthFailOp(StencilOp value) { depthFailOp = value; return *this; }
           constexpr StencilOpDesc& SetPassOp(StencilOp value) { passOp = value; return *this; }
           constexpr StencilOpDesc& SetStencilFunc(ComparisonFunc value) { stencilFunc = value; return *this; }
       };

       bool            depthTestEnable = true;
       bool            depthWriteEnable = true;
       ComparisonFunc  depthFunc = ComparisonFunc::Less;
       bool            stencilEnable = false;
       uint8_t         stencilReadMask = 0xff;
       uint8_t         stencilWriteMask = 0xff;
       uint8_t         stencilRefValue = 0;
       bool            dynamicStencilRef = false;
       StencilOpDesc   frontFaceStencil;
       StencilOpDesc   backFaceStencil;

       constexpr DepthStencilState& SetDepthTestEnable(bool value) { depthTestEnable = value; return *this; }
       constexpr DepthStencilState& EnableDepthTest() { depthTestEnable = true; return *this; }
       constexpr DepthStencilState& DisableDepthTest() { depthTestEnable = false; return *this; }
       constexpr DepthStencilState& SetDepthWriteEnable(bool value) { depthWriteEnable = value; return *this; }
       constexpr DepthStencilState& EnableDepthWrite() { depthWriteEnable = true; return *this; }
       constexpr DepthStencilState& DisableDepthWrite() { depthWriteEnable = false; return *this; }
       constexpr DepthStencilState& SetDepthFunc(ComparisonFunc value) { depthFunc = value; return *this; }
       constexpr DepthStencilState& SetStencilEnable(bool value) { stencilEnable = value; return *this; }
       constexpr DepthStencilState& EnableStencil() { stencilEnable = true; return *this; }
       constexpr DepthStencilState& DisableStencil() { stencilEnable = false; return *this; }
       constexpr DepthStencilState& SetStencilReadMask(uint8_t value) { stencilReadMask = value; return *this; }
       constexpr DepthStencilState& SetStencilWriteMask(uint8_t value) { stencilWriteMask = value; return *this; }
       constexpr DepthStencilState& SetStencilRefValue(uint8_t value) { stencilRefValue = value; return *this; }
       constexpr DepthStencilState& SetFrontFaceStencil(const StencilOpDesc& value) { frontFaceStencil = value; return *this; }
       constexpr DepthStencilState& SetBackFaceStencil(const StencilOpDesc& value) { backFaceStencil = value; return *this; }
       constexpr DepthStencilState& SetDynamicStencilRef(bool value) { dynamicStencilRef = value; return *this; }
   };


    enum class PrimitiveType : uint8_t
    {
        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan,
        TriangleListWithAdjacency,
        TriangleStripWithAdjacency,
        PatchList
    };




    struct RenderState
    {
        BlendState blendState;
        DepthStencilState depthStencilState;
        RasterState rasterState;

        constexpr RenderState& setBlendState(const BlendState& value) { blendState = value; return *this; }
        constexpr RenderState& setDepthStencilState(const DepthStencilState& value) { depthStencilState = value; return *this; }
        constexpr RenderState& setRasterState(const RasterState& value) { rasterState = value; return *this; }
    };

    // 图形管线
    struct GraphicsPipelineDesc
    {
        PrimitiveType primType = PrimitiveType::TriangleList;
        uint32_t patchControlPoints = 0;
        InputLayoutHandle inputLayout;

        ShaderHandle VS;
        ShaderHandle HS;
        ShaderHandle DS;
        ShaderHandle GS;
        ShaderHandle PS;

        RenderState renderState;

        BindingLayoutVector bindingLayouts;
        
        GraphicsPipelineDesc& SetPrimType(PrimitiveType value) { primType = value; return *this; }
        GraphicsPipelineDesc& SetPatchControlPoints(uint32_t value) { patchControlPoints = value; return *this; }
        GraphicsPipelineDesc& SetInputLayout(IInputLayout* value) { inputLayout = value; return *this; }
        GraphicsPipelineDesc& SetVertexShader(IShader* value) { VS = value; return *this; }
        GraphicsPipelineDesc& SetHullShader(IShader* value) { HS = value; return *this; }
        GraphicsPipelineDesc& SetTessellationControlShader(IShader* value) { HS = value; return *this; }
        GraphicsPipelineDesc& SetDomainShader(IShader* value) { DS = value; return *this; }
        GraphicsPipelineDesc& SetTessellationEvaluationShader(IShader* value) { DS = value; return *this; }
        GraphicsPipelineDesc& SetGeometryShader(IShader* value) { GS = value; return *this; }
        GraphicsPipelineDesc& SetPixelShader(IShader* value) { PS = value; return *this; }
        GraphicsPipelineDesc& SetFragmentShader(IShader* value) { PS = value; return *this; }
        GraphicsPipelineDesc& SetRenderState(const RenderState& value) { renderState = value; return *this; }
        GraphicsPipelineDesc& AddBindingLayout(IBindingLayout* layout) { bindingLayouts.PushBack(BindingLayoutHandle{layout}); return *this; }
    };

    class IGraphicsPipeline : public IResource
    {
    public:
        [[nodiscard]] virtual const GraphicsPipelineDesc& GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfo& GetFramebufferInfo() const = 0;
    };
    using GraphicsPipelineHandle = RefPtr<IGraphicsPipeline>;


    struct ComputePipelineDesc
    {
        ShaderHandle CS;

        BindingLayoutVector bindingLayouts;

        ComputePipelineDesc& SetComputeShader(IShader* value) { CS = value; return *this; }
        ComputePipelineDesc& AddBindingLayout(IBindingLayout* layout) { bindingLayouts.PushBack(BindingLayoutHandle{layout}); return *this; }
    };

    // 计算管线
    class IComputePipeline : public IResource
    {
    public:
        [[nodiscard]] virtual const ComputePipelineDesc& GetDesc() const = 0;
    };
    using ComputePipelineHandle = RefPtr<IComputePipeline>;


    struct MeshletPipelineDesc
    {
        PrimitiveType primType = PrimitiveType::TriangleList;
        
        ShaderHandle AS;
        ShaderHandle MS;
        ShaderHandle PS;

        RenderState renderState;

        BindingLayoutVector bindingLayouts;
        
        MeshletPipelineDesc& SetPrimType(PrimitiveType value) { primType = value; return *this; }
        MeshletPipelineDesc& SetTaskShader(IShader* value) { AS = value; return *this; }
        MeshletPipelineDesc& SetAmplificationShader(IShader* value) { AS = value; return *this; }
        MeshletPipelineDesc& SetMeshShader(IShader* value) { MS = value; return *this; }
        MeshletPipelineDesc& SetPixelShader(IShader* value) { PS = value; return *this; }
        MeshletPipelineDesc& SetFragmentShader(IShader* value) { PS = value; return *this; }
        MeshletPipelineDesc& SetRenderState(const RenderState& value) { renderState = value; return *this; }
        MeshletPipelineDesc& AddBindingLayout(IBindingLayout* layout) { bindingLayouts.PushBack(BindingLayoutHandle{layout}); return *this; }
    };

    // 网格着色器
    class IMeshletPipeline : public IResource
    {
    public:
        [[nodiscard]] virtual const MeshletPipelineDesc& GetDesc() const = 0;
        [[nodiscard]] virtual const FramebufferInfo& GetFramebufferInfo() const = 0;
    };
    using MeshletPipelineHandle = RefPtr<IMeshletPipeline>;



    
    struct VertexBufferBinding
    {
        IBuffer* buffer = nullptr;
        uint32_t slot;
        uint64_t offset;

        bool operator ==(const VertexBufferBinding& b) const
        {
            return buffer == b.buffer
                && slot == b.slot
                && offset == b.offset;
        }
        bool operator !=(const VertexBufferBinding& b) const { return !(*this == b); }

        VertexBufferBinding& SetBuffer(IBuffer* value) { buffer = value; return *this; }
        VertexBufferBinding& SetSlot(uint32_t value) { slot = value; return *this; }
        VertexBufferBinding& SetOffset(uint64_t value) { offset = value; return *this; }
    };

    struct IndexBufferBinding
    {
        IBuffer* buffer = nullptr;
        Format format;
        uint32_t offset;

        bool operator ==(const IndexBufferBinding& b) const
        {
            return buffer == b.buffer
                && format == b.format
                && offset == b.offset;
        }
        bool operator !=(const IndexBufferBinding& b) const { return !(*this == b); }

        IndexBufferBinding& SetBuffer(IBuffer* value) { buffer = value; return *this; }
        IndexBufferBinding& SetFormat(Format value) { format = value; return *this; }
        IndexBufferBinding& SetOffset(uint32_t value) { offset = value; return *this; }
    };

    

    struct GraphicsState
    {
        IGraphicsPipeline* pipeline = nullptr;
        IFramebuffer* framebuffer = nullptr;
        ViewportState viewport;
        Color blendConstantColor{};
        uint8_t dynamicStencilRefValue = 0;

        BindingSetVector bindings;

        StaticVector<VertexBufferBinding, c_MaxVertexAttributes> vertexBuffers;
        IndexBufferBinding indexBuffer;

        IBuffer* indirectParams = nullptr;

        GraphicsState& SetPipeline(IGraphicsPipeline* value) { pipeline = value; return *this; }
        GraphicsState& SetFramebuffer(IFramebuffer* value) { framebuffer = value; return *this; }
        GraphicsState& SetViewport(const ViewportState& value) { viewport = value; return *this; }
        GraphicsState& SetBlendColor(const Color& value) { blendConstantColor = value; return *this; }
        GraphicsState& SetDynamicStencilRefValue(uint8_t value) { dynamicStencilRefValue = value; return *this; }
        GraphicsState& AddBindingSet(IBindingSet* value) { bindings.PushBack(BindingSetHandle{value}); return *this; }
        GraphicsState& AddVertexBuffer(const VertexBufferBinding& value) { vertexBuffers.PushBack(value); return *this; }
        GraphicsState& SetIndexBuffer(const IndexBufferBinding& value) { indexBuffer = value; return *this; }
        GraphicsState& SetIndirectParams(IBuffer* value) { indirectParams = value; return *this; }
    };
    
    struct ComputeState
    {
        IComputePipeline* pipeline = nullptr;

        BindingSetVector bindings;

        IBuffer* indirectParams = nullptr;

        ComputeState& SetPipeline(IComputePipeline* value) { pipeline = value; return *this; }
        ComputeState& AddBindingSet(IBindingSet* value) { bindings.PushBack(BindingSetHandle{value}); return *this; }
        ComputeState& SetIndirectParams(IBuffer* value) { indirectParams = value; return *this; }
    };

    struct MeshletState
    {
        IMeshletPipeline* pipeline = nullptr;
        IFramebuffer* framebuffer = nullptr;
        ViewportState viewport;
        Color blendConstantColor{};
        uint8_t dynamicStencilRefValue = 0;

        BindingSetVector bindings;

        IBuffer* indirectParams = nullptr;

        MeshletState& SetPipeline(IMeshletPipeline* value) { pipeline = value; return *this; }
        MeshletState& SetFramebuffer(IFramebuffer* value) { framebuffer = value; return *this; }
        MeshletState& SetViewport(const ViewportState& value) { viewport = value; return *this; }
        MeshletState& SetBlendColor(const Color& value) { blendConstantColor = value; return *this; }
        MeshletState& AddBindingSet(IBindingSet* value) { bindings.PushBack(BindingSetHandle{value}); return *this; }
        MeshletState& SetIndirectParams(IBuffer* value) { indirectParams = value; return *this; }
        MeshletState& SetDynamicStencilRefValue(uint8_t value) { dynamicStencilRefValue = value; return *this; }
    };

} // namespace DSM 

#endif