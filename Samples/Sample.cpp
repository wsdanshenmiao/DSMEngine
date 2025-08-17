#include <DSMEngine.h>
#include <Core/Layer.h>
#include <print>
#include "Graphics/D3D12.h"
#include "Render/Renderer.h"

using namespace DSM;

class RenderPass : public IRenderPass
{
public:
    void Render(Renderer* renderer, IFramebuffer* fb) override
    {
        // auto device = renderer->GetDevice();

        // auto cmdList = device->CreateCommandList(
        //     CommandListParameters().SetQueueType(CommandQueueType::Graphics));

        // cmdList->Open();
        // const auto& rendertarget = fb->GetDesc().colorAttachments[0];
        // cmdList->BeginTrackingTextureState(rendertarget.texture, AllSubresources);
        // cmdList->ClearTextureFloat(rendertarget.texture, AllSubresources, Color{1, 0.7f, 0.75f, 1});
        // ID3D12GraphicsCommandList* nativeList = cmdList->GetNativeObject(ObjectTypes::D3D12_GraphicsCommandList);
        // auto descriptor = rendertarget.texture->GetNativeView(ObjectTypes::D3D12_RenderTargetViewDescriptor);
        // auto rtv = D3D12_CPU_DESCRIPTOR_HANDLE{descriptor.integer};
        // nativeList->OMSetRenderTargets(1, &rtv, false, nullptr);
        // cmdList->Close();

        // device->ExecuteCommandList(cmdList); 
    }
};

class ExampleLayer : public DSM::Layer
{
public:
    ExampleLayer() : DSM::Layer("ExampleLayer") {}

    void OnAttach() override
    {
    }

    void OnUpdate() override
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
        m_Renderer->AddRenderPass(&m_RenderPass);
    }

    RenderPass m_RenderPass{};
};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

