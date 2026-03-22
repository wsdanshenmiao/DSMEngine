#pragma once
#ifndef __SETUPPASS_H__
#define __SETUPPASS_H__

#include <random>
#include "ShadowPass.h"
#include "Runtime/Render/Renderer/Renderer.h"
#include "Runtime/Render/ShaderCompiler.h"
#include "Shaders/ForwardShader/ResourceData.h"

namespace DSM {
    class SetupPass : public IRenderPass
    {
    public:
        SetupPass(Renderer& renderer)
        {
            auto device = renderer.GetDevice();

            CreateSamplers(renderer);
            CreateNoiseTexture(renderer);

            // 创建纹理 Bindless 描述符布局
            auto bindlessDesc = BindlessLayoutDesc()
                .SetVisibility(ShaderType::Pixel)
                .SetFirstSlot(0)
                .AddRegisterSpace(BindingLayoutItem::Texture_SRV(1));
            g_RenderResources.textureBindlessLayout = device->CreateBindlessLayout(bindlessDesc);
			// 创建纹理 Bindless 描述符表
            g_RenderResources.textureBindlessTable = device->CreateDescriptorTable(g_RenderResources.textureBindlessLayout);

            const auto& viewport = renderer.GetCamera().GetViewPort();
            OnResize(renderer, (uint32_t)viewport.Width(), (uint32_t)viewport.Height());
        }

        void Render(DSM::Renderer& renderer, float deltaTime) override 
        {
            auto scene = DSMEngine::sm_GlobalContext.scene;
            auto device = renderer.GetDevice();

            g_RenderResources.objInFrustum.clear();
            g_RenderResources.objects.clear();

            auto cameraFrustum = renderer.GetCamera().GetFrustum();
            Math::Matrix4 invView = Math::Matrix4::Inverse(renderer.GetCamera().GetViewMatrix());
            auto objView = scene->GetObjectsWithComponents<Mesh, ShaderResource::MaterialData, Math::Transform>();

            // 为所有的物体生成 MeshBuffer 和 MaterialBuffer
            auto meshBufferSize = sizeof(ShaderResource::MeshData) * objView.size_hint();
            auto& meshBuffer = g_RenderResources.meshBuffer;
            if(meshBuffer == nullptr || meshBufferSize > meshBuffer->GetDesc().byteSize){
                meshBuffer = device->CreateBuffer(BufferDesc()
                    .SetByteSize(meshBufferSize)
                    .SetStructStride(sizeof(ShaderResource::MeshData))
                    .SetDebugName("MeshBuffer"));
            }
            auto materialBufferSize = sizeof(ShaderResource::MaterialData) * objView.size_hint();
            auto& materialBuffer = g_RenderResources.materialBuffer;
            if(materialBuffer == nullptr || materialBufferSize > materialBuffer->GetDesc().byteSize){
                materialBuffer = device->CreateBuffer(BufferDesc()
                    .SetByteSize(materialBufferSize)
                    .SetStructStride(sizeof(ShaderResource::MaterialData))
                    .SetDebugName("MaterialBuffer"));
            }

            // 更新 BVH 中的物体包围盒，或将不再具有有效包围盒的物体加入待移除列表
            std::vector<ShaderResource::MeshData> meshDataList{};
            std::vector<ShaderResource::MaterialData> materialDataList{};
            meshDataList.reserve(objView.size_hint());
            materialDataList.reserve(objView.size_hint());
            for(auto [id, mesh, material, transform] : objView.each()){
                auto obj = scene->GetObjectByID(id).lock();
                
                ShaderResource::MeshData meshCB{};
                meshCB.world = Math::Matrix4::Transpose(transform.GetLocalToWorld());
                meshCB.worldIT = Math::Matrix4::InverseTranspose(meshCB.world);
                meshDataList.push_back(std::move(meshCB));

                // 将网格的纹理添加到资源列表中
                for(const auto& [index, tex] : mesh.textures | std::views::enumerate){
                    auto& textures = g_RenderResources.textures;
                    if(!textures.contains(tex)){
                        auto& bindlessTable = g_RenderResources.textureBindlessTable;
                        if(bindlessTable->GetCapacity() <= textures.size()){
                            auto newCapacity = std::max(size_t(bindlessTable->GetCapacity() * 2), textures.size() + 1);
                            device->ResizeDescriptorTable(bindlessTable, newCapacity);
                        }
                        device->WriteDescriptorTable(bindlessTable, BindingSetItem::Texture_SRV(textures.size(), tex));
                        textures.insert({tex, textures.size()});
                    }
                    // 将纹理的索引写入缓冲区
                    material.textureIndex[index] = (int)textures[tex];
                }
                materialDataList.push_back(material);

                // 将世界矩阵及材质写入缓冲区
                auto objIndex = g_RenderResources.objects.size();
                if(auto boundingBox = obj->GetComponent<Math::AxisAlignedBox>(); boundingBox != nullptr){
                    // 将视锥体变换到局部空间
                    auto invWorld = Math::Matrix4::Inverse(transform.GetLocalToWorld());
                    auto frustum = cameraFrustum * (invView * invWorld);
                    // 判断是否在视锥体内
                    if(frustum.Intersects(*boundingBox)){
                        g_RenderResources.objInFrustum.emplace_back(objIndex, obj);
                    }
                }
                else{
                    g_RenderResources.objInFrustum.emplace_back(objIndex, obj);
                }

                g_RenderResources.objects.emplace_back(obj);
            }

            auto cmdList = device->CreateCommandList(CommandListParameters{}.SetDebugName("SetupPass CmdList"));
            cmdList->Open();
            cmdList->WriteBuffer(meshBuffer, meshDataList.data(), sizeof(ShaderResource::MeshData) * meshDataList.size());
            cmdList->WriteBuffer(materialBuffer, materialDataList.data(), sizeof(ShaderResource::MaterialData) * materialDataList.size());
            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

        void OnResize(Renderer& renderer, uint32_t width, uint32_t height) override
        {
            auto colorTex = renderer.GetColorTexture();
            auto& depthTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Depth];
            depthTex = renderer.GetDevice()->CreateTexture(TextureDesc()
                .SetWidth(width)
                .SetHeight(height)
                .SetFormat(Format::D32)
                .SetClearValue(Color{1, 0, 0, 0})
                .SetInitialState(ResourceStates::DepthWrite)
                .SetIsRenderTarget(true)    // 深度纹理也需要设置
                .SetDebugName("DepthTex"));
            auto preFramebuffer = g_RenderResources.framebuffer;
            g_RenderResources.framebuffer = renderer.GetDevice()->CreateFramebuffer(FramebufferDesc()
                .AddColorAttachment(colorTex).SetDepthAttachment(depthTex));
        }
        
