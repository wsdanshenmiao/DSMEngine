#pragma once
#ifndef __KEYEVENT_H__
#define __KEYEVENT_H__

#include "Event.h"
#include "Runtime/Core/Input/KeyCodes.h"
#include <format>

namespace DSM {
    class KeyEvent : public Event
    {
    public:
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryKeyboard)

        KeyCode GetKeyCode() const noexcept { return m_KeyCode; }

    protected:
        KeyEvent(KeyCode code) : m_KeyCode(code) {}

        KeyCode m_KeyCode;
    };

    class KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(KeyCode code, bool isRepeat = false)
            : KeyEvent(code), m_IsRepeat(isRepeat) {}

        EVENT_CLASS_TYPE(KeyPressed)

        bool IsRepeat() const noexcept { return m_IsRepeat; }

        std::string ToString() const override
        {
            return std::format("KeyPressedEvent: {}, (Repeat = {})", m_KeyCode, m_IsRepeat);
        }

    private:
        bool m_IsRepeat;
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(KeyCode code)
            : KeyEvent(code) {}

        EVENT_CLASS_TYPE(KeyReleased)

        std::string ToString() const override
        {
            return std::format("KeyReleasedEvent: {}", m_KeyCode);
        }
    };

    class KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(KeyCode code)
            : KeyEvent(code) {}

        EVENT_CLASS_TYPE(KeyTyped)

        std::string ToString() const override
        {
            return std::format("KeyTypedEvent: {}", m_KeyCode);
        }
    };


} // namespace DSM 

#endif