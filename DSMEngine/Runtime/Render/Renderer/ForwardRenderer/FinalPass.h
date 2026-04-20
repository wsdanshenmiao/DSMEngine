#pragma once
#ifndef __FINALPASS_H__
#define __FINALPASS_H__

namespace DSM {
    class FinalPass : public IRenderPass
    {
    public:
        FinalPass(GraphicsRenderer& renderer)
        {
            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            device->QueueWaitForCommandList(
                CommandQueueType::Graphics, 
                CommandQueueType::Compute, 
                RenderResource::GetInstance().GetRenderPassFinishFence(RenderPass::PostEffect));
            device->RunGarbageCollection();
            return 0;
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override { }

    public:
        inline static TimerQueryHandle sm_TimerQuery{};
    };

} // namespace DSM


#endif