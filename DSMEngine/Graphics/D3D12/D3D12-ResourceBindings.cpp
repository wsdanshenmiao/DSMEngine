#include "D3D12-ResourceBindings.h"
#include "D3D12-Sampler.h"
#include "D3D12-Buffer.h"
#include "D3D12-Texture.h"
#include "D3D12-Device.h"

namespace DSM::D3D12 {
    static ResourceType GetNormalizedResourceType(ResourceType type)
    {
        switch (type)
        {
        case ResourceType::StructuredBuffer_UAV:
        case ResourceType::RawBuffer_UAV:
            return ResourceType::TypedBuffer_UAV;
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::RawBuffer_SRV:
            return ResourceType::TypedBuffer_SRV;
        default:
            return type;
        }
    }
    
    // 检测两个类型是否兼容
    static bool AreResourceTypesCompatible(ResourceType a, ResourceType b)
    {
        if (a == b)
            return true;

        a = GetNormalizedResourceType(a);
        b = GetNormalizedResourceType(b);

        if ((a == ResourceType::TypedBuffer_SRV && b == ResourceType::Texture_SRV) ||
            (b == ResourceType::TypedBuffer_SRV && a == ResourceType::Texture_SRV) ||
            (a == ResourceType::TypedBuffer_SRV && b == ResourceType::RayTracingAccelStruct) ||
            (a == ResourceType::Texture_SRV && b == ResourceType::RayTracingAccelStruct) ||
            (b == ResourceType::TypedBuffer_SRV && a == ResourceType::RayTracingAccelStruct) ||
            (b == ResourceType::Texture_SRV && a == ResourceType::RayTracingAccelStruct))
            return true;

        if ((a == ResourceType::TypedBuffer_UAV && b == ResourceType::Texture_UAV) ||
            (b == ResourceType::TypedBuffer_UAV && a == ResourceType::Texture_UAV))
            return true;

        return false;
    }

