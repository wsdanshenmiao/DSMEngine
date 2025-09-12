#pragma once 
#ifndef __INPUTSYSTEM_H__
#define __INPUTSYSTEM_H__

#include "KeyCodes.h"
#include "MouseCodes.h"
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    struct Window;

    class InputSystem
    {
    public:
        InputSystem(Window* window);

        bool IsKeyPressed(KeyCode keycode);
        bool IsMouseButtonPressed(MouseCode mouseCode);
        Math::Vector2 GetMousePosition();
        float GetMouseX();
        float GetMouseY();

    private:
        Window* m_Window{};
    };

} // namespace DSM 

#endif