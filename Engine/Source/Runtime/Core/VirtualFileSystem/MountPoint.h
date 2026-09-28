#pragma once
#ifndef __DSM_MOUNT_POINT_H__
#define __DSM_MOUNT_POINT_H__

#include "VirtualPath.h"

namespace DSM
{
    enum class MountOwner
    {
        Engine,
        Project,
        Plugin,
        Package,
    };

    struct MountPoint
    {
        std::string virtualRoot{};
        std::filesystem::path physicalRoot{};
        MountOwner owner = MountOwner::Project;
        int priority = 0;
        bool readOnly = false;
        bool recursive = true;
    };

    std::expected<std::string, PathError> NormalizeMountRoot(std::string_view value);
}

#endif
