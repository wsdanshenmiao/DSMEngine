#include "RenderResource.h"
#include "Shaders/ForwardShader/ResourceData.h"
#include "Runtime/Framework/Component/MeshRenderer.h"

#include <random>

namespace DSM{
    void RenderResource::Create(IDevice *device)
    {
        DSM_ASSERT(device != nullptr);
        if(GetInstance().m_Device != nullptr)
            return;

        GetInstance().m_Device = device;

        // 创建纹理 Bindless 描述符布局
        auto bindlessDesc = BindlessLayoutDesc()
            .SetVisibility(ShaderType::Pixel)
            .SetFirstSlot(0)
            .AddRegisterSpace(BindingLayoutItem::Texture_SRV(1));
        GetInstance().m_TextureBindlessLayout = device->CreateBindlessLayout(bindlessDesc);
        // 创建纹理 Bindless 描述符表
        GetInstance().m_TextureBindlessTable = device->CreateDescriptorTable(GetInstance().m_TextureBindlessLayout);
        
        GetInstance().m_CmdList = device->CreateCommandList(CommandListParameters{}.SetDebugName("RenderResource CmdList"));
    
		GetInstance().CreateSamplers(device);
        GetInstance().CreateNoiseTexture(device);
    }
    
    void RenderResource::Destroy()
    {
        auto& renderRes = GetInstance();
        
        renderRes.m_Device = nullptr;
        renderRes.m_Framebuffer = nullptr;
        renderRes.m_CommonTextures.fill(nullptr);
        renderRes.m_CommonSamplers.fill(nullptr);
        
        renderRes.m_BVH = {};

        renderRes.m_OpaqueObjects.clear();
        renderRes.m_TransparentObjects.clear();
        renderRes.m_ObjInFrustum.clear();
        renderRes.m_ObjectIndex.clear();
        renderRes.m_LastFrameObjectIndex.clear();
        renderRes.m_ObjectMaterialIndex.clear();

        renderRes.m_MeshBuffer = nullptr;
        renderRes.m_LastFrameMeshBuffer = nullptr;
        renderRes.m_MaterialBuffer = nullptr;
        renderRes.m_Textures.clear();

        renderRes.m_TextureBindlessLayout = nullptr;
        renderRes.m_TextureBindlessTable = nullptr;

        renderRes.m_CmdList = nullptr;

        renderRes.m_TextureBindlessLayout = nullptr;
        renderRes.m_TextureBindlessTable = nullptr;
        renderRes.m_RenderPassFinishFence.fill(0);
    }

