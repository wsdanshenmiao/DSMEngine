#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Core/ContentPaths.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Render/Renderer/ForwardRenderer/ForwardRenderPipeline.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string_view>

using namespace DSM;

int main(int argc, char** argv)
{
    std::filesystem::path projectPath{};
    bool validateProject = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--project" && index + 1 < argc) {
            projectPath = argv[++index];
        }
        else if (argument == "--validate-project") {
            validateProject = true;
        }
        else {
            std::fprintf(stderr, "PBR 参数无效: %s\n", argv[index]);
            return 2;
        }
    }
    if (validateProject && projectPath.empty()) {
        std::fprintf(stderr, "--validate-project 需要 --project <file.dsmproj>\n");
        return 2;
    }

    Instrumentor::BeginSession("PBR Profiling");
    DSMEngine engine;
    EngineParameters params{};
    params.enableDebugLayer = false;
    if (!engine.StartEngine(params)) {
        return 1;
    }
    engine.SetRenderPipeline(std::make_unique<ForwardRenderPipeline>());

    if (!projectPath.empty() && !Project::GetInstance().LoadProject(projectPath.string())) {
        std::fprintf(stderr, "PBR 项目加载失败: %s\n", projectPath.string().c_str());
        engine.ShutDownEngine();
        return 3;
    }
    if (validateProject) {
        const auto& scene = DSMEngine::sm_GlobalContext.scene;
        const size_t objectCount = scene != nullptr ? scene->GetAllObjects().size() : 0;
        size_t loadedMeshCount = 0;
        if (scene != nullptr) {
            for (const auto [entity, renderer] : scene->GetObjectsWithComponents<MeshRenderer>().each()) {
                if (renderer.GetModel() != nullptr && renderer.GetMesh() != nullptr) {
                    ++loadedMeshCount;
                }
            }
        }
        const bool cleanScene = scene != nullptr && !scene->IsDirty();
        const auto sceneVirtual = ContentPaths::ResolveVirtualForRead("/Game/Scenes/Sponza.dsmscene");
        const auto engineSkybox = ContentPaths::ResolveVirtualForRead("/Engine/Textures/DefaultSkybox/daylight0.png");
        const auto projectWrite = ContentPaths::ResolveVirtualForWrite("/Game/Generated/virtual-path-check.tmp");
        const auto engineWrite = ContentPaths::ResolveVirtualForWrite("/Engine/Generated/virtual-path-check.tmp");
        const bool virtualPathPassed = sceneVirtual.has_value() && engineSkybox.has_value() &&
            projectWrite.has_value() && !engineWrite.has_value();
        std::fprintf(stdout, "PBR 项目验证：对象=%zu，有效网格=%zu，加载后未标脏=%s，虚拟路径=%s，文件=%s\n",
            objectCount, loadedMeshCount, cleanScene ? "是" : "否",
            virtualPathPassed ? "通过" : "失败", projectPath.string().c_str());
        engine.ShutDownEngine();
        return objectCount > 0 && loadedMeshCount > 0 && cleanScene && virtualPathPassed ? 0 : 4;
    }

    DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();
    engine.ShutDownEngine();
    return 0;
}
