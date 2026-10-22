#pragma once
#ifndef __BVH_H__
#define __BVH_H__

#include <stack>
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/TransformComponent.h"

namespace DSM::Math {
    class BVHTree
    {
    public:
        struct BVHNode
        {
            AxisAlignedBox bounds;
            std::shared_ptr<GameObject> object;
            std::shared_ptr<BVHNode> left;
            std::shared_ptr<BVHNode> right;
            std::weak_ptr<BVHNode> parent;
            int height = 0;

            void Refit()
            {
                if (left != nullptr && right != nullptr) {
                    bounds = AxisAlignedBox::Union(left->bounds, right->bounds);
                    height = std::max(left->height, right->height) + 1;
                }
                else {
                    height = 0;
                }
            }

            inline int BalanceFactor()
            {
                return (left != nullptr ? left->height : 0) - (right != nullptr ? right->height : 0);
            }
        };

        BVHTree() = default;
        BVHTree(std::span<std::shared_ptr<GameObject>> objs)
        {
            if (objs.empty())
                return;

            for (const auto& obj : objs)
                InsertNode(obj);
        }

        inline std::shared_ptr<BVHNode> GetRoot() { return m_Root; }

        inline auto& GetLeafNodes() { return m_LeafNodes; }

        std::shared_ptr<BVHNode> FindNode(std::shared_ptr<GameObject> object)
        {
            if (object == nullptr || m_Root == nullptr)
                return nullptr;

            std::stack<std::shared_ptr<BVHNode>> stack;
            stack.push(m_Root);
            while(!stack.empty())
            {
                auto node = stack.top();
                stack.pop();
                if(node == nullptr)
                    continue;
                if (node->object == object)
                    return node;

                if(node->left != nullptr)
                    stack.push(node->left);
                if(node->right != nullptr)
                    stack.push(node->right);
            }

            return nullptr;
        }

        std::shared_ptr<BVHNode> InsertNode(std::shared_ptr<GameObject> object)
        {
            if (object == nullptr)
                return nullptr;
            if (auto it = m_LeafNodes.find(object); it != m_LeafNodes.end()) {
                UpdateNode(it->second);
                it = m_LeafNodes.find(object);
                return it != m_LeafNodes.end() ? it->second : nullptr;
            }

            auto [transform, renderer] = object->GetComponent<TransformComponent, MeshRenderer>();
            if (transform == nullptr || renderer == nullptr)
                return nullptr;

            auto bounds = renderer->GetBounds();
            if (!bounds.IsValid())
                return nullptr;

            // 搜索合适的插入位置
            std::shared_ptr<BVHNode> sibling = FindBestNode(bounds);
            if (sibling == nullptr) {
                m_Root = std::make_shared<BVHNode>();
                m_Root->object = object;
                m_Root->bounds = bounds;
                m_LeafNodes[object] = m_Root;
                return m_Root;
            }

            // 创建新的父节点并插入新的节点
            auto newNode = std::make_shared<BVHNode>();
            newNode->object = object;
            newNode->bounds = bounds;

            auto oldParent = sibling->parent.lock();
            auto newParent = std::make_shared<BVHNode>();
            newParent->parent = oldParent;

            if (oldParent != nullptr) {
                // 更新父节点的子节点
                if (oldParent->left == sibling)
                    oldParent->left = newParent;
                else
                    oldParent->right = newParent;
            }
            else {
                m_Root = newParent;
            }

            newParent->left = sibling;
            newParent->right = newNode;
            sibling->parent = newParent;
            newNode->parent = newParent;

            m_LeafNodes[object] = newNode;
            Balance(newParent);
            return newNode;
        }

        void RemoveNode(std::shared_ptr<BVHNode> node)
        {
            if (node == nullptr || node->object == nullptr)
                return;
            auto it = m_LeafNodes.find(node->object);
            if (it == m_LeafNodes.end() || it->second != node)
                return;

            auto nodeParent = node->parent.lock();
            if (nodeParent == nullptr) {
                m_Root = nullptr;
                m_LeafNodes.erase(it);
                return;
            }

            auto otherChild = nodeParent->left == node ? nodeParent->right : nodeParent->left;
            auto parent = nodeParent->parent.lock();
            otherChild->parent = parent;
            if (parent != nullptr) {
                if (parent->left == nodeParent)
                    parent->left = otherChild;
                else
                    parent->right = otherChild;
            }
            else {
                m_Root = otherChild;
            }

            node->parent.reset();
            nodeParent->left.reset();
            nodeParent->right.reset();
            m_LeafNodes.erase(it);
            Balance(parent != nullptr ? parent : otherChild);
        }

        void RemoveNode(std::shared_ptr<GameObject> object)
        {
            if(object != nullptr)
            {
                auto node = FindNode(object);
                RemoveNode(node);
            }
        }

