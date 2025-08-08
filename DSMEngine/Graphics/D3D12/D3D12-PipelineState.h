#pragma once
#ifndef __D3D12_PIPELINESTATE_H__
#define __D3D12_PIPELINESTATE_H__

#include "D3D12Common.h"

namespace DSM::D3D12 {

    class GraphicsPipeline : public IGraphicsPipeline
    {
    public:
        GraphicsPipeline(GraphicsPipelineDesc desc, FramebufferInfo framebufferInfo);

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

    class ComputePipeline : public IComputePipeline
    {
    public:
        ComputePipeline(ComputePipelineDesc desc);

        const ComputePipelineDesc& GetDesc() const override { return m_Desc; }
        Object GetNativeObject(ObjectType objectType) override;

    public:
        RefPtr<RootSignature> rootSignature;
        RefPtr<ID3D12PipelineState> pipelineState;

    private:
        ComputePipelineDesc m_Desc;
    };

    class MeshletPipeline : public IMeshletPipeline
    {
    public:
        MeshletPipeline(MeshletPipelineDesc desc, FramebufferInfo fbInfo);

        const MeshletPipelineDesc& GetDesc() const override { return m_Desc; }
        const FramebufferInfo& GetFramebufferInfo() const override { return m_FrameBufferInfo; }
        Object GetNativeObject(ObjectType type) override;

    public:
        RefPtr<RootSignature> rootSignature{};
        RefPtr<ID3D12PipelineState> pipelineState{};

        bool requiresBlendFactor = false;

    private:
        MeshletPipelineDesc m_Desc{};
        FramebufferInfo m_FrameBufferInfo{};
    };

} // namespace DSM::D3D12

#endif