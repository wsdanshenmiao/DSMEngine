#pragma once
#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include "Core.h"

namespace DSM {
    class Application
    {
    public:
        Application();
        virtual ~Application();

        void Run();

    protected:

    };


    Application* CreateApplication();


} // namespace DSM 

#endif