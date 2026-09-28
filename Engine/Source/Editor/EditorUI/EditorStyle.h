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
        enum StyleType
        {
            Classic = 0,
            Light,
            Dark,
            ImGuiClassic,
            ImGuiLight,
            ImGuiDark,
            StyleTypeCount
        };

        EditorStyle(EditorUI* editorUI);
        void OnGUIEnabled() override;
        StyleType GetStyleType() const { return m_StyleType; }
        void UpdateStyle();


    public:
        inline static ImVec4 sm_ColorInfo{235.0f / 255.0f, 235.0f / 255.0f, 235.0f / 255.0f, 1.0f};
        inline static ImVec4 sm_ColorWarning{255.0f / 255.0f, 149.0f / 255.0f, 49.0f / 255.0f, 1.0f};
        inline static ImVec4 sm_ColorError{255.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f};

    private:
        StyleType m_StyleType = StyleType::Classic;
        ImGuiStyle m_OriginalStyle{};
        bool m_Changes = false;
    };

} // namespace DSM


#endif // __EDITOR_STYLE_H__
