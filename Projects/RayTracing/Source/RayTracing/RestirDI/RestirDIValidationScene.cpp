#include "RestirDIValidationScene.h"

#include "Runtime/Framework/Component/Light.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Render/Geometry.h"
#include "Runtime/Render/Material.h"
#include "Runtime/Render/Mesh.h"

#include <array>
#include <cmath>
#include <numbers>

namespace DSM::RestirDI {
    namespace {
        std::shared_ptr<Mesh> CreateMesh(const std::string& name, Geometry::GeometryMesh geometry)
        {
            auto mesh = std::make_shared<Mesh>();
            std::vector<Math::Vector3> positions{};
            std::vector<Math::Vector3> normals{};
            std::vector<Math::Vector4> tangents{};
            std::vector<Math::Vector2> uv{};
            positions.reserve(geometry.vertices.size());
            normals.reserve(geometry.vertices.size());
            tangents.reserve(geometry.vertices.size());
            uv.reserve(geometry.vertices.size());
            for (const auto& vertex : geometry.vertices) {
                positions.push_back(vertex.position);
                normals.push_back(vertex.normal);
                tangents.push_back(vertex.tangent);
                uv.push_back(vertex.texCoord);
            }
            mesh->SetName(name)
                .SetIndexFormat(Format::R32_UINT)
                .SetVertices(std::move(positions))
                .SetNormals(std::move(normals))
                .SetTangents(std::move(tangents))
                .SetUVs(std::move(uv))
                .SetIndices<uint32_t>(geometry.indices32, PrimitiveType::TriangleList, 0);
            return mesh;
        }

        std::shared_ptr<Mesh> CreateAlphaQuad()
        {
            Geometry::GeometryMesh geometry{};
            geometry.vertices = {
                {{-1.8f, 0.0f, 0.0f}, {0, 0, -1}, {1, 0, 0, 1}, {}, {0, 1}},
                {{-1.8f, 3.0f, 0.0f}, {0, 0, -1}, {1, 0, 0, 1}, {}, {0, 0}},
                {{ 1.8f, 3.0f, 0.0f}, {0, 0, -1}, {1, 0, 0, 1}, {}, {1, 0}},
                {{ 1.8f, 0.0f, 0.0f}, {0, 0, -1}, {1, 0, 0, 1}, {}, {1, 1}},
            };
            geometry.indices32 = {0, 1, 2, 0, 2, 3};
            return CreateMesh("ReSTIR DI Alpha Quad", std::move(geometry));
        }

        std::shared_ptr<Material> CreateMaterial(
            Math::Vector4 baseColor,
            float metallic = 0.0f,
            float roughness = 0.6f)
        {
            auto material = std::make_shared<Material>(std::shared_ptr<Shader>{});
            material->SetBaseColor(baseColor);
            material->SetMetallicFactor(metallic);
            material->SetRoughnessFactor(roughness);
            return material;
        }

        ObjectID AddMeshObject(
            Scene& scene,
            const std::string& name,
            const std::shared_ptr<Mesh>& mesh,
            const std::shared_ptr<Material>& material,
            Math::Vector3 position,
            bool castShadow = true)
        {
            const ObjectID id = scene.CreateObject(name);
            const auto object = scene.GetObjectByID(id).lock();
            object->GetComponent<TransformComponent>()->SetPosition(position);
            auto renderer = object->AddComponent<MeshRenderer>();
            renderer->SetMesh(mesh);
            renderer->SetMaterial(0, material);
            renderer->SetMaterialIndex(0, 0);
            renderer->SetCastShadow(castShadow);
            return id;
        }

