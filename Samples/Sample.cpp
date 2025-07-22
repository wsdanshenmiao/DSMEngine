#include <DSMEngine.h>
#include <Core/Layer.h>
#include <print>
#include "Graphics/Device.h"

class ExampleLayer : public DSM::Layer
{
public:
    ExampleLayer() : DSM::Layer("ExampleLayer") {}

    void OnUpdate() override
    {
        DSM_INFO("ExampleLayer::OnUpdate");
    }

    void OnEvent(DSM::Event& event) override
    {
        DSM_TRACE("{}", event);
    }
};

class Sample : public DSM::Application
{
public:
    Sample()
    {
        m_LayerStack.PushLayer(new ExampleLayer());
    }

};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

