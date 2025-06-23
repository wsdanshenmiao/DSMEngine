#pragma once
#ifndef __ENTERPINT_H__
#define __ENTERPINT_H__


#if defined(DSM_PLATFORM_WINDOWS)

extern DSM::Application* DSM::CreateApplication(); 

int main(int argc, char** argv)
{
    DSM::Log::Init();
    DSM_CORE_WARN("Initialized Log");
    auto app = DSM::CreateApplication();
    app->Run();
    delete app;
    return 0;
}

#endif

#endif