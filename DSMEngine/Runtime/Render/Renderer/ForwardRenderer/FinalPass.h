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
            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();
        }

        void Render(Renderer& renderer, float deltaTime) override
        {
            renderer.GetDevice()->RunGarbageCollection();
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override { }

    public:
        inline static TimerQueryHandle sm_TimerQuery{};
    };

} // namespace DSM


#endif