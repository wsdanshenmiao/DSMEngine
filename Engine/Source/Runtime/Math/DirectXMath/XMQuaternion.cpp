#include "XMQuaternion.h"
#include "XMMatrix.h"

using namespace DirectX;

namespace DSM{
    XMQuaternion::XMQuaternion(XMVector3 v) noexcept
    {
        v = XMVectorModAngles(v);
        m_Vector = DirectX::XMQuaternionRotationRollPitchYawFromVector(v);
    }

    XMQuaternion::XMQuaternion(const XMMatrix3 &matrix)
         : XMQuaternion(XMMATRIX(matrix)) {}

    XMQuaternion::XMQuaternion(const XMMatrix4 &matrix)
         : XMQuaternion(XMMATRIX(matrix)) {}

    XMScalar XMQuaternion::Get(size_t index) const
    {
        switch (index) {
        case 0: return XMScalar{DirectX::XMVectorSplatX(m_Vector)}; 
        case 1: return XMScalar{DirectX::XMVectorSplatY(m_Vector)}; 
        case 2: return XMScalar{DirectX::XMVectorSplatZ(m_Vector)};
        case 3: return XMScalar{DirectX::XMVectorSplatW(m_Vector)};
        default:
            throw std::out_of_range("Index out of range.");
        }
        return XMScalar{};
    }

    void XMQuaternion::Set(size_t index, XMScalar val)
    { 
        switch (index) {
        case 0: m_Vector = DirectX::XMVectorPermute<4,1,2,3>(m_Vector, val);
        case 1: m_Vector = DirectX::XMVectorPermute<0,5,2,3>(m_Vector, val);
        case 2: m_Vector = DirectX::XMVectorPermute<0,1,6,3>(m_Vector, val);
        case 3: m_Vector = DirectX::XMVectorPermute<0,1,2,7>(m_Vector, val);
        default:
            throw std::out_of_range("Index out of range.");
        }
    }
    
    XMVector3 XMQuaternion::ToEulerAngles() const
    {
        float x = DirectX::XMVectorGetX(m_Vector);
        float y = DirectX::XMVectorGetY(m_Vector);
        float z = DirectX::XMVectorGetZ(m_Vector);
        float w = DirectX::XMVectorGetW(m_Vector);

        // pitch (X轴)
        float sinX = 2.0f * (w * x - y * z);
        sinX = sinX > 1.0f ? 1.0f : sinX;
        sinX = sinX < -1.0f ? -1.0f : sinX;
        float pitch = std::asin(sinX);

        // roll (Y轴)
        float sinY_cosX = 2.0f * (w * y + x * z);
        float cosY_cosX = 1.0f - 2.0f * (x * x + y * y);
        float yaw = std::atan2(sinY_cosX, cosY_cosX);

        // yaw (Z轴)
        float sinZ_cosX = 2.0f * (w * z + x * y);
        float cosZ_cosX = 1.0f - 2.0f * (x * x + z * z);
        float roll = std::atan2(sinZ_cosX, cosZ_cosX);

        return XMVector3{pitch, yaw, roll};
    }
}