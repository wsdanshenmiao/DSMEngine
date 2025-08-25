#pragma once
#ifndef __D3D12_RESOURCE_BINDINGS_H__
#define __D3D12_RESOURCE_BINDINGS_H__

#include "D3D12Common.h"

namespace DSM::D3D12 {
    class DeviceResources;

    class BindingLayout : public IBindingLayout
    {
    public:
        BindingLayout(BindingLayoutDesc desc);

        const BindingLayoutDesc* GetDesc() const override { return &m_Desc; }
        const BindlessLayoutDesc* GetBindlessDesc() const override { return nullptr; }

    public:
        using VolatileCBDescriptorVector = StaticVector<std::pair<uint32_t, D3D12_ROOT_DESCRIPTOR1>, c_MaxVolatileConstantBuffersPerLayout>;
        VolatileCBDescriptorVector rootParametersVolatileCBs;

        uint32_t pushConstantByteSize = 0;

        uint32_t descriptorTableSizeSamplers = 0;;
        uint32_t descriptorTableSizeSRVs = 0;
        std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRangeSamplers;
        std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRangeSRVs;
        
        uint32_t rootConstantsIndex = 0;
        uint32_t rootParameterSamplers = 0;
        uint32_t rootParameterSRVs = 0;
        std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
        
    private:
        BindingLayoutDesc m_Desc;
    };

    // Bindless 使用描述符表实现
    class BindlessLayout : public IBindingLayout
    {
    public:
        BindlessLayout(BindlessLayoutDesc desc);

        const BindingLayoutDesc* GetDesc() const override { return nullptr; }
        const BindlessLayoutDesc* GetBindlessDesc() const override { return &m_Desc; }

    public:
        StaticVector<D3D12_DESCRIPTOR_RANGE1, 32> descriptorRanges;
        D3D12_ROOT_PARAMETER1 rootParameter{};

    private:
        BindlessLayoutDesc m_Desc;
    };

    class RootSignature : public IRootSignature
    {
    public:
        RootSignature(DeviceResources& deviceResources)
            : m_Resources(deviceResources) {}
        ~RootSignature() override;
        
        Object GetNativeObject(ObjectType type) override;

    public:
        size_t hash = 0;
        RefPtr<ID3D12RootSignature> rootSignature{};
        StaticVector<std::pair<uint32_t, BindingLayoutHandle>, c_MaxBindingLayouts> pipelineLayouts;

        uint32_t pushConstantByteSize = 0;
        uint32_t rootConstantsIndex;
        
    private:
        DeviceResources& m_Resources;
    };

    class BindingSet : public IBindingSet
    {
    public:
        BindingSet(const Context& context ,
            DeviceResources& deviceResources,
            BindingSetDesc desc,
            BindingLayout* layout);
        ~BindingSet() override;

        const BindingSetDesc* GetDesc() const override { return &m_Desc; }
        IBindingLayout* GetLayout() const override { return bindingLayout.Get(); }

    private:
        void CreateSamplerDescriptors(const Context& context);
        void CreateSRVDescriptors(const Context& context);

    public:
        struct VolatileBufferBinding
        {
            uint32_t rootIndex;
            IBuffer* buffer;
            uint64_t offset;
        };
        using VolatileCBVector = StaticVector<VolatileBufferBinding, c_MaxVolatileConstantBuffersPerLayout>;
        VolatileCBVector rootParametersVolatileCBs{};
        RefPtr<BindingLayout> bindingLayout;
        std::vector<ResourceHandle> resources;

        // 是否有描述符
        bool hasSamplers = false;
        bool hasSRVs = false;
        bool hasUAVs = false;

        // 描述符在堆中的索引
        uint32_t descriptorIndexSamplers = 0;
        uint32_t descriptorIndexSRVs = 0;

    private:
        DeviceResources& m_Resources;
        BindingSetDesc m_Desc;
    };


    class DescriptorTable : public IDescriptorTable
    {
    public:
        DescriptorTable(DeviceResources& resources);
        ~DescriptorTable() override;

        const BindingSetDesc* GetDesc() const override { return nullptr; }
        IBindingLayout* GetLayout() const override { return nullptr; }
        uint32_t GetCapacity() const override { return capacity; }
        uint32_t GetFirstDescriptorIndex() const override { return firstDescriptor; }

    public:
        uint32_t capacity = 0;
        uint32_t firstDescriptor = 0;

    private:
        DeviceResources& m_Resources;
    };

} // namespace DSM 

#endif