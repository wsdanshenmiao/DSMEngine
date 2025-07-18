#pragma once
#ifndef __GRAPHICSCOMMON_H__
#define __GRAPHICSCOMMON_H__

#include <cmath>
#include <string>
#include <memory>
#include <array>
#include "Utils/EnumUtil.h"
#include "Core/Core.h"
#include "Utils/Container.h"

namespace DSM {
    using GpuVirtualAddress = uint64_t;

    static constexpr uint32_t c_MaxRenderTargets = 8;
    static constexpr uint32_t c_MaxViewports = 16;
    static constexpr uint32_t c_MaxBindlessRegisterSpaces = 16;


    using ObjectType = uint32_t;

    namespace ObjectTypes
    {
        constexpr ObjectType SharedHandle                           = 0x00000001;
        constexpr ObjectType D3D12_Device                           = 0x00010001;
        constexpr ObjectType D3D12_CommandQueue                     = 0x00010002;
        constexpr ObjectType D3D12_GraphicsCommandList              = 0x00010003;
        constexpr ObjectType D3D12_Resource                         = 0x00010004;
        constexpr ObjectType D3D12_RenderTargetViewDescriptor       = 0x00010005;
        constexpr ObjectType D3D12_DepthStencilViewDescriptor       = 0x00010006;
        constexpr ObjectType D3D12_ShaderResourceViewGpuDescripror  = 0x00010007;
        constexpr ObjectType D3D12_UnorderedAccessViewGpuDescripror = 0x00010008;
        constexpr ObjectType D3D12_RootSignature                    = 0x00010009;
        constexpr ObjectType D3D12_PipelineState                    = 0x0001000a;
        constexpr ObjectType D3D12_CommandAllocator                 = 0x0001000b;
    };

    struct Object
    {
        union 
        {
            uint64_t integer;
            void* pointer;
        };

        Object(uint64_t i) : integer(i) { }
        Object(void* p) : pointer(p) { }

        template<typename T> operator T* () const { return static_cast<T*>(pointer); }
    };

    class IResource
    {
    protected:
        IResource() = default;
        virtual ~IResource() = default;

    public:
        virtual unsigned long AddRef() = 0;
        virtual unsigned long Release() = 0;

        // 返回一个原始对象
        virtual Object getNativeObject(ObjectType objectType) { (void)objectType; return nullptr; }
        
        IResource(const IResource&) = delete;
        IResource(const IResource&&) = delete;
        IResource& operator=(const IResource&) = delete;
        IResource& operator=(const IResource&&) = delete;
    };

    struct Color
    {
        float r, g, b, a;

        Color() : r(0.f), g(0.f), b(0.f), a(0.f) { }
        Color(float c) : r(c), g(c), b(c), a(c) { }
        Color(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) { }

        bool operator ==(const Color& _b) const { return r == _b.r && g == _b.g && b == _b.b && a == _b.a; }
    };
    

