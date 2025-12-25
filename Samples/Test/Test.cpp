#include "Runtime/DSMEngine.h"
#include "Editor/DSMEditor.h"
#include "Runtime/Render/Renderer/Renderer.h"


class DSMRenderPipeline : public DSM::IRenderPipeline
{
public:
    void Render(DSM::Renderer& renderer, float deltaTime) override
    {
        if(!m_Initialized){
            Initialize(renderer);
        }
    }
    
    void RenderUI(DSM::Renderer& renderer) override
    {

    }

    void OnResize(DSM::Renderer& renderer, uint32_t width, uint32_t height) override
    {

    }

private:
    void Initialize(DSM::Renderer& renderer)
    {
        m_Initialized = true;
    }

private:
    bool m_Initialized = false;
};

int main()
{
    DSM::DSMEngine engine;
    DSM::EngineParameters params{};
    params.enableDebugLayer = false;
    engine.StartEngine(params);
    engine.SetRenderPipeline(std::make_unique<DSMRenderPipeline>());

    DSM::DSMEditor editor{};
    editor.StartEditor(&engine);
    editor.Run();
    editor.ShutDownEditor();

    engine.ShutDownEngine();

    return 0;
}