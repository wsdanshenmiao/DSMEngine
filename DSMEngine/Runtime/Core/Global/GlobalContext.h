#pragma once
#ifndef __GLOBALCONTEXT_H__
#define __GLOBALCONTEXT_H__

#include <memory>

namespace DSM{
    class Window;
    class Renderer;
    class InputSystem;
    class LogSystem;
    class World;

    struct GlobalContext
    {
        std::shared_ptr<InputSystem> inputSystem;
        std::shared_ptr<LogSystem> loggerSystem;
        std::shared_ptr<Window> window;
        std::shared_ptr<Renderer> renderer;
        std::shared_ptr<World> world;

        void CreateContext();
        void ShutdownContext();
    };
    extern GlobalContext g_GlobalContext;
}

#endif