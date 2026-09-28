#pragma once
#ifndef __TEXTUREMANAGER__H__
#define __TEXTUREMANAGER__H__

#include "Runtime/Graphics/Device.h"

namespace DSM::TextureManager {
    enum DefaultTexture
    {
        kMagenta2D,
        kBlackOpaque2D,
        kWhiteOpaque2D,
        kBlackTransparent2D,
        kWhiteTransparent2D,
        kDefaultNormalTex,
        kBlackCubeTex,

        kNumDefaultTexture
    };

	void Init(IDevice* device);
	void Destroy();

    TextureHandle GetDefaultTexture(DefaultTexture texID);
	
	TextureHandle LoadTextureFromFile(const std::string& fileName, bool forceSRGB = false);
	TextureHandle LoadTextureFromMemory(const std::string& name, const TextureDesc& texDesc, const void* data);
	bool DestroyTexture(const std::string& name);
}

#endif