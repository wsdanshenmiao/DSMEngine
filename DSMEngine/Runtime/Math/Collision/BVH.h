#ifndef __BVH_H__
#define __BVH_H__

#include <algorithm>
#include "Runtime/Math/Collision/BoundingBox.h"

namespace DSM::Math{

    class BVHNode
    {
    public:
        BVHNode(const AxisAlignedBox& box) : m_Bounds(box), m_Left(nullptr), m_Right(nullptr) {}
        BVHNode(const std::span<AxisAlignedBox> boxes) : BVHNode(boxes, 0, boxes.size()) {}
        BVHNode(const std::span<AxisAlignedBox> boxes, size_t begin, size_t end)
        {
            for(auto objIndex = begin; objIndex < end; ++objIndex){
                m_Bounds = AxisAlignedBox::Union(m_Bounds, boxes[objIndex]);
            }
            std::size_t axisIndex = m_Bounds.GetLongestAxis();

            auto cmpFunc = [axisIndex](const auto& box0, const auto& box1){
                return BoxCompare(box0, box1, axisIndex);
            };

            std::size_t objSpan = end - begin;
            if(objSpan == 1){
                m_Left = m_Right = nullptr;
            }
            else if(objSpan == 2){
                m_Left = std::make_shared<BVHNode>(boxes[begin]);
                m_Right = std::make_shared<BVHNode>(boxes[begin + 1]);
            }
            else{
                std::sort(std::begin(boxes) + begin, std::begin(boxes) + end, cmpFunc);
                std::size_t mid = begin + objSpan * .5f;
                m_Left = std::make_shared<BVHNode>(boxes, begin, mid);
                m_Right = std::make_shared<BVHNode>(boxes, mid, end);
            }
        }

        AxisAlignedBox BoundingBox() const { return m_Bounds; }

        bool Hit(const Vector3& origin, const Vector3& dir, const Vector2& tRange, Vector2& time) const
        {
            if(!m_Bounds.Hit(origin, dir, tRange, time) || m_Left == nullptr || m_Right == nullptr)
                return false;

            Vector2 leftTime{}, rightTime{};
            bool hitLeft = m_Left->Hit(origin, dir, tRange, leftTime);
            Vector2 rightRange{tRange.Get(0), hitLeft ? leftTime.Get(1) : tRange.Get(1)};
            bool hitRight = m_Right->Hit(origin, dir, rightRange, rightTime);
            time = Vector2{std::max(leftTime.Get(0), rightTime.Get(0)), std::min(leftTime.Get(1), rightTime.Get(1))};
            return hitLeft || hitRight;
        }

    private:
        static bool BoxCompare(const AxisAlignedBox& a, const AxisAlignedBox& b, size_t axis)
        {
            return a.GetMin().Get(axis) < b.GetMin().Get(axis);
        }

    private:
        AxisAlignedBox m_Bounds;
        std::shared_ptr<BVHNode> m_Left;
        std::shared_ptr<BVHNode> m_Right;
    };
}

#endif