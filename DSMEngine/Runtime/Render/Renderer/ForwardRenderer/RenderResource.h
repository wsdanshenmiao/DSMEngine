#pragma once
#ifndef __RENDERRESOURCE_H__
#define __RENDERRESOURCE_H__

#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Runtime/Framework/Scene.h"
#include "Runtime/Math/Collision/BVH.h"
#include "Runtime/Utils/Singleton.h"
#include "Runtime/Utils/LinearAllocator.h"

#include <array>

namespace DSM {

    struct IRenderPass
    {
        virtual ~IRenderPass() = default;
        virtual uint64_t Render(DSM::GraphicsRenderer& renderer, float deltaTime) = 0;
        virtual void OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height) = 0;
    };

    enum class CommonTextureSlot : uint32_t
    {
        Color = 0,
        Depth,
        Normal, //  视图空间下的法线
        Noise,
        SSAO,
        DirectionalShadowMap,
        OtherShadowMap,
        MotionVector,
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

    enum class RenderPass : uint8_t
    {
        Geometry = 0,
        MotionVector,
        SSAO,
        Lighting,
        Lit,
        Skybox,
        Transparent,
        TAA,
        PostEffect,
        Final,
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

        inline BufferHandle& GetMeshBuffer() noexcept { return m_MeshBuffer; }
        inline BufferHandle& GetLastFrameMeshBuffer() noexcept { return m_LastFrameMeshBuffer; }
        inline BufferHandle& GetMaterialBuffer() noexcept { return m_MaterialBuffer; }

        inline FramebufferHandle& GetFramebuffer() noexcept { return m_Framebuffer; }

        inline BindingLayoutHandle& GetTextureBindlessLayout() noexcept { return m_TextureBindlessLayout; }
        inline DescriptorTableHandle& GetTextureBindlessTable() noexcept { return m_TextureBindlessTable; }

        inline const auto& GetObjectInFrustum() noexcept { return m_ObjInFrustum; }
        inline const auto& GetOpaqueObjects() noexcept { return m_OpaqueObjects; }
        inline const auto& GetTransparentObjects() noexcept { return m_TransparentObjects; }
        inline const auto& GetObjectIndex() noexcept { return m_ObjectIndex; }
        inline const auto& GetLastFrameObjectIndex() noexcept { return m_LastFrameObjectIndex; }
        inline const auto& GetObjectMaterialIndex() noexcept { return m_ObjectMaterialIndex; }

        inline Math::BVHTree& GetBVH() noexcept { return m_BVH; }

        inline uint64_t GetRenderPassFinishFence(RenderPass pass) const { return m_RenderPassFinishFence[(size_t)pass]; }

        inline void SetRenderPassFinishFence(RenderPass pass, uint64_t fenceValue) { m_RenderPassFinishFence[(size_t)pass] = fenceValue; }

        inline void SetCommonSampler(SamplerSlot slot, SamplerHandle sampler)
        {
            m_CommonSamplers[(size_t)slot] = sampler;
        }
        inline void SetCommonTexture(CommonTextureSlot slot, TextureHandle texture)
        {
            m_CommonTextures[(size_t)slot] = texture;
        }

        void UpdateRenderResource(const Camera& camera);

        void OnResize(DSM::GraphicsRenderer& renderer, uint32_t width, uint32_t height);


    private:
        void CreateSamplers(IDevice* device);
        void CreateNoiseTexture(IDevice* device);

    private:
        IDevice* m_Device = nullptr;

        FramebufferHandle m_Framebuffer{};

        std::array<TextureHandle, (size_t)CommonTextureSlot::Count> m_CommonTextures;
        std::array<SamplerHandle, (size_t)SamplerSlot::Count> m_CommonSamplers;

        // BVH 树，包含场景中所有的 MeshRenderer 组件对应的物体
        Math::BVHTree m_BVH;
        // 不透明的物体列表
        std::vector<std::shared_ptr<GameObject>> m_OpaqueObjects{};
        // 透明的物体列表
        std::vector<std::shared_ptr<GameObject>> m_TransparentObjects{};
        // 当前帧在相机视锥内的物体列表
        std::vector<std::shared_ptr<GameObject>> m_ObjInFrustum{};
        // 物体的全局索引
        std::unordered_map<std::shared_ptr<GameObject>, size_t> m_ObjectIndex{};
        // 物体上一帧的全局索引
        std::unordered_map<std::shared_ptr<GameObject>, size_t> m_LastFrameObjectIndex{};
        // 物体对应的材质索引列表
        std::unordered_map<std::shared_ptr<GameObject>, std::vector<size_t>> m_ObjectMaterialIndex{};
        
        BufferHandle m_MeshBuffer{};
        BufferHandle m_LastFrameMeshBuffer{};
        BufferHandle m_MaterialBuffer{};

        std::unordered_map<TextureHandle, size_t> m_Textures{};
        
        BindingLayoutHandle m_TextureBindlessLayout{};
        DescriptorTableHandle m_TextureBindlessTable{};

        std::array<uint64_t, (size_t)RenderPass::Count> m_RenderPassFinishFence{};

        CommandListHandle m_CmdList{};
    };


} // namespace DSM

#endif