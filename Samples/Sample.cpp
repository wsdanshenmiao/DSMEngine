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

using namespace DSM;

class RenderPass : public IRenderPass
{
public:
    void Render(Renderer* renderer, IFramebuffer* fb) override
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

        auto matrix = Matrix<float, 3, 4>::Identity * Matrix<float, 4, 3>::Identity;;

        float width = (float)fb->GetFramebufferInfo().width;
        float height = (float)fb->GetFramebufferInfo().height;
        float aspectRatio = width / height;

        std::array<Vector3f, 3> vertexPos = {
            Vector3f{ 0.0f, 0.25f * aspectRatio, 0.0f },
            Vector3f{ 0.25f, -0.25f * aspectRatio, 0.0f },
            Vector3f{ -0.25f, -0.25f * aspectRatio, 0.0f } };

        std::array<Vector4f, 8> vertexColor = {
            Vector4f{ 1.0f, 0.0f, 0.0f, 1.0f },
            Vector4f{ 0.0f, 1.0f, 0.0f, 1.0f },
            Vector4f{ 0.0f, 0.0f, 1.0f, 1.0f } };

        auto device = renderer->GetDevice();
        auto cmdList = device->CreateCommandList(
            CommandListParameters().SetQueueType(CommandQueueType::Graphics));
        cmdList->Open();

        uint32_t posByteSize = sizeof(Vector3f) * vertexPos.size();
        uint32_t colorByteSize = sizeof(Vector4f) * vertexColor.size();
        auto vertexBuffer = device->CreateBuffer(BufferDesc()
            .SetByteSize(posByteSize + colorByteSize)
            .SetDebugName("VertexBuffer")
            .SetIsVertexBuffer(true));
        // auto indexBuffer = device->CreateBuffer(BufferDesc()
        //     .SetByteSize(sizeof(uint32_t) * indices.size())
        //     .SetDebugName("IndexBuffer")
        //     .SetIsIndexBuffer(true)
        //     .SetFormat(Format::R32_UINT));

        cmdList->WriteBuffer(vertexBuffer, vertexPos.data(), posByteSize);
        cmdList->WriteBuffer(vertexBuffer, vertexColor.data(), colorByteSize, posByteSize);
        // cmdList->WriteBuffer(indexBuffer, indices.data(), indexBuffer->GetDesc().byteSize);

        const auto& rendertarget = fb->GetDesc().colorAttachments[0];
        cmdList->BeginTrackingTextureState(rendertarget.texture, AllSubresources);
        cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{1, 0.7f, 0.75f, 1});

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
        
        ShaderHandle vertex = device->CreateShader(ShaderDesc()
            .SetEntryName(vs.GetDesc().m_EnterPoint)
            .SetShaderType(vs.GetDesc().m_Type)
            .SetDebugName("ColorVS"), 
            vs.GetByteCode(), vs.GetByteCodeSize());
        ShaderHandle pixel = device->CreateShader(ShaderDesc()
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
        InputLayoutHandle layout = device->CreateInputLayout(attributes, vertex);

        auto binding = device->CreateBindingLayout(BindingLayoutDesc()
            .SetVisibility(ShaderType::All)
            .AddItem(BindingLayoutItem{}.ConstantBuffer(0))
            .AddItem(BindingLayoutItem{}.ConstantBuffer(1)));

        RenderState renderState{};
        renderState.SetBlendState(BlendState{}.SetRenderTarget(0, BlendState::RenderTarget{}))
            .SetRasterState(RasterState{})
            .SetDepthStencilState(DepthStencilState{}.SetDepthTestEnable(false));

        auto pso = device->CreateGraphicsPipeline(GraphicsPipelineDesc()
            .SetInputLayout(layout)
            .SetVertexShader(vertex)
            .SetPixelShader(pixel)
            .SetRenderState(renderState)
            .AddBindingLayout(binding), fb);

        GraphicsState state{};
        state.SetFramebuffer(fb).SetPipeline(pso)
            .SetViewport(ViewportState{}.AddViewportAndScissorRect(Viewport{width, height}))
            // .SetIndexBuffer(IndexBufferBinding{}.SetBuffer(indexBuffer).SetFormat(Format::R32_UINT))
            .AddVertexBuffer(VertexBufferBinding{}.SetBuffer(vertexBuffer).SetSlot(0))
            .AddVertexBuffer(VertexBufferBinding{}.SetBuffer(vertexBuffer).SetSlot(1).SetOffset(posByteSize));

        cmdList->SetGraphicsState(state);
        
        // cmdList->DrawIndexed(DrawArguments{}.SetVertexCount(indices.size()));
        cmdList->Draw(DrawArguments{}.SetVertexCount(vertexPos.size()));

        cmdList->Close();

        device->ExecuteCommandList(cmdList);

        device->RunGarbageCollection();
    }
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
        m_Renderer->AddRenderPass(&m_RenderPass);
    }

    RenderPass m_RenderPass{};
};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

