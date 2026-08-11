#pragma once
#ifndef __RAYTRACING_H__
#define __RAYTRACING_H__

namespace DSM {
    struct IBuffer;

    namespace RT {
        class IPipeline;
        class IAccelStruct;

        using AffineTransform = float[12]; // 3x4 行主序矩阵

        constexpr AffineTransform c_IdentityTransform = {
        //  +----+----+---------  rotation and scaling
        //  v    v    v
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f
        //                 ^
        //                 +----  translation
        };

        enum class GeometryFlags : uint8_t
        {
            None,
            Opaque,
            NoDuplicateAnyHitInvocation
        };

        enum class GeometryType : uint8_t
        {
            Triangles = 0,
            AABBs
        };

        struct GeometryAABB
        {
            float minX{}, minY{}, minZ{};
            float maxX{}, maxY{}, maxZ{};
        };

        struct GeometryTriangles
        {
            IBuffer* indexBuffer = nullptr;
            IBuffer* vertexBuffer = nullptr;

            uint64_t indexOffset = 0;
            uint64_t vertexOffset = 0;

            uint32_t vertexCount = 0;
            uint32_t indexCount = 0;
            
            uint32_t vertexStride = 0;
            
            Format indexFormat = Format::UNKNOWN;
            Format vertexFormat = Format::UNKNOWN;

            GeometryTriangles& SetIndexBuffer(IBuffer* buffer) { indexBuffer = buffer; return *this; }
            GeometryTriangles& SetVertexBuffer(IBuffer* buffer) { vertexBuffer = buffer; return *this; }
            GeometryTriangles& SetIndexFormat(Format format) { indexFormat = format; return *this; }
            GeometryTriangles& SetVertexFormat(Format format) { vertexFormat = format; return *this; }
            GeometryTriangles& SetIndexOffset(uint64_t offset) { indexOffset = offset; return *this; }
            GeometryTriangles& SetVertexOffset(uint64_t offset) { vertexOffset = offset; return *this; }
            GeometryTriangles& SetVertexCount(uint32_t count) { vertexCount = count; return *this; }
            GeometryTriangles& SetIndexCount(uint32_t count) { indexCount = count; return *this; }
            GeometryTriangles& SetVertexStride(uint32_t stride) { vertexStride = stride; return *this; }
        };

        struct GeometryAABBs
        {
            IBuffer* buffer = nullptr;
            uint64_t offset = 0;
            uint32_t stride = 0;
            uint32_t count = 0;

            GeometryAABBs& SetBuffer(IBuffer* buffer) { buffer = buffer; return *this; }
            GeometryAABBs& SetOffset(uint64_t offset) { offset = offset; return *this; }
            GeometryAABBs& SetStride(uint32_t stride) { stride = stride; return *this; }
            GeometryAABBs& SetCount(uint32_t count) { count = count; return *this; }
        };

        struct GeometryDesc
        {
            union GeometryTypeUnion
            {
                GeometryTriangles triangles;
                GeometryAABBs aabbs;
            } geometryData{};

            bool useTransform = false;
            AffineTransform transform{};
            GeometryType geometryType = GeometryType::Triangles;
            GeometryFlags flags = GeometryFlags::None;

            GeometryDesc& SetTransform(const AffineTransform& t) { useTransform = true; std::memcpy(transform, t, sizeof(AffineTransform)); return *this; }
            GeometryDesc& SetFlags(GeometryFlags f) { flags = f; return *this; }
            GeometryDesc& SetTriangles(const GeometryTriangles& t) { geometryType = GeometryType::Triangles; geometryData.triangles = t; return *this; }
            GeometryDesc& SetAABBs(const GeometryAABBs& a) { geometryType = GeometryType::AABBs; geometryData.aabbs = a; return *this; }
        };

        enum class InstanceFlags : uint32_t
        {
            None = 0,
            TriangleCullDisable = 1 << 0,
            TriangleFrontCounterclockwise = 1 << 1,
            ForceOpaque = 1 << 2,
            ForceNonOpaque = 1 << 3
        };

        struct InstanceDesc
        {
            AffineTransform transform{};
            uint32_t instanceID : 24;
            uint32_t instanceMask : 8;
            uint32_t instanceContributionToHitGroupIndex : 24;
            InstanceFlags flags : 8;

            union
            {
                IAccelStruct* bottomLevelAS; // for buildTopLevelAccelStruct
                uint64_t blasDeviceAddress;  // for buildTopLevelAccelStructFromBuffer - use IAccelStruct::getDeviceAddress()
            };

