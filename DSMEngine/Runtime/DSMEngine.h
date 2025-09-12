#pragma once
#ifndef __DSMENGINE_H__
#define __DSMENGINE_H__

#include <unordered_set>
#include <memory>
#include "Runtime/Core/CpuTimer.h"

namespace DSM {
    class IRenderPipeline;

    class DSMEngine
    {
    public:
        void StartEngine();
        void ShutDownEngine();

        void Run();
        void Update();

        void SetRenderPipeline(std::unique_ptr<IRenderPipeline> renderPipeline);

        inline bool IsRunning() const { return m_Running; }

        const CpuTimer& GetTimer() const { return m_Timer; }

    protected:
        void Render(float deltaTime);

        void CalculateFPS();

    protected:
        bool m_Running = true;
        CpuTimer m_Timer{};  
    };
}



#endif