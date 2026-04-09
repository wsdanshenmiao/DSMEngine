#pragma once
#ifndef __GRAPHICSCOMMON_H__
#define __GRAPHICSCOMMON_H__

#include <cmath>
#include <string>
#include <memory>
#include <array>
#include <format>
#include "Runtime/Utils/EnumUtil.h"
#include "Runtime/Utils/Container.h"
#include "Runtime/Utils/Utils.h"
#include "Runtime/Math/MathCommon.h"

namespace DSM {
    using GpuVirtualAddress = uint64_t;

    static constexpr uint32_t c_MaxRenderTargets = 8u;
    static constexpr uint32_t c_MaxViewports = 16u;
    static constexpr uint32_t c_MaxVertexAttributes = 16u;
    static constexpr uint32_t c_MaxBindingLayouts = 8u;
    static constexpr uint32_t c_MaxBindlessRegisterSpaces = 16u;
    static constexpr uint32_t c_ConstantBufferOffsetSizeAlignment = 256u;
    static constexpr uint32_t c_MaxVolatileConstantBuffersPerLayout = 6u;
    static constexpr uint32_t c_MaxVolatileConstantBuffers = 32;


    using ObjectType = uint32_t;

    namespace ObjectTypes
    {
        constexpr ObjectType SharedHandle                           = 0x00000001;
        constexpr ObjectType D3D12_Device                           = 0x00010001;
        constexpr ObjectType D3D12_CommandQueue                     = 0x00010002;
        constexpr ObjectType D3D12_GraphicsCommandList              = 0x00010003;
        constexpr ObjectType D3D12_Resource                         = 0x00010004;
        constexpr ObjectType D3D12_RenderTargetViewDescriptor       = 0x00010005;
        constexpr ObjectType D3D12_DepthStencilViewDescriptor       = 0x00010006;
        constexpr ObjectType D3D12_ShaderResourceViewGpuDescriptor  = 0x00010007;
        constexpr ObjectType D3D12_UnorderedAccessViewGpuDescriptor = 0x00010008;
        constexpr ObjectType D3D12_RootSignature                    = 0x00010009;
        constexpr ObjectType D3D12_PipelineState                    = 0x0001000a;
        constexpr ObjectType D3D12_CommandAllocator                 = 0x0001000b;
        constexpr ObjectType D3D12_ShaderResourceViewCpuDescriptor  = 0x0001000c;
        constexpr ObjectType D3D12_UnorderedAccessViewCpuDescriptor = 0x0001000d;
    };

    

    // 引用指针 From Microsoft::WRL::RefPtr<T>
    template <typename T>
    class RefPtr
    {
    public:
        typedef T InterfaceType;

    protected:
        InterfaceType *ptr_;
        template<class U> friend class RefPtr;

        void InternalAddRef() const noexcept
        {
            if (ptr_ != nullptr)
            {
                ptr_->AddRef();
            }
        }

        unsigned long InternalRelease() noexcept
        {
            unsigned long ref = 0;
            T* temp = ptr_;

            if (temp != nullptr)
            {
                ptr_ = nullptr;
                ref = temp->Release();
            }

            return ref;
        }

    public:
        RefPtr() noexcept : ptr_(nullptr)
        {
        }

        RefPtr(std::nullptr_t) noexcept : ptr_(nullptr)
        {
        }

        template<class U>
        RefPtr(U *other) noexcept : ptr_(other)
        {
            InternalAddRef();
        }

        RefPtr(const RefPtr& other) noexcept : ptr_(other.ptr_)
        {
            InternalAddRef();
        }

        // copy constructor that allows to instantiate class when U* is convertible to T*
        template<class U>
        RefPtr(const RefPtr<U> &other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void *> * = 0) noexcept :
            ptr_(other.ptr_)
        {
            InternalAddRef();
        }

        RefPtr(RefPtr &&other) noexcept : ptr_(nullptr)
        {
            if (this != reinterpret_cast<RefPtr*>(&reinterpret_cast<unsigned char&>(other)))
            {
                Swap(other);
            }
        }

        // Move constructor that allows instantiation of a class when U* is convertible to T*
        template<class U>
        RefPtr(RefPtr<U>&& other, typename std::enable_if_t<std::is_convertible_v<U*, T*>, void *> * = 0) noexcept :
            ptr_(other.ptr_)
        {
            other.ptr_ = nullptr;
        }
        ~RefPtr() noexcept
        {
            InternalRelease();
        }