            InstanceDesc()
                : instanceID(0)
                , instanceMask(0)
                , instanceContributionToHitGroupIndex(0)
                , flags(InstanceFlags::None)
                , bottomLevelAS(nullptr)
            {
                SetTransform(c_IdentityTransform);
            }

            InstanceDesc& SetTransform(const AffineTransform& t) { std::memcpy(transform, t, sizeof(AffineTransform)); return *this; }
            InstanceDesc& SetInstanceID(uint32_t id) { instanceID = id; return *this; }
            InstanceDesc& SetInstanceMask(uint32_t mask) { instanceMask = mask; return *this; }
            InstanceDesc& SetInstanceContributionToHitGroupIndex(uint32_t index) { instanceContributionToHitGroupIndex = index; return *this; }
            InstanceDesc& SetFlags(InstanceFlags f) { flags = f; return *this; }
            InstanceDesc& SetBottomLevelAS(IAccelStruct* blas) { bottomLevelAS = blas; return *this; }
        };
        
        enum class AccelStructBuildFlags : uint8_t
        {
            None = 0,
            AllowUpdate = 1 << 0,
            AllowCompaction = 1 << 1,
            PreferFastTrace = 1 << 2,
            PreferFastBuild = 1 << 3,
            MinimizeMemory = 1 << 4,
            PerformUpdate = 1 << 5,
        };

        // 加速结构描述。
        struct AccelStructDesc
        {
            size_t topLevelMaxInstances = 0; // only applies when isTopLevel = true
            std::vector<GeometryDesc> bottomLevelGeometries{}; // only applies when isTopLevel = false
            std::string debugName{};
            AccelStructBuildFlags buildFlags = AccelStructBuildFlags::None;
            bool trackLiveness = true;
            bool isTopLevel = false;
            bool isVirtual = false;

            AccelStructDesc& SetTopLevelMaxInstances(size_t maxInstances) { topLevelMaxInstances = maxInstances; return *this; }
            AccelStructDesc& AddBottomLevelGeometry(const GeometryDesc& geometry) { bottomLevelGeometries.push_back(geometry); return *this; }
            AccelStructDesc& SetDebugName(const std::string& name) { debugName = name; return *this; }
            AccelStructDesc& SetBuildFlags(AccelStructBuildFlags flags) { buildFlags = flags; return *this; }
            AccelStructDesc& SetTrackLiveness(bool track) { trackLiveness = track; return *this; }
            AccelStructDesc& SetIsTopLevel(bool topLevel) { isTopLevel = topLevel; return *this; }
            AccelStructDesc& SetIsVirtual(bool virtualAS) { isVirtual = virtualAS; return *this; }
        };

        class IAccelStruct : public IResource
        {
        public:
            [[nodiscard]] virtual const AccelStructDesc& GetDesc() const = 0;
            [[nodiscard]] virtual uint64_t GetDeviceAddress() const = 0;
            [[nodiscard]] virtual IBuffer* GetDataBuffer() const = 0;
        };
        using AccelStructHandle = RefPtr<IAccelStruct>;


        struct PipelineShaderDesc
        {
            std::string exportName{};
            ShaderHandle shader = nullptr;
            BindingLayoutHandle bindingLayout = nullptr;

            PipelineShaderDesc& SetExportName(const std::string& name) { exportName = name; return *this; }
            PipelineShaderDesc& SetShader(IShader* s) { shader = s; return *this; }
            PipelineShaderDesc& SetBindingLayout(IBindingLayout* layout) { bindingLayout = layout; return *this; }
        };

        struct PipelineHitGroupDesc
        {
            std::string exportName{};
            ShaderHandle closestHitShader = nullptr;
            ShaderHandle anyHitShader = nullptr;
            ShaderHandle intersectionShader = nullptr;
            BindingLayoutHandle bindingLayout = nullptr;
            bool isProceduralPrimitive = false;

            PipelineHitGroupDesc& SetExportName(const std::string& name) { exportName = name; return *this; }
            PipelineHitGroupDesc& SetClosestHitShader(ShaderHandle shader) { closestHitShader = shader; return *this; }
            PipelineHitGroupDesc& SetAnyHitShader(ShaderHandle shader) { anyHitShader = shader; return *this; }
            PipelineHitGroupDesc& SetIntersectionShader(ShaderHandle shader) { intersectionShader = shader; return *this; }
            PipelineHitGroupDesc& SetBindingLayout(IBindingLayout* layout) { bindingLayout = layout; return *this; }
            PipelineHitGroupDesc& SetIsProceduralPrimitive(bool isProcedural) { isProceduralPrimitive = isProcedural; return *this; }
        };

