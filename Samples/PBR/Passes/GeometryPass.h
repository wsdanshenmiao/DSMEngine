#pragma once
#ifndef __GEOMETRYPASS_H__
#define __GEOMETRYPASS_H__

#include "IRenderPass.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ResourceData.h"

namespace DSM {
    // 绘制所有模型
    class GeometryPass : public IRenderPass
    {
    public:
        GeometryPass(Renderer& renderer, std::span<std::shared_ptr<Model>> models)
        {
            m_Models.assign_range(models);
            m_PassCB = renderer.GetDevice()->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(PassConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("PassConstants"));
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();

            auto& renderConfig = g_RenderResources.renderConfigs;
            auto fb = g_RenderResources.framebuffer;
            assert(fb != nullptr && 
                fb->GetDesc().colorAttachments.size() > 0 && 
                fb->GetDesc().depthAttachment.Valid());

            float width = (float)fb->GetFramebufferInfo().width;
            float height = (float)fb->GetFramebufferInfo().height;

            auto cmdList = device->CreateCommandList(
                CommandListParameters().SetDebugName("GeometryPassCmdList"));
            cmdList->Open();

            const auto& rendertarget = fb->GetDesc().colorAttachments[0];
            cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{1, 0.7f, 0.75f, 1});
            cmdList->ClearDepthStencilTexture(fb->GetDesc().depthAttachment.texture, AllSubresources, true, 1, false, 0);
            
            PassConstants passCB{};
            passCB.view = Math::Matrix4::Transpose(renderer.GetCamera().GetViewMatrix());
            passCB.viewInv = Math::Matrix4::Inverse(passCB.view);
            passCB.proj = Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix());
            passCB.projInv = Math::Matrix4::Inverse(passCB.proj);
            passCB.cameraPos = renderer.GetCamera().GetPosition();
            passCB.deltaTime = deltaTime;
            cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(PassConstants));

            for(const auto& model : m_Models) {
                for(const auto& mesh : model->meshes){
                    for(const auto& [name, submesh] : mesh->subMeshes){
                        MeshConstants meshCB{};
                        meshCB.world = Math::Matrix4::Transpose(model->transform.GetLocalToWorld());
                        meshCB.worldIT = Math::Matrix4::Inverse(meshCB.world);
                        auto& meshBuffer = renderConfig[mesh->psoIndex].meshCB;
                        cmdList->WriteBuffer(meshBuffer, &meshCB, sizeof(MeshConstants));

                        // 绑定资源
                        auto matByteSize = Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment));
                        auto matBufferRange = BufferRange().SetByteSize(sizeof(Material)).SetByteOffset(matByteSize * submesh.materialIndex);
                        auto bindingDesc = g_RenderResources.bindingSetDesc;
                        bindingDesc.AddItem(BindingSetItem().ConstantBuffer(0, meshBuffer))
                            .AddItem(BindingSetItem().ConstantBuffer(1, model->materialData, matBufferRange))
                            .AddItem(BindingSetItem().ConstantBuffer(2, m_PassCB));
                        for(size_t i = 0; i < kNumTextures; ++i){
                            bindingDesc.AddItem(BindingSetItem().Texture_SRV(i, submesh.textures[i]));
                        }
                        auto bindingSet = device->CreateBindingSet(bindingDesc, g_RenderResources.bindingLayout);

                        GraphicsState state{};
                        state.SetFramebuffer(fb)
                            .SetPipeline(g_RenderResources.psoCache[renderConfig[mesh->psoIndex].pipelineDesc])
                            .AddBindingSet(bindingSet)
                            .SetViewport(ViewportState{}.AddViewportAndScissorRect(Viewport{width, height}))
                            .SetIndexBuffer(mesh->indexBufferViews);
                        if(HasFlags(PSOFlags(mesh->psoFlags), kHasPosition)){
                            state.AddVertexBuffer(mesh->positionStream);
                        }
                        if(HasFlags(PSOFlags(mesh->psoFlags), kHasUV)){
                            state.AddVertexBuffer(mesh->uvStream);
                        }
                        if(HasFlags(PSOFlags(mesh->psoFlags), kHasNormal)){
                            state.AddVertexBuffer(mesh->normalStream);
                        }
                        if(HasFlags(PSOFlags(mesh->psoFlags), kHasTangent)){
                            state.AddVertexBuffer(mesh->tangentStream);
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

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override {}
    private:
        std::vector<std::shared_ptr<Model>> m_Models;
        BufferHandle m_PassCB;
    };

} // namespace DSM


#endif