    struct Viewport
    {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;

        Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f) { }

        Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f) { }

        Viewport(float _minX, float _maxX, float _minY, float _maxY, float _minZ, float _maxZ)
            : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY), minZ(_minZ), maxZ(_maxZ) { }

        bool operator ==(const Viewport& b) const noexcept
        {
            return minX == b.minX
                && minY == b.minY
                && minZ == b.minZ
                && maxX == b.maxX
                && maxY == b.maxY
                && maxZ == b.maxZ;
        }
        bool operator !=(const Viewport& b) const { return !(*this == b); }

        [[nodiscard]] float Width() const noexcept { return maxX - minX; }
        [[nodiscard]] float Height() const noexcept { return maxY - minY; }
    };

    struct Rect
    {
        int minX, maxX;
        int minY, maxY;

        Rect() : minX(0), maxX(0), minY(0), maxY(0) { }
        Rect(int width, int height) : minX(0), maxX(width), minY(0), maxY(height) { }
        Rect(int _minX, int _maxX, int _minY, int _maxY) : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY) { }
        explicit Rect(const Viewport& viewport)
            : minX(int(std::floorf(viewport.minX)))
            , maxX(int(std::ceilf(viewport.maxX)))
            , minY(int(std::floorf(viewport.minY)))
            , maxY(int(std::ceilf(viewport.maxY))) { }

        bool operator ==(const Rect& b) const 
        {
            return minX == b.minX && minY == b.minY && maxX == b.maxX && maxY == b.maxY;
        }
        bool operator !=(const Rect& b) const { return !(*this == b); }

        [[nodiscard]] int Width() const { return maxX - minX; }
        [[nodiscard]] int Height() const { return maxY - minY; }
    };

    enum class GraphicsAPI : uint8_t
    {
        D3D12
    };

    enum class Format : uint8_t
    {
        UNKNOWN,

        R8_UINT,
        R8_SINT,
        R8_UNORM,
        R8_SNORM,
        RG8_UINT,
        RG8_SINT,
        RG8_UNORM,
        RG8_SNORM,
        R16_UINT,
        R16_SINT,
        R16_UNORM,
        R16_SNORM,
        R16_FLOAT,
        BGRA4_UNORM,
        B5G6R5_UNORM,
        B5G5R5A1_UNORM,
        RGBA8_UINT,
        RGBA8_SINT,
        RGBA8_UNORM,
        RGBA8_SNORM,
        BGRA8_UNORM,
        SRGBA8_UNORM,
        SBGRA8_UNORM,
        R10G10B10A2_UNORM,
        R11G11B10_FLOAT,
        RG16_UINT,
        RG16_SINT,
        RG16_UNORM,
        RG16_SNORM,
        RG16_FLOAT,
        R32_UINT,
        R32_SINT,
        R32_FLOAT,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_FLOAT,
        RGBA16_UNORM,
        RGBA16_SNORM,
        RG32_UINT,
        RG32_SINT,
        RG32_FLOAT,
        RGB32_UINT,
        RGB32_SINT,
        RGB32_FLOAT,
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA32_FLOAT,
        
        D16,
        D24S8,
        X24G8_UINT,
        D32,
        D32S8,
        X32G8_UINT,

        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UFLOAT,
        BC6H_SFLOAT,
        BC7_UNORM,
        BC7_UNORM_SRGB,

        COUNT,
    };

    enum class FormatKind : uint8_t
    {
        Integer,
        Normalized,
        Float,
        DepthStencil
    };

    struct FormatInfo
    {
        Format format;
        const char* name;
        uint8_t bytesPerBlock;
        uint8_t blockSize;
        FormatKind kind;
        bool hasRed : 1;
        bool hasGreen : 1;
        bool hasBlue : 1;
        bool hasAlpha : 1;
        bool hasDepth : 1;
        bool hasStencil : 1;
        bool isSigned : 1;
        bool isSRGB : 1;
    };
    
    const FormatInfo& GetFormatInfo(Format format);


    enum class FormatSupport : uint32_t
    {
        None            = 0,

        Buffer          = (1 << 0),
        VertexBuffer    = (1 << 1),
        IndexBuffer     = (1 << 2),
        Texture         = (1 << 3),
        DepthStencil    = (1 << 4),
        RenderTarget    = (1 << 5),
        Blendable       = (1 << 6),
        ShaderLoad      = (1 << 7),
        ShaderSample    = (1 << 8),
        ShaderUavLoad   = (1 << 9),
        ShaderUavStore  = (1 << 10),
        ShaderAtomic    = (1 << 11)
    };
    ENABLE_ENUM_BIT_OPERATOR(FormatSupport)


    enum class ResourceStates : uint32_t
    {
        Unknown                     = 0,
        Common                      = (1 << 0),
        ConstantBuffer              = (1 << 1),
        VertexBuffer                = (1 << 2),
        IndexBuffer                 = (1 << 3),
        IndirectArgument            = (1 << 4),
        ShaderResource              = (1 << 5),
        UnorderedAccess             = (1 << 6),
        RenderTarget                = (1 << 7),
        DepthWrite                  = (1 << 8),
        DepthRead                   = (1 << 9),
        StreamOut                   = (1 << 10),
        CopyDest                    = (1 << 11),
        CopySource                  = (1 << 12),
        ResolveDest                 = (1 << 13),
        ResolveSource               = (1 << 14),
        Present                     = (1 << 15),
        AccelerationStructrue       = (1 << 16),
        ShadingRateSurface          = (1 << 17)
    };
    ENABLE_ENUM_BIT_OPERATOR(ResourceStates)

    // Flags for resources that need to be shared with other graphics APIs or other GPU devices.
    enum class SharedResourceFlags : uint32_t
    {
        None                = 0,

        // D3D11: adds D3D11_RESOURCE_MISC_SHARED
        // D3D12: adds D3D12_HEAP_FLAG_SHARED
        // Vulkan: adds vk::ExternalMemoryImageCreateInfo and vk::ExportMemoryAllocateInfo/vk::ExternalMemoryBufferCreateInfo
        Shared              = 0x01,

        // D3D11: adds (D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        // D3D12, Vulkan: ignored
        Shared_NTHandle     = 0x02,

        // D3D12: adds D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER and D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
        // D3D11, Vulkan: ignored
        Shared_CrossAdapter = 0x04,
    };
    ENABLE_ENUM_BIT_OPERATOR(SharedResourceFlags)




    struct VertexAttributeDesc
    {
        std::string name;
        Format format = Format::UNKNOWN;
        uint32_t arraySize = 1;
        uint32_t bufferIndex = 0;
        uint32_t offset = 0;
        uint32_t elementStride = 0;
        bool isInstanced = false;

        constexpr VertexAttributeDesc& SetFormat(Format value) { format = value; return *this; }
        constexpr VertexAttributeDesc& SetArraySize(uint32_t value) { arraySize = value; return *this; }
        constexpr VertexAttributeDesc& SetBufferIndex(uint32_t value) { bufferIndex = value; return *this; }
        constexpr VertexAttributeDesc& SetOffset(uint32_t value) { offset = value; return *this; }
        constexpr VertexAttributeDesc& SetElementStride(uint32_t value) { elementStride = value; return *this; }
        constexpr VertexAttributeDesc& SetIsInstanced(bool value) { isInstanced = value; return *this; }

        VertexAttributeDesc& SetName(const std::string& value) { name = value; return *this; }    
    };

    class IInputLayout : public IResource
    {
    public:
        [[nodiscard]] virtual uint32_t GetNumAttributes() const = 0;
        [[nodiscard]] virtual const VertexAttributeDesc* GetAttributeDesc(uint32_t index) const = 0;
    };

    enum class CpuAccessMode : uint8_t
    {
        None,
        Read,
        Write
    };






    //////////////////////////////////////////////////////////////////////////
    // Blend State
    //////////////////////////////////////////////////////////////////////////

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




    //////////////////////////////////////////////////////////////////////////
    // Raster State
    //////////////////////////////////////////////////////////////////////////

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




    //////////////////////////////////////////////////////////////////////////
    // Depth Stencil State
    //////////////////////////////////////////////////////////////////////////

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


    //////////////////////////////////////////////////////////////////////////
    // Viewport State
    //////////////////////////////////////////////////////////////////////////

    struct ViewportState
    {
        StaticVector<Viewport, c_MaxViewports> viewports;
        StaticVector<Rect, c_MaxViewports> scissorRects;

        ViewportState& AddViewport(const Viewport& v) { viewports.PushBack(v); return *this; }
        ViewportState& AddScissorRect(const Rect& r) { scissorRects.PushBack(r); return *this; }
        ViewportState& AddViewportAndScissorRect(const Viewport& v) { return AddViewport(v).AddScissorRect(Rect(v)); }
    };


    


} // namespace DSM 

#endif