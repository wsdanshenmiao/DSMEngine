#pragma once
#ifndef __EDITOR_STYLE_H__
#define __EDITOR_STYLE_H__

#include <imgui.h>
#include "Editor/EditorUI/Widget.h"

struct ImVec4;

namespace DSM {

    class EditorStyle : public Widget 
    {
    public:
        EditorStyle(EditorUI* editorUI);

        void OnGUIEnabled() override;

    public:
        inline static ImVec4 sm_ColorInfo{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
        inline static ImVec4 sm_ColorWarning{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
        inline static ImVec4 sm_ColorError{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

    private:
        int m_StyleType = -1;
        bool m_Changes = false;
    };

} // namespace DSM


#endif // __EDITOR_STYLE_H__
