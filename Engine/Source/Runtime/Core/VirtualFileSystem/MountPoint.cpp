#include "MountPoint.h"

namespace DSM
{
    std::expected<std::string, PathError> NormalizeMountRoot(std::string_view value)
    {
        auto parsed = VirtualPath::Parse(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (parsed->relativePath != std::filesystem::path{}) {
            return std::unexpected(PathError{
                PathErrorCode::InvalidMount,
                "挂载根不能包含相对路径: " + std::string(value)});
        }
        return parsed->mount;
    }
}
