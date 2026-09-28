#include "VirtualFileSystem.h"

#include <algorithm>
#include <map>
#include <system_error>

namespace DSM
{
    namespace
    {
        PathError Error(PathErrorCode code, std::string message)
        {
            return PathError{code, std::move(message)};
        }

        std::filesystem::path NormalizeAbsolute(const std::filesystem::path& value)
        {
            std::error_code error;
            const auto absolute = std::filesystem::absolute(value, error);
            return (error ? value : absolute).lexically_normal();
        }

        std::filesystem::path NormalizeForContainment(const std::filesystem::path& value)
        {
            std::error_code error;
            const auto canonical = std::filesystem::weakly_canonical(value, error);
            return (error ? NormalizeAbsolute(value) : canonical).lexically_normal();
        }

        bool IsSafeRelativeDirectory(const std::filesystem::path& value)
        {
            if (value.empty()) {
                return true;
            }
            if (value.is_absolute()) {
                return false;
            }
            for (const auto& component : value) {
                const auto name = component.generic_string();
                if (name.empty() || name == "." || name == "..") {
                    return false;
                }
            }
            return true;
        }

        bool AllowsPath(const MountPoint& mount, const VirtualPath& path)
        {
            return mount.recursive || !path.relativePath.has_parent_path();
        }
    }

    void VirtualFileSystem::ClearMounts()
    {
        m_Mounts.clear();
    }

    std::expected<void, PathError> VirtualFileSystem::Mount(MountPoint mount)
    {
        auto normalizedRoot = NormalizeMountRoot(mount.virtualRoot);
        if (!normalizedRoot) {
            return std::unexpected(normalizedRoot.error());
        }
        if (mount.physicalRoot.empty()) {
            return std::unexpected(Error(PathErrorCode::InvalidFormat, "挂载物理根不能为空"));
        }

        mount.virtualRoot = *normalizedRoot;
        mount.physicalRoot = NormalizeAbsolute(mount.physicalRoot);
        for (const auto& existing : m_Mounts) {
            if (existing.virtualRoot == mount.virtualRoot && existing.physicalRoot == mount.physicalRoot) {
                return std::unexpected(Error(PathErrorCode::AlreadyMounted, "挂载点已存在: " + mount.virtualRoot));
            }
        }
        m_Mounts.push_back(std::move(mount));
        return {};
    }

    std::expected<void, PathError> VirtualFileSystem::Unmount(
        std::string_view virtualRoot,
        const std::filesystem::path& physicalRoot)
    {
        auto normalizedRoot = NormalizeMountRoot(virtualRoot);
        if (!normalizedRoot) {
            return std::unexpected(normalizedRoot.error());
        }
        const auto normalizedPhysicalRoot = NormalizeAbsolute(physicalRoot);
        const auto oldSize = m_Mounts.size();
        std::erase_if(m_Mounts, [&](const MountPoint& mount) {
            return mount.virtualRoot == *normalizedRoot &&
                mount.physicalRoot == normalizedPhysicalRoot;
        });
        if (m_Mounts.size() == oldSize) {
            return std::unexpected(Error(PathErrorCode::NotMounted,
                "挂载点不存在: " + *normalizedRoot));
        }
        return {};
    }

    bool VirtualFileSystem::IsWithin(
        const std::filesystem::path& root,
        const std::filesystem::path& candidate)
    {
        const auto normalizedRoot = NormalizeForContainment(root);
        const auto normalizedCandidate = NormalizeForContainment(candidate);
        if (normalizedRoot == normalizedCandidate) {
            return true;
        }
        const auto relative = normalizedCandidate.lexically_relative(normalizedRoot);
        if (relative.empty() || relative == ".") {
            return false;
        }
        const auto first = relative.begin();
        return first == relative.end() || first->generic_string() != "..";
    }

    std::vector<const MountPoint*> VirtualFileSystem::FindMounts(std::string_view virtualRoot) const
    {
        std::vector<const MountPoint*> result{};
        for (const auto& mount : m_Mounts) {
            if (mount.virtualRoot == virtualRoot) {
                result.push_back(&mount);
            }
        }
        std::ranges::sort(result, [](const MountPoint* lhs, const MountPoint* rhs) {
            if (lhs->priority != rhs->priority) {
                return lhs->priority > rhs->priority;
            }
            return lhs->physicalRoot.generic_string() < rhs->physicalRoot.generic_string();
        });
        return result;
    }

