#pragma once
#ifndef __INPUTSYSTEM_H__
#define __INPUTSYSTEM_H__

#include "KeyCodes.h"
#include "MouseCodes.h"
#include "Runtime/Math/MathCommon.h"

#include <optional>

namespace DSM {
    struct Window;

    // Runtime 只消费语义化输入；编辑器可提供自己的输入来源。
    struct CameraInputState
    {
        int forward = 0;
        int strafe = 0;
        bool rotate = false;
        float mouseDeltaX = 0.0f;
        float mouseDeltaY = 0.0f;
    };

    class InputSystem
    {
    public:
        explicit InputSystem(Window* window);

        bool IsKeyPressed(KeyCode keycode);
        bool IsMouseButtonPressed(MouseCode mouseCode);
        Math::Vector2 GetMousePosition();
        float GetMouseX();
        float GetMouseY();

        CameraInputState SampleCameraInput();
        void SetCameraInputOverride(const CameraInputState& state);
        void ClearCameraInputOverride();

    private:
        Window* m_Window{};
        std::optional<CameraInputState> m_CameraInputOverride{};
        Math::Vector2 m_LastMousePosition{};
        bool m_HasMousePosition = false;
    };

} // namespace DSM

#endif
