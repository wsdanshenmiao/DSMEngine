#pragma once 
#ifndef __INPUT_H__
#define __INPUT_H__

#include "KeyCodes.h"
#include "MouseCodes.h"
#include "Math/MathCommon.h"

namespace DSM::Input {

    bool IsKeyPressed(KeyCode keycode);
    bool IsMouseButtonPressed(MouseCode mouseCode);
    Math::Vector2 GetMousePosition();
    float GetMouseX();
    float GetMouseY();

} // namespace DSM 

#endif