#pragma once
#ifndef __GEOMETRYPASS_H__
#define __GEOMETRYPASS_H__

#include "RenderResource.h"
#include "Runtime/Render/Model.h"
#include "Shaders/ForwardShader/ResourceData.h"
#include "ShadowPass.h"
#include "SSAOPass.h"
#include "LightingPass.h"
#include "Runtime/Core/InstrumentorTimer.h"

namespace DSM {
    // 绘制所有模型
    class LitPass : public IRenderPass
    {
        enum ShaderSlot
        {
            LitVS,
            LitVSNoTangent,
            LitPS,
            LitPSPCF3,
            LitPSPCF5,
            LitPSPCF7,
            LitPSNoTangent,
            LitPSNoTangentPCF3,
            LitPSNoTangentPCF5,
            LitPSNoTangentPCF7,
            Count
        };
        
    public:
        LitPass(GraphicsRenderer& renderer, bool isTransparentPass = false)
            :m_IsTransparentPass(isTransparentPass)
        {
            auto device = renderer.GetDevice();
            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(ShaderResource::PassConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName(std::string(isTransparentPass ? " Transparent" : " Opaque") + " CB"));

            // 将 ShadowMap 绑定到管线
            auto bindingLayoutDesc = BindingLayoutDesc{}
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(0))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(1))
                .AddItem(BindingLayoutItem::Texture_SRV(2))
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem::PushConstants(1, sizeof(PushConstants)))
                .AddItem(BindingLayoutItem::ConstantBuffer(2))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(3))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(4))
                .AddItem(BindingLayoutItem::ConstantBuffer(3))
                .AddItem(BindingLayoutItem::Texture_SRV(5))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::Shadow)))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::AnisoWrap)));
            m_BindingLayout = device->CreateBindingLayout(bindingLayoutDesc);

            CreateShader(renderer);
        }

        uint64_t Render(DSM::GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();

            auto fb = renderRes.GetFramebuffer();
            assert(fb != nullptr && 
                fb->GetDesc().colorAttachments.size() > 0 && 
                fb->GetDesc().depthAttachment.Valid());
            float width = (float)fb->GetFramebufferInfo().width;
            float height = (float)fb->GetFramebufferInfo().height;

            if(m_CacheMeshBuffer != renderRes.GetMeshBuffer() ||
                m_CacheMaterialBuffer != renderRes.GetMaterialBuffer() ||
                m_CacheShadowMap != renderRes.GetCommonTexture(CommonTextureSlot::ShadowMap)){
                m_CacheMeshBuffer = renderRes.GetMeshBuffer();
                m_CacheMaterialBuffer = renderRes.GetMaterialBuffer();
                m_CacheShadowMap = renderRes.GetCommonTexture(CommonTextureSlot::ShadowMap);
                CreateBindingSet(device);
            }

            auto cmdListName = std::string{m_IsTransparentPass ? "Transparent" : "Opaque"} +"Lit Pass Command List";
            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName(cmdListName));
            cmdList->Open();

            // 使用了 PreZ Pass 无需清除深度
            const auto& rendertarget = fb->GetDesc().colorAttachments[0];
            if(!m_IsTransparentPass){
                cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{0.0f, 0.0f, 0.0f, 1.0f});
            }

            float cameraNear = renderer.GetCamera().GetNearZ();
            float cameraFar = renderer.GetCamera().GetFarZ();
            auto proj = renderer.GetCamera().GetProjMatrix();
            auto offset = TaaPass::GetJitterOffset(renderer.GetFrameIndex()) / Math::Vector2{width, height};
            proj.Set(2, 0, proj.Get(2, 0) + offset.Get(0) * 2.f);
            proj.Set(2, 1, proj.Get(2, 1) + offset.Get(1) * 2.f);
            ShaderResource::PassConstants passCB{};
            passCB.view = Math::Matrix4::Transpose(renderer.GetCamera().GetViewMatrix());
            passCB.viewInv = Math::Matrix4::Inverse(passCB.view);
            passCB.proj = Math::Matrix4::Transpose(proj);
            passCB.projInv = Math::Matrix4::Inverse(passCB.proj);
            passCB.cameraPos = renderer.GetCamera().GetPosition();
            passCB.deltaTime = deltaTime;
            passCB.renderTargetSize = Math::Vector4{ width, height, 1.0f / width, 1.0f / height };
            passCB.nearFarZ = Math::Vector4{ cameraNear, cameraFar, 1.0f / cameraNear, 1.0f / cameraFar };

            cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(ShaderResource::PassConstants));

            auto state = GraphicsState{}
                .SetFramebuffer(fb)
                .AddBindingSet(m_BindingSet, 0)
                .AddBindingSet(renderRes.GetTextureBindlessTable(), 1)
                .SetViewport(ViewportState{}.AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()));
            for(const auto& object : renderRes.GetObjectInFrustum()) {
                PushConstants pushConstants{};
                pushConstants.objectIndex = (int)renderRes.GetObjectIndex().at(object);
                const auto& materialIndex = renderRes.GetObjectMaterialIndex().at(object);
                auto meshRenderer = object->GetComponent<MeshRenderer>();
                if(meshRenderer == nullptr)
                    continue;
                auto mesh = meshRenderer->GetMesh();
                if(mesh == nullptr)
                    continue;

                for(size_t subMeshIndex = 0; subMeshIndex < mesh->GetSubMeshCount(); ++subMeshIndex){
                    auto matIndex = meshRenderer->GetMaterialIndex(subMeshIndex);
                    auto material = meshRenderer->GetMaterial(matIndex);
                    // 透明物体在渲染天空盒后渲染
                    if((material->IsTransparent() && !m_IsTransparentPass) ||
                        (!material->IsTransparent() && m_IsTransparentPass)){
                        continue;
                    }

                    state.vertexBuffers.resize(0);
                    state.SetPipeline(GetPipelineState(renderer, *mesh, *material))
                        .SetIndexBuffer(mesh->GetIndexBufferBinding(subMeshIndex));
                    if(auto slot = Mesh::VertexAttributeSlot::Position; mesh->HasVertexAttribute(slot)) {
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    }
                    if(auto slot = Mesh::VertexAttributeSlot::UV; mesh->HasVertexAttribute(slot)) {
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    }
                    if(auto slot = Mesh::VertexAttributeSlot::Normal; mesh->HasVertexAttribute(slot)) {
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    }
                    if(auto slot = Mesh::VertexAttributeSlot::Tangent; mesh->HasVertexAttribute(slot)) {
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    }
                    
                    cmdList->SetGraphicsState(state);
                    
                    pushConstants.materialIndex = materialIndex[subMeshIndex];
                    cmdList->SetPushConstants(&pushConstants, sizeof(pushConstants));

                    // 绘制
                    cmdList->DrawIndexed(DrawArguments{}
                        .SetStartIndexLocation(mesh->GetIndexOffset(subMeshIndex))
                        .SetStartVertexLocation(mesh->GetVertexOffset(subMeshIndex))
                        .SetVertexCount(mesh->GetIndexCount(subMeshIndex)));
                }
            }

            if (m_IsTransparentPass) {
                cmdList->SetTextureState(renderRes.GetCommonTexture(CommonTextureSlot::Color), AllSubresources, ResourceStates::NoPixelShaderResource);
            }

            cmdList->Close();

            // 等待 ssao 计算完成
            if(!m_IsTransparentPass){
                device->QueueWaitForCommandList(
                    CommandQueueType::Graphics, 
                    CommandQueueType::Compute, 
                    RenderResource::GetInstance().GetRenderPassFinishFence(RenderPass::SSAO));
            }
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            CreateBindingSet(renderer.GetDevice());
        }

    private:
        size_t GetPSOIndex(Mesh& mesh, Material& material, bool reverseZ) const
        {
            size_t index = 0;
            auto filterMode = ShadowPass::sm_Setting.directionalSetting.filter;
            if(!mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Position)) index |= 1 << 0;
            if(!mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Normal)) index |= 1 << 1;
            if(!mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::UV)) index |= 1 << 2;
            if(mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Tangent)) index |= 1 << 3;
            if(!HasFlags(filterMode, ShadowSetting::_PCF3x3)) index |= 1 << 4;
            if(HasFlags(filterMode, ShadowSetting::_PCF5x5)) index |= 1 << 5;
            if(material.IsTransparent()) index |= 1 << 6;
            if(material.IsBothSide() && material.IsTransparent()) index |= 1 << 7;
            if(reverseZ) index |= 1 << 8;
            return index;
        }

        GraphicsPipelineHandle GetPipelineState(GraphicsRenderer& renderer, Mesh& mesh, Material& material)
        {
            auto psoIndex = GetPSOIndex(mesh, material, renderer.GetCamera().IsReversedZ());
            if(psoIndex >= std::size(m_Pipelines) || m_Pipelines[psoIndex] == nullptr){
                m_Pipelines.resize(std::max(m_Pipelines.size() * 2, psoIndex + 1));
                // 创建对应的 PSO
                m_Pipelines[psoIndex] = CreatePipelineState(renderer, mesh, material);
            }
            return m_Pipelines[psoIndex];
        }

        GraphicsPipelineHandle CreatePipelineState(GraphicsRenderer& renderer, Mesh& mesh, Material& material)
        {
            auto device = renderer.GetDevice();

            BlendState hasBlend = BlendState{}.SetRenderTarget(0, 
                BlendState::RenderTarget{}
                    .SetBlendEnable(true)
                    .SetSrcBlend(BlendFactor::SrcAlpha)
                    .SetDestBlend(BlendFactor::InvSrcAlpha));
            BlendState noBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{});

            auto reverseZ = renderer.GetCamera().IsReversedZ();
            DepthStencilState readDepth = DepthStencilState{}
                .SetDepthWriteEnable(false)
                .SetDepthFunc(reverseZ ? ComparisonFunc::GreaterOrEqual : ComparisonFunc::LessOrEqual);

            RasterState defaultRaster = RasterState{};
            RasterState twoSided = RasterState{}.SetCullMode(RasterCullMode::None);

            auto litVS = m_Shaders[ShaderSlot::LitVS];
            auto litVSNoTangent = m_Shaders[ShaderSlot::LitVSNoTangent];

            std::vector<VertexAttributeDesc> attributes{};
            if(auto slot = Mesh::VertexAttributeSlot::Position; mesh.HasVertexAttribute(slot)) {
                attributes.push_back(mesh.GetVertexAttribute(slot));
            }
            if(auto slot = Mesh::VertexAttributeSlot::UV; mesh.HasVertexAttribute(slot)) {
                attributes.push_back(mesh.GetVertexAttribute(slot));
            }
            if(auto slot = Mesh::VertexAttributeSlot::Normal; mesh.HasVertexAttribute(slot)) {
                attributes.push_back(mesh.GetVertexAttribute(slot));
            }
            bool hasTangent = mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Tangent);
            if(hasTangent) {
                attributes.push_back(mesh.GetVertexAttribute(Mesh::VertexAttributeSlot::Tangent));
            }
            InputLayoutHandle layout = device->CreateInputLayout(attributes, hasTangent ? litVS : litVSNoTangent);

            const auto& depthState = readDepth;
            const auto& blendState = material.IsTransparent() ? hasBlend : noBlend;
            const auto& rasterState = (material.IsBothSide() && material.IsTransparent()) ? twoSided : defaultRaster;
            // 创建渲染配置
            auto shadowFilter = ShadowPass::sm_Setting.directionalSetting.filter;
            ShaderHandle ps = hasTangent ? m_Shaders[size_t(ShaderSlot::LitPS) + shadowFilter] : 
                m_Shaders[size_t(ShaderSlot::LitPSNoTangent) + shadowFilter];
            auto pipelineDesc = GraphicsPipelineDesc()
                .SetInputLayout(layout)
                .SetVertexShader(hasTangent ? litVS : litVSNoTangent)
                .SetPixelShader(ps)
                .SetRenderState(RenderState{ blendState, depthState, rasterState })
                .AddBindingLayout(m_BindingLayout, 0)
                .AddBindingLayout(RenderResource::GetInstance().GetTextureBindlessLayout(), 1);

            return device->CreateGraphicsPipeline(pipelineDesc, RenderResource::GetInstance().GetFramebuffer());
        }

        void CreateShader(GraphicsRenderer& renderer)
        {
            // 创建着色器
            ShaderCompileDesc litVSDesc{};
            litVSDesc.SetType(ShaderType::Vertex)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/ForwardShader/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassVS");
            ShaderByteCode litVSNoTangent{litVSDesc};
            ShaderByteCode litVS{litVSDesc.AddDefine("USE_TANGENT", "1")};

            ShaderCompileDesc litPSDesc{};
            litPSDesc.SetType(ShaderType::Pixel)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename("Shaders/ForwardShader/Passes/LitPass.hlsl")
                .SetEnterPoint("LitPassPS");
            ShaderByteCode litPSNoTangent{litPSDesc};
            ShaderByteCode litPSNoTangentPCF3{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF3", "1")};
            ShaderByteCode litPSNoTangentPCF5{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF5", "1")};
            ShaderByteCode litPSNoTangentPCF7{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF7", "1")};
            ShaderByteCode litPS{litPSDesc.AddDefine("USE_TANGENT", "1")};
            ShaderByteCode litPSPCF3{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF3", "1")};
            ShaderByteCode litPSPCF5{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF5", "1")};
            ShaderByteCode litPSPCF7{ShaderCompileDesc{litPSDesc}.AddDefine("DIRECTIONAL_PCF7", "1")};

            auto createShader = [&](const ShaderByteCode& byteCode, const auto& name) {
                return renderer.GetDevice()->CreateShader(ShaderDesc()
                    .SetEntryName(byteCode.GetDesc().enterPoint)
                    .SetShaderType(byteCode.GetDesc().type)
                    .SetDebugName(name), 
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            m_Shaders[ShaderSlot::LitVS] = createShader(litVS, "LitPassVS");
            m_Shaders[ShaderSlot::LitVSNoTangent] = createShader(litVSNoTangent, "LitPassVSNoTangent");
            m_Shaders[ShaderSlot::LitPS] = createShader(litPS, "LitPassPS");
            m_Shaders[ShaderSlot::LitPSPCF3] = createShader(litPSPCF3, "LitPassPSPCF3");
            m_Shaders[ShaderSlot::LitPSPCF5] = createShader(litPSPCF5, "LitPassPSPCF5");
            m_Shaders[ShaderSlot::LitPSPCF7] = createShader(litPSPCF7, "LitPassPSPCF7");
            m_Shaders[ShaderSlot::LitPSNoTangent] = createShader(litPSNoTangent, "LitPassPSNoTangent");
            m_Shaders[ShaderSlot::LitPSNoTangentPCF3] = createShader(litPSNoTangentPCF3, "LitPassPSNoTangentPCF3");
            m_Shaders[ShaderSlot::LitPSNoTangentPCF5] = createShader(litPSNoTangentPCF5, "LitPassPSNoTangentPCF5");
            m_Shaders[ShaderSlot::LitPSNoTangentPCF7] = createShader(litPSNoTangentPCF7, "LitPassPSNoTangentPCF7");
        }

        void CreateBindingSet(IDevice* device)
        {
            auto& renderRes = RenderResource::GetInstance();
            auto bindingSetDesc = BindingSetDesc{}
                .AddItem(BindingSetItem::StructuredBuffer_SRV(0, m_CacheMeshBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(1, m_CacheMaterialBuffer))
                .AddItem(BindingSetItem::Texture_SRV(2, renderRes.GetCommonTexture(CommonTextureSlot::SSAO)))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_PassCB))
                .AddItem(BindingSetItem::PushConstants(1, sizeof(PushConstants)))
                .AddItem(BindingSetItem::ConstantBuffer(2, LightingPass::sm_LightDataBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(3, LightingPass::sm_DirLightDataBuffer))
                .AddItem(BindingSetItem::StructuredBuffer_SRV(4, LightingPass::sm_OtherLightDataBuffer))
                .AddItem(BindingSetItem::ConstantBuffer(3, ShadowPass::sm_ShadowCB))
                .AddItem(BindingSetItem::Texture_SRV(5, m_CacheShadowMap))
                .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::Shadow), renderRes.GetCommonSampler(SamplerSlot::Shadow)))
                .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::AnisoWrap), renderRes.GetCommonSampler(SamplerSlot::AnisoWrap)));
            m_BindingSet = device->CreateBindingSet(bindingSetDesc, m_BindingLayout);
        }

    private:
        struct PushConstants
        {
            int objectIndex;
            int materialIndex;
        };

        BufferHandle m_PassCB{};
        IBuffer* m_CacheMeshBuffer = nullptr;
        IBuffer* m_CacheMaterialBuffer = nullptr;
        ITexture* m_CacheShadowMap = nullptr;
        
        BindingLayoutHandle m_BindingLayout{};
        BindingSetHandle m_BindingSet{};
        
        std::vector<GraphicsPipelineHandle> m_Pipelines{};

        std::array<ShaderHandle, ShaderSlot::Count> m_Shaders{};

        bool m_IsTransparentPass = false;
    };

} // namespace DSM


#endif