#pragma once 
#ifndef __RESOURCE_BINDINGS_H__
#define __RESOURCE_BINDINGS_H__

#include "Shader.h"
#include "Texture.h"
#include "Buffer.h"
#include "Sampler.h"

namespace DSM {
    // identifies the underlying resource type in a binding
    enum class ResourceType : uint8_t
    {
        None,
        Texture_SRV,
        Texture_UAV,
        TypedBuffer_SRV,
        TypedBuffer_UAV,
        StructuredBuffer_SRV,
        StructuredBuffer_UAV,
        RawBuffer_SRV,
        RawBuffer_UAV,
        ConstantBuffer,
        VolatileConstantBuffer,
        Sampler,
        RayTracingAccelStruct,
        PushConstants,

        Count
    };

    // 描述资源的绑定布局
    struct BindingLayoutItem
    {
        uint32_t slot;
        uint16_t size : 16;
        ResourceType type : 8;
        uint8_t pad0 : 8;

        bool operator ==(const BindingLayoutItem& b) const noexcept
        {
            return slot == b.slot
                && type == b.type
                && size == b.size;
        }

        constexpr BindingLayoutItem& SetSlot(uint32_t value) noexcept { slot = value; return *this; }
        constexpr BindingLayoutItem& SetType(ResourceType value) noexcept { type = value; return *this; }
        constexpr BindingLayoutItem& SetSize(uint32_t value) noexcept { size = uint16_t(value); return *this; }

        uint32_t GetArraySize() const noexcept { return (type == ResourceType::PushConstants) ? 1 : size; }

