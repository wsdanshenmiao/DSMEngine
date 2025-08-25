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

using namespace DSM;

class RenderPass : public IRenderPass
{
public:

    struct ObjectConstants
    {
        Math::Matrix4 World;
        Math::Matrix4 WorldInvTranspose;
    };

    struct PassConstants
    {
        Math::Matrix4 View;
        Math::Matrix4 InvView;
        Math::Matrix4 Proj;
        Math::Matrix4 InvProj;
        Math::Vector3 EyePosW;
    };

    RenderPass(IDevice* device, uint32_t width, uint32_t height)
    {

        // std::array<Vector3f, 8> vertexPos = {
        //     Vector3f{-1.0f, -1.0f, -1.0f},
        //     Vector3f{-1.0f, +1.0f, -1.0f},
        //     Vector3f{+1.0f, +1.0f, -1.0f},
        //     Vector3f{+1.0f, -1.0f, -1.0f},
        //     Vector3f{-1.0f, -1.0f, +1.0f},
        //     Vector3f{-1.0f, +1.0f, +1.0f},
        //     Vector3f{+1.0f, +1.0f, +1.0f},
        //     Vector3f{+1.0f, -1.0f, +1.0f}};
        // std::array<Vector4f, 8> vertexColor = {
        //     Vector4f{DirectX::Colors::White.f},
        //     Vector4f{DirectX::Colors::Black.f},
        //     Vector4f{DirectX::Colors::Red.f},
        //     Vector4f{DirectX::Colors::Green.f},
        //     Vector4f{DirectX::Colors::Blue.f},
        //     Vector4f{DirectX::Colors::Yellow.f},
        //     Vector4f{DirectX::Colors::Cyan.f},
        //     Vector4f{DirectX::Colors::Magenta.f}};

        // std::array<std::uint32_t, 36> indices ={
        //     0, 1, 2,
        //     0, 2, 3,
        //     4, 6, 5,
        //     4, 7, 6,
        //     4, 5, 1,
        //     4, 1, 0,
        //     3, 2, 6,
        //     3, 6, 7,
        //     1, 5, 6,
        //     1, 6, 2,
        //     4, 0, 3,
        //     4, 3, 7
        // };

        std::array<Vector3f, 3> vertexPos = {
            Vector3f{ 0.0f, 0.25f, 1.0f },
            Vector3f{ 0.25f, -0.25f, 1.0f },
            Vector3f{ -0.25f, -0.25f, 1.0f } };

        std::array<Vector4f, 8> vertexColor = {
            Vector4f{ 1.0f, 0.0f, 0.0f, 1.0f },
            Vector4f{ 0.0f, 1.0f, 0.0f, 1.0f },
            Vector4f{ 0.0f, 0.0f, 1.0f, 1.0f } };

        m_VertexCount = vertexPos.size();

        m_PosByteSize = sizeof(Vector3f) * m_VertexCount;
        uint32_t colorByteSize = sizeof(Vector4f) * vertexColor.size();
        m_VertexBuffer = device->CreateBuffer(BufferDesc()
            .SetByteSize(m_PosByteSize + colorByteSize)
            .SetDebugName("VertexBuffer")
            .SetIsVertexBuffer(true));
        // m_IndexBuffer = device->CreateBuffer(BufferDesc()
        //     .SetByteSize(sizeof(uint32_t) * indices.size())
        //     .SetDebugName("IndexBuffer")
        //     .SetIsIndexBuffer(true)
        //     .SetFormat(Format::R32_UINT));

        auto objectCBByteSize = Math::Align(sizeof(ObjectConstants), uint64_t(c_ConstantBufferOffsetSizeAlignment));
        auto passCBByteSize = sizeof(PassConstants);
        m_ConstBuffer = device->CreateBuffer(BufferDesc()
            .SetIsConstantBuffer(true)
            .SetByteSize(objectCBByteSize + passCBByteSize)
            .SetDebugName("ConstBuffer")
            .SetIsVolatile(true));
        

        auto cmdList = device->CreateCommandList(
            CommandListParameters().SetQueueType(CommandQueueType::Graphics).SetDebugName("Write VertexBuffer"));
        cmdList->Open();

        cmdList->WriteBuffer(m_VertexBuffer, vertexPos.data(), m_PosByteSize);
        cmdList->WriteBuffer(m_VertexBuffer, vertexColor.data(), colorByteSize, m_PosByteSize);
        // cmdList->WriteBuffer(indexBuffer, indices.data(), indexBuffer->GetDesc().byteSize);

        cmdList->Close();
        device->ExecuteCommandList(cmdList);

        // 创建着色器
        ShaderByteCode vs{ShaderCompileDesc()
            .SetType(ShaderType::Vertex)
            .SetMode(ShaderMode::SM_6_1)
            .SetFilename("Shaders/Color.hlsl")
            .SetEnterPoint("VS")};
        ShaderByteCode ps{ShaderCompileDesc()
            .SetType(ShaderType::Pixel)
            .SetMode(ShaderMode::SM_6_1)
            .SetFilename("Shaders/Color.hlsl")
            .SetEnterPoint("PS")};
        
        m_VS = device->CreateShader(ShaderDesc()
            .SetEntryName(vs.GetDesc().m_EnterPoint)
            .SetShaderType(vs.GetDesc().m_Type)
            .SetDebugName("ColorVS"), 
            vs.GetByteCode(), vs.GetByteCodeSize());
        m_PS = device->CreateShader(ShaderDesc()
            .SetEntryName(ps.GetDesc().m_EnterPoint)
            .SetShaderType(ps.GetDesc().m_Type)
            .SetDebugName("ColorVS"), 
            ps.GetByteCode(), ps.GetByteCodeSize());

        // 创建输入布局
        std::vector<VertexAttributeDesc> attributes(2);
        attributes[0].SetName("POSITION")
            .SetBufferIndex(0)
            .SetFormat(Format::RGB32_FLOAT)
            .SetElementStride(GetFormatInfo(Format::RGB32_FLOAT).bytesPerBlock);
        attributes[1].SetName("COLOR")
            .SetBufferIndex(1)
            .SetFormat(Format::RGBA32_FLOAT)
            .SetElementStride(GetFormatInfo(Format::RGBA32_FLOAT).bytesPerBlock);
        m_Layout = device->CreateInputLayout(attributes, m_VS);

        m_Camera = std::make_unique<Camera>();
        m_CameraController = std::make_unique<CameraController>();
        m_CameraController->InitCamera(m_Camera.get());
        OnResize(width, height);
    }

