#include <DSMEngine.h>
#include <Core/Layer.h>
#include <print>
#include "Graphics/D3D12.h"

using namespace DSM;

class MessageCallback : public IMessageCallback
{
public:
    void Message(MessageSeverity severity, const char* messageText) override
    {
        std::string msg{messageText};
        switch (severity) {
        case MessageSeverity::Error:{
            //msg += "Error: ";
            DSM_ERROR(msg);
            break;
        }
        case MessageSeverity::Fatal:{
            //msg += "Fatal: "; 
            DSM_CRITICAL(msg);
            break;
        }
        case MessageSeverity::Info:{
            //msg += "Info: "; 
            DSM_INFO(msg);
            break;
        }
        case MessageSeverity::Warning:{
            //msg += "Warning: "; 
            DSM_WARN(msg);
            break;
        }
        default:
            break;
        }
        //std::println("{}{}", msg, std::string{messageText});
    }
};

class ExampleLayer : public DSM::Layer
{
public:
    ExampleLayer() : DSM::Layer("ExampleLayer") {}

    void OnAttach() override
    {
        using namespace DSM::D3D12;
        DeviceDesc desc{};
        desc.errorCB = &m_Callback;

        m_Device = CreateDevice(desc);
    }

    void OnUpdate() override
    {
        //DSM_INFO("ExampleLayer::OnUpdate");
        CommandListParameters cmdListDesc{};
        cmdListDesc.SetQueueType(CommandQueueType::Graphics);
        auto cmdList = m_Device->CreateCommandList(cmdListDesc);
        cmdList->Open();


        cmdList->Close();
        m_Device->ExecuteCommandList(cmdList);
    }

    void OnEvent(DSM::Event& event) override
    {
        //DSM_TRACE("{}", event);
    }

private:
    DSM::DeviceHandle m_Device;
    MessageCallback m_Callback{};
};

class Sample : public DSM::Application
{
public:
    Sample()
    {
        PushLayer(new ExampleLayer());
    }

};

DSM::Application* DSM::CreateApplication()
{
    return new Sample();
}

