#include "DSMEngine.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Event/ApplicationEvent.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Core/InstrumentorMacro.h"

namespace DSM {

    void DSMEngine::StartEngine(const EngineParameters& params)
    {
        sm_GlobalContext.loggerSystem = std::make_shared<LogSystem>();
        sm_GlobalContext.window = std::make_shared<Window>(WindowProps{});
        sm_GlobalContext.inputSystem = std::make_shared<InputSystem>(sm_GlobalContext.window.get());
        RenderParameters renderParams{};
        renderParams.window = sm_GlobalContext.window.get();
        renderParams.enableDebugLayer = params.enableDebugLayer;
        renderParams.callback = params.graphicsMessageCallback;
        sm_GlobalContext.renderer = std::make_shared<GraphicsRenderer>(renderParams);
        sm_GlobalContext.scene = std::make_shared<Scene>();

        sm_GlobalContext.window->SetEventCallback([this](Event& event){
            EventDispatcher dispatcher{event};
            dispatcher.Dispatch<WindowCloseEvent>([this](auto& event) { m_Running = false; return true; });

            DSMEngine::sm_GlobalContext.renderer->OnEvent(event);
        });

        m_Timer.Reset();
    }

    void DSMEngine::ShutDownEngine()
    {
		// ��Ҫע����Դ�ͷŵ�˳��
        DSMEngine::sm_GlobalContext.scene = nullptr;
        DSMEngine::sm_GlobalContext.renderer = nullptr;
        DSMEngine::sm_GlobalContext.inputSystem = nullptr;
        DSMEngine::sm_GlobalContext.window = nullptr;
        DSMEngine::sm_GlobalContext.loggerSystem = nullptr;
    }

    void DSMEngine::Run()
    {
        while (m_Running) {
            Update();
        }
    }

    void DSMEngine::Update()
    {
        m_Timer.Tick();
        float deltaTime = m_Timer.DeltaTime();
        CalculateFPS();

        DSMEngine::sm_GlobalContext.window->Update();
        DSMEngine::sm_GlobalContext.scene->Update(deltaTime);

        Render(deltaTime);
    }


    void DSMEngine::SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline)
    {
        DSMEngine::sm_GlobalContext.renderer->SetRenderPipeline(std::move(renderPipeline));
    }

    void DSMEngine::ResetRenderPipeline()
    {
        DSMEngine::sm_GlobalContext.renderer->ResetRenderPipeline();
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
