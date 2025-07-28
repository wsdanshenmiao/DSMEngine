#pragma once
#ifndef __D3D12_DEVICE_H__
#define __D3D12_DEVICE_H__

#include "DescriptorHeap.h"

namespace DSM::D3D12 {
    
    class DeviceResources
    {
    public:
        explicit DeviceResources(const Context& context, const DeviceDesc& desc)
            :m_Context(context), renderTargetViewHeap(context), depthStencilViewHeap(context),
            shaderResourceViewHeap(context), samplerHeap(context) {}

    public:
        DescriptorHeap renderTargetViewHeap;
        DescriptorHeap depthStencilViewHeap;
        DescriptorHeap shaderResourceViewHeap;
        DescriptorHeap samplerHeap;

        // 根签名的缓存
        std::unordered_map<size_t, RootSignature*> rootsigCache;

    private:
        const Context& m_Context;
    };

    class Device final : public IDevice
    {
    public:
        TextureHandle CreateTexture(const TextureDesc& desc) override;

    private:
        Context m_Context;
        DeviceResources m_Resources;
    };
} // namespace DSM 

#endif