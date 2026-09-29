#pragma once
#ifndef __CONTENT_PATHS_H__
#define __CONTENT_PATHS_H__

#include "Runtime/Core/VirtualFileSystem/VirtualFileSystem.h"

#include <filesystem>
#include <string_view>

namespace DSM::ContentPaths
{
    void Initialize(const std::filesystem::path& startPath = {});
    void SetProjectRoot(const std::filesystem::path& projectRoot);
    void ClearProjectRoot();

    const std::filesystem::path& GetRepositoryRoot() noexcept;
    const std::filesystem::path& GetProjectRoot() noexcept;
    std::filesystem::path GetEngineContentRoot();
    std::filesystem::path GetEngineShaderRoot();
    std::filesystem::path GetProjectContentRoot();
    std::filesystem::path GetProjectShaderRoot();

    VirtualFileSystem& GetFileSystem() noexcept;

    std::expected<std::filesystem::path, PathError>
        ResolveVirtualForRead(std::string_view virtualPath);
    std::expected<std::filesystem::path, PathError>
        ResolveVirtualForWrite(std::string_view virtualPath);
    std::expected<VirtualPath, PathError>
        ToVirtualPath(const std::filesystem::path& physicalPath);

    // 仅用于内部加载流程：虚拟路径走 VFS，绝对物理路径用于解析模型内部相对依赖。
    std::filesystem::path ResolveContent(const std::filesystem::path& path);
    std::filesystem::path ResolveShader(const std::filesystem::path& path);
    std::filesystem::path ResolveEngineContent(const std::filesystem::path& path);
}

#endif
