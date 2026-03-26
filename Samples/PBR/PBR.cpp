#include "Editor/DSMEditor.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/Geometry.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/ScriptableObject.h"
#include "Runtime/Core/Input/InputSystem.h"
#include "Runtime/Framework/Component/Component.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Render/Renderer/ForwardRenderer/ForwardRenderPipeline.h"

#include <imgui.h>
#include <print>

using namespace DSM;

int main()
{
    Instrumentor::BeginSession("PBR Profiling");
    DSM::DSMEngine engine;
    DSM::EngineParameters params{};
    params.enableDebugLayer = false;
    engine.StartEngine(params);
    engine.SetRenderPipeline(std::make_unique<ForwardRenderPipeline>());

    DSM::DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();

    return 0;
}