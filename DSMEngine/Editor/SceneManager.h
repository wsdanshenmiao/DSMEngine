#pragma once
#ifndef __SCENEMANAGER_H__
#define __SCENEMANAGER_H__

#include <string>
#include "Runtime/Utils/Singleton.h"

namespace DSM {
    class SceneManager
    {
    public:
        static void NewScene();
        static void LoadScene(const std::string& filepath);
        static void SaveScene(const std::string& filepath);
    };
}


#endif