    BindingLayout::BindingLayout(BindingLayoutDesc desc)
        : m_Desc(std::move(desc)){
        D3D12_ROOT_CONSTANTS rootConstants{};   // 根常数
        
        ResourceType currType = ResourceType(-1);
        uint32_t currSlot = uint32_t(-1);

        for(const BindingLayoutItem& binding : m_Desc.bindings){
            if(binding.type == ResourceType::VolatileConstantBuffer){
                D3D12_ROOT_DESCRIPTOR1 rootDescriptor{};
                rootDescriptor.ShaderRegister = binding.slot;
                rootDescriptor.RegisterSpace = m_Desc.registerSpace;
                // 描述数据的波动性，驱动层会启动对应优化
                rootDescriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;
                rootParametersVolatileCB.EmplaceBack(-1, rootDescriptor);
            }
            else if(binding.type == ResourceType::PushConstants){
                pushConstantByteSize = binding.size;

                rootConstants.ShaderRegister = binding.slot;
                rootConstants.RegisterSpace = m_Desc.registerSpace;
                rootConstants.Num32BitValues = binding.size / 4; // 每个32位值占用4个字节
            }
            else if(!AreResourceTypesCompatible(binding.type, currType) && binding.slot != currSlot + 1){
                // 创建一个新的 DescriptorRange
                if(binding.type == ResourceType::Sampler){
                    D3D12_DESCRIPTOR_RANGE1 range{};
                    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                    range.NumDescriptors = binding.GetArraySize();
                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = m_Desc.registerSpace;
                    range.OffsetInDescriptorsFromTableStart = descriptorTableSizeSamplers;
                    
                    descriptorTableSizeSamplers += binding.size;
                    descriptorRangeSamplers.push_back(std::move(range));
                }
                else {
                    D3D12_DESCRIPTOR_RANGE1 range{};

                    switch (binding.type) {
                    case ResourceType::Texture_SRV:
                    case ResourceType::TypedBuffer_SRV:
                    case ResourceType::StructuredBuffer_SRV:
                    case ResourceType::RawBuffer_SRV:
                    case ResourceType::RayTracingAccelStruct:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        break;
                    case ResourceType::Texture_UAV:
                    case ResourceType::TypedBuffer_UAV:
                    case ResourceType::StructuredBuffer_UAV:
                    case ResourceType::RawBuffer_UAV:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;
                    case ResourceType::ConstantBuffer:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                        break;
                    default:
                        assert(!"Invalid resource type.");
                        continue;
                    }
                    range.NumDescriptors = binding.size;
                    range.BaseShaderRegister = binding.slot;
                    range.RegisterSpace = desc.registerSpace;
                    range.OffsetInDescriptorsFromTableStart = descriptorTableSizeSRVs;
                    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

                    descriptorTableSizeSRVs += binding.size;

                    descriptorRangeSRVs.push_back(std::move(range));
                }

                currType = binding.type;
                currSlot = binding.slot;
            }
            else{   // 兼容则扩展当前 descriptor range
                if(binding.type == ResourceType::Sampler){
                    assert(!descriptorRangeSamplers.empty());
                    auto& range = descriptorRangeSamplers.back();
                    range.NumDescriptors += binding.GetArraySize();
                    descriptorTableSizeSamplers += binding.GetArraySize();
                }
                else{
                    assert(!descriptorRangeSRVs.empty());
                    auto& range = descriptorRangeSRVs.back();
                    range.NumDescriptors += binding.size;
                    descriptorTableSizeSRVs += binding.size;
                }
                currSlot = binding.slot;
            }
        }

        // 根常量
        if(rootConstants.Num32BitValues > 0){
            D3D12_ROOT_PARAMETER1 rootParameter{};
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            rootParameter.ShaderVisibility = ConvertShaderStage(m_Desc.visibility);
            rootParameter.Constants = rootConstants;

            rootParameters.push_back(std::move(rootParameter));
            rootConstantsIndex = rootParameters.size() - 1;
        }
        // 常量缓冲区
        for(auto& [index, rootDescriptor] : rootParametersVolatileCB){
            D3D12_ROOT_PARAMETER1 rootParameter{};
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParameter.ShaderVisibility = ConvertShaderStage(m_Desc.visibility);
            rootParameter.Descriptor = rootDescriptor;

            rootParameters.push_back(std::move(rootParameter));
            index = rootParameters.size() - 1;
        }
        // 采样器
        if(descriptorTableSizeSamplers > 0){
            D3D12_ROOT_PARAMETER1 rootParameter{};
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParameter.ShaderVisibility = ConvertShaderStage(m_Desc.visibility);
            rootParameter.DescriptorTable.NumDescriptorRanges = descriptorRangeSamplers.size();
            rootParameter.DescriptorTable.pDescriptorRanges = descriptorRangeSamplers.data();

            rootParameters.push_back(std::move(rootParameter));
            rootParameterSamplers = rootParameters.size() - 1;
        }
        // SRVs
        if(descriptorTableSizeSRVs > 0){
            D3D12_ROOT_PARAMETER1 rootParameter{};
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParameter.ShaderVisibility = ConvertShaderStage(m_Desc.visibility);
            rootParameter.DescriptorTable.NumDescriptorRanges = descriptorRangeSRVs.size();
            rootParameter.DescriptorTable.pDescriptorRanges = descriptorRangeSRVs.data();

            rootParameters.push_back(std::move(rootParameter));
            rootParameterSRVs = rootParameters.size() - 1;
        }
    }
    
