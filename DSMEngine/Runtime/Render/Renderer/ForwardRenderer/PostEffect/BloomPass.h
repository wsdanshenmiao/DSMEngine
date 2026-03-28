#pragma once
#ifndef __BLOOMPASS_H__
#define __BLOOMPASS_H__

#include "PostEffectManager.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Runtime/Math/MathCommon.h"

#include <algorithm>
#include <vector>

namespace DSM{
    class BloomPass : public IPostEffect
    {
    public:
        BloomPass()
        {
        }

        void Render(Renderer& renderer, float deltaTime, ITexture* srcTex, ITexture* dstTex) override
        {
            auto device = renderer.GetDevice();
            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("BloomPass Command List"));
            cmdList->Open();


            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

    private:
    };
}


#endif // __BLOOMPASS_H__