    void Render(Renderer* renderer, IFramebuffer* fb) override
    {
        m_CameraController->Update(0.01f);

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

        auto cbOffset = Math::Align(sizeof(ObjectConstants), uint64_t(c_ConstantBufferOffsetSizeAlignment));
        PassConstants passCB{};
        passCB.View = Math::Matrix4::Transpose(m_Camera->GetViewMatrix());
        passCB.InvView = Math::Matrix4::Inverse(passCB.View);
        passCB.Proj = Math::Matrix4::Transpose(m_Camera->GetProjMatrix());
        passCB.InvProj = Math::Matrix4::Inverse(passCB.Proj);
        passCB.EyePosW = m_Camera->GetPosition();
        ObjectConstants objectCB{};
        objectCB.World = Math::Matrix4::Identity;
        objectCB.WorldInvTranspose = objectCB.World;
        cmdList->WriteBuffer(m_ConstBuffer, &objectCB, sizeof(objectCB));
        cmdList->WriteBuffer(m_ConstBuffer, &passCB, sizeof(passCB), cbOffset);

        auto binding = device->CreateBindingLayout(BindingLayoutDesc()
            .SetVisibility(ShaderType::All)
            .AddItem(BindingLayoutItem{}.VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem{}.VolatileConstantBuffer(1)));

        auto bindingSet = device->CreateBindingSet(BindingSetDesc()
            .AddItem(BindingSetItem().ConstantBuffer(0, m_ConstBuffer, 
                BufferRange().SetByteOffset(0).SetByteSize(sizeof(ObjectConstants))))
            .AddItem(BindingSetItem().ConstantBuffer(1, m_ConstBuffer, 
                BufferRange().SetByteOffset(cbOffset).SetByteSize(sizeof(PassConstants)))), binding);

        RenderState renderState{};
        renderState.SetBlendState(BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}))
            .SetRasterState(RasterState{})
            .SetDepthStencilState(DepthStencilState{}.SetDepthTestEnable(false));

        auto pso = device->CreateGraphicsPipeline(GraphicsPipelineDesc()
            .SetInputLayout(m_Layout)
            .SetVertexShader(m_VS)
            .SetPixelShader(m_PS)
            .SetRenderState(renderState)
            .AddBindingLayout(binding), fb);

        GraphicsState state{};
        state.SetFramebuffer(fb).SetPipeline(pso).AddBindingSet(bindingSet)
            .SetViewport(ViewportState{}.AddViewportAndScissorRect(Viewport{width, height}))
            // .SetIndexBuffer(IndexBufferBinding{}.SetBuffer(indexBuffer).SetFormat(Format::R32_UINT))
            .AddVertexBuffer(VertexBufferBinding{}.SetBuffer(m_VertexBuffer).SetSlot(0))
            .AddVertexBuffer(VertexBufferBinding{}.SetBuffer(m_VertexBuffer).SetSlot(1).SetOffset(m_PosByteSize));

        cmdList->SetGraphicsState(state);
        
        // cmdList->DrawIndexed(DrawArguments{}.SetVertexCount(indices.size()));
        cmdList->Draw(DrawArguments{}.SetVertexCount(m_VertexCount));

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
    BufferHandle m_VertexBuffer;
    BufferHandle m_IndexBuffer;
    BufferHandle m_ConstBuffer;

    ShaderHandle m_VS;
    ShaderHandle m_PS;
    InputLayoutHandle m_Layout;

    uint32_t m_PosByteSize;
    uint32_t m_VertexCount;

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

