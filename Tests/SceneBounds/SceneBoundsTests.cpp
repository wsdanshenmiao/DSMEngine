#include "Runtime/Framework/Scene.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Math/Collision/BVH.h"
#include "Editor/Serializer/Serializer.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
    using DSM::GameObject;
    using DSM::MeshRenderer;
    using DSM::Scene;
    using DSM::TransformComponent;
    using DSM::Math::AxisAlignedBox;
    using DSM::Math::BVHTree;
    using DSM::Math::Vector3;

    bool Check(bool condition, std::string_view message)
    {
        if (!condition) std::cerr << "[FAIL] " << message << '\n';
        return condition;
    }

    bool SameVector(const Vector3& actual, const Vector3& expected)
    {
        for (size_t axis = 0; axis < 3; ++axis) {
            if (std::abs(static_cast<float>(actual.Get(axis)) - static_cast<float>(expected.Get(axis))) > 0.0002f)
                return false;
        }
        return true;
    }

    bool SameBox(const AxisAlignedBox& actual, const AxisAlignedBox& expected)
    {
        return actual.IsValid() && expected.IsValid() &&
            SameVector(actual.GetMin(), expected.GetMin()) &&
            SameVector(actual.GetMax(), expected.GetMax());
    }

    std::shared_ptr<GameObject> MakeObject(Scene& scene, std::string_view name,
        const AxisAlignedBox& localBounds, float x)
    {
        auto object = scene.GetObjectByID(scene.CreateObject(std::string(name))).lock();
        object->AddComponent<MeshRenderer>()->SetLocalBounds(localBounds);
        object->GetComponent<TransformComponent>()->SetPosition(x, 0, 0);
        return object;
    }

    bool CheckTree(const std::shared_ptr<BVHTree::BVHNode>& node,
        const std::shared_ptr<BVHTree::BVHNode>& parent = nullptr)
    {
        if (!Check(node != nullptr, "BVH 节点意外为空")) return false;
        if (!Check(node->parent.lock() == parent, "BVH 父节点链接不正确")) return false;
        if (node->object != nullptr) {
            return Check(node->left == nullptr && node->right == nullptr && node->height == 0,
                    "BVH 叶节点结构或高度不正确") &&
                Check(SameBox(node->bounds, node->object->GetComponent<MeshRenderer>()->GetBounds()),
                    "BVH 叶节点 bounds 没有匹配当前变换");
        }
        if (!Check(node->left != nullptr && node->right != nullptr,
                "BVH 内部节点缺少子节点")) return false;
        return CheckTree(node->left, node) && CheckTree(node->right, node) &&
            Check(node->height == std::max(node->left->height, node->right->height) + 1,
                "BVH 内部节点高度没有 refit") &&
            Check(SameBox(node->bounds, AxisAlignedBox::Union(node->left->bounds, node->right->bounds)),
                "BVH 内部节点 bounds 没有 refit");
    }

    bool TestRendererBounds()
    {
        Scene scene;
        auto object = scene.GetObjectByID(scene.CreateObject("renderer")).lock();
        auto renderer = object->AddComponent<MeshRenderer>();
        auto transform = object->GetComponent<TransformComponent>();
        const AxisAlignedBox local{{-1, -2, -3}, {1, 2, 3}};
        if (!Check(!renderer->GetBounds().IsValid(), "未设置局部盒时世界盒应无效")) return false;
        scene.SetDirty(false);
        renderer->SetLocalBounds(local);
        if (!Check(!scene.IsDirty(), "运行时设置局部盒不应隐式标脏场景")) return false;
        if (!Check(SameBox(renderer->GetBounds(), local), "默认 Transform 下的世界盒不等于局部盒")) return false;
        transform->SetPosition(10, 0, 0);
        transform->SetScale(2, 3, 4);
        if (!Check(SameBox(renderer->GetBounds(), AxisAlignedBox{{8, -6, -12}, {12, 6, 12}}),
                "平移和缩放后 GetBounds 没有更新")) return false;
        transform->SetPosition(-5, 7, 11);
        if (!Check(SameBox(renderer->GetBounds(), AxisAlignedBox{{-7, 1, -1}, {-3, 13, 23}}),
                "再次改变 Transform 后 GetBounds 仍是旧值")) return false;
        transform->SetScale(-2, 3, 4);
        if (!Check(SameBox(renderer->GetBounds(), AxisAlignedBox{{-7, 1, -1}, {-3, 13, 23}}),
                "负缩放后的世界盒错误")) return false;
        transform->SetPosition(0, 0, 0);
        transform->SetScale(1, 1, 1);
        transform->SetRotation(DSM::Math::Quaternion{Vector3{0, 0, 1}, std::numbers::pi_v<float> * 0.5f});
        return Check(SameBox(renderer->GetBounds(), AxisAlignedBox{{-2, -1, -3}, {2, 1, 3}}),
                "旋转后的世界盒错误") &&
            Check(SameBox(renderer->GetLocalBounds(), local), "Transform 修改了局部盒") &&
            Check(renderer->GetMesh() == nullptr, "盒测试不应创建网格或 GPU 资源");
    }

    bool TestSingleRoot()
    {
        Scene scene;
        auto object = MakeObject(scene, "single", AxisAlignedBox{{0, 0, 0}, {1, 1, 1}}, 3);
        BVHTree tree;
        auto leaf = tree.InsertNode(object);
        if (!Check(leaf != nullptr && tree.GetRoot() == leaf && tree.GetLeafNodes().size() == 1,
                "单根 BVH 插入失败") || !CheckTree(tree.GetRoot())) return false;
        tree.RemoveNode(leaf);
        return Check(tree.GetRoot() == nullptr && tree.GetLeafNodes().empty() && tree.FindNode(object) == nullptr,
            "单根删除后 BVH 未清空");
    }

    bool TestMultiNode()
    {
        Scene scene;
        const AxisAlignedBox local{{0, 0, 0}, {1, 1, 1}};
        auto a = MakeObject(scene, "a", local, 0);
        auto b = MakeObject(scene, "b", AxisAlignedBox{{10, 0, 0}, {11, 1, 1}}, 0);
        auto c = MakeObject(scene, "c", AxisAlignedBox{{20, 0, 0}, {21, 1, 1}}, 0);
        auto invalid = scene.GetObjectByID(scene.CreateObject("invalid")).lock();
        invalid->AddComponent<MeshRenderer>();
        BVHTree tree;
        if (!Check(tree.InsertNode(invalid) == nullptr && tree.GetRoot() == nullptr,
                "BVH 接受无效的初始盒")) return false;
        tree.InsertNode(a);
        tree.InsertNode(b);
        tree.InsertNode(c);
        tree.InsertNode(a);
        if (!Check(tree.GetLeafNodes().size() == 3 &&
                tree.GetRoot() != nullptr && SameBox(tree.GetRoot()->bounds, AxisAlignedBox{{0, 0, 0}, {21, 1, 1}}),
                "三叶树初始范围错误") || !CheckTree(tree.GetRoot())) return false;

        b->GetComponent<MeshRenderer>()->SetLocalBounds(AxisAlignedBox{{13, 0, 0}, {14, 1, 1}});
        tree.UpdateNode(b);
        if (!Check(tree.GetLeafNodes().size() == 3 && tree.FindNode(b) != nullptr,
                "BVH 更新近处节点后丢失叶节点") || !CheckTree(tree.GetRoot())) return false;
        c->GetComponent<MeshRenderer>()->SetLocalBounds(AxisAlignedBox{{40, 0, 0}, {41, 1, 1}});
        tree.UpdateNode(c);
        if (!Check(tree.GetRoot() != nullptr && SameBox(tree.GetRoot()->bounds, AxisAlignedBox{{0, 0, 0}, {41, 1, 1}}),
                "BVH 节点移出旧范围后根盒未扩张") || !CheckTree(tree.GetRoot())) return false;
        c->GetComponent<MeshRenderer>()->SetLocalBounds(AxisAlignedBox{{25, 0, 0}, {26, 1, 1}});
        tree.UpdateNode(c);
        if (!Check(tree.GetRoot() != nullptr && SameBox(tree.GetRoot()->bounds, AxisAlignedBox{{0, 0, 0}, {26, 1, 1}}),
                "BVH 节点移回后根盒未收缩") || !CheckTree(tree.GetRoot())) return false;

        tree.RemoveNode(b);
        if (!Check(tree.FindNode(b) == nullptr && tree.GetLeafNodes().size() == 2 &&
                tree.GetRoot() != nullptr && SameBox(tree.GetRoot()->bounds, AxisAlignedBox{{0, 0, 0}, {26, 1, 1}}),
                "BVH 多节点删除后叶索引或根盒错误") || !CheckTree(tree.GetRoot())) return false;
        a->GetComponent<MeshRenderer>()->SetLocalBounds({});
        tree.UpdateNode(a);
        if (!Check(tree.FindNode(a) == nullptr && tree.GetLeafNodes().size() == 1 &&
                tree.GetRoot() != nullptr && SameBox(tree.GetRoot()->bounds, AxisAlignedBox{{25, 0, 0}, {26, 1, 1}}),
                "局部盒无效后 BVH 未移除叶节点并 refit") || !CheckTree(tree.GetRoot())) return false;
        tree.RemoveNode(c);
        return Check(tree.GetRoot() == nullptr && tree.GetLeafNodes().empty(), "删除最后一个节点后 BVH 未清空");
    }

    bool TestSceneCopy()
    {
        Scene source;
        const AxisAlignedBox local{{-2, -1, -1}, {3, 2, 1}};
        auto original = MakeObject(source, "copy", local, 7);
        original->GetComponent<TransformComponent>()->SetScale(2, 1, 1);
        Scene copy{source};
        if (!Check(copy.GetAllObjects().size() == 1, "复制场景对象数量错误")) return false;
        auto object = copy.GetAllObjects().begin()->second;
        auto renderer = object->GetComponent<MeshRenderer>();
        if (!Check(renderer != nullptr && renderer->GetMesh() == nullptr,
                "复制场景丢失无网格 MeshRenderer")) return false;
        return Check(SameBox(renderer->GetLocalBounds(), local), "复制场景未保留局部盒") &&
            Check(SameBox(renderer->GetBounds(), original->GetComponent<MeshRenderer>()->GetBounds()),
                "复制场景世界盒不匹配原场景");
    }

    bool TestLegacyRecovery()
    {
        DSM::Model model{};
        auto makeMesh = [](std::string name, const AxisAlignedBox& bounds) {
            auto mesh = std::make_shared<DSM::Mesh>();
            mesh->name = std::move(name);
            mesh->bounds = bounds;
            return mesh;
        };
        model.meshes = {
            makeMesh("first", AxisAlignedBox{{0, 0, 0}, {1, 1, 1}}),
            makeMesh("second", AxisAlignedBox{{5, 0, 0}, {6, 1, 1}})
        };
        const auto old = nlohmann::json{{"meshIndex", 0}, {"localBounds", nlohmann::json{
            {"min", nlohmann::json::array({5, 0, 0})}, {"max", nlohmann::json::array({6, 1, 1})}}}};
        if (!Check(DSM::SerializationDetail::ResolveLegacyMeshIndex(old, model, "second") == 1,
                "旧下标与名称冲突时没有按稳定身份找回网格")) return false;
        if (!Check(DSM::SerializationDetail::ResolveLegacyMeshIndex(old, model, "renamed") == 1,
                "对象改名后没有通过唯一盒恢复网格")) return false;
        try {
            DSM::SerializationDetail::ResolveLegacyMeshIndex(old, model, "first");
            return Check(false, "名称和另一个网格的盒冲突时应拒绝加载");
        }
        catch (const std::runtime_error&) {}
        model.meshes.push_back(makeMesh("duplicate", model.meshes[1]->bounds));
        try {
            DSM::SerializationDetail::ResolveLegacyMeshIndex(old, model, "renamed");
            return Check(false, "同盒网格无法唯一恢复时应拒绝加载");
        }
        catch (const std::runtime_error&) {}
        return true;
    }

    bool TestSerializerToJson()
    {
        Scene scene;
        auto object = scene.GetObjectByID(scene.CreateObject("model-mesh")).lock();
        auto renderer = object->AddComponent<MeshRenderer>();
        auto model = std::make_shared<DSM::Model>();
        model->filePath = "/Game/Models/in-memory.glb";
        model->meshes = {std::make_shared<DSM::Mesh>(), std::make_shared<DSM::Mesh>()};
        model->meshNodeKeys = {"node/first", "node/second"};
        const AxisAlignedBox local{{-1, -2, -3}, {4, 5, 6}};
        model->meshes[1]->bounds = local;
        renderer->SetMesh(model->meshes[1]);
        renderer->SetModel(model);
        const auto value = DSM::Serializer::Serialize(*renderer);
        if (!Check(value.value("assetPath", std::string{}) == model->filePath &&
                value.value("meshKey", std::string{}) == "node/second" &&
                !value.contains("meshIndex") && !value.contains("mesh") && !value.contains("bounds"),
                "模型渲染器 to_json 未保存正确的稳定 meshKey")) return false;
        if (!Check(value.contains("localBounds") &&
                value["localBounds"]["min"] == nlohmann::json::array({-1, -2, -3}) &&
                value["localBounds"]["max"] == nlohmann::json::array({4, 5, 6}),
                "模型渲染器 to_json 未保存局部盒")) return false;
        renderer->SetLocalBounds({});
        if (!Check(DSM::Serializer::Serialize(*renderer)["localBounds"].is_null(),
                "无效局部盒没有保存为 null")) return false;
        renderer->SetMesh(std::make_shared<DSM::Mesh>());
        try {
            DSM::Serializer::Serialize(*renderer);
            return Check(false, "不属于模型的网格应拒绝保存");
        }
        catch (const std::runtime_error&) {
            return true;
        }
    }
}

int main()
{
    try {
        const bool rendererOk = TestRendererBounds();
        const bool singleRootOk = TestSingleRoot();
        const bool multiNodeOk = TestMultiNode();
        const bool copyOk = TestSceneCopy();
        const bool legacyOk = TestLegacyRecovery();
        const bool serializerOk = TestSerializerToJson();
        if (!rendererOk || !singleRootOk || !multiNodeOk || !copyOk || !legacyOk || !serializerOk) return 1;
        std::cout << "SceneBoundsTests 通过\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "[FAIL] SceneBoundsTests 异常: " << error.what() << '\n';
        return 1;
    }
}
