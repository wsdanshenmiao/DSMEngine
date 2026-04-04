#include "RenderResource.h"
#include "Shaders/ForwardShader/ResourceData.h"
#include "Runtime/Core/InstrumentorTimer.h"
#include "Runtime/Framework/Component/MeshRenderer.h"

#include <random>

namespace DSM{
    void RenderResource::Create(IDevice *device)
    {
        DSM_ASSERT(device != nullptr);
        if(GetInstance().m_Device != nullptr)
            return;

        GetInstance().m_Device = device;

        // 鍒涘缓绾圭悊 Bindless 鎻忚堪绗﹀竷灞€
        auto bindlessDesc = BindlessLayoutDesc()
            .SetVisibility(ShaderType::Pixel)
            .SetFirstSlot(0)
            .AddRegisterSpace(BindingLayoutItem::Texture_SRV(1));
        GetInstance().m_TextureBindlessLayout = device->CreateBindlessLayout(bindlessDesc);
        // 鍒涘缓绾圭悊 Bindless 鎻忚堪绗﹁〃
        GetInstance().m_TextureBindlessTable = device->CreateDescriptorTable(GetInstance().m_TextureBindlessLayout);
        
        GetInstance().m_CmdList = device->CreateCommandList(CommandListParameters{}.SetDebugName("RenderResource CmdList"));
    
		GetInstance().CreateSamplers(device);
        GetInstance().CreateNoiseTexture(device);
    }
    
    void RenderResource::Destroy()
    {
        auto& renderResource = GetInstance();
        renderResource.m_Device = nullptr;
        renderResource.m_Framebuffer = nullptr;
        renderResource.m_CommonTextures.fill(nullptr);
        renderResource.m_CommonSamplers.fill(nullptr);
        renderResource.m_BVH = {};
        renderResource.m_Textures.clear();
        renderResource.m_MeshBuffer = nullptr;
        renderResource.m_MaterialBuffer = nullptr;
        renderResource.m_TextureBindlessLayout = nullptr;
        renderResource.m_TextureBindlessTable = nullptr;
    }

