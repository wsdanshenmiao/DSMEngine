#pragma once
#ifndef __FUNCTION_WRAPPER_H__
#define __FUNCTION_WRAPPER_H__

#include <memory>

namespace DSM {

    class FunctionWrapper
    {
    private:
        struct ImplBase 
        {
            virtual void Call() = 0;
            virtual ~ImplBase() = default;
        };

        template <typename F>
        struct ImplType : ImplBase 
        {
            F f;
            ImplType(F&& func) : f(std::forward<F>(func)) {}
            void Call() override { f(); }
        };

    public:
        FunctionWrapper() = default;
        template <typename F>
        FunctionWrapper(F&& func)
            : m_Impl(std::make_unique<ImplType<F>>(std::forward<F>(func))) {}
        
        FunctionWrapper(FunctionWrapper&& other) noexcept
            : m_Impl(std::forward<std::unique_ptr<ImplBase>>(other.m_Impl)) {}
        FunctionWrapper& operator=(FunctionWrapper&& other) noexcept
        {
            m_Impl = std::forward<std::unique_ptr<ImplBase>>(other.m_Impl);
            return *this;
        }

        FunctionWrapper(const FunctionWrapper&) = delete;
        FunctionWrapper& operator=(const FunctionWrapper&) = delete;

        void operator()() { if(m_Impl != nullptr) m_Impl->Call(); }


    private:
        std::unique_ptr<ImplBase> m_Impl;
    };

}


#endif // __FUNCTION_WRAPPER_H__