#pragma once
#ifndef __SHADING_PASS_H__
#define __SHADING_PASS_H__

#include "IRenderPass.h"
#include "Runtime/Render/Model.h"

namespace DSM {
    // 在该 Pass 中进行着色
    class GeometryPass : public IRenderPass {
    public:
        GeometryPass(Renderer& renderer, std::span<std::shared_ptr<Model>> models)
        {
            m_Models.assign_range(models);
            IDevice* device = renderer.GetDevice();

            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(Math::Matrix4))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("GeometryPassConstants"));

            auto fb = g_RenderResources.framebuffer;
            // 创建法线法线纹理等资源
            OnResize(renderer, fb->GetFramebufferInfo().width, fb->GetFramebufferInfo().height);

            // 编译 Shader
            ShaderCompileDesc compileDesc = ShaderCompileDesc()
                .SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/Passes/GeometryPass.hlsl")
                .SetEnterPoint("GeometryPassVS");
            ShaderByteCode geometryPassVS{compileDesc};
            ShaderHandle vs = device->CreateShader(ShaderDesc()
                .SetShaderType(compileDesc.type)
                .SetEntryName(compileDesc.enterPoint)
                .SetDebugName(compileDesc.enterPoint),
                geometryPassVS.GetByteCode(), geometryPassVS.GetByteCodeSize());
            compileDesc.SetType(ShaderType::Pixel)
                .SetEnterPoint("GeometryPassPS");
            ShaderByteCode geometryPassPS{compileDesc};
            ShaderHandle ps = device->CreateShader(ShaderDesc()
                .SetShaderType(compileDesc.type)
                .SetEntryName(compileDesc.enterPoint)
                .SetDebugName(compileDesc.enterPoint),
                geometryPassPS.GetByteCode(), geometryPassPS.GetByteCodeSize());

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

            auto inputLayout = device->CreateInputLayout(attributes, vs);
            
            auto bindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(1)));
            m_Pipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc()
                .SetVertexShader(vs)
                .SetPixelShader(ps)
                .SetInputLayout(inputLayout)
                .SetRenderState(RenderState{})
                .AddBindingLayout(bindingLayout, 0),
                m_Framebuffer);
            g_RenderResources.psoCache[m_Pipeline->GetDesc()] = m_Pipeline;
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();

            uint32_t width = m_Framebuffer->GetFramebufferInfo().width;
            uint32_t height = m_Framebuffer->GetFramebufferInfo().height;

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("GeometryPassCmdList"));
            cmdList->Open();

            Math::Matrix4 viewProj = Math::Matrix4::Transpose(renderer.GetCamera().GetViewProjMatrix());
            cmdList->WriteBuffer(m_PassCB, &viewProj, sizeof(viewProj));
            for(const auto& model : m_Models){
                for(const auto& mesh : model->meshes){
                    MeshConstants meshCB{};
                    meshCB.world = Math::Matrix4::Transpose(model->transform.GetLocalToWorld());
                    meshCB.worldIT = Math::Matrix4::InverseTranspose(meshCB.world);
                    auto& meshBuffer = g_RenderResources.renderConfigs[mesh->psoIndex].meshCB;
                    cmdList->WriteBuffer(meshBuffer, &meshCB, sizeof(MeshConstants));
                    for(const auto& [name, submesh] : mesh->subMeshes){
                        auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                            .AddItem(BindingSetItem().ConstantBuffer(0, meshBuffer))
                            .AddItem(BindingSetItem().ConstantBuffer(1, m_PassCB)), 
                        m_Pipeline->GetDesc().bindingLayouts[0]);

                        GraphicsState state = GraphicsState()
                            .SetFramebuffer(m_Framebuffer)
                            .SetPipeline(m_Pipeline)
                            .SetViewport(ViewportState().
                                AddViewportAndScissorRect({float(width), float(height)}))
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
                        // 绘制
                        cmdList->DrawIndexed(DrawArguments{}
                            .SetStartIndexLocation(submesh.indexOffset)
                            .SetStartVertexLocation(submesh.vertexOffset)
                            .SetVertexCount(submesh.indexCount));
                    }
                }
            }

            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            IDevice* device = renderer.GetDevice();
            // Resize normal texture
            // 法线纹理
            g_RenderResources.normalTex = device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::RG32_FLOAT)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("NormalTexture"));
            m_Framebuffer = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(g_RenderResources.normalTex)
                .SetDepthAttachment(g_RenderResources.framebuffer->GetDesc().depthAttachment.texture));
        }

    private:
        BufferHandle m_PassCB{};
        FramebufferHandle m_Framebuffer{};
        GraphicsPipelineHandle m_Pipeline;

        std::vector<std::shared_ptr<Model>> m_Models;
    };
} // namespace DSM


#endif // !__SHADING_PASS_H__