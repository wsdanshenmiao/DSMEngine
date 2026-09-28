#include "Runtime/Core/VirtualFileSystem/VirtualFileSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using DSM::MountPoint;
    using DSM::MountOwner;
    using DSM::VirtualFileSystem;
    using DSM::VirtualPath;

    bool Check(bool condition, std::string_view message)
    {
        if (!condition) {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        return true;
    }

    template <typename T>
    bool Check(const std::expected<T, DSM::PathError>& value, std::string_view message)
    {
        return Check(value.has_value(), message);
    }

    bool HasPath(const std::vector<VirtualPath>& paths, std::string_view value)
    {
        return std::ranges::any_of(paths, [value](const VirtualPath& path) {
            return path.ToString() == value;
        });
    }
}

int main()
{
    const auto root = std::filesystem::current_path() /
        "build/verification/virtual-path-2026-09-28/vfs-behavior";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    if (!Check(!error, "清理 VFS 行为测试目录失败")) {
        return 1;
    }

    const auto lowRoot = root / "low";
    const auto highRoot = root / "high";
    const auto engineRoot = root / "engine";
    std::filesystem::create_directories(lowRoot / "nested");
    std::filesystem::create_directories(highRoot / "nested");
    std::filesystem::create_directories(engineRoot);
    std::ofstream(lowRoot / "priority.txt") << "low";
    std::ofstream(highRoot / "priority.txt") << "high";
    std::ofstream(highRoot / "high-only.txt") << "high";
    std::ofstream(lowRoot / "nested" / "asset.txt") << "asset";
    std::ofstream(engineRoot / "readonly.txt") << "engine";

    VirtualFileSystem fileSystem;
    if (!Check(fileSystem.Mount(MountPoint{
            "/Game/", lowRoot, MountOwner::Project, 1, false, true}),
        "低优先级 Game 挂载失败")) {
        return 1;
    }
    if (!Check(fileSystem.Mount(MountPoint{
            "/Game/", highRoot, MountOwner::Package, 10, true, true}),
        "高优先级 Game 挂载失败")) {
        return 1;
    }
    if (!Check(fileSystem.Mount(MountPoint{
            "/Engine/", engineRoot, MountOwner::Engine, 100, true, true}),
        "Engine 挂载失败")) {
        return 1;
    }

    auto priority = VirtualPath::Parse("/Game/priority.txt");
    auto highOnly = VirtualPath::Parse("/Game/high-only.txt");
    auto nested = VirtualPath::Parse("/Game/nested/asset.txt");
    auto readonly = VirtualPath::Parse("/Engine/readonly.txt");
    if (!Check(priority && highOnly && nested && readonly, "合法虚拟路径解析失败")) {
        return 1;
    }

    const auto priorityPath = fileSystem.ResolveForRead(*priority);
    if (!Check(priorityPath && priorityPath->parent_path() == highRoot,
        "读路径没有选择最高优先级 Provider")) {
        return 1;
    }
    const auto provider = fileSystem.FindProvider(*priority);
    if (!Check(provider && provider->physicalRoot == highRoot,
        "FindProvider 没有返回实际 Provider")) {
        return 1;
    }
    if (!Check(fileSystem.ResolveForRead(*nested), "低优先级挂载中的资源无法读取")) {
        return 1;
    }

    const auto writable = VirtualPath::Parse("/Game/new/asset.txt");
    const auto writablePath = fileSystem.ResolveForWrite(*writable);
    if (!Check(writablePath && writablePath->parent_path() == lowRoot / "new",
        "写路径没有跳过只读 Provider 并选择可写 Provider")) {
        return 1;
    }
    if (!Check(!fileSystem.ResolveForWrite(*readonly), "只读 Engine 挂载被错误地判定为可写")) {
        return 1;
    }

    const auto entries = fileSystem.Enumerate("/Game/", {});
    if (!Check(HasPath(entries, "/Game/priority.txt") &&
               HasPath(entries, "/Game/high-only.txt") &&
               HasPath(entries, "/Game/nested"),
        "虚拟目录枚举结果不完整")) {
        return 1;
    }
    if (!Check(fileSystem.Enumerate("/Game/", "../").empty(),
        "目录枚举允许 .. 越界")) {
        return 1;
    }

    const auto reversePath = fileSystem.ToVirtualPath(highRoot / "high-only.txt");
    if (!Check(reversePath && reversePath->ToString() == "/Game/high-only.txt",
        "物理路径反向映射失败")) {
        return 1;
    }

    const std::vector<std::string> invalidPaths{
        "D:/outside/file.txt", "/Game/../outside.txt", "/Game//asset.txt",
        "/Game/./asset.txt", "/Game/asset.txt/", "/Game\\asset.txt", "/"};
    for (const auto& value : invalidPaths) {
        if (!Check(!VirtualPath::Parse(value), "非法虚拟路径被接受: " + value)) {
            return 1;
        }
    }

    const auto outsideRoot = root / "outside";
    std::filesystem::create_directories(outsideRoot);
    std::ofstream(outsideRoot / "secret.txt") << "secret";
    const auto link = lowRoot / "link-outside";
    std::filesystem::create_directory_symlink(outsideRoot, link, error);
    if (!error) {
        const auto escaped = VirtualPath::Parse("/Game/link-outside/secret.txt");
        if (!Check(escaped && !fileSystem.ResolveForRead(*escaped),
            "符号链接把虚拟读路径带出了挂载根")) {
            return 1;
        }
        if (!Check(!fileSystem.ResolveForWrite(
                VirtualPath::Parse("/Game/link-outside/new.txt").value()),
            "符号链接把虚拟写路径带出了挂载根")) {
            return 1;
        }
    }
    else {
        std::cout << "[SKIP] 当前权限不允许创建目录符号链接，未执行符号链接越界用例\n";
    }

    if (!Check(fileSystem.Unmount("/Engine/", engineRoot), "卸载 Engine 挂载失败") ||
        !Check(!fileSystem.ResolveForRead(*readonly), "卸载后仍能解析 Engine 资源")) {
        return 1;
    }

    std::cout << "VirtualFileSystem 行为测试通过\n";
    return 0;
}