        RefPtr& operator=(std::nullptr_t) noexcept
        {
            InternalRelease();
            return *this;
        }

        RefPtr& operator=(T *other) noexcept
        {
            if (ptr_ != other)
            {
                RefPtr(other).Swap(*this);
            }
            return *this;
        }

        template <typename U>
        RefPtr& operator=(U *other) noexcept
        {
            RefPtr(other).Swap(*this);
            return *this;
        }

        RefPtr& operator=(const RefPtr &other) noexcept
        {
            if (ptr_ != other.ptr_)
            {
                RefPtr(other).Swap(*this);
            }
            return *this;
        }

        template<class U>
        RefPtr& operator=(const RefPtr<U>& other) noexcept
        {
            RefPtr(other).Swap(*this);
            return *this;
        }

        RefPtr& operator=(RefPtr &&other) noexcept
        {
            RefPtr(static_cast<RefPtr&&>(other)).Swap(*this);
            return *this;
        }

        template<class U>
        RefPtr& operator=(RefPtr<U>&& other) noexcept
        {
            RefPtr(static_cast<RefPtr<U>&&>(other)).Swap(*this);
            return *this;
        }
        
        void Swap(RefPtr&& r) noexcept
        {
            T* tmp = ptr_;
            ptr_ = r.ptr_;
            r.ptr_ = tmp;
        }

        void Swap(RefPtr& r) noexcept
        {
            T* tmp = ptr_;
            ptr_ = r.ptr_;
            r.ptr_ = tmp;
        }
        
        T* Get() const noexcept
        {
            return ptr_;
        }

        operator T*() const
        {
            return ptr_;
        }
        
        InterfaceType* operator->() const noexcept
        {
            return ptr_;
        }
        
        T** operator&()
        {
            return &ptr_;
        }

        T* const* GetAddressOf() const noexcept
        {
            return &ptr_;
        }

        T** GetAddressOf() noexcept
        {
            return &ptr_;
        }

        T** ReleaseAndGetAddressOf() noexcept
        {
            InternalRelease();
            return &ptr_;
        }

        T* Detach() noexcept
        {
            T* ptr = ptr_;
            ptr_ = nullptr;
            return ptr;
        }

        void Attach(InterfaceType* other) noexcept
        {
            if (ptr_ != nullptr)
            {
                auto ref = ptr_->Release();
                (void)ref;
                // Attaching to the same object only works if duplicate references are being coalesced. Otherwise
                // re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
                __WRL_ASSERT__(ref != 0 || ptr_ != other);
            }

            ptr_ = other;
        }

        unsigned long Reset()
        {
            return InternalRelease();
        }
    };    // RefPtr


    // 引用计数器
    class RefCounter
    {
    public:
        virtual ~RefCounter() = default;

        virtual unsigned long AddRef() 
        {
            return ++m_RefCount;
        }

        virtual unsigned long Release()
        {
            unsigned long result = --m_RefCount;
            if (result == 0) {
                delete this;
            }
            return result;
        }

    private:
        std::atomic<unsigned long> m_RefCount = 0;
    };



    struct Object
    {
        union 
        {
            uint64_t integer;
            void* pointer;
        };

        Object(uint64_t i) : integer(i) { }
        Object(void* p) : pointer(p) { }

        template<typename T> operator T* () const { return static_cast<T*>(pointer); }
    };

    class IResource : public RefCounter
    {
    protected:
        IResource() = default;
        virtual ~IResource() = default;

    public:
        // 返回一个原始对象
        virtual Object GetNativeObject(ObjectType objectType) { (void)objectType; return nullptr; }
        
        IResource(const IResource&) = delete;
        IResource(const IResource&&) = delete;
        IResource& operator=(const IResource&) = delete;
        IResource& operator=(const IResource&&) = delete;
    };
    using ResourceHandle = RefPtr<IResource>;

    // 从GPU上获取事件信息
    struct IEventQuery : public IResource { };
    using EventQueryHandle = RefPtr<IEventQuery>;

    // 从GPU上获取时间信息
    struct ITimerQuery : public IResource { };
    using TimerQueryHandle = RefPtr<ITimerQuery>;

    struct Color
    {
        float r, g, b, a;

        Color() : r(0.f), g(0.f), b(0.f), a(0.f) { }
        Color(float c) : r(c), g(c), b(c), a(c) { }
        Color(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) { }

        bool operator ==(const Color& _b) const { return r == _b.r && g == _b.g && b == _b.b && a == _b.a; }
    };
    

