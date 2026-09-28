#include "VirtualPath.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace DSM
{
    namespace
    {
        PathError Error(PathErrorCode code, std::string message)
        {
            return PathError{code, std::move(message)};
        }
    }

    std::expected<VirtualPath, PathError> VirtualPath::Parse(std::string_view value)
    {
        if (value.size() < 2 || value.front() != '/' ||
            value.find('\\') != std::string_view::npos || value.find(':') != std::string_view::npos) {
            return std::unexpected(Error(PathErrorCode::InvalidFormat,
                std::format("无效虚拟路径，必须使用 /Root/Path 格式: {}", value)));
        }

        const size_t rootEnd = value.find('/', 1);
        const std::string_view rootName = value.substr(1, rootEnd == std::string_view::npos
            ? value.size() - 1 : rootEnd - 1);
        if (rootName.empty() || !std::ranges::all_of(rootName, [](unsigned char c) {
                return std::isalnum(c) != 0 || c == '_' || c == '-'; }))
        {
            return std::unexpected(Error(PathErrorCode::InvalidMount, std::format("虚拟挂载根无效: {}", value)));
        }

        const std::string mount = "/" + std::string(rootName) + "/";
        const std::string_view relative = rootEnd == std::string_view::npos
            ? std::string_view{} : value.substr(rootEnd + 1);
        if (!relative.empty()) {
            if (relative.back() == '/') {
                return std::unexpected(Error(PathErrorCode::InvalidFormat,
                    std::format("虚拟资源路径不能以 / 结尾: {}", value)));
            }
            size_t begin = 0;
            while (begin < relative.size()) {
                const size_t end = relative.find('/', begin);
                const auto part = relative.substr(begin, end == std::string_view::npos
                    ? relative.size() - begin : end - begin);
                if (part.empty()) {
                    return std::unexpected(Error(PathErrorCode::InvalidFormat,
                        std::format("虚拟路径包含空路径段: {}", value)));
                }
                if (part == "." || part == "..") {
                    return std::unexpected(Error(PathErrorCode::Traversal,
                        std::format("虚拟路径禁止 . 和 ..: {}", value)));
                }
                if (end == std::string_view::npos) {
                    break;
                }
                begin = end + 1;
            }
        }
        return VirtualPath{mount, std::filesystem::path(relative)};
    }

    std::string VirtualPath::ToString() const
    {
        return mount + relativePath.generic_string();
    }

    bool VirtualPath::IsEnginePath() const noexcept
    {
        return mount == s_EngineMount;
    }   

    bool VirtualPath::IsProjectPath() const noexcept
    {
        return mount == s_ProjectMount;
    }

    bool VirtualPath::IsShaderPath() const noexcept
    {
        return mount == s_GameShaderMount || mount == s_EngineShaderMount;
    }
}
