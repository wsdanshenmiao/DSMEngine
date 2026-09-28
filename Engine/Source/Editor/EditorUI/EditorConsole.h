#pragma once
#ifndef __EDITORCONSOLE_H__
#define __EDITORCONSOLE_H__

#include "Widget.h"
#include "Runtime/Core/LogSystem.h"
#include "Runtime/Graphics/Texture.h"

#include <deque>

namespace DSM {

    class EditorConsole : public Widget
    {
    public:
        EditorConsole(EditorUI* editorUI);
        ~EditorConsole();

        void OnGUIEnabled() override;

    private:
        static void Log(LogSystem::LogLevel level, const std::string& text);

    private:
        TextureHandle m_Icon;
    };
}

#endif