    private:
        void CreateSamplers(Renderer& renderer) 
        {
            auto device = renderer.GetDevice();
            bool reverseZ = renderer.GetCamera().IsReversedZ();
            auto& samplers = g_RenderResources.commonSamplers;

            samplers[uint8_t(SamplerSlot::PointClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::AnisoClamp)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Clamp)
                .SetMaxAnisotropy(4));
            samplers[uint8_t(SamplerSlot::PointWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::AnisoWrap)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Wrap)
                .SetMaxAnisotropy(4));
            samplers[uint8_t(SamplerSlot::PointBorder)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(false));
            samplers[uint8_t(SamplerSlot::LinearBorder)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(true));
            samplers[uint8_t(SamplerSlot::Shadow)] = device->CreateSampler(SamplerDesc()
                .SetAllAddressModes(SamplerAddressMode::Border)
                .SetAllFilters(false)   // 点采样
                .SetComparisonFunc(ComparisonFunc::LessOrEqual)
                .SetReductionType(SamplerReductionType::Comparison));
        }

        void CreateNoiseTexture(Renderer& renderer)
        {
            IDevice* device = renderer.GetDevice();
            auto& noiseTex = g_RenderResources.commonTextures[(size_t)CommonTextureSlot::Noise];
            noiseTex = device->CreateTexture(TextureDesc()
                .SetWidth(256)
                .SetHeight(256)
                .SetFormat(Format::RGBA8_UNORM)
                .SetDebugName("NoiseTex"));
            // 获取随机值
            std::array<uint8_t, 256 * 256 * 4> noiseData;
            std::mt19937 gen{std::random_device{}()};
            std::uniform_int_distribution<int> dist(0, std::numeric_limits<uint8_t>::max());
            for (size_t i = 0; i < noiseData.size(); ++i) {
                noiseData[i] = static_cast<uint8_t>(dist(gen));
            }
            auto cmdList = device->CreateCommandList(CommandListParameters().SetDebugName("SetupPass Noise Upload"));
            cmdList->Open();
            auto rowPitch = GetRowPitch(noiseTex->GetDesc().format, noiseTex->GetDesc().width);
            cmdList->WriteTexture(noiseTex, 0, 0, noiseData.data(), rowPitch);
            cmdList->SetTextureState(noiseTex, AllSubresources, ResourceStates::ShaderResource);
            cmdList->Close();
            device->ExecuteCommandList(cmdList);
        }

    };

} // namespace DSM


#endif