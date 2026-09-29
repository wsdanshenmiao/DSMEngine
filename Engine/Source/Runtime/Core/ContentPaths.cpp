#include "ContentPaths.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace DSM::ContentPaths
{
    namespace
    {
        std::filesystem::path g_RepositoryRoot{};
        std::filesystem::path g_ProjectRoot{};
        std::filesystem::path g_EngineContentRoot{};
        std::filesystem::path g_EngineShaderRoot{};
        VirtualFileSystem g_FileSystem{};

        std::filesystem::path Normalize(const std::filesystem::path& path)
        {
            if (path.empty()) {
                return {};
            }
            std::error_code error;
            const auto absolute = std::filesystem::absolute(path, error);
            return (error ? path : absolute).lexically_normal();
        }

        std::filesystem::path FindRepositoryRoot(std::filesystem::path start)
        {
            if (start.empty()) {
                return {};
            }
            start = Normalize(start);
            if (std::filesystem::is_regular_file(start)) {
                start = start.parent_path();
            }
            for (auto current = start; !current.empty(); current = current.parent_path()) {
                if (std::filesystem::is_directory(current / "Engine") &&
                    std::filesystem::is_directory(current / "Projects")) {
                    return current;
                }
                if (current == current.parent_path()) {
                    break;
                }
            }
            return {};
        }

        std::filesystem::path ExecutablePath()
        {
#ifdef _WIN32
            std::wstring buffer(32768, L'\0');
            const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (size > 0 && size < buffer.size()) {
                buffer.resize(size);
                return std::filesystem::path(buffer);
            }
#endif
            return {};
        }

        std::filesystem::path ResolveEngineDescriptorRoot(const std::filesystem::path& relative)
        {
            if (relative.is_absolute()) {
                return Normalize(relative);
            }
            const auto candidate = (g_RepositoryRoot / "Engine" / relative).lexically_normal();
            const auto engineRoot = Normalize(g_RepositoryRoot / "Engine");
            const auto rel = candidate.lexically_relative(engineRoot);
            if (rel.empty() || rel.generic_string().find("..") == 0) {
                throw std::runtime_error("Engine descriptor 路径越出 Engine 根目录");
            }
            return candidate;
        }

        void LoadEngineDescriptor()
        {
            const auto descriptorPath = g_RepositoryRoot / "Engine" / "Engine.dsmengine.json";
            std::ifstream input(descriptorPath);
            if (!input) {
                throw std::runtime_error("Engine descriptor 不存在: " + descriptorPath.string());
            }
            nlohmann::json descriptor;
            input >> descriptor;
            if (descriptor.value("schemaVersion", 0) != 1 ||
                descriptor.value("name", std::string{}) != "DSMEngine") {
                throw std::runtime_error("Engine descriptor schema 或 name 无效: " + descriptorPath.string());
            }
            g_EngineContentRoot = ResolveEngineDescriptorRoot(
                descriptor.value("contentRoot", std::string{"Content"}));
            g_EngineShaderRoot = ResolveEngineDescriptorRoot(
                descriptor.value("shaderRoot", std::string{"Shaders"}));
        }

        void MountOrThrow(MountPoint mount)
        {
            auto result = g_FileSystem.Mount(std::move(mount));
            if (!result) {
                throw std::runtime_error(result.error().message);
            }
        }

        void MountEngine()
        {
            g_FileSystem.ClearMounts();
            MountOrThrow(MountPoint{
                "/Engine/", g_EngineContentRoot, MountOwner::Engine, 100, true, true});
            MountOrThrow(MountPoint{
                "/EngineShaders/", g_EngineShaderRoot, MountOwner::Engine, 100, true, true});
        }

        void MountProject()
        {
            if (g_ProjectRoot.empty()) {
                return;
            }
            MountOrThrow(MountPoint{
                "/Game/", g_ProjectRoot / "Content", MountOwner::Project, 300, false, true});
            MountOrThrow(MountPoint{
                "/GameShaders/", g_ProjectRoot / "Shaders", MountOwner::Project, 300, false, true});
        }

        void RebuildMounts()
        {
            MountEngine();
            MountProject();
        }
    }

    void Initialize(const std::filesystem::path& startPath)
    {
        const auto fromExplicit = FindRepositoryRoot(startPath);
        const auto fromExecutable = FindRepositoryRoot(ExecutablePath());
        const auto fromCwd = FindRepositoryRoot(std::filesystem::current_path());
        g_RepositoryRoot = !fromExplicit.empty() ? fromExplicit
            : !fromExecutable.empty() ? fromExecutable : fromCwd;
        if (g_RepositoryRoot.empty()) {
            throw std::runtime_error("无法从项目路径、可执行文件或当前目录定位 Engine/Projects 根目录");
        }
        LoadEngineDescriptor();
        RebuildMounts();
    }

    void SetProjectRoot(const std::filesystem::path& projectRoot)
    {
        g_ProjectRoot = Normalize(projectRoot);
        RebuildMounts();
    }

    void ClearProjectRoot()
    {
        g_ProjectRoot.clear();
        RebuildMounts();
    }

    const std::filesystem::path& GetRepositoryRoot() noexcept
    {
        return g_RepositoryRoot;
    }

    const std::filesystem::path& GetProjectRoot() noexcept
    {
        return g_ProjectRoot;
    }

    std::filesystem::path GetEngineContentRoot()
    {
        return g_EngineContentRoot;
    }

    std::filesystem::path GetEngineShaderRoot()
    {
        return g_EngineShaderRoot;
    }

    std::filesystem::path GetProjectContentRoot()
    {
        return g_ProjectRoot.empty() ? std::filesystem::path{} : g_ProjectRoot / "Content";
    }

    std::filesystem::path GetProjectShaderRoot()
    {
        return g_ProjectRoot.empty() ? std::filesystem::path{} : g_ProjectRoot / "Shaders";
    }

    VirtualFileSystem& GetFileSystem() noexcept
    {
        return g_FileSystem;
    }

    std::expected<std::filesystem::path, PathError> ResolveVirtualForRead(std::string_view virtualPath)
    {
        auto parsed = VirtualPath::Parse(virtualPath);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return g_FileSystem.ResolveForRead(*parsed);
    }

    std::expected<std::filesystem::path, PathError> ResolveVirtualForWrite(std::string_view virtualPath)
    {
        auto parsed = VirtualPath::Parse(virtualPath);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return g_FileSystem.ResolveForWrite(*parsed);
    }

    std::expected<VirtualPath, PathError>
    ToVirtualPath(const std::filesystem::path& physicalPath)
    {
        return g_FileSystem.ToVirtualPath(physicalPath);
    }

    std::filesystem::path ResolveContent(const std::filesystem::path& path)
    {
        if (path.empty()) {
            return {};
        }
        const auto generic = path.generic_string();
        if (generic.starts_with("/")) {
            auto resolved = ResolveVirtualForRead(generic);
            return resolved ? *resolved : std::filesystem::path{};
        }
        if (path.is_absolute()) {
            auto virtualPath = ToVirtualPath(path);
            if (!virtualPath) {
                return {};
            }
            auto resolved = ResolveVirtualForRead(virtualPath->ToString());
            return resolved ? *resolved : std::filesystem::path{};
        }
        auto resolved = ResolveVirtualForRead("/Game/" + generic);
        return resolved ? *resolved : std::filesystem::path{};
    }

    std::filesystem::path ResolveShader(const std::filesystem::path& path)
    {
        if (path.empty()) {
            return {};
        }
        const auto generic = path.generic_string();
        if (generic.starts_with("/")) {
            auto resolved = ResolveVirtualForRead(generic);
            return resolved ? *resolved : std::filesystem::path{};
        }
        if (path.is_absolute()) {
            auto virtualPath = ToVirtualPath(path);
            if (!virtualPath) {
                return {};
            }
            auto resolved = ResolveVirtualForRead(virtualPath->ToString());
            return resolved ? *resolved : std::filesystem::path{};
        }
        auto resolved = ResolveVirtualForRead("/EngineShaders/" + generic);
        return resolved ? *resolved : std::filesystem::path{};
    }

    std::filesystem::path ResolveEngineContent(const std::filesystem::path& path)
    {
        if (path.empty()) {
            return {};
        }
        const auto generic = path.generic_string();
        const auto virtualPath = generic.starts_with("/")
            ? generic : "/Engine/" + generic;
        auto resolved = ResolveVirtualForRead(virtualPath);
        if (resolved) {
            return *resolved;
        }
        if (path.is_absolute()) {
            auto mapped = ToVirtualPath(path);
            if (mapped && mapped->IsEnginePath()) {
                return path.lexically_normal();
            }
        }
        return {};
    }}
