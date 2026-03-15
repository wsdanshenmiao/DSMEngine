#pragma once
#ifndef __GEOMETRYPASS_H__
#define __GEOMETRYPASS_H__

#include "IRenderPass.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ResourceData.h"
#include "ShadowPass.h"
#include "SSAOPass.h"

namespace DSM {
    // 绘制所有模型
    class LitPass : public IRenderPass
    {
    public:
        LitPass(Renderer& renderer)
        {
            m_PassCB = renderer.GetDevice()->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(PassConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("PassConstants"));

            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Local]
                .AddItem(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0).SetSize(kNumTextures))   // 10 个用于 PBR 的纹理
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(LitPassBindingLayout::Constants::MeshConstants))
                .AddItem(BindingLayoutItem().ConstantBuffer(LitPassBindingLayout::Constants::MaterialConstants));
            g_RenderResources.bindingLayoutDescs[(size_t)BindingLayoutSlot::Common]
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(LitPassBindingLayout::Constants::PassConstants));

            g_RenderResources.commonBindingSetDesc.AddItem(BindingSetItem().ConstantBuffer(LitPassBindingLayout::Constants::PassConstants, m_PassCB));

            sm_TimerQuery = renderer.GetDevice()->CreateTimerQuery();
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

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("Lit Pass Command List"));
            cmdList->Open();

            cmdList->BeginTimerQuery(sm_TimerQuery);

            // 使用了 PreZ Pass 无需清除深度
            const auto& rendertarget = fb->GetDesc().colorAttachments[0];
            cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{0.0f, 0.0f, 0.0f, 1.0f});

            PassConstants passCB{};
            passCB.view = Math::Matrix4::Transpose(renderer.GetCamera().GetViewMatrix());
            passCB.viewInv = Math::Matrix4::Inverse(passCB.view);
            passCB.proj = Math::Matrix4::Transpose(renderer.GetCamera().GetProjMatrix());
            passCB.projInv = Math::Matrix4::Inverse(passCB.proj);
            passCB.cameraPos = renderer.GetCamera().GetPosition();
            passCB.deltaTime = deltaTime;
            passCB.renderTargetSize = Math::Vector2{ width, height };
            passCB.nearFarZ = Math::Vector2{ renderer.GetCamera().GetNearZ(), renderer.GetCamera().GetFarZ() };

            cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(PassConstants));

            renderer.GetDevice()->QueueWaitForCommandList(cmdList->GetDesc().queueType, CommandQueueType::Compute, SSAOPass::sm_LastFrameTime);
            auto view = DSMEngine::sm_GlobalContext.scene->GetAllObjectsWithComponents<Model, Math::Transform>();
            for(const auto& [entity, model, transform] : view.each()) {
                for(const auto& mesh : model.meshes){
                    MeshConstants meshCB{};
                    meshCB.world = Math::Matrix4::Transpose(transform.GetLocalToWorld());
                    meshCB.worldIT = Math::Matrix4::InverseTranspose(meshCB.world);
                    auto& meshBuffer = renderConfig[mesh->psoIndex].meshCB;
                    cmdList->WriteBuffer(meshBuffer, &meshCB, sizeof(MeshConstants));

                    for(const auto& submesh : mesh->subMeshes){
                        // 绑定资源
                        auto matByteSize = Math::Align(sizeof(Material), size_t(c_ConstantBufferOffsetSizeAlignment));
                        auto matBufferRange = BufferRange().SetByteSize(sizeof(Material)).SetByteOffset(matByteSize * submesh.materialIndex);
                        BindingSetDesc bindingDesc{};
                        bindingDesc.AddItem(BindingSetItem().ConstantBuffer(0, meshBuffer))
                            .AddItem(BindingSetItem().ConstantBuffer(1, model.materialData, matBufferRange));
                        for(size_t i = 0; i < kNumTextures; ++i){
                            bindingDesc.AddItem(BindingSetItem().Texture_SRV(i, submesh.textures[i]));
                        }
                        auto localBindingSet = device->CreateBindingSet(bindingDesc, g_RenderResources.bindingLayouts[(size_t)BindingLayoutSlot::Local]);

                        GraphicsState state{};
                        state.SetFramebuffer(fb)
                            .SetPipeline(g_RenderResources.psoCache[renderConfig[mesh->psoIndex].pipelineDesc])
                            .SetViewport(ViewportState{}.AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()))
                            .SetIndexBuffer(mesh->indexBufferViews)
                            .AddBindingSet(localBindingSet, (uint32_t)BindingLayoutSlot::Local)
                            .AddBindingSet(g_RenderResources.commonBindingSet, (uint32_t)BindingLayoutSlot::Common);
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
            
            cmdList->EndTimerQuery(sm_TimerQuery);

            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override {}

    public:
        inline static TimerQueryHandle sm_TimerQuery{};
        

    private:
        BufferHandle m_PassCB{};
    };

} // namespace DSM


#endif