    std::expected<std::filesystem::path, PathError> VirtualFileSystem::ResolveForRead(const VirtualPath& path) const
    {
        const auto mounts = FindMounts(path.mount);
        for (const auto* mount : mounts) {
            if (!AllowsPath(*mount, path)) {
                continue;
            }
            const auto candidate = (mount->physicalRoot / path.relativePath).lexically_normal();
            if (!IsWithin(mount->physicalRoot, candidate)) {
                continue;
            }
            std::error_code error;
            if (std::filesystem::exists(candidate, error) && !error) {
                return candidate;
            }
        }
        return std::unexpected(Error(PathErrorCode::NotFound, "虚拟资源不存在: " + path.ToString()));
    }

    std::expected<std::filesystem::path, PathError> VirtualFileSystem::ResolveForWrite(const VirtualPath& path) const
    {
        const auto mounts = FindMounts(path.mount);
        for (const auto* mount : mounts) {
            if (mount->readOnly || !AllowsPath(*mount, path)) {
                continue;
            }
            const auto candidate = (mount->physicalRoot / path.relativePath).lexically_normal();
            if (IsWithin(mount->physicalRoot, candidate)) {
                return candidate;
            }
        }
        return std::unexpected(Error(PathErrorCode::ReadOnly,
            "虚拟资源没有可写挂载点: " + path.ToString()));
    }

    std::vector<VirtualPath> VirtualFileSystem::Enumerate(
        std::string_view virtualRoot,
        const std::filesystem::path& relativeDirectory) const
    {
        auto normalizedRoot = NormalizeMountRoot(virtualRoot);
        if (!normalizedRoot || !IsSafeRelativeDirectory(relativeDirectory)) {
            return {};
        }

        std::map<std::string, int, std::less<>> selectedPriority{};
        const auto mounts = FindMounts(*normalizedRoot);
        for (const auto* mount : mounts) {
            const auto directory = (mount->physicalRoot / relativeDirectory).lexically_normal();
            if (!IsWithin(mount->physicalRoot, directory)) {
                continue;
            }
            std::error_code error;
            if (!std::filesystem::is_directory(directory, error) || error) {
                continue;
            }
            std::filesystem::directory_iterator iterator(
                directory, std::filesystem::directory_options::skip_permission_denied, error);
            const std::filesystem::directory_iterator end{};
            for (; !error && iterator != end; iterator.increment(error)) {
                const auto name = iterator->path().filename().generic_string();
                if (!selectedPriority.contains(name) || selectedPriority[name] < mount->priority) {
                    selectedPriority[name] = mount->priority;
                }
            }
        }

        std::vector<VirtualPath> result{};
        for (const auto& [name, priority] : selectedPriority) {
            (void)priority;
            auto parsed = VirtualPath::Parse(
                *normalizedRoot + (relativeDirectory.empty() ? "" : relativeDirectory.generic_string() + "/") + name);
            if (parsed) {
                result.push_back(*parsed);
            }
        }
        return result;
    }

    std::optional<MountPoint> VirtualFileSystem::FindProvider(const VirtualPath& path) const
    {
        const auto mounts = FindMounts(path.mount);
        for (const auto* mount : mounts) {
            if (!AllowsPath(*mount, path)) {
                continue;
            }
            const auto candidate = (mount->physicalRoot / path.relativePath).lexically_normal();
            std::error_code error;
            if (IsWithin(mount->physicalRoot, candidate) &&
                std::filesystem::exists(candidate, error) && !error) {
                return *mount;
            }
        }
        return std::nullopt;
    }

    std::expected<VirtualPath, PathError> VirtualFileSystem::ToVirtualPath(const std::filesystem::path& physicalPath) const
    {
        const auto normalized = NormalizeForContainment(physicalPath);
        const MountPoint* selected = nullptr;
        std::filesystem::path selectedRelative{};
        for (const auto& mount : m_Mounts) {
            if (!IsWithin(mount.physicalRoot, normalized)) {
                continue;
            }
            const auto relative = normalized.lexically_relative(
                NormalizeForContainment(mount.physicalRoot));
            if (selected == nullptr || mount.physicalRoot.native().size() > selected->physicalRoot.native().size() ||
                (mount.physicalRoot == selected->physicalRoot && mount.priority > selected->priority)) {
                selected = &mount;
                selectedRelative = relative;
            }
        }
        if (selected == nullptr) {
            return std::unexpected(Error(PathErrorCode::OutsideMount,
                "物理路径不属于任何虚拟挂载点: " + normalized.string()));
        }
        auto parsed = VirtualPath::Parse(selected->virtualRoot + selectedRelative.generic_string());
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return *parsed;
    }

    bool VirtualFileSystem::Exists(const VirtualPath& path) const
    {
        return ResolveForRead(path).has_value();
    }
}
