#include "IRenderPass.h"

namespace DSM{

    RenderResource g_RenderResources;
    
    RenderResource::RenderResource()
    {
        shaders.resize(static_cast<uint32_t>(ShaderSlot::Count));
    }
}