#pragma once
#ifndef __SHADING_PASS_H__
#define __SHADING_PASS_H__

#include "RenderResource.h"
#include "Runtime/Render/Model.h"
#include "Runtime/Render/ShaderCompiler.h"

namespace DSM {
    // 在该 Pass 中进行着色
    class GeometryPass : public IRenderPass {
    public:
        GeometryPass(GraphicsRenderer& renderer)
        {
            IDevice* device = renderer.GetDevice();

            // create constant buffer
            m_PassCB = device->CreateBuffer(BufferDesc()
                .SetByteSize(sizeof(Math::Matrix4) * 2)
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true)
                .SetDebugName("GeometryPassConstants"));

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
                    .SetFilename("Shaders/ForwardShader/Passes/GeometryPass.hlsl")
                    .SetEnterPoint(entryPoint);
                ShaderByteCode geometryPass{compileDesc};
                return device->CreateShader(ShaderDesc()
                    .SetShaderType(compileDesc.type)
                    .SetEntryName(compileDesc.enterPoint)
                    .SetDebugName(compileDesc.enterPoint),
                    geometryPass.GetByteCode(), geometryPass.GetByteCodeSize());
            };

            m_VertexShader = createShader(ShaderType::Vertex, "GeometryPassVS");
            m_PixelShader = createShader(ShaderType::Pixel, "GeometryPassPS");
            m_InputLayout = device->CreateInputLayout(attributes, m_VertexShader);
            m_BindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
                .AddItem(BindingLayoutItem().PushConstants(0, sizeof(int))) // obj index
                .AddItem(BindingLayoutItem().VolatileConstantBuffer(1))
                .AddItem(BindingLayoutItem().StructuredBuffer_SRV(0)));

            const Viewport& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, viewport.Width(), viewport.Height());

            sm_TimerQuery = device->CreateTimerQuery();
        }

        uint64_t Render(DSM::GraphicsRenderer& renderer, float deltaTime) override
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

            if(auto meshBuffer = RenderResource::GetInstance().GetMeshBuffer(); 
                m_BindingSet == nullptr || m_CacheMeshBuffer != meshBuffer)
            {
                auto bindingSet = device->CreateBindingSet(BindingSetDesc()
                    .AddItem(BindingSetItem().ConstantBuffer(1, m_PassCB))
                    .AddItem(BindingSetItem().StructuredBuffer_SRV(0, meshBuffer)),
                    m_Pipeline->GetDesc().bindingLayouts[0]);
                m_BindingSet = bindingSet;
                m_CacheMeshBuffer = meshBuffer;
            }

            for(const auto& [index, obj] : RenderResource::GetInstance().GetObjectInFrustum()){
                auto meshRenderer = obj->GetComponent<MeshRenderer>();
                if(meshRenderer == nullptr || meshRenderer->GetMesh() == nullptr)
                    continue;
                auto& mesh = *meshRenderer->GetMesh();

                // 透明物体不需要渲染深度
                if(HasFlags(PSOFlags(mesh.psoFlags), kAlphaBlend))
                    continue;

                GraphicsState state = GraphicsState()
                    .SetFramebuffer(m_Framebuffer)
                    .SetPipeline(m_Pipeline)
                    .SetViewport(ViewportState().
                        AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()))
                    .SetIndexBuffer(mesh.indexBufferViews)
                    .AddBindingSet(m_BindingSet, 0);
                if(HasFlags(PSOFlags(mesh.psoFlags), kHasPosition)){
                    state.AddVertexBuffer(mesh.positionStream);
                }
                auto vertexBinding = mesh.normalStream;
                vertexBinding.SetSlot(1);
                if(HasFlags(PSOFlags(mesh.psoFlags), kHasNormal)){
                    state.AddVertexBuffer(vertexBinding);
                }

                cmdList->SetGraphicsState(state);

                int objIndex = index;
                cmdList->SetPushConstants(&objIndex, sizeof(int));

                // 绘制
                cmdList->DrawIndexed(DrawArguments{}
                    .SetStartIndexLocation(mesh.indexOffset)
                    .SetStartVertexLocation(mesh.vertexOffset)
                    .SetVertexCount(mesh.indexCount));
            }

            cmdList->SetTextureState(fbDesc.colorAttachments[0].texture, AllSubresources, ResourceStates::ShaderResource);
            cmdList->SetTextureState(fbDesc.depthAttachment.texture, AllSubresources, ResourceStates::ShaderResource);

            // 结束计时
            cmdList->EndTimerQuery(sm_TimerQuery);
            
            cmdList->Close();
            return device->ExecuteCommandList(cmdList);
        }

        void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override
        {
            // 创建法线法线纹理等资源
            IDevice* device = renderer.GetDevice();
            auto& renderRes = RenderResource::GetInstance();
            renderRes.SetCommonTexture(CommonTextureSlot::Normal, device->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::RG32_FLOAT)
                .SetIsRenderTarget(true)
                .SetClearValue({})
                .SetDebugName("NormalTexture")));
            m_Framebuffer = device->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(renderRes.GetCommonTexture(CommonTextureSlot::Normal))
                .SetDepthAttachment(renderRes.GetCommonTexture(CommonTextureSlot::Depth)));

            m_Pipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc()
                .SetVertexShader(m_VertexShader)
                .SetPixelShader(m_PixelShader)
                .SetInputLayout(m_InputLayout)
                .SetRenderState(RenderState{})
                .AddBindingLayout(m_BindingLayout, 0),
                m_Framebuffer);
        }

    public:
        inline static TimerQueryHandle sm_TimerQuery{};

    private:
        BufferHandle m_PassCB{};
        IBuffer* m_CacheMeshBuffer = nullptr;

        FramebufferHandle m_Framebuffer{};

        GraphicsPipelineHandle m_Pipeline{};
        
        ShaderHandle m_VertexShader{};
        ShaderHandle m_PixelShader{};

        InputLayoutHandle m_InputLayout{};
        BindingLayoutHandle m_BindingLayout{};

        BindingSetHandle m_BindingSet{};
    };
} // namespace DSM


#endif // !__SHADING_PASS_H__