    BindlessLayout::BindlessLayout(BindlessLayoutDesc desc)
        :m_Desc(std::move(desc)){
        // 每个寄存器空间绑定一个描述符表
        for(const auto& binding : m_Desc.registerSpaces){
            D3D12_DESCRIPTOR_RANGE1 range{};
            range.NumDescriptors = UINT(-1);    // 无边界
            range.BaseShaderRegister = m_Desc.firstSlot;
            range.RegisterSpace = binding.slot;
            range.OffsetInDescriptorsFromTableStart = 0;
            range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

            switch (binding.type)
            {
            case ResourceType::Texture_SRV: 
            case ResourceType::TypedBuffer_SRV:
            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RayTracingAccelStruct:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                break;
            case ResourceType::ConstantBuffer:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                break;
            case ResourceType::Texture_UAV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_UAV:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                break;
            case ResourceType::Sampler:
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                break;
            default:
                assert(!"Invalid resource type for bindless layout");
                continue;
            }

            descriptorRanges.PushBack(std::move(range));
        }

        if (desc.layoutType == BindlessLayoutDesc::LayoutType::Immutable)
        {
            rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParameter.ShaderVisibility = ConvertShaderStage(desc.visibility);
            rootParameter.DescriptorTable.NumDescriptorRanges = descriptorRanges.Size();
            rootParameter.DescriptorTable.pDescriptorRanges = descriptorRanges.Data();
        }
    }
    
    
    
    Object RootSignature::GetNativeObject(ObjectType type)
    {
        if(type == ObjectTypes::D3D12_RootSignature){
            return Object{rootSignature.Get()};
        }
        else{
            return Object{nullptr};
        }
    }
    
    RootSignature::~RootSignature()
    {
        if(auto it = m_Resources.rootsigCache.find(hash); 
            it != m_Resources.rootsigCache.end()) {
            m_Resources.rootsigCache.erase(it);
        }
    }

    BindingSet::BindingSet(const Context &context, DeviceResources &deviceResources, BindingSetDesc desc, BindingLayout *layout)
        : m_Resources(deviceResources), m_Desc(std::move(desc)), bindingLayout(layout)
    {
        assert(bindingLayout != nullptr);

        // 处理易变的常量常量缓冲区
        for(const auto& [index, descriptor] : bindingLayout->rootParametersVolatileCB) {
            IBuffer* buffer{};
            auto it = std::find_if(m_Desc.bindings.begin(), m_Desc.bindings.end(),
                [descriptor](const BindingSetItem& binding) {
                    return binding.type == ResourceType::VolatileConstantBuffer && 
                        descriptor.ShaderRegister == binding.slot;
                });
            
                if(it != m_Desc.bindings.end()){
                assert(it->resourceHandle != nullptr);
                buffer = Utility::CheckedCast<IBuffer*>(it->resourceHandle);
                resources.push_back(ResourceHandle{it->resourceHandle});
            }

            rootParametersVolatileCB.EmplaceBack(index, buffer);
        }

        // 处理采样器
        if(bindingLayout->descriptorTableSizeSamplers > 0){
            CreateSamplerDescriptors(context);
        }
        // 处理 SRVs
        if(bindingLayout->descriptorTableSizeSRVs > 0){
            CreateSRVDescriptors(context);
        }
    }

    BindingSet::~BindingSet()
    {
        m_Resources.shaderResourceViewHeap.ReleaseDescriptors(descriptorIndexSRVs, bindingLayout->descriptorTableSizeSRVs);
        m_Resources.samplerHeap.ReleaseDescriptors(descriptorIndexSamplers, bindingLayout->descriptorTableSizeSamplers);
    }

