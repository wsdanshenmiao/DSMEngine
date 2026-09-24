#include "RestirDI.h"

#include "RestirDIRenderPipeline.h"
#include "RestirDIValidation.h"

#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"

#include <chrono>
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
    ValidationOptions validationOptions{};

    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const std::string_view argument = argv[argumentIndex];
        if (argument == "--validate-render") {
            validateRender = true;
        }
        else if (argument == "--validate-editor") {
            validateEditor = true;
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
    std::filesystem::current_path(ExecutableDirectory());

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

    DSMEditor editor;
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();
    return 0;
}

} // namespace DSM::RestirDI