    void RenderResource::UpdateRenderResource(const Camera& camera)
    {
        m_LastFrameObjectIndex = m_ObjectIndex;

        m_ObjectIndex.clear();
        m_ObjInFrustum.clear();
        m_OpaqueObjects.clear();
        m_TransparentObjects.clear();
        m_ObjectMaterialIndex.clear();
        m_NoBoundsObjects.clear();

        auto scene = DSMEngine::sm_GlobalContext.scene;
        auto objView = scene->GetObjectsWithComponents<MeshRenderer, TransformComponent>();
        if(objView.size_hint() == 0){
            return;
        }

        // 保存需要从 BVH 中删除的物体，当当前物体在 BVH 中存在时，从 BVH 中删除
        auto objShouldBeErase = m_BVH.GetLeafNodes();
        std::vector<ShaderResource::MeshData> meshDataArr{};
        std::vector<ShaderResource::MaterialData> matDataArr{};
        std::unordered_map<std::shared_ptr<Material>, size_t> matMap{};
        meshDataArr.reserve(objView.size_hint());
        matDataArr.reserve(objView.size_hint());
        for(auto [id, meshRenderer, transform] : objView.each()){
            auto obj = scene->GetObjectByID(id).lock();
            auto mesh = meshRenderer.GetMesh();
            if (obj == nullptr || mesh == nullptr)
                continue;

            auto objIndex = std::size(m_OpaqueObjects) + std::size(m_TransparentObjects);

            // 更新 BVH 树
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
                m_ObjInFrustum.push_back(obj);
                m_NoBoundsObjects.push_back(obj);
            }

            ShaderResource::MeshData meshData{};
            meshData.world = Math::Matrix4::Transpose(transform.GetLocalToWorld());
            meshData.worldIT = Math::Matrix4::Inverse(meshData.world);
            meshDataArr.push_back(std::move(meshData));

            bool isTransparent = false;
            for(size_t subMeshIndex = 0; subMeshIndex < mesh->GetSubMeshCount(); ++subMeshIndex){
                size_t matIndex = meshRenderer.GetMaterialIndex(subMeshIndex);
                auto material = meshRenderer.GetMaterial(matIndex);
                if(material == nullptr){
                    material = std::make_shared<Material>(Shader::Find("Shaders/ForwardShader/Passes/LitPass.hlsl"));
                    meshRenderer.SetMaterial(matIndex, material);
                }

                // 如果材质已经存在，直接使用已有的材质索引，否则创建新的材质数据并分配新的材质索引
                if(auto it = matMap.find(material); it != std::end(matMap)){
                    m_ObjectMaterialIndex[obj].push_back(it->second);
                }
                else{
                    matMap[material] = matMap.size();
                    m_ObjectMaterialIndex[obj].push_back(matMap[material]);

                    ShaderResource::MaterialData matData{};
                    matData.baseColor = material->GetBaseColor();
                    matData.emissiveColor = material->GetEmissiveColor();
                    matData.normalTexScale = material->GetNormalTexScale();
                    matData.metallicFactor = material->GetMetallicFactor();
                    matData.roughnessFactor = material->GetRoughnessFactor();

                    for (const auto& [index, tex] : material->GetTextures() | std::views::enumerate) {
                        if (auto texSize = std::size(m_Textures); !m_Textures.contains(tex)) {
                            m_Textures[tex] = texSize;
                            if (m_TextureBindlessTable->GetCapacity() < std::size(m_Textures)) {
                                // 扩大描述符表的大小
                                m_Device->ResizeDescriptorTable(m_TextureBindlessTable, std::max(texSize * 2, 1zu));
                            }
                            m_Device->WriteDescriptorTable(m_TextureBindlessTable, BindingSetItem::Texture_SRV(texSize, tex));
                        }
                        matData.textureIndex[index] = m_Textures[tex];
                    }
                    matDataArr.push_back(matData);
                }

                isTransparent |= material->IsTransparent();
            }

            m_ObjectIndex[obj] = objIndex;
            auto& objects = isTransparent ? m_TransparentObjects : m_OpaqueObjects;
            objects.push_back(obj);
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
                    m_ObjInFrustum.push_back(node->object);
                }
                if(node->left != nullptr)
                    stack.push(node->left);
                if(node->right != nullptr)
                    stack.push(node->right);
            }
        }

        // 为所有的物体生成 MeshBuffer 和 MaterialBuffer
        auto resizeBuffer = [this] <typename T> (BufferHandle& buffer, const std::vector<T>& dataArr) {
            auto bufferSize = sizeof(T) * dataArr.size();
            bool isNull = buffer == nullptr;
            if(isNull || bufferSize > buffer->GetDesc().byteSize){
                buffer = m_Device->CreateBuffer(BufferDesc()
                    .SetByteSize(isNull ? bufferSize : std::max(bufferSize, buffer->GetDesc().byteSize * 2))
                    .SetStructStride(sizeof(T))
                    .SetDebugName(typeid(T).name()));
            }
        };
        resizeBuffer(m_MeshBuffer, meshDataArr);
        resizeBuffer(m_MaterialBuffer, matDataArr);

        m_CmdList->Open();

        if(m_MeshBuffer != nullptr){
            auto meshBufferSize = m_MeshBuffer->GetDesc().byteSize;
            if(m_LastFrameMeshBuffer == nullptr || m_LastFrameMeshBuffer->GetDesc().byteSize < meshBufferSize){
                m_LastFrameMeshBuffer = m_Device->CreateBuffer(BufferDesc{}
                    .SetByteSize(meshBufferSize)
                    .SetStructStride(sizeof(ShaderResource::MeshData))
                    .SetDebugName("Last Frame Mesh Buffer"));
            }
            m_CmdList->CopyBuffer(m_LastFrameMeshBuffer, 0, m_MeshBuffer, 0, meshBufferSize);
        }
        m_CmdList->WriteBuffer(m_MeshBuffer, meshDataArr.data(), meshDataArr.size() * sizeof(ShaderResource::MeshData));
        m_CmdList->WriteBuffer(m_MaterialBuffer, matDataArr.data(), matDataArr.size() * sizeof(ShaderResource::MaterialData));
        m_CmdList->Close();
        m_Device->ExecuteCommandList(m_CmdList);
    }
    
    void RenderResource::OnResize(GraphicsRenderer& renderer, uint32_t width, uint32_t height)
    {
        auto device = renderer.GetDevice();
        SetCommonTexture(CommonTextureSlot::Color, renderer.GetColorTexture());
        SetCommonTexture(CommonTextureSlot::Depth, device->CreateTexture(TextureDesc()
            .SetWidth(width)
            .SetHeight(height)
            .SetFormat(Format::D32)
            .SetClearValue(Color{1, 0, 0, 0})
            .SetInitialState(ResourceStates::DepthWrite)
            .SetIsRenderTarget(true)    // 娣卞害绾圭悊涔熼渶瑕佽缃?
            .SetDebugName("DepthTex")));
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
            .SetAllFilters(false)   // 点采样
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


}