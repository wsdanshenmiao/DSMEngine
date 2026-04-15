#ifndef __MOTION_VECTOR_PASS_H__
#define __MOTION_VECTOR_PASS_H__

#include "RenderResource.h"

namespace DSM {
    class MotionVectorPass : public IRenderPass
    {
    public:
        MotionVectorPass(GraphicsRenderer& renderer)
        {
            auto device = renderer.GetDevice();

            m_Constants = device->CreateBuffer(BufferDesc{}
                .SetIsVolatile(true)
                .SetIsConstantBuffer(true)
                .SetByteSize(sizeof(MotionVecFullScreenConstants))
                .SetDebugName("Motion Vector Constants"));

            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc{}
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
                .AddItem(BindingLayoutItem::PushConstants(1, sizeof(MotionVecObjConstants)))
                .AddItem(BindingLayoutItem::Texture_SRV(0))
                .AddItem(BindingLayoutItem::StructuredBuffer_SRV(1))
                .AddItem(BindingLayoutItem::Sampler((size_t)SamplerSlot::PointClamp)));

            auto createShader = [device](ShaderType type, const std::string& entryPoint) {
                auto byteCode = ShaderByteCode{ShaderCompileDesc()
                    .SetType(type)
                    .SetMode(ShaderMode::SM_6_6)
                    .SetFilename("Shaders/ForwardShader/Passes/MotionVectorPass.hlsl")
                    .SetEnterPoint(entryPoint)};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(type)
                    .SetDebugName("TAA " + entryPoint)
                    .SetEntryName(entryPoint),
                    byteCode.GetByteCode(), byteCode.GetByteCodeSize());
            };
            m_MotionVecVS = createShader(ShaderType::Vertex, "MotionVectorFullScreenVS");
            m_MotionVecPS = createShader(ShaderType::Pixel, "MotionVectorFullScreenPS");
            m_MotionVecObjectVS = createShader(ShaderType::Vertex, "MotionVectorVS");
            m_MotionVecObjectPS = createShader(ShaderType::Pixel, "MotionVectorPS");

            auto& vertexAttr = Mesh::GetVertexAttribute(Mesh::VertexAttributeSlot::Position);
            m_InputLayout = device->CreateInputLayout({&vertexAttr, 1}, m_MotionVecObjectVS);

