#pragma once
#ifndef __TAA_PASS_H__
#define __TAA_PASS_H__

#include "RenderResource.h"
#include "PostEffect/ToneMappingPass.h"

namespace DSM {
    class TaaPass : public IRenderPass
    {
    public:
        struct TaaSettings
        {
            size_t sampleCount = 8;
        };

        TaaPass(GraphicsRenderer& renderer)
        {

        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            return 0;
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();
            auto colorTexDesc = RenderResource::GetInstance().GetCommonTexture(CommonTextureSlot::Color)->GetDesc();
            m_HistoryColorTex = device->CreateTexture(colorTexDesc
                .SetIsUAV(true)
                .SetKeepInitialState(true)
                .SetInitialState(ResourceStates::UnorderedAccess)
                .SetDebugName("TAA History Color Texture"));
        }

        void SetSettings(const TaaSettings& settings) { m_Settings = settings; }

    private:
        float Halton(uint32_t index, uint32_t base)
        {
            float result = 0.0f;
            float invBase = 1.0f / float(base);
            float fraction = invBase;
            uint32_t i = index + 1; // 通常从1开始，避免第一个点全0

            while (i > 0) {
                result += float(i % base) * fraction;
                i /= base;
                fraction *= invBase;
            }
            return result;
        }

    private:
        static constexpr uint32_t sm_ThreadSize = 8;

        std::unique_ptr<ToneMappingPass> m_ToneMappingPass;
        TaaSettings m_Settings{};

        TextureHandle m_HistoryColorTex{};
    };
}

#endif // !__TAA_PASS_H__