        TextureHandle CreateAlphaChecker(IDevice* device)
        {
            constexpr uint32_t size = 8;
            std::array<uint32_t, size * size> pixels{};
            for (uint32_t y = 0; y < size; ++y) {
                for (uint32_t x = 0; x < size; ++x) {
                    const bool opaque = ((x / 2u) + (y / 2u)) % 2u == 0u;
                    pixels[y * size + x] = opaque ? 0xFF80FF80u : 0x0080FF80u;
                }
            }
            auto texture = device->CreateTexture(TextureDesc{}
                .SetWidth(size)
                .SetHeight(size)
                .SetFormat(Format::RGBA8_UNORM)
                .SetInitialState(ResourceStates::CopyDest)
                .SetDebugName("ReSTIR DI Alpha Checker"));
            auto commandList = device->CreateCommandList(CommandListParameters{}
                .SetDebugName("Upload ReSTIR DI Alpha Checker"));
            commandList->Open();
            commandList->WriteTexture(texture, 0, 0, pixels.data(), size * sizeof(uint32_t));
            commandList->SetTextureState(texture, AllSubresources, ResourceStates::ShaderResource);
            commandList->Close();
            device->ExecuteCommandList(commandList);
            return texture;
        }
    }

    ValidationSceneState CreateValidationScene(IDevice* device, std::string& error)
    {
        ValidationSceneState result{};
        if (device == nullptr) {
            error = "创建 ReSTIR DI 验证场景时 Device 为空。";
            return result;
        }
        result.scene = std::make_shared<Scene>();

        const auto white = CreateMaterial({0.72f, 0.72f, 0.72f, 1.0f}, 0.0f, 0.75f);
        const auto red = CreateMaterial({0.70f, 0.08f, 0.06f, 1.0f}, 0.0f, 0.65f);
        const auto green = CreateMaterial({0.08f, 0.55f, 0.12f, 1.0f}, 0.0f, 0.65f);
        const auto metal = CreateMaterial({0.72f, 0.58f, 0.28f, 1.0f}, 0.92f, 0.18f);

        AddMeshObject(*result.scene, "Floor",
            CreateMesh("Floor", Geometry::GeometryGenerator::CreateBox(12.0f, 0.2f, 12.0f, 0)),
            white, {0, -1.1f, 2.0f});
        AddMeshObject(*result.scene, "Back Wall",
            CreateMesh("Back Wall", Geometry::GeometryGenerator::CreateBox(12.0f, 8.0f, 0.2f, 0)),
            white, {0, 2.9f, 8.0f});
        AddMeshObject(*result.scene, "Left Wall",
            CreateMesh("Left Wall", Geometry::GeometryGenerator::CreateBox(0.2f, 8.0f, 12.0f, 0)),
            red, {-6.0f, 2.9f, 2.0f});
        AddMeshObject(*result.scene, "Right Wall",
            CreateMesh("Right Wall", Geometry::GeometryGenerator::CreateBox(0.2f, 8.0f, 12.0f, 0)),
            green, {6.0f, 2.9f, 2.0f});
        AddMeshObject(*result.scene, "PBR Sphere",
            CreateMesh("PBR Sphere", Geometry::GeometryGenerator::CreateSphere(1.2f, 32, 20)),
            metal, {-2.2f, 0.2f, 3.0f});
        result.movingObject = AddMeshObject(*result.scene, "Moving PBR Box",
            CreateMesh("Moving PBR Box", Geometry::GeometryGenerator::CreateBox(1.8f, 1.8f, 1.8f, 1)),
            CreateMaterial({0.12f, 0.25f, 0.78f, 1.0f}, 0.15f, 0.32f), {2.0f, -0.1f, 2.6f});

        auto emissive = CreateMaterial({1, 1, 1, 1}, 0.0f, 0.5f);
        emissive->SetEmissiveColor({18.0f, 9.0f, 3.0f, 1.0f});
        emissive->SetBothSide(true);
        AddMeshObject(*result.scene, "Emissive Panel",
            CreateMesh("Emissive Panel", Geometry::GeometryGenerator::CreateBox(3.0f, 0.08f, 1.5f, 0)),
            emissive, {0, 5.2f, 2.5f});

        auto alpha = CreateMaterial({1, 1, 1, 1}, 0.0f, 0.55f);
        alpha->SetTransparent(true);
        alpha->SetBothSide(true);
        alpha->SetTexture(ShaderResource::kBaseColor, CreateAlphaChecker(device));
        result.alphaObject = AddMeshObject(*result.scene, "Alpha Checker",
            CreateAlphaQuad(), alpha, {0, -0.9f, 0.5f});
        result.noShadowObject = AddMeshObject(*result.scene, "No Shadow Probe",
            CreateMesh("No Shadow Probe", Geometry::GeometryGenerator::CreateBox(1.0f, 2.0f, 1.0f, 0)),
            CreateMaterial({0.85f, 0.2f, 0.8f, 1.0f}, 0.0f, 0.45f), {0, 0, 5.0f}, false);
        result.noShadowReceiver = AddMeshObject(*result.scene, "No Shadow Receiver",
            CreateMesh("No Shadow Receiver", Geometry::GeometryGenerator::CreateBox(0.7f, 0.7f, 0.2f, 0)),
            CreateMaterial({0.9f, 0.9f, 0.9f, 1.0f}, 0.0f, 0.8f), {-1.2f, 0.0f, 6.2f});

        {
            const ObjectID id = result.scene->CreateObject("Directional Key Light");
            const auto object = result.scene->GetObjectByID(id).lock();
            auto light = object->AddComponent<Light>();
            light->SetType(LightType::Directional)
                .SetColor({1.4f, 1.55f, 1.8f, 1.0f})
                .SetDirection(Math::Vector3{0.35f, -1.0f, 0.25f});
            result.analyticLightObjects.push_back(id);
        }

        for (uint32_t lightIndex = 0; lightIndex < 128; ++lightIndex) {
            const uint32_t column = lightIndex % 16u;
            const uint32_t row = lightIndex / 16u;
            const float x = -5.0f + 10.0f * (column + 0.5f) / 16.0f;
            const float z = -2.5f + 9.0f * (row + 0.5f) / 8.0f;
            const float y = 1.0f + 0.45f * float(lightIndex % 5u);
            const ObjectID id = result.scene->CreateObject("Analytic Light " + std::to_string(lightIndex));
            const auto object = result.scene->GetObjectByID(id).lock();
            auto light = object->AddComponent<Light>();
            const float hue = float(lightIndex % 7u) / 7.0f;
            const Math::Vector4 color{
                0.025f + 0.035f * std::sin(hue * 2.0f * std::numbers::pi_v<float> + 0.0f) * 0.5f + 0.0175f,
                0.025f + 0.035f * std::sin(hue * 2.0f * std::numbers::pi_v<float> + 2.1f) * 0.5f + 0.0175f,
                0.025f + 0.035f * std::sin(hue * 2.0f * std::numbers::pi_v<float> + 4.2f) * 0.5f + 0.0175f,
                1.0f};
            if ((lightIndex & 1u) == 0u) {
                light->SetType(LightType::Point)
                    .SetPosition({x, y, z})
                    .SetRange(4.5f)
                    .SetColor(color);
            }
            else {
                light->SetType(LightType::Spot)
                    .SetPosition({x, y + 1.5f, z})
                    .SetDirection(Math::Vector3{0, -1, 0})
                    .SetRange(6.0f)
                    .SetInnerAngle(0.28f)
                    .SetOuterAngle(0.62f)
                    .SetColor(color);
            }
            result.analyticLightObjects.push_back(id);
        }

        result.shadowProbeLight = result.scene->CreateObject("No Shadow Point Light");
        {
            const auto object = result.scene->GetObjectByID(result.shadowProbeLight).lock();
            object->AddComponent<Light>()
                ->SetType(LightType::Point)
                .SetPosition({1.2f, 0.0f, 3.8f})
                .SetRange(6.0f)
                .SetColor({8.0f, 8.0f, 8.0f, 1.0f});
            object->SetEnabled(false);
        }

        result.scene->SetDirty(false);
        error.clear();
        return result;
    }

}
