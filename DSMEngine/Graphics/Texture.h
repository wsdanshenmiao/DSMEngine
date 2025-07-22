#pragma once
#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#include <numeric>
#include "Heap.h"

namespace DSM {

    struct MemoryRequirements
    {
        uint64_t size = 0;
        uint64_t alignment = 0;
    };

    struct PackedMipDesc
    {
        uint32_t numStandardMips = 0;
        uint32_t numPackedMips = 0;
        uint32_t numTilesForPackedMips = 0;
        uint32_t startTileIndexInOverallResource = 0;
    };
    
    struct TileShape
    {
        uint32_t widthInTexels = 0;
        uint32_t heightInTexels = 0;
        uint32_t depthInTexels = 0;
    };

    struct SubresourceTiling
    {
        uint32_t widthInTiles = 0;
        uint32_t heightInTiles = 0;
        uint32_t depthInTiles = 0;
        uint32_t startTileIndexInOverallResource = 0;
    };

    struct TiledTextureCoordinate
    {
        uint16_t mipLevel = 0;
        uint16_t arrayLevel = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
    };

    struct TiledTextureRegion
    {
        uint32_t tilesNum = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
    };
    
    struct TextureTilesMapping
    {
        TiledTextureCoordinate* tiledTextureCoordinates = nullptr;
        TiledTextureRegion* tiledTextureRegions = nullptr;
        uint64_t* byteOffsets = nullptr;
        uint32_t numTextureRegions = 0;
        IHeap* heap = nullptr;
    };

    enum class TextureDimension : uint8_t
    {
        Unknown,
        Texture1D,
        Texture1DArray,
        Texture2D,
        Texture2DArray,
        TextureCube,
        TextureCubeArray,
        Texture2DMS,
        Texture2DMSArray,
        Texture3D
    };

    struct TextureDesc
    {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t arraySize = 1;
        uint32_t mipLevels = 1;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;
        Format format = Format::UNKNOWN;
        TextureDimension dimension = TextureDimension::Texture2D;

        std::string debugName;

        bool isShaderResource = true;
        bool isRenderTarget = false;
        bool isUAV = false;
        bool isTypeless = false;
        bool isShadingRateSurface = false;

        SharedResourceFlags sharedResourceFlags = SharedResourceFlags::None;

        // 当前纹理为保留资源，表明不占用物理内存，随后将内存绑定到纹理上
        bool isVirtual = false;
        // 对应DX12中的 D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE 或 D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE 
        bool isTiled = false;

        Color clearValue;
        bool useClearValue = false;

        ResourceStates initialState = ResourceStates::Unknown;

        // 若保持初始化的状态，使用纹理的命令列表会自动追踪其状态并在关闭的时候转换为初始状态
        bool keepInitialState = false;

        inline constexpr TextureDesc& SetWidth(uint32_t value) { width = value; return *this; }
        inline constexpr TextureDesc& SetHeight(uint32_t value) { height = value; return *this; }
        inline constexpr TextureDesc& SetDepth(uint32_t value) { depth = value; return *this; }
        inline constexpr TextureDesc& SetArraySize(uint32_t value) { arraySize = value; return *this; }
        inline constexpr TextureDesc& SetMipLevels(uint32_t value) { mipLevels = value; return *this; }
        inline constexpr TextureDesc& SetSampleCount(uint32_t value) { sampleCount = value; return *this; }
        inline constexpr TextureDesc& SetSampleQuality(uint32_t value) { sampleQuality = value; return *this; }
        inline constexpr TextureDesc& SetFormat(Format value) { format = value; return *this; }
        inline constexpr TextureDesc& SetDimension(TextureDimension value) { dimension = value; return *this; }
        inline constexpr TextureDesc& SetIsRenderTarget(bool value) { isRenderTarget = value; return *this; }
        inline constexpr TextureDesc& SetIsUAV(bool value) { isUAV = value; return *this; }
        inline constexpr TextureDesc& SetIsTypeless(bool value) { isTypeless = value; return *this; }
        inline constexpr TextureDesc& SetIsVirtual(bool value) { isVirtual = value; return *this; }
        inline constexpr TextureDesc& SetClearValue(const Color& value) { clearValue = value; useClearValue = true; return *this; }
        inline constexpr TextureDesc& SetUseClearValue(bool value) { useClearValue = value; return *this; }
        inline constexpr TextureDesc& SetInitialState(ResourceStates value) { initialState = value; return *this; }
        inline constexpr TextureDesc& SetKeepInitialState(bool value) { keepInitialState = value; return *this; }
        inline constexpr TextureDesc& SetSharedResourceFlags(SharedResourceFlags value) { sharedResourceFlags = value; return *this; }
        inline TextureDesc& SetDebugName(const std::string& value) { debugName = value; return *this; }
   };


   // 描述纹理的一个切片，或一个mipmap层级
   struct TextureSlice
   {
        // 该切片的起始位置
        uint32_t x = 0, y = 0, z = 0;
        // -1表示整个维度是区域的一部分, 在reslove中重新计算
        uint32_t width = uint32_t(-1);
        uint32_t height = uint32_t(-1);
        uint32_t depth = uint32_t(-1);

        uint32_t mipLevel = 0;
        uint16_t arraySlice = 0;

        // 重新计算宽高及深度
        [[nodiscard]] TextureSlice Resolve(const TextureDesc& desc)
        {
            auto ret = *this;
            if(width == uint32_t(-1)){
                ret.width = std::max(1u, desc.width >> mipLevel);
            }
            if(height == uint32_t(-1)){
                ret.height = std::max(1u, desc.height >> mipLevel);
            }
            if(depth == uint32_t(-1)){
                ret.depth = desc.dimension == TextureDimension::Texture3D ? 
                    std::max(1u, desc.depth >> mipLevel) : 1;
            }

            return ret;
        }

