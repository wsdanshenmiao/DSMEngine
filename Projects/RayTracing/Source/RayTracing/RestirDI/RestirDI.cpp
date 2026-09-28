#include "RestirDI.h"

#include "RestirDIRenderPipeline.h"
#include "RestirDIValidation.h"

#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/ContentPaths.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <windows.h>

namespace DSM::RestirDI {
namespace {

std::filesystem::path ExecutableDirectory()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

} // namespace

int Run(int argc, char** argv)
{
    const auto launchDirectory = std::filesystem::current_path();
    bool validateRender = false;
    bool validateEditor = false;
    std::filesystem::path requestedProject{};
    ValidationOptions validationOptions{};

    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const std::string_view argument = argv[argumentIndex];
        if (argument == "--validate-render") {
            validateRender = true;
        }
        else if (argument == "--validate-editor") {
            validateEditor = true;
        }
        else if (argument == "--project" && argumentIndex + 1 < argc) {
            requestedProject = argv[++argumentIndex];
        }
        else if (argument == "--output" && argumentIndex + 1 < argc) {
            validationOptions.outputDirectory = argv[++argumentIndex];
        }
        else if (argument == "--frames" && argumentIndex + 1 < argc) {
            validationOptions.editorFrameCount = static_cast<std::uint32_t>(
                std::stoul(argv[++argumentIndex]));
        }
    }

    if (!validationOptions.outputDirectory.empty() &&
        validationOptions.outputDirectory.is_relative()) {
        validationOptions.outputDirectory = std::filesystem::absolute(
            launchDirectory / validationOptions.outputDirectory);
    }
    // Editor 的字体等资源沿用项目相对路径；统一以部署目录作为运行目录，
    // 保证直接从仓库根调用 exe 与 xmake run 的行为一致。
    const auto executableDirectory = ExecutableDirectory();
    const auto projectDescriptor = requestedProject.empty()
        ? executableDirectory.parent_path().parent_path() / "RayTracing.dsmproj"
        : std::filesystem::absolute(launchDirectory / requestedProject);
    if (!std::filesystem::is_regular_file(projectDescriptor)) {
        std::fprintf(stderr, "RayTracing 项目描述文件不存在: %s\n", projectDescriptor.string().c_str());
        return 1;
    }
    ContentPaths::Initialize(projectDescriptor);
    ContentPaths::SetProjectRoot(projectDescriptor.parent_path());
    std::filesystem::current_path(executableDirectory);

    if (validateRender || validateEditor) {
        if (validationOptions.outputDirectory.empty()) {
            const auto timestamp = std::chrono::floor<std::chrono::seconds>(
                std::chrono::system_clock::now()).time_since_epoch().count();
            validationOptions.outputDirectory = launchDirectory /
                "build" / "verification" / "restir-di" /
                std::format("{}", timestamp) / "attempt-1";
        }
        if (validateRender) {
            return RunRenderValidation(validationOptions);
        }
        return RunEditorValidation(validationOptions);
    }

    DSMEngine engine;
    EngineParameters parameters{};
    parameters.enableDebugLayer = false;
    if (!engine.StartEngine(parameters)) {
        return 1;
    }
    engine.SetRenderPipeline(std::make_unique<RenderPipeline>());
    if (!Project::GetInstance().LoadProject(projectDescriptor.string())) {
        std::fprintf(stderr, "RayTracing 项目加载失败: %s\n", projectDescriptor.string().c_str());
        engine.ShutDownEngine();
        return 2;
    }

    DSMEditor editor;
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();
    return 0;
}

} // namespace DSM::RestirDI
