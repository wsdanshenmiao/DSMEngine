#pragma once
#ifndef __SCENEMANAGER_H__
#define __SCENEMANAGER_H__

#include "Runtime/Utils/Singleton.h"

#include <array>
#include <set>
#include <string>
#include <string_view>

namespace DSM {
    class Project : public Singleton<Project>
    {
    public:
        void NewProject();
        bool LoadProject(const std::string& filepath);
        void SaveProject(const std::string& filepath);

        void NewScene();
        void LoadScene(const std::string& filepath);
        void SaveScene(const std::string& filepath);

        bool IsProjectOpen() const { return !m_FilePath.empty(); }

        const std::string& GetProjectName() const { return m_Name; }
        const std::string& GetFilePath() const { return m_FilePath; }
        const std::string& GetSceneFilePath() const { return m_SceneFilePath; }
        void SetProjectName(const std::string& name) { m_Name = name; }
        void SetFilePath(const std::string& filepath) { m_FilePath = filepath; }
        void SetSceneFilePath(const std::string& sceneFilePath) { m_SceneFilePath = sceneFilePath; }

    private:

    public:
        inline static constexpr const char* s_SceneFileExtension = ".dsmscene";
        inline static constexpr const char* s_ProjectFileExtension = ".dsmproj";
        inline static constexpr const char* s_AssetsFolderName = "Assets";
        inline static constexpr const char* s_LibraryFolderName = "Library";
        inline static constexpr const char* s_ContentBrowserDragDropPayload = "CONTENT_BROWSER_ITEM";
        inline static const std::set<std::string_view> s_ModelFileExtensions = {
            ".3d",
            ".3ds",
            ".3mf",
            ".ac",
            ".ac3d",
            ".acc",
            ".amf",
            ".ase",
            ".ask",
            ".assbin",
            ".b3d",
            ".blend",
            ".bvh",
            ".cob",
            ".dae",
            ".dxf",
            ".enff",
            ".fbx",
            ".gltf",
            ".glb",
            ".hmp",
            ".ifc",
            ".iqm",
            ".irr",
            ".irrmesh",
            ".lwo",
            ".lws",
            ".lxo",
            ".md2",
            ".md3",
            ".md5anim",
            ".md5camera",
            ".md5mesh",
            ".mdc",
            ".mdl",
            ".mesh",
            ".mesh.xml",
            ".mot",
            ".ms3d",
            ".ndo",
            ".nff",
            ".obj",
            ".off",
            ".ogex",
            ".pk3",
            ".ply",
            ".pmx",
            ".prj",
            ".q3o",
            ".q3s",
            ".raw",
            ".scn",
            ".sib",
            ".smd",
            ".stl",
            ".ter",
            ".uc",
            ".vta",
            ".x",
            ".x3d",
            ".xgl",
            ".zgl",
        };

        inline static const std::set<std::string_view> s_TextureFileExtensions = {
            ".bmp",
            ".dds",
            ".gif",
            ".hdr",
            ".jpg",
            ".jpeg",
            ".pic",
            ".png",
            ".pnm",
            ".ppm",
            ".pgm",
            ".psd",
            ".tga",
        };

    private:
        std::string m_Name{};
        std::string m_FilePath{};
        std::string m_SceneFilePath{};
    };
}


#endif