        inline constexpr TextureSlice& SetOrigin(uint32_t _x, uint32_t _y, uint32_t _z) { x = _x; y = _y; z = _z; return *this; }
        inline constexpr TextureSlice& SetSize(uint32_t w = uint32_t(-1), uint32_t h = uint32_t(-1), uint32_t d = uint32_t(-1))
        {
            width = w; height = h; depth = d; return *this;
        }
        inline constexpr TextureSlice& SetWidth(uint32_t w) { width = w; return *this; }
        inline constexpr TextureSlice& SetHeight(uint32_t h) { height = h; return *this; }
        inline constexpr TextureSlice& SetDepth(uint32_t d) { depth = d; return *this; }
        inline constexpr TextureSlice& SetMipLevel(uint32_t v) { mipLevel = v; return *this; }
        inline constexpr TextureSlice& SetArraySlice(uint32_t v) { arraySlice = v; return *this; }
    };
   
    struct TextureSubresourceSet
    {
        static constexpr uint32_t AllMipLevels = uint32_t(-1);
        static constexpr uint32_t AllArraySlices = uint32_t(-1);

        uint32_t baseMipLevel = 0;
        uint32_t numMipLevels = 1;
        uint32_t baseArraySlice = 0;
        uint32_t numArraySlices = 1;
        
        TextureSubresourceSet() = default;
        TextureSubresourceSet(uint32_t _baseMipLevel, uint32_t _numMipLevels, uint32_t _baseArraySlice, uint32_t _numArraySlices)
            : baseMipLevel(_baseMipLevel)
            , numMipLevels(_numMipLevels)
            , baseArraySlice(_baseArraySlice)
            , numArraySlices(_numArraySlices) { }

        [[nodiscard]] TextureSubresourceSet Resolve(const TextureDesc& desc, bool singleMipLevel) const
        {
            auto ret = *this;
            if(singleMipLevel){
                ret.numMipLevels = 1;
            }
            else{
                auto lastMipLevel = std::min(baseMipLevel + numMipLevels, desc.mipLevels);
                ret.numMipLevels = std::max(0u, lastMipLevel - baseMipLevel);
            }

            switch (desc.dimension)
            {
            case TextureDimension::Texture1DArray:
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCube:
            case TextureDimension::TextureCubeArray:
            case TextureDimension::Texture2DMSArray:{
                auto lastArraySlice = std::min(baseArraySlice + numArraySlices, desc.arraySize);
                ret.numArraySlices = std::max(lastArraySlice - baseArraySlice, 0u);
                break;
            }
            default:{
                ret.baseArraySlice = 0;
                ret.numArraySlices = 1;
                break;
            }
            }
            return ret;
        }

        [[nodiscard]] bool IsEntireTexture(const TextureDesc& desc) const
        {
            if(0 < baseMipLevel || (baseMipLevel + numMipLevels) < desc.mipLevels) return false;

            switch (desc.dimension) {
            case TextureDimension::Texture1DArray:
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCube:
            case TextureDimension::TextureCubeArray:
            case TextureDimension::Texture2DMSArray: 
                if (0 < baseArraySlice || (baseArraySlice + numArraySlices) < desc.arraySize)
                    return false;
            default:
                return true;
            }
        }

        bool operator==(const TextureSubresourceSet& other) const noexcept
        {
            return baseMipLevel == other.baseMipLevel &&
                numMipLevels == other.numMipLevels &&
                baseArraySlice == other.baseArraySlice &&
                numArraySlices == other.numArraySlices;
        }

        constexpr TextureSubresourceSet& SetBaseMipLevel(uint32_t value) { baseMipLevel = value; return *this; }
        constexpr TextureSubresourceSet& SetNumMipLevels(uint32_t value) { numMipLevels = value; return *this; }
        constexpr TextureSubresourceSet& SetMipLevels(uint32_t base, uint32_t num) { baseMipLevel = base; numMipLevels = num; return *this; }
        constexpr TextureSubresourceSet& SetBaseArraySlice(uint32_t value) { baseArraySlice = value; return *this; }
        constexpr TextureSubresourceSet& SetNumArraySlices(uint32_t value) { numArraySlices = value; return *this; }
        constexpr TextureSubresourceSet& SetArraySlices(uint32_t base, uint32_t num) { baseArraySlice = base; numArraySlices = num; return *this; }
    };
    static const TextureSubresourceSet AllSubresources = TextureSubresourceSet{0, uint32_t(-1), 0, uint32_t(-1)};


    struct ITexture : public IResource
    {
        [[nodiscard]] virtual const TextureDesc& GetDesc() const = 0;
        virtual Object GetNativeView(
            ObjectType objType, 
            Format format = Format::UNKNOWN, 
            TextureSubresourceSet subresources = AllSubresources, 
            TextureDimension dimension = TextureDimension::Unknown, 
            bool isReadOnlyDSV = false) = 0;
    };
    using TextureHandle = RefPtr<ITexture>;
    
    

} // namespace DSM 



    template<> struct std::hash<DSM::TextureSubresourceSet>
    {
        std::size_t operator()(const DSM::TextureSubresourceSet& s) const noexcept
        {    
            std::size_t hash = 0;
            hash = DSM::Utility::HashCombine(hash, s.baseMipLevel);
            hash = DSM::Utility::HashCombine(hash, s.numMipLevels);
            hash = DSM::Utility::HashCombine(hash, s.baseArraySlice);
            hash = DSM::Utility::HashCombine(hash, s.numArraySlices);
            return hash;
        }
    };




#endif