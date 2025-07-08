#pragma once
#ifndef __MOUSEBUTTONEVENT_H__
#define __MOUSEBUTTONEVENT_H__

#include "Event.h"
#include "Core/MouseCodes.h"


namespace DSM {
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(float x, float y) noexcept
            : m_MouseX(x), m_MouseY(y) {}

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

        float GetX() const noexcept { return m_MouseX; }
        float GetY() const noexcept { return m_MouseY; }

    private:
        float m_MouseX, m_MouseY;
    };


    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(float xOffset, float yOffset) noexcept
            : m_XOffset(xOffset), m_YOffset(yOffset) {}

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

        float GetXOffset() const noexcept { return m_XOffset; }
        float GetYOffset() const noexcept { return m_YOffset; }
    
    private:
        float m_XOffset, m_YOffset;
    };

    class MouseButtonEvent : public Event
    {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
    
        MouseCode GetMouseCode() const noexcept { return m_MouseCode; }

    protected:
        MouseButtonEvent(MouseCode mouseCode) noexcept
            : m_MouseCode(mouseCode) {}

        MouseCode m_MouseCode;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(MouseCode mouseCode) noexcept
            : MouseButtonEvent(mouseCode) {}

        EVENT_CLASS_TYPE(MouseButtonPressed)

        std::string ToString() const override
        {
            return "MouseButtonPressedEvent: " + std::to_string(static_cast<uint16_t>(m_MouseCode));
        }
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(MouseCode mouseCode) noexcept
            : MouseButtonEvent(mouseCode){}

        EVENT_CLASS_TYPE(MouseButtonReleased)

        std::string ToString() const override
        {
            return "MouseButtonReleasedEvent: " + std::to_string(static_cast<uint16_t>(m_MouseCode));
        }
    };


} // namespace DSM 


#endif