            const auto& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, viewport.Width(), viewport.Height());
        }

        uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();
            auto colorTex = renderRes.GetCommonTexture(CommonTextureSlot::Color);
            if (colorTex == nullptr || m_MotionVecTex == nullptr) {
                return 0;
            }

            if(m_CachedMeshBuffer != renderRes.GetMeshBuffer() ||
                m_CachedLastFrameMeshBuffer != renderRes.GetLastFrameMeshBuffer())
            {
                m_MotionVecObjectBindingSet = device->CreateBindingSet(BindingSetDesc()
                    .AddItem(BindingSetItem::ConstantBuffer(0, m_Constants))
                    .AddItem(BindingSetItem::PushConstants(1, sizeof(MotionVecObjConstants)))
                    .AddItem(BindingSetItem::StructuredBuffer_SRV(0, renderRes.GetMeshBuffer()))
                    .AddItem(BindingSetItem::StructuredBuffer_SRV(1, renderRes.GetLastFrameMeshBuffer()))
                    , m_BindingLayout);
                m_CachedMeshBuffer = renderRes.GetMeshBuffer();
                m_CachedLastFrameMeshBuffer = renderRes.GetLastFrameMeshBuffer();
            }


            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("MotionVec Command List"));
            cmdList->Open();

            auto viewport = renderer.GetCamera().GetViewPort();
            auto view = renderer.GetCamera().GetViewMatrix();
            auto proj = renderer.GetCamera().GetProjMatrix();
            auto offset = TaaPass::GetJitterOffset(renderer.GetFrameIndex()) / Math::Vector2{viewport.Width(), viewport.Height()};
            proj.Set(2, 0, proj.Get(2, 0) + offset.Get(0) * 2.f);
            proj.Set(2, 1, proj.Get(2, 1) + offset.Get(1) * 2.f);
            MotionVecFullScreenConstants constants{};
            constants.prevMatrix = m_PreViewProjMatrix;
            m_PreViewProjMatrix = Math::Matrix4::Transpose(view * proj);
            constants.currMatrix = Math::Matrix4::Inverse(m_PreViewProjMatrix);
            cmdList->WriteBuffer(m_Constants, &constants, sizeof(constants));

            cmdList->SetGraphicsState(GraphicsState()
                .SetPipeline(m_MotionVecPipeline)
                .AddBindingSet(m_MotionVecBindingSet, 0)
                .SetFramebuffer(m_MotionVecFB)
                .SetViewport(ViewportState().AddViewportAndScissorRect(viewport)));

            cmdList->Draw(DrawArguments().SetVertexCount(3));

            constants.currMatrix = m_PreViewProjMatrix;
            cmdList->WriteBuffer(m_Constants, &constants, sizeof(constants));
            for(const auto& obj : renderRes.GetObjectInFrustum()){
                auto objIdx = renderRes.GetObjectIndex().at(obj);
                auto meshRenderer = obj->GetComponent<MeshRenderer>();
                if(!renderRes.GetLastFrameObjectIndex().contains(obj) || meshRenderer == nullptr){
                    continue;
                }

                auto mesh = meshRenderer->GetMesh();
                if(mesh == nullptr){
                    continue;
                }

                auto lastFrameObjIdx = renderRes.GetLastFrameObjectIndex().at(obj);
                MotionVecObjConstants objConstants{};
                objConstants.objectIndex = objIdx;
                objConstants.lastFrameObjectIndex = lastFrameObjIdx;

                for(size_t subMeshIndex = 0; subMeshIndex < mesh->GetSubMeshCount(); ++subMeshIndex){
                    cmdList->SetGraphicsState(GraphicsState()
                        .SetPipeline(m_MotionVecObjectPipeline)
                        .SetIndexBuffer(mesh->GetIndexBufferBinding(subMeshIndex))
                        .AddVertexBuffer(mesh->GetVertexBufferBinding(Mesh::VertexAttributeSlot::Position))
                        .AddBindingSet(m_MotionVecObjectBindingSet, 0)
                        .SetFramebuffer(m_MotionVecFBWithDepth)
                        .SetViewport(ViewportState().AddViewportAndScissorRect(renderer.GetCamera().GetViewPort())));

                    cmdList->SetPushConstants(&objConstants, sizeof(objConstants));

                    cmdList->DrawIndexed(DrawArguments{}
                        .SetStartIndexLocation(mesh->GetIndexOffset(subMeshIndex))
                        .SetStartVertexLocation(mesh->GetVertexOffset(subMeshIndex))
                        .SetVertexCount(mesh->GetIndexCount(subMeshIndex)));
                }
            }

            cmdList->SetTextureState(renderRes.GetCommonTexture(CommonTextureSlot::Depth), AllSubresources, ResourceStates::ShaderResource);

            cmdList->Close();
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            auto device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();
            auto colorTexDesc = renderRes.GetCommonTexture(CommonTextureSlot::Color)->GetDesc();
            m_MotionVecTex = device->CreateTexture(colorTexDesc
                .SetDebugName("TAA Motion Vector Texture")
                .SetFormat(Format::RG16_FLOAT)
                .SetIsRenderTarget(true));
            renderRes.SetCommonTexture(CommonTextureSlot::MotionVector, m_MotionVecTex);

            m_MotionVecFB = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(m_MotionVecTex, AllSubresources));
            m_MotionVecFBWithDepth = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(m_MotionVecTex, AllSubresources)
                .SetDepthAttachment(renderRes.GetCommonTexture(CommonTextureSlot::Depth), AllSubresources));

            m_MotionVecPipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc{}
                .SetVertexShader(m_MotionVecVS)
                .SetPixelShader(m_MotionVecPS)
                .AddBindingLayout(m_BindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthTestEnable(false))
                    .SetRasterState(RasterState().SetCullMode(RasterCullMode::None)))
                , m_MotionVecFB);
            
            auto reversedZ = renderer.GetCamera().IsReversedZ();
            m_MotionVecObjectPipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc{}
                .SetVertexShader(m_MotionVecObjectVS)
                .SetPixelShader(m_MotionVecObjectPS)
                .SetInputLayout(m_InputLayout)
                .AddBindingLayout(m_BindingLayout, 0)
                .SetRenderState(RenderState()
                    .SetDepthStencilState(DepthStencilState()
                        .SetDepthWriteEnable(false)
                        .SetDepthFunc(reversedZ ? ComparisonFunc::GreaterOrEqual : ComparisonFunc::LessOrEqual)))
                , m_MotionVecFBWithDepth);
            
            auto pointClampSampler = RenderResource::GetInstance().GetCommonSampler(SamplerSlot::PointClamp);
            m_MotionVecBindingSet = device->CreateBindingSet(BindingSetDesc()
                .AddItem(BindingSetItem::ConstantBuffer(0, m_Constants))
                .AddItem(BindingSetItem::Texture_SRV(0, renderRes.GetCommonTexture(CommonTextureSlot::Depth)))
                .AddItem(BindingSetItem::Sampler((uint32_t)SamplerSlot::PointClamp, pointClampSampler))
                , m_BindingLayout);
        }

    private:
        struct MotionVecFullScreenConstants
        {
            Math::Matrix4 currMatrix;
            Math::Matrix4 prevMatrix;
        };

        struct MotionVecObjConstants
        {
            int objectIndex;
            int lastFrameObjectIndex;
        };

        BindingLayoutHandle m_BindingLayout{};
        BindingSetHandle m_MotionVecBindingSet{};
        BindingSetHandle m_MotionVecObjectBindingSet{};

        TextureHandle m_MotionVecTex{};
        BufferHandle m_Constants{};

        IBuffer* m_CachedMeshBuffer = nullptr;
        IBuffer* m_CachedLastFrameMeshBuffer = nullptr;

        ShaderHandle m_MotionVecVS{};
        ShaderHandle m_MotionVecPS{};
        ShaderHandle m_MotionVecObjectVS{};
        ShaderHandle m_MotionVecObjectPS{};

        InputLayoutHandle m_InputLayout{};
        
        GraphicsPipelineHandle m_MotionVecPipeline{};
        GraphicsPipelineHandle m_MotionVecObjectPipeline{};
        
        FramebufferHandle m_MotionVecFB{};
        FramebufferHandle m_MotionVecFBWithDepth{};

        Math::Matrix4 m_PreViewProjMatrix = Math::Matrix4::Identity;
    };
}


#endif // !__MOTION_VECTOR_PASS_H__