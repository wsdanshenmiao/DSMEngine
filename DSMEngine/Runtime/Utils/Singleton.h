#pragma once
#ifndef __SINGLETON_H__
#define __SINGLETON_H__


namespace DSM {
    template<typename T>
    class Singleton
    {
    public:
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;

        static T& GetInstance()
        {
            static T instance;
            return instance;
        }

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;
    };
}


#endif