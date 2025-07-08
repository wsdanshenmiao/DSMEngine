#pragma once
#ifndef __EVENT_H__
#define __EVENT_H__

#include "Core/Core.h"
#include <string>

namespace DSM{
    enum class EventType
    {
        None = 0,
        WindowClose, WindowResize,
        AppTick, AppUpdate, AppRender,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
        NumTypes
    };

    enum EventCategory
    {
        None = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4),
    };

    #define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }  \
        EventType GetEventType() const override { return GetStaticType(); } \
        const char* GetName() const override { return #type; }

    #define EVENT_CLASS_CATEGORY(category) int GetCategoryFlags() const override { return category; }

    class Event
    {
    public:
        virtual ~Event() = default;

        virtual EventType GetEventType() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual const char* GetName() const = 0;
        virtual std::string ToString() const { return GetName(); }

        inline bool IsInCategoty(EventCategory category) { return GetCategoryFlags() & category; }

    protected:
        bool m_Handled = false;
    };
    
} // namespace DSM


#endif