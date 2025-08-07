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

} // namespace DSM::D3D12
