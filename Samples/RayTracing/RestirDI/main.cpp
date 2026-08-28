#include "RestirDIRenderPipeline.h"
#include "RestirDIValidation.h"

#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"

#include <memory>
#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

using namespace DSM;

int main(int argc, char** argv)
{
    bool validateRender = false;
    bool validateEditor = false;
    RestirDI::ValidationOptions validationOptions{};
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const std::string_view argument = argv[argumentIndex];
        if (argument == "--validate-render") validateRender = true;
        else if (argument == "--validate-editor") validateEditor = true;
        else if (argument == "--output" && argumentIndex + 1 < argc) {
            validationOptions.outputDirectory = argv[++argumentIndex];
        }
        else if (argument == "--frames" && argumentIndex + 1 < argc) {
            validationOptions.editorFrameCount = static_cast<uint32_t>(std::stoul(argv[++argumentIndex]));
        }
    }
    if (validateRender || validateEditor) {
        if (validationOptions.outputDirectory.empty()) {
            const auto timestamp = std::chrono::floor<std::chrono::seconds>(
                std::chrono::system_clock::now()).time_since_epoch().count();
            validationOptions.outputDirectory = std::filesystem::current_path() /
                "build" / "verification" / "restir-di" /
                std::format("{}", timestamp) / "attempt-1";
        }
        if (validateRender) return RestirDI::RunRenderValidation(validationOptions);
        return RestirDI::RunEditorValidation(validationOptions);
    }

    DSMEngine engine;
    EngineParameters parameters{};
    parameters.enableDebugLayer = false;
    engine.StartEngine(parameters);
    engine.SetRenderPipeline(std::make_unique<RestirDI::RenderPipeline>());

    DSMEditor editor;
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();
    return 0;
}
