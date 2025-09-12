#include "GlobalContext.h"
#include "Runtime/Core/Window.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Core/Input/InputSystem.h"

namespace DSM{
    GlobalContext g_GlobalContext{};

    void GlobalContext::CreateContext()
    {
        loggerSystem = std::make_shared<LogSystem>();
        window = std::make_shared<Window>(WindowProps{});
        inputSystem = std::make_shared<InputSystem>(window.get());
        RenderParameters renderParams{};
        renderParams.window = window.get();
        renderer = std::make_shared<Renderer>(renderParams);
    }

    void GlobalContext::ShutdownContext()
    {
        renderer.reset();
        inputSystem.reset();
        window.reset();
        loggerSystem.reset();
    }

}