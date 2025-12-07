#include "Runtime/DSMEngine.h"
#include "Editor/DSMEditor.h"


int main()
{
    DSM::DSMEngine engine;
    DSM::EngineParameters params{};
    params.enableDebugLayer = false;
    engine.StartEngine(params);

    DSM::DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();

    return 0;
}