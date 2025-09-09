#include <DSMEngine.h>
#include <Core/Layer.h>
#include <DirectXColors.h>
#include <print>
#include <imgui.h>
#include "Graphics/D3D12.h"
#include "Render/Renderer.h"
#include "Render/ShaderCompiler.h"
#include "Math/MathCommon.h"
#include "Math/Matrix.h"
#include "Render/CameraController.h"
#include "Core/CpuTimer.h"
#include "Render/ModelLoader.h"
#include "Shaders/ConstantBuffers.h"

using namespace DSM;

class RenderPass : public IRenderPass
{
public:
    RenderPass(IDevice* device, uint32_t width, uint32_t height)
    {
        m_Model = ModelLoader::LoadModel("Models/Sponza/sponza.gltf");
        assert(m_Model != nullptr);

        m_Camera = std::make_unique<Camera>();
        m_Camera->SetPosition(0, 0, -5);
        m_CameraController = std::make_unique<CameraController>();
        m_CameraController->InitCamera(m_Camera.get());

        // 创建着色器
        ShaderCompileDesc vsDesc{};
        vsDesc.SetType(ShaderType::Vertex)
            .SetMode(ShaderMode::SM_6_1)
            .SetFilename("Shaders/Lit.hlsl")
            .SetEnterPoint("LitPassVS");
        ShaderByteCode vsNoTangent{vsDesc};
        ShaderByteCode vs{vsDesc.AddDefine("USE_TANGENT", "1")};

        ShaderCompileDesc psDesc{};
        psDesc.SetType(ShaderType::Pixel)
            .SetMode(ShaderMode::SM_6_1)
            .SetFilename("Shaders/Lit.hlsl")
            .SetEnterPoint("LitPassPS");
        ShaderByteCode psNoTangent{psDesc};
        ShaderByteCode ps{psDesc.AddDefine("USE_TANGENT", "1")};

        m_VS = device->CreateShader(ShaderDesc()
            .SetEntryName(vs.GetDesc().m_EnterPoint)
            .SetShaderType(vs.GetDesc().m_Type)
            .SetDebugName("LitPassVS"), 
            vs.GetByteCode(), vs.GetByteCodeSize());
        m_VSNoTangent = device->CreateShader(ShaderDesc()
            .SetEntryName(vsNoTangent.GetDesc().m_EnterPoint)
            .SetShaderType(vsNoTangent.GetDesc().m_Type)
            .SetDebugName("LitPassVSNoTangent"), 
            vsNoTangent.GetByteCode(), vsNoTangent.GetByteCodeSize());
        m_PS = device->CreateShader(ShaderDesc()
            .SetEntryName(ps.GetDesc().m_EnterPoint)
            .SetShaderType(ps.GetDesc().m_Type)
            .SetDebugName("LitPassPS"), 
            ps.GetByteCode(), ps.GetByteCodeSize());
        m_PSNoTangent = device->CreateShader(ShaderDesc()
            .SetEntryName(psNoTangent.GetDesc().m_EnterPoint)
            .SetShaderType(psNoTangent.GetDesc().m_Type)
            .SetDebugName("LitPassPSNoTangent"), 
            psNoTangent.GetByteCode(), psNoTangent.GetByteCodeSize());

        m_CommonBindingLayout = device->CreateBindingLayout(BindingLayoutDesc()
            .AddItem(BindingLayoutItem().VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem().ConstantBuffer(1)) // MaterialData
            .AddItem(BindingLayoutItem().VolatileConstantBuffer(2)) // PassConstants
            .AddItem(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0).SetSize(10)) // 10 个用于 PBR 的纹理
            .AddItem(BindingLayoutItem().Sampler(0)));

        m_CommonBindlessLayout = device->CreateBindlessLayout(BindlessLayoutDesc()
            .SetFirstSlot(10)
            .SetVisibility(ShaderType::All)
            .AddRegisterSpace(BindingLayoutItem().SetType(ResourceType::Texture_SRV).SetSlot(0)));

        m_PassCB = device->CreateBuffer(BufferDesc()
            .SetByteSize(sizeof(PassConstants))
            .SetIsConstantBuffer(true)
            .SetIsVolatile(true)
            .SetDebugName("PassConstants"));

