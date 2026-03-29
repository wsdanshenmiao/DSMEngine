#include "PostEffectManager.h"
#include "BloomPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/MipmapPass.h"
#include "Runtime/Render/Renderer/ForwardRenderer/GaussianBlurPass.h"

namespace DSM{
    PostEffectManager::PostEffectManager(Renderer& renderer)
    {
        AddPostEffect(std::make_unique<BloomPass>());

        const auto& viewPort = renderer.GetCamera().GetViewPort();
        OnResize(renderer, viewPort.Width(), viewPort.Height());
    }

    void PostEffectManager::AddPostEffect(std::unique_ptr<IPostEffect> postEffect)
    {
        DSM_ASSERT(postEffect != nullptr, "Post effect cannot be null");
        m_PostEffects.push_back(std::move(postEffect));

        std::sort(m_PostEffects.begin(), m_PostEffects.end(), [](const std::unique_ptr<IPostEffect>& lhs, const std::unique_ptr<IPostEffect>& rhs) {
            return lhs->m_Priority < rhs->m_Priority;
        });
    }

    void PostEffectManager::Render(Renderer &renderer, float deltaTime)
    {
        std::vector<IPostEffect*> enabledEffects{};
        for(const auto& postEffect : m_PostEffects){
            if(postEffect != nullptr && postEffect->m_Enable){
                enabledEffects.push_back(postEffect.get());
            }
        }
        if(enabledEffects.empty()){
            return;
        }

		auto colorTex = RenderResource::GetInstance().GetCommonTexture(CommonTextureSlot::Color);
        auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters().SetDebugName("PostEffectManager Command List"));
        cmdList->Open();
        cmdList->CopyTexture(m_Textures[0], {}, colorTex, {});
        cmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(cmdList);

        for(size_t i = 0; i < std::size(enabledEffects); ++i){
           auto srcTex = m_Textures[i % 2];
           auto dstTex = m_Textures[(i + 1) % 2];
           enabledEffects[i]->Render(renderer, deltaTime, srcTex, dstTex);
        }

        auto& dstTex = m_Textures[enabledEffects.size() % 2];
        cmdList->Open();
        cmdList->CopyTexture(colorTex, {}, dstTex, {});
        cmdList->Close();
        renderer.GetDevice()->ExecuteCommandList(cmdList);
    }
    
    void PostEffectManager::OnResize(Renderer &renderer, uint32_t width, uint32_t height)
    {
        auto device = renderer.GetDevice();
        const auto backBufferFormat = renderer.GetCurrentBackBuffer()->GetDesc().format;
        for(size_t i = 0; i < std::size(m_Textures); ++i) {
            m_Textures[i] = device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::RGBA8_UNORM)
                .SetIsUAV(true)
                .SetMipLevels(std::log2(std::max(width, height)) + 1)
                .SetInitialState(ResourceStates::UnorderedAccess)
                .SetKeepInitialState(true)
                .SetDebugName("PostEffectManager Temp Texture" + std::to_string(i)));          
        }
    }
}