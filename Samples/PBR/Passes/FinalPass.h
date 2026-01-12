#pragma once
#ifndef __FINALPASS_H__
#define __FINALPASS_H__

#include "ShadowPass.h"

namespace DSM {
    class FinalPass : public IRenderPass
    {
    public:
        FinalPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();
            
            // 为前面 Pass 收集的 Desc 创建 Layout 和 Set
            for(size_t i = 0; i < (size_t)BindingLayoutSlot::Count; ++i) {
                g_RenderResources.bindingLayouts[i] = device->CreateBindingLayout(g_RenderResources.bindingLayoutDescs[i]);
            }

            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, (uint32_t)viewport.Width(), (uint32_t)viewport.Height());
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(
                CommandListParameters().SetDebugName("Final Pass Command List"));
            
            cmdList->Open();

            cmdList->CopyTexture(renderer.GetCurrentBackBuffer(), {}, renderer.GetColorTexture(), {});

            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);

            renderer.GetDevice()->RunGarbageCollection();
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override 
        {
            g_RenderResources.commonBindingSet = renderer.GetDevice()->CreateBindingSet(
                g_RenderResources.commonBindingSetDesc, g_RenderResources.bindingLayouts[(size_t)BindingLayoutSlot::Common]);
        }

    public:
        inline static TimerQueryHandle sm_TimerQuery{};
    };

} // namespace DSM


#endif