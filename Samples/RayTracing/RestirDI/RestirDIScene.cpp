#include "RestirDIScene.h"

#include "Runtime/DSMEngine.h"
#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Render/Material.h"
#include "Runtime/Render/Mesh.h"
#include "Runtime/Render/TextureManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <span>
#include <unordered_map>

namespace DSM::RestirDI {
    namespace {
        constexpr uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        void HashBytes(uint64_t& hash, const void* data, size_t byteCount)
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t index = 0; index < byteCount; ++index) {
                hash ^= bytes[index];
                hash *= kFnvPrime;
            }
        }

        template<typename T>
        void HashValue(uint64_t& hash, const T& value)
        {
            HashBytes(hash, &value, sizeof(value));
        }

        uint32_t StableID(ObjectID id)
        {
            return static_cast<uint32_t>(entt::to_integral(id));
        }

        float Luminance(const GpuFloat4& value)
        {
            return std::max(0.2126f * value.x + 0.7152f * value.y + 0.0722f * value.z, 0.0f);
        }

        Math::Vector3 TransformPoint(const Math::Vector3& point, const Math::Matrix4& matrix)
        {
            const Math::Vector4 transformed = Math::Vector4(point, 1.0f) * matrix;
            return Math::Vector3(transformed);
        }

        void ToAffineTransform(const Math::Matrix4& matrix, RT::AffineTransform& output)
        {
            for (size_t row = 0; row < 3; ++row) {
                for (size_t column = 0; column < 4; ++column) {
                    output[row * 4 + column] = matrix.Get(column, row);
                }
            }
        }

        template<typename T>
        BufferHandle CreateStructuredBuffer(IDevice* device, size_t count, const char* name, bool uav = false)
        {
            return device->CreateBuffer(BufferDesc{}
                .SetByteSize(std::max<size_t>(count, 1) * sizeof(T))
                .SetStructStride(sizeof(T))
                .SetInitialState(uav ? ResourceStates::UnorderedAccess : ResourceStates::ShaderResource)
                .SetKeepInitialState(true)
                .SetCanHaveUAVs(uav)
                .SetDebugName(name));
        }

        std::vector<ObjectID> GetSortedMeshEntities()
        {
            std::vector<ObjectID> entities{};
            const auto scene = DSMEngine::sm_GlobalContext.scene;
            if (scene == nullptr)
                return entities;
            auto view = scene->GetObjectsWithComponents<MeshRenderer, TransformComponent>();
            entities.reserve(view.size_hint());
            for (ObjectID id : view) {
                const auto object = scene->GetObjectByID(id).lock();
                const auto& renderer = view.get<MeshRenderer>(id);
                if (object != nullptr && object->IsEnabled() && renderer.IsEnabled() && renderer.GetMesh() != nullptr) {
                    entities.push_back(id);
                }
            }
            std::ranges::sort(entities, {}, StableID);
            return entities;
        }

        uint32_t AddTexture(
            std::vector<TextureHandle>& textures,
            std::unordered_map<ITexture*, uint32_t>& indices,
            const TextureHandle& texture,
            uint32_t fallback)
        {
            if (texture == nullptr)
                return fallback;
            if (const auto it = indices.find(texture.Get()); it != indices.end())
                return it->second;
            const uint32_t index = static_cast<uint32_t>(textures.size());
            textures.push_back(texture);
            indices.emplace(texture.Get(), index);
            return index;
        }

        std::span<const uint32_t> GetMeshIndices(const Mesh& mesh)
        {
            if (mesh.indexFormat != Format::R32_UINT || mesh.indices.empty()) return {};
            return {reinterpret_cast<const uint32_t*>(mesh.indices.data()), mesh.indices.size() / sizeof(uint32_t)};
        }

        std::span<const uint16_t> GetMeshIndices16(const Mesh& mesh)
        {
            if (mesh.indexFormat != Format::R16_UINT || mesh.indices.empty()) return {};
            return {reinterpret_cast<const uint16_t*>(mesh.indices.data()), mesh.indices.size() / sizeof(uint16_t)};
        }
    }

    uint64_t SceneAdapter::CalculateTopologyHash(const Settings& settings) const
    {
        uint64_t hash = kFnvOffset;
        HashValue(hash, settings.alphaCutoff);
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) return hash;
        for (ObjectID id : GetSortedMeshEntities()) {
            const auto& renderer = scene->GetObjectsWithComponents<MeshRenderer>().get<MeshRenderer>(id);
            const auto& mesh = renderer.GetMesh();
            HashValue(hash, StableID(id));
            HashValue(hash, reinterpret_cast<uintptr_t>(mesh.get()));
            HashValue(hash, mesh->indexFormat);
            HashValue(hash, mesh->GetSubMeshCount());
            HashBytes(hash, mesh->vertices.data(), mesh->vertices.size() * sizeof(Math::Vector3));
            HashBytes(hash, mesh->indices.data(), mesh->indices.size());
            for (size_t submesh = 0; submesh < mesh->GetSubMeshCount(); ++submesh) {
                const size_t materialIndex = renderer.GetMaterialIndexOrDefault(submesh);
                const auto& materials = renderer.GetMaterials();
                const auto material = materialIndex < materials.size() ? materials[materialIndex] : nullptr;
                HashValue(hash, reinterpret_cast<uintptr_t>(material.get()));
                if (material != nullptr) {
                    const auto base = ToGpuFloat4(material->GetBaseColor());
                    const auto emissive = ToGpuFloat4(material->GetEmissiveColor());
                    HashValue(hash, base);
                    HashValue(hash, emissive);
                    HashValue(hash, material->GetNormalTexScale());
                    HashValue(hash, material->GetMetallicFactor());
                    HashValue(hash, material->GetRoughnessFactor());
                    HashValue(hash, material->IsTransparent());
                    HashValue(hash, material->IsBothSide());
                    for (const auto& texture : material->GetTextures()) {
                        HashValue(hash, reinterpret_cast<uintptr_t>(texture.Get()));
                    }
                }
            }
            HashValue(hash, renderer.CastShadow());
            HashValue(hash, renderer.ReceiveShadow());
        }
        return hash;
    }

    uint64_t SceneAdapter::CalculateTransformHash() const
    {
        uint64_t hash = kFnvOffset;
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) return hash;
        auto view = scene->GetObjectsWithComponents<MeshRenderer, TransformComponent>();
        for (ObjectID id : GetSortedMeshEntities()) {
            const auto matrix = ToGpuMatrix(view.get<TransformComponent>(id).GetLocalToWorld());
            HashValue(hash, StableID(id));
            HashValue(hash, matrix);
        }
        return hash;
    }

    uint64_t SceneAdapter::CalculateLightHash() const
    {
        uint64_t hash = kFnvOffset;
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) return hash;
        auto view = scene->GetObjectsWithComponents<Light>();
        std::vector<ObjectID> ids(view.begin(), view.end());
        std::ranges::sort(ids, {}, StableID);
        for (ObjectID id : ids) {
            const auto object = scene->GetObjectByID(id).lock();
            if (object == nullptr || !object->IsEnabled()) continue;
            const auto& light = view.get<Light>(id);
            HashValue(hash, StableID(id));
            HashValue(hash, light.GetType());
            HashValue(hash, ToGpuFloat4(light.GetColor()));
            HashValue(hash, ToGpuFloat4(light.GetPosition()));
            HashValue(hash, ToGpuFloat4(light.GetDirection()));
            HashValue(hash, light.GetRange());
            HashValue(hash, light.GetInnerAngle());
            HashValue(hash, light.GetOuterAngle());
        }
        return hash;
    }

    uint64_t SceneAdapter::CalculateLightDistributionHash() const
    {
        uint64_t hash = kFnvOffset;
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) return hash;
        auto view = scene->GetObjectsWithComponents<Light>();
        std::vector<ObjectID> ids(view.begin(), view.end());
        std::ranges::sort(ids, {}, StableID);
        for (ObjectID id : ids) {
            const auto object = scene->GetObjectByID(id).lock();
            if (object == nullptr || !object->IsEnabled()) continue;
            const auto& light = view.get<Light>(id);
            HashValue(hash, StableID(id));
            HashValue(hash, light.GetType());
            HashValue(hash, ToGpuFloat4(light.GetColor()));
            HashValue(hash, light.GetRange());
            HashValue(hash, light.GetInnerAngle());
            HashValue(hash, light.GetOuterAngle());
        }
        return hash;
    }

    void SceneAdapter::GatherScene(const Settings& settings)
    {
        m_Vertices.clear();
        m_Indices.clear();
        m_Geometries.clear();
        m_Instances.clear();
        m_Materials.clear();
        m_EmissiveTriangles.clear();
        m_TLASInstances.clear();
        m_BLASRecords.clear();
        m_InstanceBLASIndices.clear();
        m_StableInstanceIDs.clear();
        m_InstanceMeshes.clear();
        m_CurrentTransforms.clear();
        m_InstanceMasks.clear();
        m_InstanceFlags.clear();
        m_Textures.clear();

        std::unordered_map<ITexture*, uint32_t> textureIndices{};
        auto addFallback = [&](TextureManager::DefaultTexture id) {
            const auto texture = TextureManager::GetDefaultTexture(id);
            const uint32_t index = static_cast<uint32_t>(m_Textures.size());
            m_Textures.push_back(texture);
            textureIndices.emplace(texture.Get(), index);
        };
        addFallback(TextureManager::kWhiteOpaque2D);
        addFallback(TextureManager::kDefaultNormalTex);
        addFallback(TextureManager::kBlackOpaque2D);

        std::unordered_map<Material*, uint32_t> materialIndices{};
        std::unordered_map<Mesh*, uint32_t> meshIndices{};
        std::unordered_map<Mesh*, uint32_t> meshVertexBases{};
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr)
            return;
        auto view = scene->GetObjectsWithComponents<MeshRenderer, TransformComponent>();

        for (ObjectID id : GetSortedMeshEntities()) {
            auto& renderer = view.get<MeshRenderer>(id);
            const auto& transform = view.get<TransformComponent>(id);
            const auto mesh = renderer.GetMesh();
            uint32_t blasIndex = 0;
            if (const auto meshIt = meshIndices.find(mesh.get()); meshIt != meshIndices.end()) {
                blasIndex = meshIt->second;
            }
            else {
                blasIndex = static_cast<uint32_t>(m_BLASRecords.size());
                meshIndices.emplace(mesh.get(), blasIndex);
                const uint32_t vertexBase = static_cast<uint32_t>(m_Vertices.size());
                meshVertexBases.emplace(mesh.get(), vertexBase);

                for (size_t vertexIndex = 0; vertexIndex < mesh->vertices.size(); ++vertexIndex) {
                    GpuVertex vertex{};
                    vertex.position = ToGpuFloat4(mesh->vertices[vertexIndex], 1.0f);
                    vertex.normal = vertexIndex < mesh->normals.size()
                        ? ToGpuFloat4(mesh->normals[vertexIndex]) : GpuFloat4{0, 1, 0, 0};
                    vertex.tangent = vertexIndex < mesh->tangents.size()
                        ? ToGpuFloat4(mesh->tangents[vertexIndex]) : GpuFloat4{1, 0, 0, 1};
                    vertex.uv = vertexIndex < mesh->uv.size()
                        ? GpuFloat4{mesh->uv[vertexIndex].Get(0), mesh->uv[vertexIndex].Get(1), 0, 0}
                        : GpuFloat4{};
                    m_Vertices.push_back(vertex);
                }

                BlasRecord record{};
                record.mesh = mesh;
                const auto indices32 = GetMeshIndices(*mesh);
                const auto indices16 = GetMeshIndices16(*mesh);
                for (size_t submeshIndex = 0; submeshIndex < mesh->GetSubMeshCount(); ++submeshIndex) {
                    const auto submesh = mesh->GetSubMesh(submeshIndex);
                    const uint32_t indexOffset = static_cast<uint32_t>(m_Indices.size());
                    for (size_t index = 0; index < submesh.indexCount; ++index) {
                        const size_t sourceIndex = submesh.indexOffset + index;
                        const uint32_t value = !indices32.empty()
                            ? (sourceIndex < indices32.size() ? indices32[sourceIndex] : 0u)
                            : (sourceIndex < indices16.size() ? indices16[sourceIndex] : 0u);
                        m_Indices.push_back(value);
                    }
                    const uint32_t submeshVertexBase = vertexBase + static_cast<uint32_t>(submesh.vertexOffset);
                    const uint32_t vertexCount = static_cast<uint32_t>(
                        mesh->vertices.size() > submesh.vertexOffset
                            ? mesh->vertices.size() - submesh.vertexOffset : 0);
                    record.buildRanges.push_back({
                        submeshVertexBase, indexOffset,
                        static_cast<uint32_t>(submesh.indexCount), vertexCount});
                }
                m_BLASRecords.push_back(std::move(record));
            }

            const uint32_t geometryBase = static_cast<uint32_t>(m_Geometries.size());
            const auto& record = m_BLASRecords[blasIndex];
            bool allOpaque = true;
            bool anyTwoSided = false;
            for (size_t submeshIndex = 0; submeshIndex < mesh->GetSubMeshCount(); ++submeshIndex) {
                const size_t rendererMaterialIndex = renderer.GetMaterialIndexOrDefault(submeshIndex);
                const auto& rendererMaterials = renderer.GetMaterials();
                const auto material = rendererMaterialIndex < rendererMaterials.size()
                    ? rendererMaterials[rendererMaterialIndex] : nullptr;
                uint32_t gpuMaterialIndex = 0;
                if (const auto materialIt = materialIndices.find(material.get()); materialIt != materialIndices.end()) {
                    gpuMaterialIndex = materialIt->second;
                }
                else {
                    gpuMaterialIndex = static_cast<uint32_t>(m_Materials.size());
                    materialIndices.emplace(material.get(), gpuMaterialIndex);
                    GpuMaterial gpuMaterial{};
                    gpuMaterial.baseColor = material != nullptr
                        ? ToGpuFloat4(material->GetBaseColor()) : GpuFloat4{1, 1, 1, 1};
                    gpuMaterial.emissiveColor = material != nullptr
                        ? ToGpuFloat4(material->GetEmissiveColor()) : GpuFloat4{};
                    gpuMaterial.factors = material != nullptr
                        ? GpuFloat4{material->GetNormalTexScale(), material->GetMetallicFactor(),
                            material->GetRoughnessFactor(), settings.alphaCutoff}
                        : GpuFloat4{1, 0, 1, settings.alphaCutoff};
                    const auto textures = material != nullptr ? material->GetTextures() : decltype(material->GetTextures()){};
                    gpuMaterial.texture0 = {
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kBaseColor], 0),
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kDiffuseRoughness], 0),
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kMetalness], 0),
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kNormal], 1)};
                    gpuMaterial.texture1 = {
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kEmissive], 0),
                        AddTexture(m_Textures, textureIndices, textures[ShaderResource::kOcclusion], 0),
                        (material != nullptr && material->IsTransparent() ? 1u : 0u) |
                            (material != nullptr && material->IsBothSide() ? 2u : 0u), 0};
                    m_Materials.push_back(gpuMaterial);
                }
                const auto& range = record.buildRanges[submeshIndex];
                m_Geometries.push_back({{range.x, range.y, range.z, gpuMaterialIndex}});
                allOpaque &= material == nullptr || !material->IsTransparent();
                anyTwoSided |= material != nullptr && material->IsBothSide();
            }

            const uint32_t stableID = StableID(id);
            const Math::Matrix4 world = transform.GetLocalToWorld();
            const auto previousIt = m_PreviousTransforms.find(stableID);
            const GpuMatrix currentGpu = ToGpuMatrix(world);
            const GpuMatrix previousGpu = previousIt != m_PreviousTransforms.end()
                ? previousIt->second : currentGpu;
            m_Instances.push_back({currentGpu, previousGpu,
                {stableID, geometryBase, static_cast<uint32_t>(mesh->GetSubMeshCount()),
                    renderer.ReceiveShadow() ? 1u : 0u}});
            m_InstanceBLASIndices.push_back(blasIndex);
            m_StableInstanceIDs.push_back(stableID);
            m_InstanceMeshes.push_back(mesh);
            m_CurrentTransforms.push_back(world);
            m_InstanceMasks.push_back(kPrimaryInstanceMask |
                (renderer.CastShadow() ? kShadowInstanceMask : 0u));
            RT::InstanceFlags flags = allOpaque
                ? RT::InstanceFlags::ForceOpaque : RT::InstanceFlags::ForceNonOpaque;
            if (anyTwoSided) flags |= RT::InstanceFlags::TriangleCullDisable;
            m_InstanceFlags.push_back(flags);
        }

        m_LogicalInstanceCount = static_cast<uint32_t>(m_Instances.size());
        if (m_LogicalInstanceCount == 0) {
            const uint32_t vertexBase = static_cast<uint32_t>(m_Vertices.size());
            m_Vertices.push_back({{-10000, -10000, 10000, 1}, {0, 0, -1, 0}, {1, 0, 0, 1}, {0, 0, 0, 0}});
            m_Vertices.push_back({{-9999, -10000, 10000, 1}, {0, 0, -1, 0}, {1, 0, 0, 1}, {1, 0, 0, 0}});
            m_Vertices.push_back({{-10000, -9999, 10000, 1}, {0, 0, -1, 0}, {1, 0, 0, 1}, {0, 1, 0, 0}});
            const uint32_t indexOffset = static_cast<uint32_t>(m_Indices.size());
            m_Indices.insert(m_Indices.end(), {0, 1, 2});
            if (m_Materials.empty()) {
                GpuMaterial material{};
                material.baseColor = {1, 1, 1, 1};
                material.factors = {1, 0, 1, settings.alphaCutoff};
                material.texture0 = {0, 0, 0, 1};
                material.texture1 = {2, 0, 0, 0};
                m_Materials.push_back(material);
            }
            m_Geometries.push_back({{vertexBase, indexOffset, 3, 0}});
            BlasRecord dummy{};
            dummy.buildRanges.push_back({vertexBase, indexOffset, 3, 3});
            m_BLASRecords.push_back(std::move(dummy));
            m_Instances.push_back({GpuMatrix{}, GpuMatrix{}, {kInvalidIndex, 0, 1, 0}});
            m_InstanceBLASIndices.push_back(0);
            m_StableInstanceIDs.push_back(kInvalidIndex);
            m_InstanceMeshes.push_back(nullptr);
            m_CurrentTransforms.push_back(Math::Matrix4::Identity);
            m_InstanceMasks.push_back(0);
            m_InstanceFlags.push_back(RT::InstanceFlags::ForceOpaque);
        }
        RefreshEmissiveDistribution();
    }

    void SceneAdapter::RefreshEmissiveDistribution()
    {
        m_EmissiveTriangles.clear();
        std::vector<float> weights{};
        for (uint32_t instanceIndex = 0; instanceIndex < m_LogicalInstanceCount; ++instanceIndex) {
            const auto& instance = m_Instances[instanceIndex];
            for (uint32_t localGeometry = 0; localGeometry < instance.data.z; ++localGeometry) {
                const auto& geometry = m_Geometries[instance.data.y + localGeometry];
                const auto& material = m_Materials[geometry.data.w];
                const float emissiveLuminance = Luminance(material.emissiveColor);
                if (!(emissiveLuminance > 1e-6f)) continue;
                for (uint32_t triangle = 0; triangle + 2 < geometry.data.z; triangle += 3) {
                    const uint32_t indexOffset = geometry.data.y + triangle;
                    const uint32_t i0 = geometry.data.x + m_Indices[indexOffset];
                    const uint32_t i1 = geometry.data.x + m_Indices[indexOffset + 1];
                    const uint32_t i2 = geometry.data.x + m_Indices[indexOffset + 2];
                    if (i0 >= m_Vertices.size() || i1 >= m_Vertices.size() || i2 >= m_Vertices.size()) continue;
                    const auto toVector = [](const GpuFloat4& value) {
                        return Math::Vector3{value.x, value.y, value.z};
                    };
                    const auto p0 = TransformPoint(toVector(m_Vertices[i0].position), m_CurrentTransforms[instanceIndex]);
                    const auto p1 = TransformPoint(toVector(m_Vertices[i1].position), m_CurrentTransforms[instanceIndex]);
                    const auto p2 = TransformPoint(toVector(m_Vertices[i2].position), m_CurrentTransforms[instanceIndex]);
                    const float area = 0.5f * float(Math::Vector3::Cross(p1 - p0, p2 - p0).Magnitude());
                    if (!(area > 1e-8f)) continue;
                    const float power = area * emissiveLuminance;
                    const uint32_t stableID = (instance.data.x * 16777619u) ^ (indexOffset + 0x9E3779B9u);
                    m_EmissiveTriangles.push_back({
                        {instanceIndex, indexOffset, geometry.data.w, stableID},
                        {area, power, 0, 0}});
                    weights.push_back(power);
                }
            }
        }
        m_EmissiveAlias = BuildAliasTable(weights);
        m_EmissiveCount = static_cast<uint32_t>(m_EmissiveTriangles.size());
    }

    void SceneAdapter::GatherLights()
    {
        m_Lights.clear();
        std::vector<float> weights{};
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) {
            m_AnalyticAlias = {};
            m_LightCount = 0;
            return;
        }
        auto view = scene->GetObjectsWithComponents<Light>();
        std::vector<ObjectID> ids(view.begin(), view.end());
        std::ranges::sort(ids, {}, StableID);
        for (ObjectID id : ids) {
            const auto object = scene->GetObjectByID(id).lock();
            if (object == nullptr || !object->IsEnabled()) continue;
            const auto& light = view.get<Light>(id);
            const auto color = ToGpuFloat4(light.GetColor());
            const float range = std::max(light.GetRange(), 1e-4f);
            const Math::Vector3 direction = -light.GetDirection();
            float power = Luminance(color);
            if (light.GetType() == LightType::Directional) {
                power *= 4.0f * std::numbers::pi_v<float>;
            }
            else if (light.GetType() == LightType::Point) {
                power *= 4.0f * std::numbers::pi_v<float> * range * range;
            }
            else {
                power *= 2.0f * std::numbers::pi_v<float> *
                    (1.0f - std::cos(light.GetOuterAngle())) * range * range;
            }
            GpuAnalyticLight gpuLight{};
            gpuLight.color = color;
            gpuLight.positionInvRange = ToGpuFloat4(light.GetPosition(), 1.0f / range);
            gpuLight.directionType = ToGpuFloat4(direction, static_cast<float>(light.GetType()));
            gpuLight.anglesPower = {light.GetInnerAngle(), light.GetOuterAngle(), power, 0};
            gpuLight.metadata = {StableID(id), 0, 0, 0};
            m_Lights.push_back(gpuLight);
            weights.push_back(power);
        }
        m_AnalyticAlias = BuildAliasTable(weights);
        m_LightCount = static_cast<uint32_t>(m_Lights.size());
    }

    void SceneAdapter::BuildTLASInstances()
    {
        m_TLASInstances.resize(m_Instances.size());
        for (uint32_t index = 0; index < m_Instances.size(); ++index) {
            RT::AffineTransform affine{};
            ToAffineTransform(m_CurrentTransforms[index], affine);
            m_TLASInstances[index]
                .SetTransform(affine)
                .SetInstanceID(index)
                .SetInstanceMask(m_InstanceMasks[index])
                .SetInstanceContributionToHitGroupIndex(0)
                .SetFlags(m_InstanceFlags[index])
                .SetBottomLevelAS(m_BLASRecords[m_InstanceBLASIndices[index]].accelerationStructure);
        }
    }

    void SceneAdapter::RecreateSceneResources(IDevice* device)
    {
        m_VertexBuffer = CreateStructuredBuffer<GpuVertex>(device, m_Vertices.size(), "ReSTIR DI Vertices");
        m_IndexBuffer = CreateStructuredBuffer<uint32_t>(device, m_Indices.size(), "ReSTIR DI Indices");
        m_GeometryBuffer = CreateStructuredBuffer<GpuGeometry>(device, m_Geometries.size(), "ReSTIR DI Geometries");
        m_InstanceBuffer = CreateStructuredBuffer<GpuInstance>(device, m_Instances.size(), "ReSTIR DI Instances");
        m_MaterialBuffer = CreateStructuredBuffer<GpuMaterial>(device, m_Materials.size(), "ReSTIR DI Materials");

        for (uint32_t blasIndex = 0; blasIndex < m_BLASRecords.size(); ++blasIndex) {
            auto& record = m_BLASRecords[blasIndex];
            record.geometries.clear();
            record.vertexBuffers.clear();
            record.indexBuffers.clear();
            RT::AccelStructDesc description{};
            description.SetIsTopLevel(false)
                .SetIsVirtual(true)
                .SetBuildFlags(RT::AccelStructBuildFlags::PreferFastTrace)
                .SetDebugName("ReSTIR DI BLAS " + std::to_string(blasIndex));
            for (const auto& range : record.buildRanges) {
                auto vertexBuffer = CreateStructuredBuffer<GpuVertex>(device, range.w,
                    "ReSTIR DI BLAS Vertices");
                auto indexBuffer = CreateStructuredBuffer<uint32_t>(device, range.z,
                    "ReSTIR DI BLAS Indices");
                RT::GeometryTriangles triangles{};
                triangles.SetVertexBuffer(vertexBuffer)
                    .SetVertexOffset(0)
                    .SetVertexStride(sizeof(GpuVertex))
                    .SetVertexCount(range.w)
                    .SetVertexFormat(Format::RGB32_FLOAT)
                    .SetIndexBuffer(indexBuffer)
                    .SetIndexOffset(0)
                    .SetIndexCount(range.z)
                    .SetIndexFormat(Format::R32_UINT);
                RT::GeometryDesc geometry{};
                geometry.SetTriangles(triangles).SetFlags(RT::GeometryFlags::None);
                record.geometries.push_back(geometry);
                record.vertexBuffers.push_back(std::move(vertexBuffer));
                record.indexBuffers.push_back(std::move(indexBuffer));
                description.AddBottomLevelGeometry(geometry);
            }
            record.accelerationStructure = device->CreateAccelStruct(description);
            const auto requirements = device->GetAccelStructMemoryRequirements(record.accelerationStructure);
            record.heap = device->CreateHeap(HeapDesc{}
                .SetCapacity(requirements.size)
                .SetType(HeapType::Default)
                .SetDebugName("ReSTIR DI BLAS Heap " + std::to_string(blasIndex)));
            device->BindAccelStructMemory(record.accelerationStructure, record.heap, 0);
        }

        RT::AccelStructDesc tlasDescription{};
        tlasDescription.SetIsTopLevel(true)
            .SetTopLevelMaxInstances(std::max<size_t>(m_Instances.size(), 1))
            .SetIsVirtual(true)
            .SetBuildFlags(RT::AccelStructBuildFlags::AllowUpdate | RT::AccelStructBuildFlags::PreferFastTrace)
            .SetDebugName("ReSTIR DI TLAS");
        m_TLAS = device->CreateAccelStruct(tlasDescription);
        const auto tlasRequirements = device->GetAccelStructMemoryRequirements(m_TLAS);
        m_TLASHeap = device->CreateHeap(HeapDesc{}
            .SetCapacity(tlasRequirements.size)
            .SetType(HeapType::Default)
            .SetDebugName("ReSTIR DI TLAS Heap"));
        device->BindAccelStructMemory(m_TLAS, m_TLASHeap, 0);
        BuildTLASInstances();
        m_NeedsFullBuild = true;
    }

    void SceneAdapter::RecreateDistributionResources(IDevice* device)
    {
        auto ensure = [device]<typename T>(BufferHandle& buffer, size_t count, const char* name) {
            const size_t byteSize = std::max<size_t>(count, 1) * sizeof(T);
            if (buffer == nullptr || buffer->GetDesc().byteSize < byteSize) {
                buffer = CreateStructuredBuffer<T>(device, count, name);
            }
        };
        ensure.template operator()<GpuAnalyticLight>(m_LightBuffer, m_Lights.size(), "ReSTIR DI Analytic Lights");
        ensure.template operator()<GpuAliasEntry>(m_LightAliasBuffer, m_AnalyticAlias.entries.size(), "ReSTIR DI Analytic Alias");
        ensure.template operator()<GpuEmissiveTriangle>(m_EmissiveBuffer, m_EmissiveTriangles.size(), "ReSTIR DI Emissive Triangles");
        ensure.template operator()<GpuAliasEntry>(m_EmissiveAliasBuffer, m_EmissiveAlias.entries.size(), "ReSTIR DI Emissive Alias");
        m_NeedsDistributionUpload = true;
    }

    void SceneAdapter::UpdateTransforms()
    {
        const auto scene = DSMEngine::sm_GlobalContext.scene;
        if (scene == nullptr) return;
        for (uint32_t index = 0; index < m_LogicalInstanceCount; ++index) {
            const uint32_t stableID = m_StableInstanceIDs[index];
            const auto object = scene->GetObjectByID(static_cast<ObjectID>(stableID)).lock();
            if (object == nullptr) continue;
            const auto transform = object->GetComponent<TransformComponent>();
            if (transform == nullptr) continue;
            const auto current = transform->GetLocalToWorld();
            const auto currentGpu = ToGpuMatrix(current);
            const auto previousIt = m_PreviousTransforms.find(stableID);
            m_Instances[index].currentLocalToWorld = currentGpu;
            m_Instances[index].previousLocalToWorld = previousIt != m_PreviousTransforms.end()
                ? previousIt->second : currentGpu;
            m_CurrentTransforms[index] = current;
        }
    }

    SceneSyncResult SceneAdapter::Synchronize(IDevice* device, const Settings& settings, std::string& error)
    {
        SceneSyncResult result{};
        if (device == nullptr || DSMEngine::sm_GlobalContext.scene == nullptr) {
            error = "ReSTIR DI 场景同步缺少 Device 或 Scene。";
            return result;
        }

        const uint64_t topologyHash = CalculateTopologyHash(settings);
        const uint64_t transformHash = CalculateTransformHash();
        const uint64_t lightHash = CalculateLightHash();
        const uint64_t lightDistributionHash = CalculateLightDistributionHash();
        const bool topologyChanged = m_FirstSync || topologyHash != m_TopologyHash;

        if (topologyChanged) {
            if (!device->WaitForIdle()) {
                error = "重建 ReSTIR DI 场景前等待 GPU 失败。";
                return result;
            }
            m_PreviousTransforms.clear();
            GatherScene(settings);
            GatherLights();
            RecreateSceneResources(device);
            RecreateDistributionResources(device);
            result.topologyRebuilt = true;
            result.materialDistributionUpdated = true;
            result.lightDistributionUpdated = true;
            result.historyResetRequired = true;
        }
        else {
            UpdateTransforms();
            if (transformHash != m_TransformHash) {
                BuildTLASInstances();
                RefreshEmissiveDistribution();
                RecreateDistributionResources(device);
                m_NeedsTLASUpdate = true;
                result.transformsUpdated = true;
            }
            if (lightHash != m_LightHash) {
                GatherLights();
                RecreateDistributionResources(device);
                result.lightDistributionUpdated = true;
                result.historyResetRequired |= lightDistributionHash != m_LightDistributionHash;
            }
        }

        for (uint32_t index = 0; index < m_LogicalInstanceCount; ++index) {
            m_PreviousTransforms[m_StableInstanceIDs[index]] = m_Instances[index].currentLocalToWorld;
        }
        m_TopologyHash = topologyHash;
        m_TransformHash = transformHash;
        m_LightHash = lightHash;
        m_LightDistributionHash = lightDistributionHash;
        m_FirstSync = false;
        error.clear();
        return result;
    }

    void SceneAdapter::RecordBuildAndUpload(ICommandList* commandList)
    {
        if (commandList == nullptr) return;
        auto write = [commandList]<typename T>(IBuffer* buffer, const std::vector<T>& data) {
            if (buffer != nullptr && !data.empty()) {
                commandList->WriteBuffer(buffer, data.data(), data.size() * sizeof(T));
            }
        };

        if (m_NeedsFullBuild) {
            write(m_VertexBuffer, m_Vertices);
            write(m_IndexBuffer, m_Indices);
            write(m_GeometryBuffer, m_Geometries);
            write(m_MaterialBuffer, m_Materials);
            for (auto& record : m_BLASRecords) {
                for (size_t geometryIndex = 0; geometryIndex < record.buildRanges.size(); ++geometryIndex) {
                    const auto& range = record.buildRanges[geometryIndex];
                    commandList->WriteBuffer(record.vertexBuffers[geometryIndex],
                        m_Vertices.data() + range.x, size_t(range.w) * sizeof(GpuVertex));
                    commandList->WriteBuffer(record.indexBuffers[geometryIndex],
                        m_Indices.data() + range.y, size_t(range.z) * sizeof(uint32_t));
                    commandList->SetBufferState(record.vertexBuffers[geometryIndex],
                        ResourceStates::AccelStructBuildInput);
                    commandList->SetBufferState(record.indexBuffers[geometryIndex],
                        ResourceStates::AccelStructBuildInput);
                }
            }
            commandList->CommitBarriers();
            for (auto& record : m_BLASRecords) {
                commandList->BuildBottomLevelAccelStruct(
                    record.accelerationStructure, record.geometries,
                    RT::AccelStructBuildFlags::PreferFastTrace);
            }
        }

        write(m_InstanceBuffer, m_Instances);
        if (m_NeedsDistributionUpload || m_NeedsFullBuild) {
            write(m_LightBuffer, m_Lights);
            write(m_LightAliasBuffer, m_AnalyticAlias.entries);
            write(m_EmissiveBuffer, m_EmissiveTriangles);
            write(m_EmissiveAliasBuffer, m_EmissiveAlias.entries);
        }

        if (m_NeedsFullBuild || m_NeedsTLASUpdate) {
            auto flags = RT::AccelStructBuildFlags::AllowUpdate | RT::AccelStructBuildFlags::PreferFastTrace;
            if (!m_NeedsFullBuild) flags |= RT::AccelStructBuildFlags::PerformUpdate;
            commandList->BuildTopLevelAccelStruct(m_TLAS, m_TLASInstances, flags);
        }
        m_NeedsFullBuild = false;
        m_NeedsTLASUpdate = false;
        m_NeedsDistributionUpload = false;
    }

    void SceneAdapter::Reset()
    {
        m_Vertices.clear();
        m_Indices.clear();
        m_Geometries.clear();
        m_Instances.clear();
        m_Materials.clear();
        m_Lights.clear();
        m_EmissiveTriangles.clear();
        m_AnalyticAlias = {};
        m_EmissiveAlias = {};
        m_Textures.clear();
        m_TLASInstances.clear();
        m_BLASRecords.clear();
        m_InstanceBLASIndices.clear();
        m_StableInstanceIDs.clear();
        m_InstanceMeshes.clear();
        m_CurrentTransforms.clear();
        m_InstanceMasks.clear();
        m_InstanceFlags.clear();
        m_PreviousTransforms.clear();
        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;
        m_GeometryBuffer = nullptr;
        m_InstanceBuffer = nullptr;
        m_MaterialBuffer = nullptr;
        m_LightBuffer = nullptr;
        m_LightAliasBuffer = nullptr;
        m_EmissiveBuffer = nullptr;
        m_EmissiveAliasBuffer = nullptr;
        m_TLAS = nullptr;
        m_TLASHeap = nullptr;
        m_TopologyHash = m_TransformHash = m_LightHash = m_LightDistributionHash = 0;
        m_LogicalInstanceCount = m_LightCount = m_EmissiveCount = 0;
        m_NeedsFullBuild = m_NeedsTLASUpdate = m_NeedsDistributionUpload = false;
        m_FirstSync = true;
    }

}
