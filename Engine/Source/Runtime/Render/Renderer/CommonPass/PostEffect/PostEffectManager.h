#pragma once
#ifndef __POSTEFFECTMANAGER_H__
#define __POSTEFFECTMANAGER_H__

#include "Runtime/Render/Renderer/CommonPass/RenderResource.h"

namespace DSM {
    struct IPostEffect
    {
        virtual ~IPostEffect() = default;
        virtual void Render(
            GraphicsRenderer& renderer, 
            ICommandList* cmdList, 
            float deltaTime, 
            ITexture* srcTex, 
            ITexture* dstTex) = 0;

        bool m_Enable = true;
        size_t m_Priority = 0; // 后处理执行顺序，数值越小优先执行
    };

    class PostEffectManager final : public IRenderPass
    {
    public:
        PostEffectManager(GraphicsRenderer& renderer);

        void AddPostEffect(std::unique_ptr<IPostEffect> postEffect);
        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override;
        
    private:
        std::vector<std::unique_ptr<IPostEffect>> m_PostEffects;
        std::array<TextureHandle, 2> m_Textures{};
    };
}


#endif // __POSTEFFECTMANAGER_H__