    void BindingSet::CreateSamplerDescriptors(const Context &context)
    {
        uint32_t rootParametersIndex = bindingLayout->rootParameterSamplers;
        uint32_t numSamplers = bindingLayout->descriptorTableSizeSamplers;
        uint32_t descriptorTableBaseIndex = m_Resources.samplerHeap.AllocateDescriptors(numSamplers);
    
        hasSamplers = true;
        descriptorIndexSamplers = descriptorTableBaseIndex;

        // 每一个 DescriptorRange
        for(const auto& range : bindingLayout->descriptorRangeSamplers){
            // Range 中的每一个元素
            for(uint32_t i = 0; i < range.NumDescriptors; ++i){
                uint32_t slot = range.BaseShaderRegister + i;
                auto descriptorIndex = descriptorTableBaseIndex + range.OffsetInDescriptorsFromTableStart + i;
                D3D12_CPU_DESCRIPTOR_HANDLE handle = m_Resources.samplerHeap.GetCpuHandle(descriptorIndex);

                auto it = std::find_if(m_Desc.bindings.begin(), m_Desc.bindings.end(),
                    [slot](const BindingSetItem& binding) {
                        return binding.type == ResourceType::Sampler && 
                            (binding.slot + binding.arrayElement) == slot;
                    });

                if (it != m_Desc.bindings.end()) {
                    resources.push_back(ResourceHandle{it->resourceHandle});
                    Utility::CheckedCast<Sampler*>(it->resourceHandle)->CreateDescriptor(handle.ptr);
                }
                else{
                    // 没有采样器则创建一个默认采样器
                    D3D12_SAMPLER_DESC samplerDesc{};
                    context.m_Device->CreateSampler(&samplerDesc, handle);
                }
            }
        }
        // 将描述符拷贝到可见堆，随后绑定到命令列表
        m_Resources.samplerHeap.CopyToShaderVisibleHeap(descriptorTableBaseIndex, numSamplers);
    }

