#define STB_IMAGE_IMPLEMENTATION

#include "TextureManager.h"
#include "stb_image.h"
#include "Runtime/Core/PlatformDetection.h"
#include <unordered_map>

#if defined(DSM_PLATFORM_WINDOWS)
#include "DDSTextureLoader12.h"
#include "Runtime/Graphics/D3D12/D3D12Common.h"
using namespace DirectX;
#endif

namespace DSM::TextureManager {
	DeviceHandle s_GraphicsDevice;
	std::unordered_map<std::string, std::pair<bool, TextureHandle>> s_Textures;
	std::array<TextureHandle, kNumDefaultTexture> s_DefaultTextures;
	std::mutex s_Mutex;

	void Init(IDevice* device)
	{
		assert(device != nullptr);
		s_GraphicsDevice = device;

		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init Default Textures"));
		cmdList->Open();

        // 创建默认纹理
        TextureDesc texDesc{};
		texDesc.format = Format::RGBA8_UNORM;
		texDesc.initialState = ResourceStates::PixelShaderResource;
		texDesc.debugName = "Magenta2D";
        uint32_t MagentaPixel = 0xFFFF00FF;
		s_DefaultTextures[kMagenta2D] = s_GraphicsDevice->CreateTexture(texDesc);
		cmdList->WriteTexture(s_DefaultTextures[kMagenta2D], 0, 0, &MagentaPixel, 4);

		texDesc.debugName = "BlackOpaque2D";
        uint32_t BlackOpaqueTexel = 0xFF000000;
        s_DefaultTextures[kBlackOpaque2D] = s_GraphicsDevice->CreateTexture(texDesc);
        cmdList->WriteTexture(s_DefaultTextures[kBlackOpaque2D], 0, 0, &BlackOpaqueTexel, 4);

		texDesc.debugName = "BlackTransparent2D";
        uint32_t BlackTransparentTexel = 0x00000000;
		s_DefaultTextures[kBlackTransparent2D] = s_GraphicsDevice->CreateTexture(texDesc);
		cmdList->WriteTexture(s_DefaultTextures[kBlackTransparent2D], 0, 0, &BlackTransparentTexel, 4);

		texDesc.debugName = "WhiteOpaque2D";
		uint32_t WhiteOpaqueTexel = 0xFFFFFFFF;
        s_DefaultTextures[kWhiteOpaque2D] = s_GraphicsDevice->CreateTexture(texDesc);
        cmdList->WriteTexture(s_DefaultTextures[kWhiteOpaque2D], 0, 0, &WhiteOpaqueTexel, 4);

		texDesc.debugName = "WhiteTransparent2D";
        uint32_t WhiteTransparentTexel = 0x00FFFFFF;
        s_DefaultTextures[kWhiteTransparent2D] = s_GraphicsDevice->CreateTexture(texDesc);
        cmdList->WriteTexture(s_DefaultTextures[kWhiteTransparent2D], 0, 0, &WhiteTransparentTexel, 4);

		texDesc.debugName = "DefaultNormalTex";
		uint32_t FlatNormalTexel = 0x00FF8080;
        s_DefaultTextures[kDefaultNormalTex] = s_GraphicsDevice->CreateTexture(texDesc);
        cmdList->WriteTexture(s_DefaultTextures[kDefaultNormalTex], 0, 0, &FlatNormalTexel, 4);

		texDesc.debugName = "BlackCubeTex";
		uint32_t BlackCubeTexels[6] = {};
        texDesc.arraySize = 6;
		texDesc.dimension = TextureDimension::TextureCube;
        s_DefaultTextures[kBlackCubeTex] = s_GraphicsDevice->CreateTexture(texDesc);
        cmdList->WriteTexture(s_DefaultTextures[kBlackCubeTex], 0, 0, BlackCubeTexels, 6 * 4);
		
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);
	}

	void Destroy()
	{
		assert(s_GraphicsDevice != nullptr);
		s_GraphicsDevice = nullptr;
		s_Textures.clear();
		std::array<TextureHandle, kNumDefaultTexture>().swap(s_DefaultTextures);
	}

    TextureHandle GetDefaultTexture(DefaultTexture texID)
    {
		return s_DefaultTextures[texID];
    }

    TextureHandle CreateTextureFromFile(const std::string& texName, const std::string& filename, bool forceSRGB)
    {
		TextureHandle texture = nullptr;
		TextureDesc texDesc{};
		
		stbi_uc* imgData = nullptr;

#if defined(DSM_PLATFORM_WINDOWS)
        std::unique_ptr<std::uint8_t[]> ddsData{};
        std::vector<D3D12_SUBRESOURCE_DATA> subResources{};
        DDS_LOADER_FLAGS loadFlags = forceSRGB ? DDS_LOADER_FORCE_SRGB : DDS_LOADER_DEFAULT;
        std::wstring wFilename = Utility::UTF8ToWString(filename);
		ID3D12Resource* resource = nullptr;
		bool isCubeMap = false;
        if (SUCCEEDED(LoadDDSTextureFromFileEx(
            s_GraphicsDevice->GetNativeObject(ObjectTypes::D3D12_Device),
            wFilename.c_str(),
            0,
            D3D12_RESOURCE_FLAG_NONE,
            loadFlags,
			&resource,
            ddsData,
            subResources,
            nullptr,
            &isCubeMap))) {
	        D3D12_RESOURCE_DESC d3dDesc = resource->GetDesc();
			texDesc = D3D12::ConvertD3D12TextureDesc(d3dDesc, texName, isCubeMap);
			texture = s_GraphicsDevice->CreateHandleForNativeTexture(ObjectTypes::D3D12_Resource, resource, texDesc);
			
			auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init Texture"));
			cmdList->Open();	
			for(size_t i = 0; i < subResources.size(); ++i){
				size_t arraySlice = i / std::max(texDesc.mipLevels, 1u);
				size_t mipLevel = i % texDesc.mipLevels;
				cmdList->WriteTexture(texture, arraySlice, mipLevel, subResources[i].pData, subResources[i].RowPitch, subResources[i].SlicePitch);
			}
			cmdList->SetTextureState(texture, AllSubresources, ResourceStates::PixelShaderResource);
			cmdList->Close();
			s_GraphicsDevice->ExecuteCommandList(cmdList);

			return texture;
		}
#endif

		int width, height, components;
		imgData = stbi_load(filename.c_str(), &width, &height, &components, 4);
		if (imgData == nullptr) return nullptr;
		
		bool isHDR = stbi_is_hdr(filename.c_str());
		texDesc.format = isHDR ? Format::RGB32_FLOAT : Format::RGBA8_UNORM;
		texDesc.width = static_cast<uint32_t>(width);
		texDesc.height = static_cast<uint32_t>(height);
		texDesc.debugName = texName;
		texDesc.initialState = ResourceStates::Common;

		texture = s_GraphicsDevice->CreateTexture(texDesc);
		if(texture != nullptr) {
			auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init Texture"));
			cmdList->Open();
			uint32_t rowPitch = GetRowPitch(texDesc.format, texDesc.width);
			uint32_t slicePitch = GetSlicePitch(texDesc.format, texDesc.width, texDesc.height);
			cmdList->WriteTexture(texture, 0, 0, imgData, rowPitch, slicePitch);
			cmdList->SetTextureState(texture, AllSubresources, ResourceStates::PixelShaderResource);
			cmdList->Close();
			s_GraphicsDevice->ExecuteCommandList(cmdList);

			stbi_image_free(imgData);
		}

        return texture;
    }
    TextureHandle CreateTextureFromMemory(const std::string& name, const TextureDesc& texDesc, const void* data)
	{
		TextureHandle texture = s_GraphicsDevice->CreateTexture(texDesc);
		if (texture == nullptr) return nullptr;

		auto cmdList = s_GraphicsDevice->CreateCommandList(CommandListParameters().SetDebugName("Init Texture"));
		cmdList->Open();
		uint32_t rowPitch = GetRowPitch(texDesc.format, texDesc.width);
		uint32_t slicePitch = GetSlicePitch(texDesc.format, texDesc.width, texDesc.height);
		cmdList->WriteTexture(texture, 0, 0, data, rowPitch, slicePitch);
		cmdList->Close();
		s_GraphicsDevice->ExecuteCommandList(cmdList);

		return texture;
	}

	TextureHandle LoadTextureFromFile(const std::string& fileName, bool forceSRGB)
	{
		std::pair<bool, TextureHandle>* ppTex = nullptr;
		std::string key = forceSRGB ? (fileName + "_SRGB") : fileName;

		std::unique_lock lock{s_Mutex};

		// 防止多线程的情况
		if (auto it = s_Textures.find(key); it != s_Textures.end()) {
			ppTex = &it->second;
			std::condition_variable cv;
			cv.wait(lock, [&] { return ppTex->first; });
			return ppTex->second;
		}
		else {
			ppTex = &s_Textures[key];
		}
		
		lock.unlock();

		ppTex->second = CreateTextureFromFile(key, fileName, forceSRGB);
		ppTex->first = true;

		return ppTex->second;
	}
	
	TextureHandle LoadTextureFromMemory(const std::string& name, const TextureDesc& texDesc, const void* data)
	{
		std::pair<bool, TextureHandle>* ppTex = nullptr;
		std::unique_lock lock{s_Mutex};

		// 防止多线程的情况
		if (auto it = s_Textures.find(name); it != s_Textures.end()) {
			ppTex = &it->second;
			std::condition_variable cv;
			cv.wait(lock, [&] { return ppTex->first; });
			return ppTex->second;
		}
		else {
			ppTex = &s_Textures[name];
		}

		lock.unlock();

		ppTex->second =  CreateTextureFromMemory(name, texDesc, data);
		ppTex->first = true;

		return ppTex->second;
	}

	bool TextureManager::DestroyTexture(const std::string& name)
	{
		std::lock_guard lock(s_Mutex);

		// 调用Unload的时候还有一个引用，因此是小于等于两个
		if (auto it = s_Textures.find(name); it != s_Textures.end()) {
			s_Textures.erase(it);
			return true;
		}
		return false;
	}

}
