#pragma once
#ifndef __SHADING_PASS_H__
#define __SHADING_PASS_H__

#include "IRenderPass.h"
#include "Runtime/Render/Model.h"

namespace DSM {
    // 在该 Pass 中进行着色
    class GeometryPass : public IRenderPass {
    public:
        GeometryPass(Renderer& renderer)
        {
            IDevice* device = renderer.GetDevice();

            // create constant buffer
            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(Math::Matrix4) * 2)
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("GeometryPassConstants"));

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            // 创建法线法线纹理等资源
            OnResize(renderer, (uint32_t)viewport.Width(), (uint32_t)viewport.Height());

            std::array<VertexAttributeDesc, 2> attributes = {
                VertexAttributeDesc()
                    .SetName("POSITION")
                    .SetFormat(Format::RGBA32_FLOAT)
                    .SetBufferIndex(0)
                    .SetElementStride(sizeof(Math::Vector4)),
                VertexAttributeDesc()
                    .SetName("NORMAL")
                    .SetFormat(Format::RGB32_FLOAT)
                    .SetBufferIndex(1)
                    .SetElementStride(sizeof(Math::Vector3))
            };

            // 编译 Shader
            auto createShader = [device](ShaderType type, const auto& entryPoint) {
                ShaderCompileDesc compileDesc = ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/Passes/GeometryPass.hlsl")
                    .SetEnterPoint(entryPoint);
                ShaderByteCode geometryPass{compileDesc};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(compileDesc.type)
                    .SetEntryName(compileDesc.enterPoint)
                    .SetDebugName(compileDesc.enterPoint),
                    geometryPass.GetByteCode(), geometryPass.GetByteCodeSize());
            };

            auto vertexShader = createShader(ShaderType::Vertex, "GeometryPassVS");
            auto inputLayout = device->CreateInputLayout(attributes, vertexShader);
            
            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().PushConstants(0, sizeof(int))) // obj index
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(1))
                .AddItem(BindingLayoutItem().StructuredBuffer_SRV(0)));
                
            m_Pipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc()
                .SetVertexShader(vertexShader)
                .SetPixelShader(createShader(ShaderType::Pixel, "GeometryPassPS"))
                .SetInputLayout(inputLayout)
                .SetRenderState(RenderState{})
                .AddBindingLayout(bindingLayout, 0),
                m_Framebuffer);

            sm_TimerQuery = device->CreateTimerQuery();
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Geometry Pass Command List"));
            cmdList->Open();

            // 开始计时
            cmdList->BeginTimerQuery(sm_TimerQuery);

            auto& fbDesc = m_Framebuffer->GetDesc();
            float depth = float(!renderer.GetCamera().IsReversedZ());
            cmdList->ClearDepthStencilTexture(fbDesc.depthAttachment.texture, AllSubresources, true, depth, false, 0);
            cmdList->ClearTextureFloat(fbDesc.colorAttachments[0].texture, AllSubresources, {});

            std::array<Math::Matrix4, 2> viewProj = {
                Math::Matrix4::Transpose(renderer.GetCamera().GetViewMatrix()),
                Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix())
            };
            cmdList->WriteBuffer(m_PassCB, viewProj.data(), sizeof(viewProj));

            auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem().ConstantBuffer(1, m_PassCB))
                .AddItem(BindingSetItem().StructuredBuffer_SRV(0, g_RenderResources.meshBuffer)),
                m_Pipeline->GetDesc().bindingLayouts[0]);

            for(const auto& [index, obj] : g_RenderResources.objInFrustum){
                auto mesh = obj->GetComponent<Mesh>();

                GraphicsState state = GraphicsState()
                    .SetFramebuffer(m_Framebuffer)
                    .SetPipeline(m_Pipeline)
                    .SetViewport(ViewportState().
                        AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()))
                    .SetIndexBuffer(mesh->indexBufferViews)
                    .AddBindingSet(bindingSet, 0);
                if(HasFlags(PSOFlags(mesh->psoFlags), kHasPosition)){
                    state.AddVertexBuffer(mesh->positionStream);
                }
                auto vertexBinding = mesh->normalStream;
                vertexBinding.SetSlot(1);
                if(HasFlags(PSOFlags(mesh->psoFlags), kHasNormal)){
                    state.AddVertexBuffer(vertexBinding);
                }

                cmdList->SetGraphicsState(state);

                int objIndex = index;
                cmdList->SetPushConstants(&objIndex, sizeof(int));

                // 绘制
                cmdList->DrawIndexed(DrawArguments{}
                    .SetStartIndexLocation(mesh->indexOffset)
                    .SetStartVertexLocation(mesh->vertexOffset)
                    .SetVertexCount(mesh->indexCount));
            }

            cmdList->SetTextureState(fbDesc.colorAttachments[0].texture, AllSubresources, ResourceStates::ShaderResource);
            cmdList->SetTextureState(fbDesc.depthAttachment.texture, AllSubresources, ResourceStates::ShaderResource);

            // 结束计时
            cmdList->EndTimerQuery(sm_TimerQuery);
            
            cmdList->Close();
            sm_LastFrameTime = device->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            IDevice* device = renderer.GetDevice();
            // Resize normal texture
            // 法线纹理
            auto slot = (size_t)CommonTextureSlot::Normal;
            g_RenderResources.commonTextures[slot] = device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::RG32_FLOAT)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("NormalTexture"));
            m_Framebuffer = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(g_RenderResources.commonTextures[slot])
                .SetDepthAttachment(g_RenderResources.framebuffer->GetDesc().depthAttachment.texture));
        }

    public:
        inline static TimerQueryHandle sm_TimerQuery{};
        inline static uint64_t sm_LastFrameTime = 0;

    private:
        BufferHandle m_PassCB{};
        FramebufferHandle m_Framebuffer{};
        GraphicsPipelineHandle m_Pipeline;
    };
} // namespace DSM


#endif // !__SHADING_PASS_H__