        m_Sampler = device->CreateSampler(SamplerDesc());

        GenerateRenderConfigs(device, m_Model);

        OnResize(width, height);
    }

    void Render(const CpuTimer& timer, Renderer* renderer, IFramebuffer* fb) override
    {
        m_CameraController->Update(timer.DeltaTime());

        float width = (float)fb->GetFramebufferInfo().width;
        float height = (float)fb->GetFramebufferInfo().height;
        float aspectRatio = width / height;

        auto device = renderer->GetDevice();
        auto cmdList = device->CreateCommandList(
            CommandListParameters().SetQueueType(CommandQueueType::Graphics));
        cmdList->Open();

        const auto& rendertarget = fb->GetDesc().colorAttachments[0];
        cmdList->BeginTrackingTextureState(rendertarget.texture, AllSubresources);
        cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{1, 0.7f, 0.75f, 1});

        PassConstants passCB{};
        passCB.view = Math::Matrix4::Transpose(m_Camera->GetViewMatrix());
        passCB.viewInv = Math::Matrix4::Inverse(passCB.view);
        passCB.proj = Math::Matrix4::Transpose(m_Camera->GetProjMatrix());
        passCB.projInv = Math::Matrix4::Inverse(passCB.proj);
        passCB.shadowTrans = Math::Matrix4::Identity; // TODO
        passCB.cameraPos = m_Camera->GetPosition();
        passCB.totalTime = timer.TotalTime();
        passCB.deltaTime = timer.DeltaTime();
        cmdList->BeginTrackingBufferState(m_PassCB);
        cmdList->WriteBuffer(m_PassCB, &passCB, sizeof(PassConstants));

        for(const auto& mesh : m_Model->meshes){        
            for(const auto& [name, submesh] : mesh->subMeshes){
                MeshConstants meshCB{};
                meshCB.world = Math::Matrix4::Transpose(Math::Matrix4::Identity);
                meshCB.worldIT = Math::Matrix4::Inverse(meshCB.world);
                auto& meshBuffer = m_RenderConfigs[mesh->psoIndex].meshCB;
                cmdList->BeginTrackingBufferState(meshBuffer);
                cmdList->WriteBuffer(meshBuffer, &meshCB, sizeof(MeshConstants));

                // 绑定资源
                auto bindingDesc = BindingSetDesc()
                    .AddItem(BindingSetItem().ConstantBuffer(0, m_RenderConfigs[mesh->psoIndex].meshCB))
                    .AddItem(BindingSetItem().ConstantBuffer(1, m_Model->materialData))
                    .AddItem(BindingSetItem().ConstantBuffer(2, m_PassCB));
                for(size_t i = 0; i < kNumTextures; ++i){
                    bindingDesc.AddItem(BindingSetItem().Texture_SRV(i, submesh.textures[i]));
                }
                bindingDesc.AddItem(BindingSetItem().Sampler(0, m_Sampler));
                auto bindingSet = device->CreateBindingSet(bindingDesc, m_CommonBindingLayout);

                // 配置管线状态
                auto pso = device->CreateGraphicsPipeline(m_RenderConfigs[mesh->psoIndex].pipelineDesc, fb);

                GraphicsState state{};
                state.SetFramebuffer(fb)
                    .SetPipeline(pso)
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

        cmdList->Close();

        device->ExecuteCommandList(cmdList);

        device->RunGarbageCollection();
    }

    void OnResize(uint32_t width, uint32_t height) override
    {
        m_Camera->SetViewPort(Viewport{float(width), float(height)});
        m_Camera->SetFrustum(std::numbers::pi * 0.5f, float(width) / height, 0.1f, 1000.f);
    }

private:
    void GenerateRenderConfigs(IDevice* device, std::shared_ptr<Model> model)
    {
        // 创建渲染配置
        GraphicsPipelineHandle pipeline{};
        std::vector<VertexAttributeDesc> attributes{};
        attributes.reserve(4);

        BlendState hasBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}.SetBlendEnable(true));
        BlendState noBlend = BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{});

        DepthStencilState readWriteDepth = DepthStencilState{};
        DepthStencilState readDepth = DepthStencilState{}.SetDepthWriteEnable(false);

        RasterState defaultRaster = RasterState{};
        RasterState twoSided = RasterState{}.SetCullMode(RasterCullMode::None);

        auto addAttribute = [&attributes](auto currFlag, auto flag, 
            const std::string& name, auto index, auto format, auto size) {
            if (HasFlags(PSOFlags(currFlag), flag)) {
                attributes.push_back(VertexAttributeDesc()
                    .SetName(name)
                    .SetBufferIndex(index)
                    .SetFormat(format)
                    .SetElementStride(size));
            }
        };

        for(auto& mesh : model->meshes){
            attributes.clear();
            addAttribute(mesh->psoFlags, kHasPosition, 
                "POSITION", 0, Format::RGB32_FLOAT, sizeof(Math::Vector3));
            addAttribute(mesh->psoFlags, kHasUV, 
                "TEXCOORD", 1, Format::RG32_FLOAT, sizeof(Math::Vector2));
            addAttribute(mesh->psoFlags, kHasNormal, 
                "NORMAL", 2, Format::RGB32_FLOAT, sizeof(Math::Vector3));
            addAttribute(mesh->psoFlags, kHasTangent, 
                "TANGENT", 3, Format::RGBA32_FLOAT, sizeof(Math::Vector4));
            
            bool hasTangent = HasFlags(PSOFlags(mesh->psoFlags), kHasTangent);

            InputLayoutHandle layout = device->CreateInputLayout(attributes, hasTangent ? m_VS : m_VSNoTangent);

            const auto& blendState = HasFlags(PSOFlags(mesh->psoFlags), kAlphaBlend) ? hasBlend : noBlend;
            const auto& depthState = HasFlags(PSOFlags(mesh->psoFlags), kAlphaBlend) ? readDepth : readWriteDepth;
            const auto& rasterState = HasFlags(PSOFlags(mesh->psoFlags), kBothSide) ? twoSided : defaultRaster;

            auto desc = GraphicsPipelineDesc()
                .SetInputLayout(layout)
                .SetVertexShader(hasTangent ? m_VS : m_VSNoTangent)
                .SetPixelShader(hasTangent ? m_PS : m_PSNoTangent)
                .SetRenderState(RenderState{blendState, depthState, rasterState})
                .AddBindingLayout(m_CommonBindingLayout)
                .AddBindingLayout(m_CommonBindlessLayout);
            auto buffer = device->CreateBuffer(BufferDesc()
                .SetDebugName(mesh->name + "MeshConstants")
                .SetByteSize(sizeof(MeshConstants))
                .SetIsConstantBuffer(true)
                .SetIsVolatile(true));
            mesh->psoIndex = m_RenderConfigs.size();
            m_RenderConfigs.push_back({ desc, buffer });
        }
    }

