#pragma once
#ifndef __SKYBOXPASS_H__
#define __SKYBOXPASS_H__

#include "Runtime/Render/TextureManager.h"
#include "SetupPass.h"

namespace DSM {
    class SkyboxPass : public IRenderPass {
    public:
        SkyboxPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();

            m_SkyboxCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(SkyboxConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("SkyboxCB"));

            // 加载环境贴图
            std::array<TextureHandle, 6> skyboxTextures{};
            for(int i = 0 ; i < 6; ++i){
                skyboxTextures[i] = TextureManager::LoadTextureFromFile(
                    "Textures/daylight" + std::to_string(i) + ".png");
            }
            
            TextureDesc skyboxTexDesc = skyboxTextures.front()->GetDesc();
            skyboxTexDesc.SetArraySize(6)
                .SetDimension(TextureDimension::TextureCube);
            m_SkyboxTexture = device->CreateTexture(skyboxTexDesc);

            // 拷贝到 CubeMap
            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Init SkyBox"));
            cmdList->Open();
            for(int i = 0; i < 6; ++i){
                cmdList->CopyTexture(m_SkyboxTexture, TextureSlice().SetArraySlice(i), skyboxTextures[i], {});
            }
            cmdList->Close();
            device->ExecuteCommandList(cmdList);

            // 创建绑定集
            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem().Texture_SRV(0))
                .AddItem(BindingLayoutItem().Sampler(uint32_t(SamplerSlot::AnisoWrap))));
            m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem().ConstantBuffer(0, m_SkyboxCB))
                .AddItem(BindingSetItem().Texture_SRV(0, m_SkyboxTexture))
                .AddItem(BindingSetItem().Sampler(uint32_t(SamplerSlot::AnisoWrap), GetCommonSampler(SamplerSlot::AnisoWrap))), 
                bindingLayout);

            // 编译 Shader
            ShaderCompileDesc compileDesc = ShaderCompileDesc()
                .SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetEnterPoint("SkyboxVS")
                .SetFilename("Shaders/Passes/SkyboxPass.hlsl");
            ShaderByteCode vsByteCode{compileDesc};
            compileDesc.SetType(ShaderType::Pixel)
                .SetEnterPoint("SkyboxPS");
            ShaderByteCode psByteCode{compileDesc};

            auto vs = device->CreateShader(ShaderDesc()
                .SetShaderType(ShaderType::Vertex)
                .SetDebugName("SkyboxVS")
                .SetEntryName("SkyboxVS"),
                vsByteCode.GetByteCode(), vsByteCode.GetByteCodeSize());
            auto ps = device->CreateShader(ShaderDesc()
                .SetShaderType(ShaderType::Pixel)
                .SetDebugName("SkyboxPS")
                .SetEntryName("SkyboxPS"),
                psByteCode.GetByteCode(), psByteCode.GetByteCodeSize());

            auto cmpFunc = renderer.GetCamera().IsReversedZ() ? 
                ComparisonFunc::GreaterOrEqual : ComparisonFunc::LessOrEqual;
            m_PipelineDesc.AddBindingLayout(bindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthFunc(cmpFunc)))
                .SetVertexShader(vs)
                .SetPixelShader(ps);
            if(!g_RenderResources.psoCache.contains(m_PipelineDesc)){
                auto pso = device->CreateGraphicsPipeline(m_PipelineDesc, g_RenderResources.framebuffer);
                g_RenderResources.psoCache.insert(std::make_pair(m_PipelineDesc, pso));
            }

            sm_TimerQuery = device->CreateTimerQuery();
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override
        {
            auto cmdList = renderer.GetDevice()->CreateCommandList(CommandListParameters().SetDebugName("Skybox Pass Command List"));
            cmdList->Open();

            // 开始计时
            cmdList->BeginTimerQuery(sm_TimerQuery);

            auto& fb = g_RenderResources.framebuffer;

            SkyboxConstants skyboxConstants{};
            skyboxConstants.isReversedZ = renderer.GetCamera().IsReversedZ();
            skyboxConstants.cameraPos = float4(renderer.GetCamera().GetPosition(), 1.0f);
            skyboxConstants.invViewProj = Math::Matrix4::InverseTranspose(renderer.GetCamera().GetViewProjMatrix());
            cmdList->WriteBuffer(m_SkyboxCB, &skyboxConstants, sizeof(SkyboxConstants));

            GraphicsState graphicsState = GraphicsState()
                .SetPipeline(g_RenderResources.psoCache.at(m_PipelineDesc))
                .SetFramebuffer(fb)
                .SetViewport(ViewportState().AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()))
                .AddBindingSet(m_BindingSet, 0);
            cmdList->SetGraphicsState(graphicsState);

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            // 结束计时
            cmdList->EndTimerQuery(sm_TimerQuery);

            cmdList->Close();
            renderer.GetDevice()->ExecuteCommandList(cmdList);
        }
        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override {}
    
    public:
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        TextureHandle m_SkyboxTexture;
        BufferHandle m_SkyboxCB;
        GraphicsPipelineDesc m_PipelineDesc;

        BindingSetHandle m_BindingSet;
    };
} // namespace DSM


#endif // !__SKYBOXPASS_H__