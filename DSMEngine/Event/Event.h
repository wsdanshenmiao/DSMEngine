#pragma once
#ifndef __EVENT_H__
#define __EVENT_H__

#include "Core/Core.h"
#include <string>
#include <format>
#include <concepts>

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

        bool m_Handled = false;
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event& event) : m_Event(event) {}

        template <typename T, typename Func> 
            requires std::is_base_of_v<Event, T> && 
                std::is_invocable_r_v<bool, Func, T&>
        bool Dispatch(const Func& func)
        {
            if(m_Event.GetEventType() == T::GetStaticType()){
                func(static_cast<T&>(m_Event));
                return true;
            }
            return false;
        }

    private:
        Event& m_Event;
    };


} // namespace DSM

inline std::ostream& operator<<(std::ostream& os, const DSM::Event& event)
{
    return os << event.ToString();
}

template<>
struct std::formatter<DSM::Event>
{
    template<typename Context>
    constexpr auto parse(Context& c) { return c.begin(); }
    template<typename Context>
    auto format(const DSM::Event& e, Context& c) const
    {
        return std::format_to(c.out(), "{}", e.ToString());
    }
};

#endif