private:
    struct RenderConfig
    {
        GraphicsPipelineDesc pipelineDesc;
        BufferHandle meshCB;
    };
    std::vector<RenderConfig> m_RenderConfigs;

    ShaderHandle m_VS;
    ShaderHandle m_PS;
    ShaderHandle m_VSNoTangent;
    ShaderHandle m_PSNoTangent;

    BufferHandle m_PassCB;
    SamplerHandle m_Sampler;

    BindingLayoutHandle m_CommonBindingLayout;
    BindingLayoutHandle m_CommonBindlessLayout;

    std::shared_ptr<Model> m_Model;

    std::unique_ptr<Camera> m_Camera;
    std::unique_ptr<CameraController> m_CameraController;
};

class ExampleLayer : public DSM::Layer
{
public:
    ExampleLayer() : DSM::Layer("ExampleLayer") {}

    void OnGUIRender() override
    {
        static bool show = true;
        ImGui::ShowDemoWindow(&show);
    }

    void OnUpdate(const CpuTimer& timer) override
    {
    }

private:
};

class Sample : public DSM::Application
{
public:
    Sample()
    {
        PushLayer(std::make_shared<ExampleLayer>());
        auto pass = std::make_unique<RenderPass>(m_Renderer->GetDevice(), m_Window->GetWidth(), m_Window->GetHeight());
        m_Renderer->AddRenderPass(pass.get());
        
        m_RenderPasses.push_back(std::move(pass));
    }

private:
    std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

