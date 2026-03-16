#pragma once
#ifndef __BVH_H__
#define __BVH_H__

#include <stack>
#include "Runtime/Framework/Object/GameObject.h"

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
            int height;

            void UpdateBounds()
            {
                for(BVHNode* node = this; node != nullptr; node = node->parent.lock().get()) {
                    if(node->left != nullptr && node->right != nullptr) {
                        node->bounds = AxisAlignedBox::Union(node->left->bounds, node->right->bounds);
                    }
                }
            }
            
            void UpdateHeight()
            {
                auto getHeight = [](BVHNode* node){
                    return node != nullptr && node->left != nullptr && node->right != nullptr ?
                        std::max(node->left->height, node->right->height) + 1 : 0;
                };
                int preHeight = getHeight(this);
                for (BVHNode* node = parent.lock().get(); 
                    node != nullptr && preHeight != (node->height - 1); 
                    node = node->parent.lock().get()) {
                    preHeight = node->height = getHeight(node);
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

            for (const auto& obj : objs) {
                if(obj->GetComponent<AxisAlignedBox>() != nullptr)
                    InsertNode(obj);
            }
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

            // 搜索合适的插入位置
            std::shared_ptr<BVHNode> sibling = FindBestNode(object);
            if (sibling == nullptr) {
                m_Root = std::make_shared<BVHNode>();
                m_Root->object = object;
                m_Root->bounds = *object->GetComponent<AxisAlignedBox>();
                m_LeafNodes[object] = m_Root;
                return m_Root;
            }

            // 创建新的父节点并插入新的节点
            auto newNode = std::make_shared<BVHNode>();
            newNode->object = object;
            newNode->bounds = *object->GetComponent<AxisAlignedBox>();

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

            // 更新插入节点的祖先节点
            newNode->UpdateBounds();
            newNode->UpdateHeight();

            // 将叶子节点插入到链表中
            m_LeafNodes[object] = newNode;

            Balance(newParent);
            return newNode;
        }

        void RemoveNode(std::shared_ptr<BVHNode> node)
        {
            if(node == nullptr)
                return;

            auto nodeParent = node->parent.lock();
            if (nodeParent == nullptr) {
                m_Root = nullptr;
                return;
            }

            auto otherChild = nodeParent->left == node ?
                nodeParent->right : nodeParent->left;
            otherChild->parent = nodeParent->parent;
            if (auto parent = nodeParent->parent.lock(); parent != nullptr) {
                if (parent->left == nodeParent)
                    parent->left = otherChild;
                else
                    parent->right = otherChild;
            }
            else {
                m_Root = otherChild;
            }
            otherChild->UpdateBounds();
            otherChild->UpdateHeight();

            // 从链表中移除叶子节点
            m_LeafNodes.erase(node->object);

            Balance(nodeParent);
        }

        void RemoveNode(std::shared_ptr<GameObject> object)
        {
            if(object != nullptr)
            {
                auto node = FindNode(object);
                RemoveNode(node);
            }
        }

    private:
        /// <summary>
        /// 使用表面启发式算法查找最佳的插入点
        /// </summary>
        std::shared_ptr<BVHNode> FindBestNode(std::shared_ptr<GameObject> object)
        {
            if(object == nullptr || m_Root == nullptr)
                return nullptr;

            float bestCost = std::numeric_limits<float>::max();
            std::shared_ptr<BVHNode> sibling = nullptr;
            for (const auto& [obj, node] : m_LeafNodes) {
                // 插入一个节点的开销为合并后包围盒的面积及所有祖先节点的面积增量
                auto objectBounds = object->GetComponent<AxisAlignedBox>();
                float cost = AxisAlignedBox::Union(node->bounds, *objectBounds).Area();
                auto parent = node->parent.lock();
                for (; parent != nullptr && cost < bestCost; parent = parent->parent.lock()) {
                    // 累加父节点的面积增量
                    auto newBounds = AxisAlignedBox::Union(parent->bounds, *objectBounds);
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
            auto newRoot = std::move(isRight ? node->left : node->right);
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
            moveNode->parent = node;

            node->UpdateHeight();
            node->UpdateBounds();
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
            if (node == nullptr) 
                return;
            int factor = node->BalanceFactor();
            std::shared_ptr<BVHNode> newRoot = node;
            // 左重
            if (factor > 1)
            {
                if (node->left != nullptr && node->left->BalanceFactor() < 0)
                    node->left = RotateLeft(node->left);  // LR
                newRoot = RotateRight(node);
            }
            else if (factor < -1)   // 右重
            {
                if (node->right != nullptr && node->right->BalanceFactor() > 0)
                    node->right = RotateRight(node->right);    // RL
                newRoot = RotateLeft(node);
            }

            if (auto parent = newRoot->parent.lock(); parent != nullptr)
                Balance(parent);
        }


    private:
        std::shared_ptr<BVHNode> m_Root = nullptr;
        std::unordered_map<std::shared_ptr<GameObject>, std::shared_ptr<BVHNode>> m_LeafNodes{};  
    };
}


#endif
