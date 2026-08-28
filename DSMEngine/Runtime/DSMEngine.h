#pragma once
#ifndef __DSMENGINE_H__
#define __DSMENGINE_H__

#include <unordered_set>
#include <memory>
#include "Runtime/Core/CpuTimer.h"

namespace DSM {
    class IRenderPipeline;
    class InputSystem;
    class LogSystem;
    class Window;
    class GraphicsRenderer;
    class Scene;
    struct IMessageCallback;

    struct EngineGlobalContext
    {
        std::shared_ptr<GraphicsRenderer> renderer;
        std::shared_ptr<InputSystem> inputSystem;
        std::shared_ptr<Window> window;
        std::shared_ptr<Scene> scene;
        std::shared_ptr<LogSystem> loggerSystem;
    };

    struct EngineParameters
    {
        bool enableDebugLayer = true;
        IMessageCallback* graphicsMessageCallback = nullptr;
    };

    class DSMEngine
    {
    public:
        void StartEngine(const EngineParameters& params);
        void ShutDownEngine();

        void Run();
        void Close() { m_Running = false; }
        void Update();

        void SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline);
        void ResetRenderPipeline();

        inline bool IsRunning() const { return m_Running; }

        const CpuTimer& GetTimer() const { return m_Timer; }

    protected:
        void Render(float deltaTime);

        void CalculateFPS();

    public:
        inline static EngineGlobalContext sm_GlobalContext{};

    protected:
        bool m_Running = true;
        CpuTimer m_Timer{};  
    };
}



#endif