        struct PipelineDesc
        {
            std::vector<PipelineShaderDesc> shaders{};
            std::vector<PipelineHitGroupDesc> hitGroups{};
            BindingLayoutVector globalBindingLayout{};
            uint32_t maxPayloadSize = 0;
            uint32_t maxAttributeSize = 0;
            // 最大递归深度
            uint32_t maxRecursionDepth = 1;
            int32_t hlslExtensionsUAV = -1;

            PipelineDesc& AddShader(const PipelineShaderDesc& shader) { shaders.push_back(shader); return *this; }
            PipelineDesc& AddHitGroup(const PipelineHitGroupDesc& hitGroup) { hitGroups.push_back(hitGroup); return *this; }
            PipelineDesc& AddBindingLayout(IBindingLayout* layout) { globalBindingLayout.push_back(layout); return *this; }
            PipelineDesc& SetMaxPayloadSize(uint32_t size) { maxPayloadSize = size; return *this; }
            PipelineDesc& SetMaxAttributeSize(uint32_t size) { maxAttributeSize = size; return *this; }
            PipelineDesc& SetMaxRecursionDepth(uint32_t depth) { maxRecursionDepth = depth; return *this; }
            PipelineDesc& SetHLSLExtensionsUAV(int32_t uav) { hlslExtensionsUAV = uav; return *this; }
        };

        struct ShaderTableDesc
        {
            bool isCached = false;
            uint32_t maxEntries = 0;
            std::string debugName{};

            ShaderTableDesc& SetIsCached(bool cache) { isCached = cache; return *this; }
            ShaderTableDesc& SetMaxEntries(uint32_t entries) { maxEntries = entries; return *this; }
            ShaderTableDesc& SetDebugName(const std::string& name) { debugName = name; return *this; }
            ShaderTableDesc& EnableCaching(uint32_t maxEntries) { isCached = true; this->maxEntries = maxEntries; return *this; }
        };

        class IShaderTable : public IResource
        {
        public:
            virtual const ShaderTableDesc& GetDesc() const = 0;
            virtual uint32_t GetNumEntries() const = 0;
            virtual IPipeline* GetPipeline() const = 0;
            virtual void SetGenerationShader(const char* exportName, IBindingSet* bindingSet = nullptr) = 0;
            virtual int AddMissShader(const char* exportName, IBindingSet* bindingSet = nullptr) = 0;
            virtual int AddHitGroup(const char* exportName, IBindingSet* bindingSet = nullptr) = 0;
            virtual int AddCallableShader(const char* exportName, IBindingSet* bindingSet = nullptr) = 0;
            virtual void ClearMissShaders() = 0;
            virtual void ClearHitGroups() = 0;
            virtual void ClearCallableShaders() = 0;
        };
        using ShaderTableHandle = RefPtr<IShaderTable>;

        class IPipeline : public IResource
        {
        public:
            [[nodiscard]] virtual const PipelineDesc& GetDesc() const = 0;
            virtual ShaderTableHandle CreateShaderTable(const ShaderTableDesc& desc = {}) = 0;
        };
        using PipelineHandle = RefPtr<IPipeline>;

        struct State
        {
            BindingSetVector bindingSets;
            IShaderTable* shaderTable;

            State& AddBindingSet(IBindingSet* bindingSet) { bindingSets.push_back(bindingSet); return *this; }
            State& SetShaderTable(IShaderTable* table) { shaderTable = table; return *this; }
        };

        struct DispatchRaysArguments
        {
            uint32_t width = 1;
            uint32_t height = 1;
            uint32_t depth = 1;

            constexpr DispatchRaysArguments& SetWidth(uint32_t w) { width = w; return *this; }
            constexpr DispatchRaysArguments& SetHeight(uint32_t h) { height = h; return *this; }
            constexpr DispatchRaysArguments& SetDepth(uint32_t d) { depth = d; return *this; }
        };

    } // namespace RT

    ENABLE_ENUM_BIT_OPERATOR(RT::InstanceFlags)
    ENABLE_ENUM_BIT_OPERATOR(RT::GeometryFlags)
    ENABLE_ENUM_BIT_OPERATOR(RT::AccelStructBuildFlags)
} // namespace DSM

#endif
