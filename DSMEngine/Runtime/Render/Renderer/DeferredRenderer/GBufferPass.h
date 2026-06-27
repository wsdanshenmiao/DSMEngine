#pragma once
#ifndef __GBUFFER_PASS_H__
#define __GBUFFER_PASS_H__

#include "Runtime/Render/Renderer/CommonPass/RenderResource.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Render/Renderer/CommonPass/TaaPass.h"

namespace DSM {
    class GBufferPass : public IRenderPass {
    public:
        GBufferPass(GraphicsRenderer& renderer)
        {
            IDevice* device = renderer.GetDevice();

            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(ShaderResource::PassConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("GBufferPassConstants"));

            auto createShader = [device](ShaderType type, const auto& entryPoint, bool useTangent) {
                ShaderCompileDesc compileDesc = ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/DeferredShader/Passes/GBufferPass.hlsl")
                    .SetEnterPoint(entryPoint);
                if (useTangent) {
                    compileDesc.AddDefine("USE_TANGENT", "1");
                }
                ShaderByteCode byteCode{compileDesc};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(compileDesc.type)
                    .SetEntryName(compileDesc.enterPoint)
                    .SetDebugName(compileDesc.enterPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };

            m_VertexShaders[0] = createShader(ShaderType::Vertex, "GBufferPassVS", false);
            m_VertexShaders[1] = createShader(ShaderType::Vertex, "GBufferPassVS", true);
            m_PixelShaders[0] = createShader(ShaderType::Pixel, "GBufferPassPS", false);
            m_PixelShaders[1] = createShader(ShaderType::Pixel, "GBufferPassPS", true);

            // Binding layout must declare all resources used by the shader,
            // including the AnisoWrap sampler used for texture sampling
            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(0))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(1))
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem::PushConstants(1, sizeof(PushConstants)))
                .AddItem(BindingLayoutItem::Sampler(uint32_t(SamplerSlot::AnisoWrap))));

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, viewport.Width(), viewport.Height());
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();

            auto fb = m_Framebuffer;
            if (fb == nullptr) return 0;
            const auto& fbDesc = fb->GetDesc();
            float width = renderer.GetCamera().GetViewPort().Width();
            float height = renderer.GetCamera().GetViewPort().Height();

            if (auto meshBuffer = renderRes.GetMeshBuffer();
                m_BindingSet == nullptr || m_CacheMeshBuffer != meshBuffer)
            {
                m_CacheMeshBuffer = meshBuffer;
                m_BindingSet = device->CreateBindingSet(BindingSetDesc()
                    .AddItem(BindingSetItem::StructuredBuffer_SRV(0, meshBuffer))
                    .AddItem(BindingSetItem::StructuredBuffer_SRV(1, renderRes.GetMaterialBuffer()))
                    .AddItem(BindingSetItem::ConstantBuffer(0, m_PassCB))
                    .AddItem(BindingSetItem::PushConstants(1, sizeof(PushConstants)))
                    .AddItem(BindingSetItem::Sampler(uint32_t(SamplerSlot::AnisoWrap), renderRes.GetCommonSampler(SamplerSlot::AnisoWrap))),
                    m_BindingLayout);
            }

            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("GBuffer Pass Command List"));
            cmdList->Open();

            float depthClear = float(!renderer.GetCamera().IsReversedZ());
            cmdList->ClearDepthStencilTexture(fbDesc.depthAttachment.texture, AllSubresources, true, depthClear, false, 0);
            for (size_t i = 0; i < fbDesc.colorAttachments.size(); ++i) {
                cmdList->ClearTextureFloat(fbDesc.colorAttachments[i].texture, AllSubresources, {});
            }

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
            passCB.renderTargetSize = Math::Vector4{width, height, 1.0f / width, 1.0f / height};
            passCB.nearFarZ = Math::Vector4{
                renderer.GetCamera().GetNearZ(), renderer.GetCamera().GetFarZ(),
                1.0f / renderer.GetCamera().GetNearZ(), 1.0f / renderer.GetCamera().GetFarZ()};
            cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(passCB));

            for (const auto& object : renderRes.GetObjectInFrustum()) {
                auto meshRenderer = object->GetComponent<MeshRenderer>();
                if (meshRenderer == nullptr) continue;
                auto mesh = meshRenderer->GetMesh();
                if (mesh == nullptr) continue;

                PushConstants pushConstants{};
                pushConstants.objectIndex = (int)renderRes.GetObjectIndex().at(object);
                const auto& materialIndex = renderRes.GetObjectMaterialIndex().at(object);

                for (size_t subMeshIndex = 0; subMeshIndex < mesh->GetSubMeshCount(); ++subMeshIndex) {
                    auto matIndex = meshRenderer->GetMaterialIndex(subMeshIndex);
                    auto material = meshRenderer->GetMaterial(matIndex);
                    if (material->IsTransparent()) continue;

                    auto pipeline = GetPipelineState(renderer, *mesh, *material);
                    if (pipeline == nullptr) continue;

                    GraphicsState state = GraphicsState{}
                        .SetPipeline(pipeline)
                        .SetFramebuffer(fb)
                        .AddBindingSet(m_BindingSet, 0)
                        .AddBindingSet(renderRes.GetTextureBindlessTable(), 1)
                        .SetViewport(ViewportState{}.AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()));

                    state.vertexBuffers.resize(0);
                    state.SetIndexBuffer(mesh->GetIndexBufferBinding(subMeshIndex));
                    if (auto slot = Mesh::VertexAttributeSlot::Position; mesh->HasVertexAttribute(slot))
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    if (auto slot = Mesh::VertexAttributeSlot::UV; mesh->HasVertexAttribute(slot))
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    if (auto slot = Mesh::VertexAttributeSlot::Normal; mesh->HasVertexAttribute(slot))
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));
                    if (auto slot = Mesh::VertexAttributeSlot::Tangent; mesh->HasVertexAttribute(slot))
                        state.AddVertexBuffer(mesh->GetVertexBufferBinding(slot));

                    cmdList->SetGraphicsState(state);
                    pushConstants.materialIndex = materialIndex[subMeshIndex];
                    cmdList->SetPushConstants(&pushConstants, sizeof(pushConstants));

                    cmdList->DrawIndexed(DrawArguments{}
                        .SetStartIndexLocation(mesh->GetIndexOffset(subMeshIndex))
                        .SetStartVertexLocation(mesh->GetVertexOffset(subMeshIndex))
                        .SetVertexCount(mesh->GetIndexCount(subMeshIndex)));
                }
            }

            for (size_t i = 0; i < fbDesc.colorAttachments.size(); ++i) {
                cmdList->SetTextureState(fbDesc.colorAttachments[i].texture, AllSubresources, ResourceStates::ShaderResource);
            }

            cmdList->Close();
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            IDevice* device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();
            auto& depthTex = renderRes.GetCommonTexture(CommonTextureSlot::Depth);

            auto albedoMetallicTex = device->CreateTexture(TextureDesc()
                .SetWidth(width).SetHeight(height)
                .SetFormat(Format::RGBA8_UNORM)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("GBuffer_AlbedoMetallic"));
            renderRes.SetCommonTexture(CommonTextureSlot::AlbedoMetallic, albedoMetallicTex);

            auto normalTex = device->CreateTexture(TextureDesc()
                .SetWidth(width).SetHeight(height)
                .SetFormat(Format::RG32_FLOAT)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("GBuffer_Normal"));
            renderRes.SetCommonTexture(CommonTextureSlot::Normal, normalTex);

            auto materialAttribTex = device->CreateTexture(TextureDesc()
                .SetWidth(width).SetHeight(height)
                .SetFormat(Format::RGBA8_UNORM)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("GBuffer_MaterialAttributes"));
            renderRes.SetCommonTexture(CommonTextureSlot::MaterialAttributes, materialAttribTex);

            m_Framebuffer = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(albedoMetallicTex)
                .AddColorAttachment(normalTex)
                .AddColorAttachment(materialAttribTex)
                .SetDepthAttachment(depthTex));
            renderRes.GetGBufferFramebuffer() = m_Framebuffer;

            m_Pipelines.clear();
        }

    private:
        struct PushConstants
        {
            int objectIndex;
            int materialIndex;
        };

        size_t GetPSOIndex(Mesh& mesh, bool reverseZ) const
        {
            size_t index = 0;
            if (mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Tangent)) index |= 1 << 0;
            if (reverseZ) index |= 1 << 1;
            return index;
        }

        GraphicsPipelineHandle GetPipelineState(GraphicsRenderer& renderer, Mesh& mesh, Material& material)
        {
            auto psoIndex = GetPSOIndex(mesh, renderer.GetCamera().IsReversedZ());
            if (psoIndex >= m_Pipelines.size() || m_Pipelines[psoIndex] == nullptr) {
                m_Pipelines.resize(std::max(m_Pipelines.size() * 2, psoIndex + 1));
                m_Pipelines[psoIndex] = CreatePipelineState(renderer, mesh, material);
            }
            return m_Pipelines[psoIndex];
        }

        GraphicsPipelineHandle CreatePipelineState(GraphicsRenderer& renderer, Mesh& mesh, Material& material)
        {
            auto device = renderer.GetDevice();
            bool hasTangent = mesh.HasVertexAttribute(Mesh::VertexAttributeSlot::Tangent);
            auto reverseZ = renderer.GetCamera().IsReversedZ();

            std::vector<VertexAttributeDesc> attributes{};
            if (auto slot = Mesh::VertexAttributeSlot::Position; mesh.HasVertexAttribute(slot))
                attributes.push_back(mesh.GetVertexAttribute(slot));
            if (auto slot = Mesh::VertexAttributeSlot::UV; mesh.HasVertexAttribute(slot))
                attributes.push_back(mesh.GetVertexAttribute(slot));
            if (auto slot = Mesh::VertexAttributeSlot::Normal; mesh.HasVertexAttribute(slot))
                attributes.push_back(mesh.GetVertexAttribute(slot));
            if (hasTangent)
                attributes.push_back(mesh.GetVertexAttribute(Mesh::VertexAttributeSlot::Tangent));

            auto vs = m_VertexShaders[hasTangent ? 1 : 0];
            auto ps = m_PixelShaders[hasTangent ? 1 : 0];
            auto inputLayout = device->CreateInputLayout(attributes, vs);

            RasterState rasterState{};
            if (material.IsBothSide()) {
                rasterState.SetCullMode(RasterCullMode::None);
            }

            auto pipelineDesc = GraphicsPipelineDesc()
                .SetInputLayout(inputLayout)
                .SetVertexShader(vs)
                .SetPixelShader(ps)
                .SetRenderState(RenderState{}
                    .SetDepthStencilState(DepthStencilState{}
                        .SetDepthWriteEnable(true)
                        .SetDepthTestEnable(true)
                        .SetDepthFunc(reverseZ ? ComparisonFunc::GreaterOrEqual : ComparisonFunc::LessOrEqual))
                    .SetRasterState(rasterState))
                .AddBindingLayout(m_BindingLayout, 0)
                .AddBindingLayout(RenderResource::GetInstance().GetTextureBindlessLayout(), 1);

            return device->CreateGraphicsPipeline(pipelineDesc, m_Framebuffer);
        }

    private:
        BufferHandle m_PassCB{};
        IBuffer* m_CacheMeshBuffer = nullptr;

        FramebufferHandle m_Framebuffer{};
        BindingLayoutHandle m_BindingLayout{};
        BindingSetHandle m_BindingSet{};
        std::vector<GraphicsPipelineHandle> m_Pipelines{};

        std::array<ShaderHandle, 2> m_VertexShaders{};
        std::array<ShaderHandle, 2> m_PixelShaders{};
    };
}

#endif // __GBUFFER_PASS_H__
