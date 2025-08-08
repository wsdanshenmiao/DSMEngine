#include "D3D12-PipelineState.h"
#include "D3D12-ResourceBindings.h"

namespace DSM::D3D12 {
    
    GraphicsPipeline::GraphicsPipeline(GraphicsPipelineDesc desc, FramebufferInfo framebufferInfo)
        : m_Desc(std::move(desc)), m_FramebufferInfo(std::move(framebufferInfo)) { }

    Object GraphicsPipeline::GetNativeObject(ObjectType objectType)
    {
        switch (objectType) {
        case ObjectTypes::D3D12_PipelineState:
            return pipelineState.Get();
        case ObjectTypes::D3D12_RootSignature:
            return rootSignature->GetNativeObject(ObjectTypes::D3D12_RootSignature);
        default:
            return Object{nullptr};
        }
    }

    ComputePipeline::ComputePipeline(ComputePipelineDesc desc)
        : m_Desc(std::move(desc)) { }

    Object ComputePipeline::GetNativeObject(ObjectType objectType)
    {
        switch (objectType) {
        case ObjectTypes::D3D12_RootSignature:
            return rootSignature->GetNativeObject(objectType);
        case ObjectTypes::D3D12_PipelineState:
            return Object(pipelineState.Get());
        default:
            return nullptr;
        }
    }

    MeshletPipeline::MeshletPipeline(MeshletPipelineDesc desc, FramebufferInfo fbInfo)
        :m_Desc(std::move(desc)), m_FrameBufferInfo(std::move(fbInfo)) {}

    Object MeshletPipeline::GetNativeObject(ObjectType type)
    {
        switch (type) {
        case ObjectTypes::D3D12_PipelineState:
            return pipelineState.Get();
        case ObjectTypes::D3D12_RootSignature:
            return rootSignature->GetNativeObject(ObjectTypes::D3D12_RootSignature);
        default:
            return Object{nullptr};
        }
    }

} // namespace DSM::D3D12