    void BindingSet::CreateSRVDescriptors(const Context &context)
    {
        uint32_t rootParametersIndex = bindingLayout->rootParameterSRVs;
        uint32_t numSRVs = bindingLayout->descriptorTableSizeSRVs;
        uint32_t descriptorTableBaseIndex = m_Resources.shaderResourceViewHeap.AllocateDescriptors(numSRVs);

        hasSRVs = true;
        descriptorIndexSRVs = descriptorTableBaseIndex;

        for(const auto& range : bindingLayout->descriptorRangeSRVs){
            for(uint32_t i = 0; i < range.NumDescriptors; ++i){
                uint32_t slot = range.BaseShaderRegister + i;
                auto descriptorIndex = descriptorTableBaseIndex + range.OffsetInDescriptorsFromTableStart + i;
                D3D12_CPU_DESCRIPTOR_HANDLE handle = m_Resources.shaderResourceViewHeap.GetCpuHandle(descriptorIndex);

                bool found = false;
                IResource* resource{};

                for(size_t j = 0; j < m_Desc.bindings.size(); ++j){
                    const BindingSetItem& binding = m_Desc.bindings[j];

                    if(binding.slot + binding.arrayElement != slot) {
                        continue;
                    }

                    ResourceType bindingType = GetNormalizedResourceType(binding.type);

                    auto checkType = [=](const auto& rangeType, ResourceType resourceType){
                        return range.RangeType == rangeType && bindingType == resourceType;
                    };

                    if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ResourceType::TypedBuffer_SRV)){
                        if(binding.resourceHandle != nullptr){
                            Buffer* buffer = Utility::CheckedCast<Buffer*>(binding.resourceHandle);
                            buffer->CreateSRV(handle.ptr, binding.format, binding.range, binding.type);
                            resource = buffer;

                            if(!buffer->permanentState){
                                needTransitionsBindingsIndices.push_back(static_cast<uint16_t>(j));
                            }
                            else{
                                VerifyPermanentResourceState(buffer->permanentState, ResourceStates::ShaderResource, 
                                    false, buffer->GetDesc().debugName, context.m_MessageCallback);
                            }
                        }
                        else{
                            Buffer::CreateNullSRV(handle.ptr, binding.format, context);
                        }
                        found = true;
                    }
                    else if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, ResourceType::TypedBuffer_UAV)){
                        if(binding.resourceHandle != nullptr){
                            Buffer* buffer = Utility::CheckedCast<Buffer*>(binding.resourceHandle);
                            buffer->CreateUAV(handle.ptr, binding.format, binding.range, binding.type);
                            resource = buffer;

                            if(!buffer->permanentState){
                                needTransitionsBindingsIndices.push_back(static_cast<uint16_t>(j));
                            }
                            else{
                                VerifyPermanentResourceState(buffer->permanentState, ResourceStates::UnorderedAccess, 
                                    false, buffer->GetDesc().debugName, context.m_MessageCallback);
                            }
                        }
                        else{
                            Buffer::CreateNullUAV(handle.ptr, binding.format, context);
                        }
                        
                        hasUAVs = true;
                        found = true;
                    }
                    else if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ResourceType::Texture_SRV)) {
                        auto texture = Utility::CheckedCast<Texture*>(binding.resourceHandle);
                        assert(texture != nullptr);
                        texture->CreateSRV(handle.ptr, binding.format, binding.dimension, binding.subresources);
                        resource = texture;

                        if(!texture->permanentState){
                            needTransitionsBindingsIndices.push_back(static_cast<uint16_t>(j));
                        }
                        else{
                            VerifyPermanentResourceState(texture->permanentState, ResourceStates::ShaderResource, 
                                false, texture->GetDesc().debugName, context.m_MessageCallback);
                        }
                        found = true;
                    }
                    else if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, ResourceType::Texture_UAV)) {
                        auto texture = Utility::CheckedCast<Texture*>(binding.resourceHandle);
                        assert(texture != nullptr);
                        texture->CreateUAV(handle.ptr, binding.format, binding.dimension, binding.subresources);
                        resource = texture;

                        if(!texture->permanentState){
                            needTransitionsBindingsIndices.push_back(static_cast<uint16_t>(j));
                        }
                        else{
                            VerifyPermanentResourceState(texture->permanentState, ResourceStates::UnorderedAccess, 
                                false, texture->GetDesc().debugName, context.m_MessageCallback);
                        }
                        found = true;
                        hasUAVs = true;
                    }
                    else if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, ResourceType::ConstantBuffer)) {
                        auto buffer = Utility::CheckedCast<Buffer*>(binding.resourceHandle);
                        assert(buffer != nullptr);
                        buffer->CreateCBV(handle.ptr, binding.range);
                        resource = buffer;

                        if(buffer->GetDesc().isVolatile){
                            std::string msg = std::format("Attempted to bind a volatile constant buffer {} to a non-volatile CB layout at slot b{}",
                                DebugNameToString(buffer->GetDesc().debugName), binding.slot);
                            context.Error(msg);
                            break;
                        }

                        if(!buffer->permanentState){
                            needTransitionsBindingsIndices.push_back(static_cast<uint16_t>(j));
                        }
                        else{
                            VerifyPermanentResourceState(buffer->permanentState, ResourceStates::ConstantBuffer, 
                                false, buffer->GetDesc().debugName, context.m_MessageCallback);
                        }
                        found = true;
                    }
                    else if(checkType(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, ResourceType::RayTracingAccelStruct)){
                        // TODO: 后续支持光追时添加资源的绑定
                    }
                    break;
                }

                if(resource != nullptr){
                    resources.push_back(ResourceHandle{resource});
                }

                if(!found){
                    switch (range.RangeType) {
                    case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                        Buffer::CreateNullSRV(handle.ptr, Format::UNKNOWN, context);
                        break;
                    case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                        Buffer::CreateNullUAV(handle.ptr, Format::UNKNOWN, context);
                        break;
                    case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                        context.m_Device->CreateConstantBufferView(nullptr, handle);
                        break;
                    case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    default:
                        assert(!"Invalid descriptor range type");
                        break;
                    }
                }
            }
        }
        
        m_Resources.shaderResourceViewHeap.CopyToShaderVisibleHeap(descriptorTableBaseIndex, numSRVs);
    }


    
    DescriptorTable::DescriptorTable(DeviceResources &resources)
        : m_Resources(resources) {}

    DescriptorTable::~DescriptorTable()
    {
        m_Resources.shaderResourceViewHeap.ReleaseDescriptors(firstDescriptor, capacity);
    }

} // namespace DSM::D3D12