        void UpdateNode(std::shared_ptr<GameObject> object)
        {
            auto node = FindNode(object);
            if(node != nullptr)
                UpdateNode(node);
        }

        void UpdateNode(std::shared_ptr<BVHNode> node)
        {
            if (node == nullptr || node->object == nullptr)
                return;
            auto object = node->object;
            auto it = m_LeafNodes.find(object);
            if (it == m_LeafNodes.end() || it->second != node)
                return;

            auto [transform, renderer] = object->GetComponent<TransformComponent, MeshRenderer>();
            if (transform == nullptr || renderer == nullptr) {
                RemoveNode(node);
                return;
            }

            auto bounds = renderer->GetBounds();
            if (!bounds.IsValid()) {
                RemoveNode(node);
                return;
            }

            auto parent = node->parent.lock();
            if (parent == nullptr) {
                node->bounds = bounds;
            }
            else if (parent->bounds.Contains(bounds)) {
                node->bounds = bounds;
                Balance(parent);
            }
            else {
                RemoveNode(node);
                InsertNode(object);
            }
        }

        void InsertOrUpdateNode(std::shared_ptr<GameObject> object)
        {
            if(object != nullptr) {
                auto node = FindNode(object);
                if(node != nullptr) {
                    UpdateNode(node);
                }
                else {
                    InsertNode(object);
                }
            }
        }

    private:
        /// <summary>
        /// 使用表面启发式算法查找最佳的插入点
        /// </summary>
        std::shared_ptr<BVHNode> FindBestNode(const AxisAlignedBox& bounds)
        {
            if(m_Root == nullptr)
                return nullptr;

            float bestCost = std::numeric_limits<float>::max();
            std::shared_ptr<BVHNode> sibling = nullptr;
            for (const auto& [obj, node] : m_LeafNodes) {
                // 插入一个节点的开销为合并后包围盒的面积及所有祖先节点的面积增量
                float cost = AxisAlignedBox::Union(node->bounds, bounds).Area();
                auto parent = node->parent.lock();
                for (; parent != nullptr && cost < bestCost; parent = parent->parent.lock()) {
                    // 累加父节点的面积增量
                    auto newBounds = AxisAlignedBox::Union(parent->bounds, bounds);
                    cost += newBounds.Area() - parent->bounds.Area();
                }

                if (cost < bestCost && parent == nullptr) {
                    bestCost = cost;
                    sibling = node;
                }
            }

            return sibling;
        }

        std::shared_ptr<BVHNode> Rotate(std::shared_ptr<BVHNode> node, bool isRight)
        {
            if (node == nullptr)
                return nullptr;

            // 旋转后新的根节点为当前节点的子节点
            auto newRoot = isRight ? node->left : node->right;
            if (newRoot == nullptr)
                return nullptr;

            // 更新父节点的子节点指向新的根节点
            newRoot->parent = node->parent;
            if (auto parent = node->parent.lock(); parent != nullptr) {
                if (parent->left == node)
                    parent->left = newRoot;
                else
                    parent->right = newRoot;
            }
            else {
                m_Root = newRoot;
            }

            auto moveNode = isRight ? newRoot->right : newRoot->left;
            if (isRight) {
                newRoot->right = node;
                node->left = moveNode;
            }
            else
            {
                newRoot->left = node;
                node->right = moveNode;
            }
            node->parent = newRoot;
            if (moveNode != nullptr)
                moveNode->parent = node;

            node->Refit();
            newRoot->Refit();
            return newRoot;
        }

        /// <summary>
        /// 树的右旋
        /// </summary>
        std::shared_ptr<BVHNode> RotateRight(std::shared_ptr<BVHNode> node)
        {
            return Rotate(node, true);
        }

        /// <summary>
        /// 树的左旋
        /// </summary>
        std::shared_ptr<BVHNode> RotateLeft(std::shared_ptr<BVHNode> node)
        {
            return Rotate(node, false);
        }

        void Balance(std::shared_ptr<BVHNode> node)
        {
            while (node != nullptr) {
                node->Refit();
                int factor = node->BalanceFactor();
                if (factor > 1) {
                    if (node->left->BalanceFactor() < 0)
                        RotateLeft(node->left);
                    node = RotateRight(node);
                }
                else if (factor < -1) {
                    if (node->right->BalanceFactor() > 0)
                        RotateRight(node->right);
                    node = RotateLeft(node);
                }
                node = node->parent.lock();
            }
        }


    private:
        std::shared_ptr<BVHNode> m_Root = nullptr;
        std::unordered_map<std::shared_ptr<GameObject>, std::shared_ptr<BVHNode>> m_LeafNodes{};  
    };
}


#endif