    void RenderResource::UpdateRenderResource(const Camera& camera)
    {
        m_ObjInFrustum.clear();
        m_OpaqueObjects.clear();
        m_TransparentObjects.clear();

        auto scene = DSMEngine::sm_GlobalContext.scene;
        auto objView = scene->GetObjectsWithComponents<MeshRenderer, Transform>();

        // 为所有的物体生成 MeshBuffer 和 MaterialBuffer
        auto resizeBuffer = [this, &objView] <typename T> (BufferHandle& buffer){
            auto bufferSize = sizeof(T) * objView.size_hint();
            bool isNull = buffer == nullptr;
            if(isNull || bufferSize > buffer->GetDesc().byteSize){
                buffer = m_Device->CreateBuffer(BufferDesc()
                    .SetByteSize(isNull ? bufferSize : std::max(1zu, buffer->GetDesc().byteSize * 2))
                    .SetStructStride(sizeof(T))
                    .SetDebugName(typeid(T).name()));
            }
        };
        resizeBuffer.operator()<ShaderResource::MeshData>(m_MeshBuffer);
        resizeBuffer.operator()<ShaderResource::MaterialData>(m_MaterialBuffer);

        // 保存需要从 BVH 中删除的物体，当当前物体在 BVH 中存在时，从 BVH 中删除
        auto objShouldBeErase = m_BVH.GetLeafNodes();
        std::vector<ShaderResource::MeshData> meshDataArr{};
        std::vector<ShaderResource::MaterialData> matDataArr{};
        meshDataArr.reserve(objView.size_hint());
        matDataArr.reserve(objView.size_hint());
        for(auto [id, meshRenderer, transform] : objView.each()){
            auto obj = scene->GetObjectByID(id).lock();
            if (obj == nullptr || meshRenderer.GetMesh() == nullptr)
                continue;

            auto objIndex = std::size(m_OpaqueObjects) + std::size(m_TransparentObjects);

            if(meshRenderer.GetBounds().IsValid()){
                // 有包围盒，检测是否在 BVH 中
                if(auto it = objShouldBeErase.find(obj); it != std::end(objShouldBeErase)){
                    m_BVH.UpdateNode(it->second);
                    objShouldBeErase.erase(obj);
                }
                else{
                    m_BVH.InsertNode(obj);
                }
            }
            else {
                m_ObjInFrustum.push_back({objIndex, obj});
            }

            ShaderResource::MaterialData material{};
            if(auto meshMat = meshRenderer.GetMaterial(); meshMat != nullptr){
                material.baseColor = meshMat->GetBaseColor();
                material.emissiveColor = meshMat->GetEmissiveColor();
                material.normalTexScale = meshMat->GetNormalTexScale();
                material.metallicFactor = meshMat->GetMetallicFactor();
                material.roughnessFactor = meshMat->GetRoughnessFactor();
            }
            else{
                material.baseColor = {1, 1, 1, 1};
                material.emissiveColor = {0, 0, 0, 0};
                material.normalTexScale = 1.0f;
                material.metallicFactor = 0.0f;
                material.roughnessFactor = 1.0f;
            }
            for (const auto& [index, tex] : meshRenderer.GetMesh()->textures | std::views::enumerate) {
                if (auto texSize = std::size(m_Textures); !m_Textures.contains(tex)) {
                    m_Textures[tex] = texSize;
                    if (m_TextureBindlessTable->GetCapacity() < std::size(m_Textures)) {
                        // 扩大描述符表的大小
                        m_Device->ResizeDescriptorTable(m_TextureBindlessTable, std::max(texSize * 2, 1zu));
                    }
                    m_Device->WriteDescriptorTable(m_TextureBindlessTable, BindingSetItem::Texture_SRV(texSize, tex));
                }
                material.textureIndex[index] = m_Textures[tex];
            }

            ShaderResource::MeshData meshData{};
            meshData.world = Math::Matrix4::Transpose(transform.GetLocalToWorld());
            meshData.worldIT = Math::Matrix4::Inverse(meshData.world);
            meshDataArr.push_back(std::move(meshData));
            matDataArr.push_back(material);

            auto& objects = HasFlags(PSOFlags{meshRenderer.GetMesh()->psoFlags}, kAlphaBlend) ? m_TransparentObjects : m_OpaqueObjects;
            objects[obj] = objIndex;
        }

        // 移除不在场景中的物体
        for(const auto& [obj, node] : objShouldBeErase) {
            m_BVH.RemoveNode(node);
        }

        Math::Frustum cameraFrustum = camera.GetFrustum();
        cameraFrustum *= Math::Matrix4::Inverse(camera.GetViewMatrix());
        std::stack<std::shared_ptr<Math::BVHTree::BVHNode>> stack{};
        stack.push(m_BVH.GetRoot());
        while (!std::empty(stack)) {
            auto node = stack.top();
            stack.pop();
            if(node == nullptr)
                continue;

            if (cameraFrustum.Intersects(node->bounds)) {
                if (node->object != nullptr) {
                    if(auto it = m_OpaqueObjects.find(node->object); it != std::end(m_OpaqueObjects)){
                        m_ObjInFrustum.push_back({it->second, node->object});
                    }
                    else if(auto it = m_TransparentObjects.find(node->object); it != std::end(m_TransparentObjects)){
                        m_ObjInFrustum.push_back({it->second, node->object});
                    }
                }
                if(node->left != nullptr)
                    stack.push(node->left);
                if(node->right != nullptr)
                    stack.push(node->right);
            }
        }

        m_CmdList->Open();
        m_CmdList->WriteBuffer(m_MeshBuffer, meshDataArr.data(), meshDataArr.size() * sizeof(ShaderResource::MeshData));
        m_CmdList->WriteBuffer(m_MaterialBuffer, matDataArr.data(), matDataArr.size() * sizeof(ShaderResource::MaterialData));
        m_CmdList->Close();
        m_Device->ExecuteCommandList(m_CmdList);
    }
    
    void RenderResource::OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height)
    {
        auto device = renderer.GetDevice();
        m_CommonTextures[(size_t)CommonTextureSlot::Color] = renderer.GetColorTexture();
        m_CommonTextures[(size_t)CommonTextureSlot::Depth] = device->CreateTexture(TextureDesc()
            .SetWidth(width)
            .SetHeight(height)
            .SetFormat(Format::D32)
            .SetClearValue(Color{1, 0, 0, 0})
            .SetInitialState(ResourceStates::DepthWrite)
            .SetIsRenderTarget(true)    // 娣卞害绾圭悊涔熼渶瑕佽缃?
            .SetDebugName("DepthTex"));
        m_Framebuffer = device->CreateFramebuffer(FramebufferDesc()
            .AddColorAttachment(GetCommonTexture(CommonTextureSlot::Color))
            .SetDepthAttachment(GetCommonTexture(CommonTextureSlot::Depth)));
    }

    void RenderResource::CreateSamplers(IDevice* device) 
    {
        auto& samplers = m_CommonSamplers;

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
            .SetAllFilters(false)   // 鐐归噰鏍?
            .SetComparisonFunc(ComparisonFunc::LessOrEqual)
            .SetReductionType(SamplerReductionType::Comparison));
    }

    void RenderResource::CreateNoiseTexture(IDevice* device)
    {
        auto& noiseTex = m_CommonTextures[(size_t)CommonTextureSlot::Noise];
        SetCommonTexture(CommonTextureSlot::Noise, device->CreateTexture(TextureDesc()
            .SetWidth(256)
            .SetHeight(256)
            .SetFormat(Format::RGBA8_UNORM)
            .SetDebugName("NoiseTex")));
        // 鑾峰彇闅忔満鍊?
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


}