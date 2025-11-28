#pragma once
#ifndef __WINDOWUI_H__
#define __WINDOWUI_H__


#include "Runtime/Event/Event.h"

namespace DSM {
    struct WindowUI
    {
        virtual void Render() = 0;
        virtual void OnEvent(Event& event) = 0;
    };
} // namespace DSM

#endif