    struct Viewport
    {
        float minX, maxX;
        float minY, maxY;
        float minZ, maxZ;

        Viewport() : minX(0.f), maxX(0.f), minY(0.f), maxY(0.f), minZ(0.f), maxZ(1.f) { }

        Viewport(float width, float height) : minX(0.f), maxX(width), minY(0.f), maxY(height), minZ(0.f), maxZ(1.f) { }

        Viewport(float _minX, float _maxX, float _minY, float _maxY, float _minZ, float _maxZ)
            : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY), minZ(_minZ), maxZ(_maxZ) { }

        bool operator ==(const Viewport& b) const noexcept
        {
            return minX == b.minX
                && minY == b.minY
                && minZ == b.minZ
                && maxX == b.maxX
                && maxY == b.maxY
                && maxZ == b.maxZ;
        }
        bool operator !=(const Viewport& b) const { return !(*this == b); }

        [[nodiscard]] float Width() const noexcept { return maxX - minX; }
        [[nodiscard]] float Height() const noexcept { return maxY - minY; }
    };

    struct Rect
    {
        int minX, maxX;
        int minY, maxY;

        Rect() : minX(0), maxX(0), minY(0), maxY(0) { }
        Rect(int width, int height) : minX(0), maxX(width), minY(0), maxY(height) { }
        Rect(int _minX, int _maxX, int _minY, int _maxY) : minX(_minX), maxX(_maxX), minY(_minY), maxY(_maxY) { }
        explicit Rect(const Viewport& viewport)
            : minX(int(std::floorf(viewport.minX)))
            , maxX(int(std::ceilf(viewport.maxX)))
            , minY(int(std::floorf(viewport.minY)))
            , maxY(int(std::ceilf(viewport.maxY))) { }

        bool operator ==(const Rect& b) const 
        {
            return minX == b.minX && minY == b.minY && maxX == b.maxX && maxY == b.maxY;
        }
        bool operator !=(const Rect& b) const { return !(*this == b); }

        [[nodiscard]] int Width() const { return maxX - minX; }
        [[nodiscard]] int Height() const { return maxY - minY; }
    };

    enum class GraphicsAPI : uint8_t
    {
        D3D12
    };

    enum class Format : uint8_t
    {
        UNKNOWN,

        R8_UINT,
        R8_SINT,
        R8_UNORM,
        R8_SNORM,
        RG8_UINT,
        RG8_SINT,
        RG8_UNORM,
        RG8_SNORM,
        R16_UINT,
        R16_SINT,
        R16_UNORM,
        R16_SNORM,
        R16_FLOAT,
        BGRA4_UNORM,
        B5G6R5_UNORM,
        B5G5R5A1_UNORM,
        RGBA8_UINT,
        RGBA8_SINT,
        RGBA8_UNORM,
        RGBA8_SNORM,
        BGRA8_UNORM,
        SRGBA8_UNORM,
        SBGRA8_UNORM,
        R10G10B10A2_UNORM,
        R11G11B10_FLOAT,
        RG16_UINT,
        RG16_SINT,
        RG16_UNORM,
        RG16_SNORM,
        RG16_FLOAT,
        R32_UINT,
        R32_SINT,
        R32_FLOAT,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_FLOAT,
        RGBA16_UNORM,
        RGBA16_SNORM,
        RG32_UINT,
        RG32_SINT,
        RG32_FLOAT,
        RGB32_UINT,
        RGB32_SINT,
        RGB32_FLOAT,
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA32_FLOAT,
        
        D16,
        D24S8,
        X24G8_UINT,
        D32,
        D32S8,
        X32G8_UINT,

        BC1_UNORM,
        BC1_UNORM_SRGB,
        BC2_UNORM,
        BC2_UNORM_SRGB,
        BC3_UNORM,
        BC3_UNORM_SRGB,
        BC4_UNORM,
        BC4_SNORM,
        BC5_UNORM,
        BC5_SNORM,
        BC6H_UFLOAT,
        BC6H_SFLOAT,
        BC7_UNORM,
        BC7_UNORM_SRGB,

        COUNT,
    };

    enum class FormatKind : uint8_t
    {
        Integer,
        Normalized,
        Float,
        DepthStencil
    };

    struct FormatInfo
    {
        Format format;
        const char* name;
        uint8_t bytesPerBlock;
        uint8_t blockSize;
        FormatKind kind;
        bool hasRed : 1;
        bool hasGreen : 1;
        bool hasBlue : 1;
        bool hasAlpha : 1;
        bool hasDepth : 1;
        bool hasStencil : 1;
        bool isSigned : 1;
        bool isSRGB : 1;
    };
    

    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    static const FormatInfo c_FormatInfo[] = {
    //        format                   name             bytes blk         kind               red   green   blue  alpha  depth  stencl signed  srgb
        { Format::UNKNOWN,           "UNKNOWN",           0,   0, FormatKind::Integer,      false, false, false, false, false, false, false, false },
        { Format::R8_UINT,           "R8_UINT",           1,   1, FormatKind::Integer,      true,  false, false, false, false, false, false, false },
        { Format::R8_SINT,           "R8_SINT",           1,   1, FormatKind::Integer,      true,  false, false, false, false, false, true,  false },
        { Format::R8_UNORM,          "R8_UNORM",          1,   1, FormatKind::Normalized,   true,  false, false, false, false, false, false, false },
        { Format::R8_SNORM,          "R8_SNORM",          1,   1, FormatKind::Normalized,   true,  false, false, false, false, false, true,  false },
        { Format::RG8_UINT,          "RG8_UINT",          2,   1, FormatKind::Integer,      true,  true,  false, false, false, false, false, false },
        { Format::RG8_SINT,          "RG8_SINT",          2,   1, FormatKind::Integer,      true,  true,  false, false, false, false, true,  false },
        { Format::RG8_UNORM,         "RG8_UNORM",         2,   1, FormatKind::Normalized,   true,  true,  false, false, false, false, false, false },
        { Format::RG8_SNORM,         "RG8_SNORM",         2,   1, FormatKind::Normalized,   true,  true,  false, false, false, false, true,  false },
        { Format::R16_UINT,          "R16_UINT",          2,   1, FormatKind::Integer,      true,  false, false, false, false, false, false, false },
        { Format::R16_SINT,          "R16_SINT",          2,   1, FormatKind::Integer,      true,  false, false, false, false, false, true,  false },
        { Format::R16_UNORM,         "R16_UNORM",         2,   1, FormatKind::Normalized,   true,  false, false, false, false, false, false, false },
        { Format::R16_SNORM,         "R16_SNORM",         2,   1, FormatKind::Normalized,   true,  false, false, false, false, false, true,  false },
        { Format::R16_FLOAT,         "R16_FLOAT",         2,   1, FormatKind::Float,        true,  false, false, false, false, false, true,  false },
        { Format::BGRA4_UNORM,       "BGRA4_UNORM",       2,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::B5G6R5_UNORM,      "B5G6R5_UNORM",      2,   1, FormatKind::Normalized,   true,  true,  true,  false, false, false, false, false },
        { Format::B5G5R5A1_UNORM,    "B5G5R5A1_UNORM",    2,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA8_UINT,        "RGBA8_UINT",        4,   1, FormatKind::Integer,      true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA8_SINT,        "RGBA8_SINT",        4,   1, FormatKind::Integer,      true,  true,  true,  true,  false, false, true,  false },
        { Format::RGBA8_UNORM,       "RGBA8_UNORM",       4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA8_SNORM,       "RGBA8_SNORM",       4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, true,  false },
        { Format::BGRA8_UNORM,       "BGRA8_UNORM",       4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::SRGBA8_UNORM,      "SRGBA8_UNORM",      4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, true  },
        { Format::SBGRA8_UNORM,      "SBGRA8_UNORM",      4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::R10G10B10A2_UNORM, "R10G10B10A2_UNORM", 4,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::R11G11B10_FLOAT,   "R11G11B10_FLOAT",   4,   1, FormatKind::Float,        true,  true,  true,  false, false, false, false, false },
        { Format::RG16_UINT,         "RG16_UINT",         4,   1, FormatKind::Integer,      true,  true,  false, false, false, false, false, false },
        { Format::RG16_SINT,         "RG16_SINT",         4,   1, FormatKind::Integer,      true,  true,  false, false, false, false, true,  false },
        { Format::RG16_UNORM,        "RG16_UNORM",        4,   1, FormatKind::Normalized,   true,  true,  false, false, false, false, false, false },
        { Format::RG16_SNORM,        "RG16_SNORM",        4,   1, FormatKind::Normalized,   true,  true,  false, false, false, false, true,  false },
        { Format::RG16_FLOAT,        "RG16_FLOAT",        4,   1, FormatKind::Float,        true,  true,  false, false, false, false, true,  false },
        { Format::R32_UINT,          "R32_UINT",          4,   1, FormatKind::Integer,      true,  false, false, false, false, false, false, false },
        { Format::R32_SINT,          "R32_SINT",          4,   1, FormatKind::Integer,      true,  false, false, false, false, false, true,  false },
        { Format::R32_FLOAT,         "R32_FLOAT",         4,   1, FormatKind::Float,        true,  false, false, false, false, false, true,  false },
        { Format::RGBA16_UINT,       "RGBA16_UINT",       8,   1, FormatKind::Integer,      true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA16_SINT,       "RGBA16_SINT",       8,   1, FormatKind::Integer,      true,  true,  true,  true,  false, false, true,  false },
        { Format::RGBA16_FLOAT,      "RGBA16_FLOAT",      8,   1, FormatKind::Float,        true,  true,  true,  true,  false, false, true,  false },
        { Format::RGBA16_UNORM,      "RGBA16_UNORM",      8,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA16_SNORM,      "RGBA16_SNORM",      8,   1, FormatKind::Normalized,   true,  true,  true,  true,  false, false, true,  false },
        { Format::RG32_UINT,         "RG32_UINT",         8,   1, FormatKind::Integer,      true,  true,  false, false, false, false, false, false },
        { Format::RG32_SINT,         "RG32_SINT",         8,   1, FormatKind::Integer,      true,  true,  false, false, false, false, true,  false },
        { Format::RG32_FLOAT,        "RG32_FLOAT",        8,   1, FormatKind::Float,        true,  true,  false, false, false, false, true,  false },
        { Format::RGB32_UINT,        "RGB32_UINT",        12,  1, FormatKind::Integer,      true,  true,  true,  false, false, false, false, false },
        { Format::RGB32_SINT,        "RGB32_SINT",        12,  1, FormatKind::Integer,      true,  true,  true,  false, false, false, true,  false },
        { Format::RGB32_FLOAT,       "RGB32_FLOAT",       12,  1, FormatKind::Float,        true,  true,  true,  false, false, false, true,  false },
        { Format::RGBA32_UINT,       "RGBA32_UINT",       16,  1, FormatKind::Integer,      true,  true,  true,  true,  false, false, false, false },
        { Format::RGBA32_SINT,       "RGBA32_SINT",       16,  1, FormatKind::Integer,      true,  true,  true,  true,  false, false, true,  false },
        { Format::RGBA32_FLOAT,      "RGBA32_FLOAT",      16,  1, FormatKind::Float,        true,  true,  true,  true,  false, false, true,  false },
        { Format::D16,               "D16",               2,   1, FormatKind::DepthStencil, false, false, false, false, true,  false, false, false },
        { Format::D24S8,             "D24S8",             4,   1, FormatKind::DepthStencil, false, false, false, false, true,  true,  false, false },
        { Format::X24G8_UINT,        "X24G8_UINT",        4,   1, FormatKind::Integer,      false, false, false, false, false, true,  false, false },
        { Format::D32,               "D32",               4,   1, FormatKind::DepthStencil, false, false, false, false, true,  false, false, false },
        { Format::D32S8,             "D32S8",             8,   1, FormatKind::DepthStencil, false, false, false, false, true,  true,  false, false },
        { Format::X32G8_UINT,        "X32G8_UINT",        8,   1, FormatKind::Integer,      false, false, false, false, false, true,  false, false },
        { Format::BC1_UNORM,         "BC1_UNORM",         8,   4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::BC1_UNORM_SRGB,    "BC1_UNORM_SRGB",    8,   4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, true  },
        { Format::BC2_UNORM,         "BC2_UNORM",         16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::BC2_UNORM_SRGB,    "BC2_UNORM_SRGB",    16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, true  },
        { Format::BC3_UNORM,         "BC3_UNORM",         16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::BC3_UNORM_SRGB,    "BC3_UNORM_SRGB",    16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, true  },
        { Format::BC4_UNORM,         "BC4_UNORM",         8,   4, FormatKind::Normalized,   true,  false, false, false, false, false, false, false },
        { Format::BC4_SNORM,         "BC4_SNORM",         8,   4, FormatKind::Normalized,   true,  false, false, false, false, false, true,  false },
        { Format::BC5_UNORM,         "BC5_UNORM",         16,  4, FormatKind::Normalized,   true,  true,  false, false, false, false, false, false },
        { Format::BC5_SNORM,         "BC5_SNORM",         16,  4, FormatKind::Normalized,   true,  true,  false, false, false, false, true,  false },
        { Format::BC6H_UFLOAT,       "BC6H_UFLOAT",       16,  4, FormatKind::Float,        true,  true,  true,  false, false, false, false, false },
        { Format::BC6H_SFLOAT,       "BC6H_SFLOAT",       16,  4, FormatKind::Float,        true,  true,  true,  false, false, false, true,  false },
        { Format::BC7_UNORM,         "BC7_UNORM",         16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, false },
        { Format::BC7_UNORM_SRGB,    "BC7_UNORM_SRGB",    16,  4, FormatKind::Normalized,   true,  true,  true,  true,  false, false, false, true  },
    };

    static const FormatInfo& GetFormatInfo(Format format)
    {
        static_assert(sizeof(c_FormatInfo) / sizeof(FormatInfo) == size_t(Format::COUNT), 
            "The format info table doesn't have the right number of elements");

        if (uint32_t(format) >= uint32_t(Format::COUNT))
            return c_FormatInfo[0]; // UNKNOWN

        const FormatInfo& info = c_FormatInfo[uint32_t(format)];
        assert(info.format == format);
        return info;
    }

	static uint64_t GetRowPitch(Format format, uint32_t width, uint32_t mipIndex = 0)
    {
        const auto& formatInfo = GetFormatInfo(format);
    	uint64_t numBlocks = (std::max)(1u, Math::DivideByMultiple(width >> mipIndex, formatInfo.blockSize));
    	return numBlocks * formatInfo.bytesPerBlock;
    }

	static uint64_t GetSlicePitch(Format format, uint32_t width, uint32_t height, uint32_t mipIndex = 0)
    {
        const auto& formatInfo = GetFormatInfo(format);
    	uint64_t numBlocksX = (std::max)(1u, Math::DivideByMultiple(width >> mipIndex, formatInfo.blockSize));
    	uint64_t numBlocksY = (std::max)(1u, Math::DivideByMultiple(height >> mipIndex, formatInfo.blockSize));
    	return numBlocksX * numBlocksY * formatInfo.bytesPerBlock;
    }


    enum class FormatSupport : uint32_t
    {
        None            = 0,

        Buffer          = (1 << 0),
        VertexBuffer    = (1 << 1),
        IndexBuffer     = (1 << 2),
        Texture         = (1 << 3),
        DepthStencil    = (1 << 4),
        RenderTarget    = (1 << 5),
        Blendable       = (1 << 6),
        ShaderLoad      = (1 << 7),
        ShaderSample    = (1 << 8),
        ShaderUavLoad   = (1 << 9),
        ShaderUavStore  = (1 << 10),
        ShaderAtomic    = (1 << 11)
    };
    ENABLE_ENUM_BIT_OPERATOR(FormatSupport)


    enum class ResourceStates : uint32_t
    {
        Unknown                     = 0,
        Common                      = (1 << 0),
        ConstantBuffer              = (1 << 1),
        VertexBuffer                = (1 << 2),
        IndexBuffer                 = (1 << 3),
        IndirectArgument            = (1 << 4),
        PixelShaderResource         = (1 << 5),
        NoPixelShaderResource       = (1 << 6),
        UnorderedAccess             = (1 << 7),
        RenderTarget                = (1 << 8),
        DepthWrite                  = (1 << 9),
        DepthRead                   = (1 << 10),
        StreamOut                   = (1 << 11),
        CopyDest                    = (1 << 12),
        CopySource                  = (1 << 13),
        ResolveDest                 = (1 << 14),
        ResolveSource               = (1 << 15),
        Present                     = (1 << 16),
        AccelerationStructrue       = (1 << 17),
        ShadingRateSurface          = (1 << 18),
        ShaderResource              = PixelShaderResource | NoPixelShaderResource
    };
    ENABLE_ENUM_BIT_OPERATOR(ResourceStates)

    // Flags for resources that need to be shared with other graphics APIs or other GPU devices.
    enum class SharedResourceFlags : uint32_t
    {
        None                = 0,

        // D3D11: adds D3D11_RESOURCE_MISC_SHARED
        // D3D12: adds D3D12_HEAP_FLAG_SHARED
        // Vulkan: adds vk::ExternalMemoryImageCreateInfo and vk::ExportMemoryAllocateInfo/vk::ExternalMemoryBufferCreateInfo
        Shared              = 0x01,

        // D3D11: adds (D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        // D3D12, Vulkan: ignored
        Shared_NTHandle     = 0x02,

        // D3D12: adds D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER and D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER
        // D3D11, Vulkan: ignored
        Shared_CrossAdapter = 0x04,
    };
    ENABLE_ENUM_BIT_OPERATOR(SharedResourceFlags)




    struct VertexAttributeDesc
    {
        std::string name;
        Format format = Format::UNKNOWN;
        uint32_t arraySize = 1;
        uint32_t bufferIndex = 0;
        uint32_t offset = 0;
        uint32_t elementStride = 0;
        bool isInstanced = false;

        constexpr VertexAttributeDesc& SetFormat(Format value) { format = value; return *this; }
        constexpr VertexAttributeDesc& SetArraySize(uint32_t value) { arraySize = value; return *this; }
        constexpr VertexAttributeDesc& SetBufferIndex(uint32_t value) { bufferIndex = value; return *this; }
        constexpr VertexAttributeDesc& SetOffset(uint32_t value) { offset = value; return *this; }
        constexpr VertexAttributeDesc& SetElementStride(uint32_t value) { elementStride = value; return *this; }
        constexpr VertexAttributeDesc& SetIsInstanced(bool value) { isInstanced = value; return *this; }

        VertexAttributeDesc& SetName(const std::string& value) { name = value; return *this; }

        constexpr bool operator==(const VertexAttributeDesc& other) const = default;
    };

    struct IInputLayout : public IResource
    {
        [[nodiscard]] virtual uint32_t GetNumAttributes() const = 0;
        [[nodiscard]] virtual const VertexAttributeDesc* GetAttributeDesc(uint32_t index) const = 0;
    };
    using InputLayoutHandle = RefPtr<IInputLayout>;

    enum class CpuAccessMode : uint8_t
    {
        None,
        Read,
        Write
    };




    //////////////////////////////////////////////////////////////////////////
    // Viewport State
    //////////////////////////////////////////////////////////////////////////

    struct ViewportState
    {
        StaticVector<Viewport, c_MaxViewports> viewports;
        StaticVector<Rect, c_MaxViewports> scissorRects;

        ViewportState& AddViewport(const Viewport& v) { viewports.push_back(v); return *this; }
        ViewportState& AddScissorRect(const Rect& r) { scissorRects.push_back(r); return *this; }
        ViewportState& AddViewportAndScissorRect(const Viewport& v) { return AddViewport(v).AddScissorRect(Rect(v)); }
    
        bool operator==(const ViewportState&) const = default;
    };



    

    struct DrawArguments
    {
        uint32_t vertexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startIndexLocation = 0;
        uint32_t startVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawArguments& SetVertexCount(uint32_t value) { vertexCount = value; return *this; }
        constexpr DrawArguments& SetInstanceCount(uint32_t value) { instanceCount = value; return *this; }
        constexpr DrawArguments& SetStartIndexLocation(uint32_t value) { startIndexLocation = value; return *this; }
        constexpr DrawArguments& SetStartVertexLocation(uint32_t value) { startVertexLocation = value; return *this; }
        constexpr DrawArguments& SetStartInstanceLocation(uint32_t value) { startInstanceLocation = value; return *this; }
    };

    struct DrawIndirectArguments
    {
        uint32_t vertexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawIndirectArguments& SetVertexCount(uint32_t value) { vertexCount = value; return *this; }
        constexpr DrawIndirectArguments& SetInstanceCount(uint32_t value) { instanceCount = value; return *this; }
        constexpr DrawIndirectArguments& SetStartVertexLocation(uint32_t value) { startVertexLocation = value; return *this; }
        constexpr DrawIndirectArguments& SetStartInstanceLocation(uint32_t value) { startInstanceLocation = value; return *this; }
    };

    struct DrawIndexedIndirectArguments
    {
        uint32_t indexCount = 0;
        uint32_t instanceCount = 1;
        uint32_t startIndexLocation = 0;
        int32_t  baseVertexLocation = 0;
        uint32_t startInstanceLocation = 0;

        constexpr DrawIndexedIndirectArguments& SetIndexCount(uint32_t value) { indexCount = value; return *this; }
        constexpr DrawIndexedIndirectArguments& SetInstanceCount(uint32_t value) { instanceCount = value; return *this; }
        constexpr DrawIndexedIndirectArguments& SetStartIndexLocation(uint32_t value) { startIndexLocation = value; return *this; }
        constexpr DrawIndexedIndirectArguments& SetBaseVertexLocation(int32_t value) { baseVertexLocation = value; return *this; }
        constexpr DrawIndexedIndirectArguments& SetStartInstanceLocation(uint32_t value) { startInstanceLocation = value; return *this; }
    };
    
    struct DispatchIndirectArguments
    {
        uint32_t groupsX = 1;
        uint32_t groupsY = 1;
        uint32_t groupsZ = 1;

        constexpr DispatchIndirectArguments& SetGroupsX(uint32_t value) { groupsX = value; return *this; }
        constexpr DispatchIndirectArguments& SetGroupsY(uint32_t value) { groupsY = value; return *this; }
        constexpr DispatchIndirectArguments& SetGroupsZ(uint32_t value) { groupsZ = value; return *this; }
        constexpr DispatchIndirectArguments& SetGroups2D(uint32_t x, uint32_t y) { groupsX = x; groupsY = y; return *this; }
        constexpr DispatchIndirectArguments& SetGroups3D(uint32_t x, uint32_t y, uint32_t z) { groupsX = x; groupsY = y; groupsZ = z; return *this; }
    };

    


    enum class Feature : uint8_t
    {
        ComputeQueue,
        ConservativeRasterization,
        ConstantBufferRanges,
        CopyQueue,
        DeferredCommandLists,
        FastGeometryShader,
        HeapDirectlyIndexed,
        HlslExtensionUAV,
        LinearSweptSpheres,
        Meshlets,
        RayQuery,
        RayTracingAccelStruct,
        RayTracingClusters,
        RayTracingOpacityMicromap,
        RayTracingPipeline,
        SamplerFeedback,
        ShaderExecutionReordering,
        ShaderSpecializations,
        SinglePassStereo,
        Spheres,
        VariableRateShading,
        VirtualResources,
        WaveLaneCountMinMax,
        CooperativeVectorInferencing,
        CooperativeVectorTraining
    };

    enum class MessageSeverity : uint8_t
    {
        Info,
        Warning,
        Error,
        Fatal
    };

    enum class CommandQueueType : uint8_t
    {
        Graphics = 0,
        Compute,
        Copy,

        Count
    };

    // 有客户端实现消息回调
    class IMessageCallback
    {
    protected:
        IMessageCallback() = default;
        virtual ~IMessageCallback() = default;

    public:
        // 通过该接口传递消息
        virtual void Message(MessageSeverity severity, const char* messageText) const = 0;

        IMessageCallback(const IMessageCallback&) = delete;
        IMessageCallback(const IMessageCallback&&) = delete;
        IMessageCallback& operator=(const IMessageCallback&) = delete;
        IMessageCallback& operator=(const IMessageCallback&&) = delete;
    };

    static const char* DebugNameToString(const std::string& debugName)
    {
        return debugName.empty() ? "<UNNAMED>" : debugName.c_str();
    }

    static bool VerifyPermanentResourceState(ResourceStates permanentState, ResourceStates requiredState, 
        bool isTexture, const std::string& debugName, IMessageCallback* callback)
    {
        assert(callback != nullptr);
        // 当前状态必须是永久状态的子集
        if ((requiredState & permanentState) != requiredState)
        {
            std::string msg = std::format("Permanent {} {} doesn't have the right state bits. Required: 0x{:x}, present: 0x{:x}.",
                (isTexture ? "texture " : "buffer "), DebugNameToString(debugName), uint32_t(requiredState), uint32_t(permanentState));
            callback->Message(MessageSeverity::Error, msg.c_str());
            return false;
        }
        return true;
    }


   enum class ComparisonFunc : uint8_t
   {
        Never = 1,
        Less = 2,
        Equal = 3,
        LessOrEqual = 4,
        Greater = 5,
        NotEqual = 6,
        GreaterOrEqual = 7,
        Always = 8
   };


} // namespace DSM 


template <typename T>
struct std::hash<DSM::RefPtr<T>>
{
    std::size_t operator()(const DSM::RefPtr<T>& s) const
    {
        std::hash<T*> hash{};
        return hash(s.Get());
    }
};

template <>
struct std::hash<DSM::VertexAttributeDesc>
{
    std::size_t operator()(const DSM::VertexAttributeDesc& v) const
    {
        using namespace DSM::Utility;
        std::size_t hash = 0;
        hash = HashCombine(hash, v.name);
        hash = HashCombine(hash, v.format);
        hash = HashCombine(hash, v.arraySize);
        hash = HashCombine(hash, v.bufferIndex);
        hash = HashCombine(hash, v.elementStride);
        hash = HashCombine(hash, v.isInstanced);
        return hash;
    }
};

#endif