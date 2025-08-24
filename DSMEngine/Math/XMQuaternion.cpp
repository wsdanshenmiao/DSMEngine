#include "XMQuaternion.h"
#include "XMMatrix.h"

namespace DSM{
    XMQuaternion::XMQuaternion(const XMMatrix3 &matrix)
         : XMQuaternion(DirectX::XMMATRIX(matrix)) {}

    XMQuaternion::XMQuaternion(const XMMatrix4 &matrix)
         : XMQuaternion(DirectX::XMMATRIX(matrix)) {}

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
}