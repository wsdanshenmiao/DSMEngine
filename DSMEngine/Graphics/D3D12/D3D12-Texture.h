#pragma once
#ifndef __D3D12_TEXTURE_H__
#define __D3D12_TEXTURE_H__

#include "D3D12Common.h"
#include "D3D12-Device.h"

namespace DSM::D3D12{
    class Texture : public ITexture
    {
    public:
        Texture(const Context& context, DeviceResources& resources, 
            TextureDesc desc, D3D12_RESOURCE_DESC resourceDesc)
            :m_Context(context), m_Resources(resources), 
            m_Desc(std::move(desc)), resourceDesc(std::move(resourceDesc)) {}
        ~Texture();

        const TextureDesc& GetDesc() const override { return m_Desc; }
        Object GetNativeObject(ObjectType type) override;
        Object GetNativeView(
            ObjectType objType, 
            Format format = Format::UNKNOWN, 
            TextureSubresourceSet subresources = AllSubresources, 
            TextureDimension dimension = TextureDimension::Unknown, 
            bool isReadOnlyDSV = false) override;

        // 创建资源的各种视图
        void CreateSRV(size_t descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const;
        void CreateUAV(size_t descriptor, Format format, TextureDimension dimension, TextureSubresourceSet subresources) const;
        void CreateRTV(size_t descriptor, Format format, TextureSubresourceSet subresources) const;
        void CreateUAV(size_t descriptor, TextureSubresourceSet subresources, bool isReadOnly = false) const;

        uint32_t GetClearMipLevelUAV(uint32_t mipLevel);

    public:
        const D3D12_RESOURCE_DESC resourceDesc;
        RefPtr<ID3D12Resource> resource;
        HANDLE sharedHandle;
        HeapHandle heap;
        ResourceStates permanentState = ResourceStates::Unknown;

    private:
        using TextureBindingHashMap = std::unordered_map<TextureBindingKey, uint32_t>;

        const Context& m_Context;
        const TextureDesc m_Desc;
        DeviceResources& m_Resources;

        // 储存子资源对应的描述符索引
        TextureBindingHashMap m_RenderTargetViews{};
        TextureBindingHashMap m_DepthStencilViews{};
        TextureBindingHashMap m_CustomSRVs{};
        TextureBindingHashMap m_CustomUAVs{};
        std::vector<uint32_t> m_ClearMipLevelUAVs;
    };


} // namespace DSM::D3D12

#endif