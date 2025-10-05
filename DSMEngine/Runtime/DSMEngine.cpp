#include "DSMEngine.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Framework/World.h"

namespace DSM {

    void DSMEngine::StartEngine(const EngineParameters& params)
    {
        sm_GlobalContext.loggerSystem = std::make_shared<LogSystem>();
        sm_GlobalContext.window = std::make_shared<Window>(WindowProps{});
        sm_GlobalContext.inputSystem = std::make_shared<InputSystem>(sm_GlobalContext.window.get());
        RenderParameters renderParams{};
        renderParams.window = sm_GlobalContext.window.get();
        renderParams.enableDebugLayer = params.enableDebugLayer;
        sm_GlobalContext.renderer = std::make_shared<Renderer>(renderParams);
        sm_GlobalContext.world = std::make_shared<World>();

        sm_GlobalContext.window->SetEventCallback([this](Event& event){
            EventDispatcher dispatcher{event};
            dispatcher.Dispatch<WindowCloseEvent>([this](auto& event) { m_Running = false; return true; });

            DSMEngine::sm_GlobalContext.renderer->OnEvent(event);
        });
    }

    void DSMEngine::ShutDownEngine()
    {
        DSMEngine::sm_GlobalContext = {};
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
        DSMEngine::sm_GlobalContext.window->Update();

        Render(deltaTime);
    }


    void DSMEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline)
    {
        DSMEngine::sm_GlobalContext.renderer->SetRenderPipeline(std::move(renderPipeline));
    }

    void DSMEngine::Render(float deltaTime)
    {
        DSMEngine::sm_GlobalContext.renderer->Render(deltaTime);
    }

    void DSMEngine::CalculateFPS()
    {
        static int frameCnt = 0;
        static float timeElapsed = 0.0f;
        static std::string originTitle = DSMEngine::sm_GlobalContext.window->GetTitle();

        frameCnt++;

        if ((m_Timer.TotalTime() - timeElapsed) >= 1.0f) {
            float fps = (float)frameCnt; // fps = frameCnt / 1
            float mspf = 1000.0f / fps;

            auto title = std::format("{}    FPS: {}    Frame Time: {} (ms)", originTitle, fps, mspf);
            DSMEngine::sm_GlobalContext.window->SetTitle(title);

            // Reset for next average.
            frameCnt = 0;
            timeElapsed += 1.0f;
        }
    }

} // namespace DSM