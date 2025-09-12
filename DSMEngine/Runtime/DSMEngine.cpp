#include "DSMEngine.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Core/Global/GlobalContext.h"
#include "Runtime/Event/ApplicationEvent.h"

namespace DSM {

    void DSMEngine::StartEngine()
    {
        g_GlobalContext.CreateContext();
        g_GlobalContext.window->SetEventCallback([this](Event& event){
            EventDispatcher dispatcher{event};
            dispatcher.Dispatch<WindowCloseEvent>([this](auto& event) { m_Running = false; return true; });

            g_GlobalContext.renderer->OnEvent(event);
        });
    }

    void DSMEngine::ShutDownEngine()
    {
        g_GlobalContext.ShutdownContext();
    }

    void DSMEngine::Run()
    {
        m_Timer.Reset();
        while (m_Running) {
            m_Timer.Tick();
            Update();
        }
    }

    void DSMEngine::Update()
    {
        float deltaTime = m_Timer.DeltaTime();
        CalculateFPS();
        g_GlobalContext.window->Update();

        Render(deltaTime);
    }


    void DSMEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline)
    {
        g_GlobalContext.renderer->SetRenderPipeline(std::move(renderPipeline));
    }

    void DSMEngine::Render(float deltaTime)
    {
        g_GlobalContext.renderer->Render(deltaTime);
    }

    void DSMEngine::CalculateFPS()
    {
        static int frameCnt = 0;
        static float timeElapsed = 0.0f;
        static std::string originTitle = g_GlobalContext.window->GetTitle();

        frameCnt++;

        if ((m_Timer.TotalTime() - timeElapsed) >= 1.0f) {
            float fps = (float)frameCnt; // fps = frameCnt / 1
            float mspf = 1000.0f / fps;

            auto title = std::format("{}    FPS: {}    Frame Time: {} (ms)", originTitle, fps, mspf);
            g_GlobalContext.window->SetTitle(title);

            // Reset for next average.
            frameCnt = 0;
            timeElapsed += 1.0f;
        }
    }

} // namespace DSM