#pragma once
#ifndef __RENDERRESOURCE_H__
#define __RENDERRESOURCE_H__

#include "Runtime/Render/Light.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Math/Collision/BVH.h"
#include "Runtime/Utils/Singleton.h"
#include "Runtime/Utils/LinearAllocator.h"

#include <array>

namespace DSM {

    struct IRenderPass
    {
        virtual ~IRenderPass() = default;
        virtual void Render(DSM::Renderer& renderer, float deltaTime) = 0;
        virtual void OnResize(Renderer& renderer, uint32_t width, uint32_t height) = 0;
    };

    enum class CommonTextureSlot : uint32_t
    {
        Color = 0,
        Depth,
        Normal, //  视图空间下的法线
        Noise,
        SSAO,
        ShadowMap,
        Count
    };

    enum class SamplerSlot : uint8_t
    {
        PointWrap = 0,
        LinearWrap,
        AnisoWrap,
        PointClamp,
        LinearClamp,
        AnisoClamp,
        PointBorder,
        LinearBorder,
        Shadow,
        Count
    };

    class RenderResource : public Singleton<RenderResource>
    {
    public:
        static void Create(IDevice* device);
        static void Destroy();

        inline SamplerHandle& GetCommonSampler(SamplerSlot slot)
        {
            return m_CommonSamplers[static_cast<size_t>(slot)];
        }
        inline SamplerHandle& GetCommonSampler(size_t slot)
        {
            assert(slot < (size_t)SamplerSlot::Count);
            return m_CommonSamplers[slot];
        }
        inline TextureHandle& GetCommonTexture(CommonTextureSlot slot)
        {
            return m_CommonTextures[static_cast<size_t>(slot)];
        }
        inline TextureHandle& GetCommonTexture(size_t slot)
        {
            assert(slot < (size_t)CommonTextureSlot::Count);
            return m_CommonTextures[slot];
        }

        inline BufferHandle& GetMeshBuffer() { return m_MeshBuffer; }
        inline BufferHandle& GetMaterialBuffer() { return m_MaterialBuffer; }

        inline FramebufferHandle& GetFramebuffer() { return m_Framebuffer; }

        inline BindingLayoutHandle& GetTextureBindlessLayout() { return m_TextureBindlessLayout; }
        inline DescriptorTableHandle& GetTextureBindlessTable() { return m_TextureBindlessTable; }

        inline auto& GetObjectInFrustum() { return m_ObjInFrustum; }
        inline auto& GetQpaqueObjects() { return m_OpaqueObjects; }
        inline auto& GetTransparentObjects() { return m_TransparentObjects; }

        inline Math::BVHTree& GetBVH() { return m_BVH; }

        void UpdateRenderResource(const Camera& camera);

        inline void SetCommonSampler(SamplerSlot slot, SamplerHandle sampler)
        {
            m_CommonSamplers[(size_t)slot] = sampler;
        }
        inline void SetCommonTexture(CommonTextureSlot slot, TextureHandle texture)
        {
            m_CommonTextures[(size_t)slot] = texture;
        }

        void OnResize(DSM::Renderer& renderer, uint32_t width, uint32_t height);


    private:
        void CreateSamplers(IDevice* device);
        void CreateNoiseTexture(IDevice* device);

    private:
        IDevice* m_Device = nullptr;

        FramebufferHandle m_Framebuffer{};

        std::array<TextureHandle, (size_t)CommonTextureSlot::Count> m_CommonTextures;
        std::array<SamplerHandle, (size_t)SamplerSlot::Count> m_CommonSamplers;

        // 场景中的物体
        Math::BVHTree m_BVH;
        std::unordered_map<std::shared_ptr<GameObject>, size_t> m_OpaqueObjects{};
        std::unordered_map<std::shared_ptr<GameObject>, size_t> m_TransparentObjects{};
        std::vector<std::pair<size_t, std::shared_ptr<GameObject>>> m_ObjInFrustum{};
        
        BufferHandle m_MeshBuffer{};
        BufferHandle m_MaterialBuffer{};

        std::unordered_map<TextureHandle, size_t> m_Textures{};
        
        BindingLayoutHandle m_TextureBindlessLayout{};
        DescriptorTableHandle m_TextureBindlessTable{};

        CommandListHandle m_CmdList{};
    };


} // namespace DSM

#endif