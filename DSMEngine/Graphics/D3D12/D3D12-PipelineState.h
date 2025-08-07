#pragma once
#ifndef __D3D12_PIPELINESTATE_H__
#define __D3D12_PIPELINESTATE_H__

#include "D3D12Common.h"

namespace DSM::D3D12 {

    class GraphicsPipeline : public IGraphicsPipeline
    {
    public:
        GraphicsPipeline(GraphicsPipelineDesc desc, FramebufferInfo framebufferInfo);
        ~GraphicsPipeline();

        const GraphicsPipelineDesc& GetDesc() const override { return m_Desc; }
        const FramebufferInfo& GetFramebufferInfo() const override { return m_FramebufferInfo; }
        Object GetNativeObject(ObjectType objectType) override;

    public:
        RefPtr<ID3D12PipelineState> pipelineState;
        RefPtr<RootSignature> rootSignature;

        bool requiresBlendFactor = false;

    private:
        GraphicsPipelineDesc m_Desc;
        FramebufferInfo m_FramebufferInfo;
    };

} // namespace DSM::D3D12

#endif