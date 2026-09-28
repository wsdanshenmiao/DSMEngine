#pragma once
#ifndef __DSM_VIRTUAL_FILE_SYSTEM_H__
#define __DSM_VIRTUAL_FILE_SYSTEM_H__

#include "MountPoint.h"

#include <optional>
#include <vector>

namespace DSM
{
    class VirtualFileSystem
    {
    public:
        void ClearMounts();

        std::expected<void, PathError> Mount(MountPoint mount);
        std::expected<void, PathError> Unmount(
            std::string_view virtualRoot,
            const std::filesystem::path& physicalRoot);

        std::expected<std::filesystem::path, PathError> ResolveForRead(const VirtualPath& path) const;

        std::expected<std::filesystem::path, PathError> ResolveForWrite(const VirtualPath& path) const;

        std::vector<VirtualPath> Enumerate(
            std::string_view virtualRoot,
            const std::filesystem::path& relativeDirectory) const;

        std::optional<MountPoint> FindProvider(const VirtualPath& path) const;
        std::expected<VirtualPath, PathError> ToVirtualPath(const std::filesystem::path& physicalPath) const;

        [[nodiscard]] bool Exists(const VirtualPath& path) const;
        [[nodiscard]] const std::vector<MountPoint>& GetMounts() const noexcept { return m_Mounts; }

    private:
        static bool IsWithin(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate);

        std::vector<const MountPoint*> FindMounts(std::string_view virtualRoot) const;

    private:
        std::vector<MountPoint> m_Mounts{};
    };
}

#endif
