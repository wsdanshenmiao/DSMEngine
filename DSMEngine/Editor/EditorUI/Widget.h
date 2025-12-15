#ifndef __WIDGET_H__
#define __WIDGET_H__

#include <string>

#include "Runtime/Math/MathCommon.h"

// 部件的默认参数
constexpr float c_DefaultWidgetValue = -1.0f;

struct ImGuiWindow;

namespace DSM {
    class Widget 
    {
    public:
        Widget();
        virtual ~Widget() = default;

        void OnGUI();
        virtual void OnGUIDefault() {}
        virtual void OnGUIEnabled() {}

        virtual void OnEnable() {}
        virtual void OnDisable() {}

        const char* GetTitle() const noexcept { return m_Title; }
        ImGuiWindow* GetWindow() const;

    protected:
        bool m_Enabled = true;
        int m_Flags;
        float m_Alpha = c_DefaultWidgetValue;
        Math::Vector2 m_Size = c_DefaultWidgetValue;
        Math::Vector2 m_MinSize = c_DefaultWidgetValue;
        Math::Vector2 m_MaxSize = std::numeric_limits<float>::max();
        Math::Vector2 m_Padding = c_DefaultWidgetValue;

        const char* m_Title;
    };
}

#endif // __WIDGET_H__