        // Helper functions for strongly typed initialization
#define DSM_BINDING_LAYOUT_ITEM_INITIALIZER(TYPE) \
        static BindingLayoutItem TYPE(const uint32_t slot) noexcept { \
            BindingLayoutItem result{}; \
            result.slot = slot; \
            result.type = ResourceType::TYPE; \
            result.size = 1; \
            return result; }

        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(Texture_SRV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(Texture_UAV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(TypedBuffer_SRV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(TypedBuffer_UAV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(StructuredBuffer_SRV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(StructuredBuffer_UAV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(RawBuffer_SRV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(RawBuffer_UAV)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(ConstantBuffer)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(VolatileConstantBuffer)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(Sampler)
        DSM_BINDING_LAYOUT_ITEM_INITIALIZER(RayTracingAccelStruct)

        static BindingLayoutItem PushConstants(const uint32_t slot, const size_t size)
        {
            BindingLayoutItem result{};
            result.slot = slot;
            result.type = ResourceType::PushConstants;
            result.size = uint16_t(size);
            return result;
        }
#undef DSM_BINDING_LAYOUT_ITEM_INITIALIZER
    };


    struct BindingLayoutDesc
    {
        ShaderType visibility = ShaderType::None;
        uint32_t registerSpace = 0;
        std::vector<BindingLayoutItem> bindings;

        BindingLayoutDesc& SetVisibility(ShaderType value) { visibility = value; return *this; }
        BindingLayoutDesc& SetRegisterSpace(uint32_t value) { registerSpace = value; return *this; }
        BindingLayoutDesc& AddItem(const BindingLayoutItem& value) { bindings.push_back(value); return *this; }
    };

    struct BindlessLayoutDesc
    {

        // BindlessDescriptorType bridges the DX12 and Vulkan in supporting HLSL ResourceDescriptorHeap and SamplerDescriptorHeap
        // For DX12: 
        // - MutableSrvUavCbv, MutableCounters will enable D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED for the Root Signature
        // - MutableSampler will enable D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED for the Root Signature
        // - The BindingLayout will be ignored in terms of setting a descriptor set. DescriptorIndexing should use GetDescriptorIndexInHeap()
        // For Vulkan:
        // - The type corresponds to the SPIRV bindings which map to ResourceDescriptorHeap and SamplerDescriptorHeap
        // - The shader needs to be compiled with the same descriptor set index as is passed into setState
        // https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst#resourcedescriptorheaps-samplerdescriptorheaps
        enum class LayoutType
        {
            Immutable = 0,      // Must use registerSpaces to define a fixed descriptor type

            MutableSrvUavCbv,   // Corresponds to SPIRV binding -fvk-bind-resource-heap (Counter resources ResourceDescriptorHeap)
                                // Valid descriptor types: Texture_SRV, Texture_UAV, TypedBuffer_SRV, TypedBuffer_UAV,
                                // StructuredBuffer_SRV, StructuredBuffer_UAV, RawBuffer_SRV, RawBuffer_UAV, ConstantBuffer

            MutableCounters,    // Corresponds to SPIRV binding -fvk-bind-counter-heap (Counter resources accessed via ResourceDescriptorHeap)
                                // Valid descriptor types: StructuredBuffer_UAV

            MutableSampler,     // Corresponds to SPIRV binding -fvk-bind-sampler-heap (SamplerDescriptorHeap)
                                // Valid descriptor types: Sampler
        };

        ShaderType visibility = ShaderType::None;
        uint32_t firstSlot = 0;
        uint32_t maxCapacity = 0;
        StaticVector<BindingLayoutItem, c_MaxBindlessRegisterSpaces> registerSpaces;

        LayoutType layoutType = LayoutType::Immutable;

        BindlessLayoutDesc& SetVisibility(ShaderType value) { visibility = value; return *this; }
        BindlessLayoutDesc& SetFirstSlot(uint32_t value) { firstSlot = value; return *this; }
        BindlessLayoutDesc& SetMaxCapacity(uint32_t value) { maxCapacity = value; return *this; }
        BindlessLayoutDesc& AddRegisterSpace(const BindingLayoutItem& value) { registerSpaces.PushBack(value); return *this; }
        BindlessLayoutDesc& SetLayoutType(LayoutType value) { layoutType = value; return *this; }
    };

    struct IBindingLayout : public IResource
    {
        [[nodiscard]] virtual const BindingLayoutDesc* GetDesc() const = 0;           // returns nullptr for bindless layouts
        [[nodiscard]] virtual const BindlessLayoutDesc* GetBindlessDesc() const = 0;  // returns nullptr for regular layouts
    };
    using BindingLayoutHandle = std::shared_ptr<IBindingLayout>;



    
    struct BindingSetItem
    {
        IResource* resourceHandle;

        uint32_t slot;

        // Specifies the index in a binding array.
        // Must be less than the 'size' property of the matching BindingLayoutItem.
        // - DX11/12: Effective binding slot index is calculated as (slot + arrayElement), i.e. arrays are flattened
        // - Vulkan: Descriptor arrays are used.
        // This behavior matches the behavior of HLSL resource array declarations when compiled with DXC.
        uint32_t arrayElement;

        ResourceType type           : 8;
        TextureDimension dimension  : 8; // valid for Texture_SRV, Texture_UAV
        Format format               : 8; // valid for Texture_SRV, Texture_UAV, Buffer_SRV, Buffer_UAV
        uint8_t pad0                : 8;

        uint32_t pad1; // padding

        union 
        {
            TextureSubresourceSet subresources; // valid for Texture_SRV, Texture_UAV
            BufferRange range; // valid for Buffer_SRV, Buffer_UAV, ConstantBuffer
            uint64_t rawData[2];
        };

        // verify that the `subresources` and `range` have the same size and are covered by `rawData`
        static_assert(sizeof(TextureSubresourceSet) == 16, "sizeof(TextureSubresourceSet) is supposed to be 16 bytes");
        static_assert(sizeof(BufferRange) == 16, "sizeof(BufferRange) is supposed to be 16 bytes");

        BindingSetItem() {}

        bool operator ==(const BindingSetItem& b) const
        {
            return resourceHandle == b.resourceHandle
                && slot == b.slot
                && type == b.type
                && dimension == b.dimension
                && format == b.format
                && rawData[0] == b.rawData[0]
                && rawData[1] == b.rawData[1];
        }

        // Helper functions for strongly typed initialization

        static BindingSetItem None(uint32_t slot = 0)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::None;
            result.resourceHandle = nullptr;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.rawData[0] = 0;
            result.rawData[1] = 0;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem Texture_SRV(
            uint32_t slot, 
            ITexture* texture, 
            Format format = Format::UNKNOWN,
            TextureSubresourceSet subresources = AllSubresources, 
            TextureDimension dimension = TextureDimension::Unknown)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Texture_SRV;
            result.resourceHandle = texture;
            result.format = format;
            result.dimension = dimension;
            result.subresources = subresources;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem Texture_UAV(
            uint32_t slot, 
            ITexture* texture, 
            Format format = Format::UNKNOWN,
            TextureSubresourceSet subresources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::AllArraySlices),
            TextureDimension dimension = TextureDimension::Unknown)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Texture_UAV;
            result.resourceHandle = texture;
            result.format = format;
            result.dimension = dimension;
            result.subresources = subresources;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem TypedBuffer_SRV(uint32_t slot, IBuffer* buffer, Format format = Format::UNKNOWN, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::TypedBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem TypedBuffer_UAV(uint32_t slot, IBuffer* buffer, Format format = Format::UNKNOWN, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::TypedBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem ConstantBuffer(uint32_t slot, IBuffer* buffer, BufferRange range = EntireBuffer)
        {
            bool isVolatile = buffer && buffer->GetDesc().isVolatile;

            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = isVolatile ? ResourceType::VolatileConstantBuffer : ResourceType::ConstantBuffer;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem Sampler(uint32_t slot, ISampler* sampler)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::Sampler;
            result.resourceHandle = sampler;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.rawData[0] = 0;
            result.rawData[1] = 0;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem StructuredBuffer_SRV(uint32_t slot, IBuffer* buffer, Format format = Format::UNKNOWN, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::StructuredBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem StructuredBuffer_UAV(uint32_t slot, IBuffer* buffer, Format format = Format::UNKNOWN, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::StructuredBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = format;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem RawBuffer_SRV(uint32_t slot, IBuffer* buffer, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::RawBuffer_SRV;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem RawBuffer_UAV(uint32_t slot, IBuffer* buffer, BufferRange range = EntireBuffer)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::RawBuffer_UAV;
            result.resourceHandle = buffer;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range = range;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        static BindingSetItem PushConstants(uint32_t slot, uint32_t byteSize)
        {
            BindingSetItem result;
            result.slot = slot;
            result.arrayElement = 0;
            result.type = ResourceType::PushConstants;
            result.resourceHandle = nullptr;
            result.format = Format::UNKNOWN;
            result.dimension = TextureDimension::Unknown;
            result.range.byteOffset = 0;
            result.range.byteSize = byteSize;
            result.pad0 = 0;
            result.pad1 = 0;
            return result;
        }

        BindingSetItem& SetArrayElement(uint32_t value) { arrayElement = value; return *this; }
        BindingSetItem& SetFormat(Format value) { format = value; return *this; }
        BindingSetItem& SetDimension(TextureDimension value) { dimension = value; return *this; }
        BindingSetItem& SetSubresources(TextureSubresourceSet value) { subresources = value; return *this; }
        BindingSetItem& SetRange(BufferRange value) { range = value; return *this; }
    };

    // verify the packing of BindingSetItem for good alignment
    static_assert(sizeof(BindingSetItem) == 40, "sizeof(BindingSetItem) is supposed to be 40 bytes");

    // Describes a set of bindings corresponding to one binidng layout
    struct BindingSetDesc
    {
        std::vector<BindingSetItem> bindings;
       
        // Enables automatic liveness tracking of this binding set by nvrhi command lists.
        // By setting trackLiveness to false, you take the responsibility of not releasing it 
        // until all rendering commands using the binding set are finished.
        bool trackLiveness = true;

        bool operator ==(const BindingSetDesc& b) const
        {
            if (bindings.size() != b.bindings.size())
                return false;

            for (size_t i = 0; i < bindings.size(); ++i)
            {
                if (bindings[i] != b.bindings[i])
                    return false;
            }

            return true;
        }

        bool operator !=(const BindingSetDesc& b) const
        {
            return !(*this == b);
        }

        BindingSetDesc& AddItem(const BindingSetItem& value) { bindings.push_back(value); return *this; }
        BindingSetDesc& SetTrackLiveness(bool value) { trackLiveness = value; return *this; }
    };

    class IBindingSet : public IResource
    {
    public:
        [[nodiscard]] virtual const BindingSetDesc* GetDesc() const = 0;  // returns nullptr for descriptor tables
        [[nodiscard]] virtual IBindingLayout* GetLayout() const = 0;
    };
    using BindingSetHandle = std::shared_ptr<IBindingSet>;

    // Descriptor tables are bare, without extra mappings, state, or liveness tracking.
    // Unlike binding sets, descriptor tables are mutable - moreover, modification is the only way to populate them.
    // They can be grown or shrunk, and they are not tied to any binding layout.
    // All tracking is off, so applications should use descriptor tables with great care.
    // IDescriptorTable is derived from IBindingSet to allow mixing them in the binding arrays.
    class IDescriptorTable : public IBindingSet
    {
    public:
        [[nodiscard]] virtual uint32_t GetCapacity() const = 0;
        [[nodiscard]] virtual uint32_t GetFirstDescriptorIndexInHeap() const = 0;
    };
    using DescriptorTableHandle = std::shared_ptr<IDescriptorTable>;


} // namespace DSM 

#endif