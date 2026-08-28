#include "RestirDIRenderPipeline.h"

#include "RestirDIEnvironment.h"
#include "RestirDIScene.h"
#include "RestirDIValidationScene.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Graphics/Framebuffer.h"
#include "Runtime/Render/Camera/CameraController.h"
#include "Runtime/Render/ShaderCompiler.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <span>
#include <windows.h>

namespace DSM::RestirDI {
    namespace {
        constexpr uint32_t kMaxMaterialTextures = 256;

        struct FrameBindings
        {
            IBuffer* surfaceCurrent{};
            IBuffer* surfacePrevious{};
            IBuffer* reservoirCurrentSample{};
            IBuffer* reservoirCurrentStats{};
            IBuffer* reservoirHistorySample{};
            IBuffer* reservoirHistoryStats{};
            IBuffer* hdrInput{};
            IBuffer* acceptanceInput{};
            IBuffer* surfaceOutput{};
            IBuffer* reservoirSampleOutput{};
            IBuffer* reservoirStatsOutput{};
            IBuffer* acceptanceOutput{};
            IBuffer* hdrOutput{};

            bool operator==(const FrameBindings&) const = default;
        };

        std::filesystem::path ExecutableDirectory()
        {
            std::wstring buffer(32768, L'\0');
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }

        std::filesystem::path FindExisting(std::initializer_list<std::filesystem::path> candidates)
        {
            for (const auto& candidate : candidates) {
                std::error_code error{};
                if (std::filesystem::exists(candidate, error)) return std::filesystem::absolute(candidate);
            }
            return {};
        }

        BufferHandle CreateGpuBuffer(
            IDevice* device, size_t count, size_t stride, const std::string& name, bool uav = true)
        {
            return device->CreateBuffer(BufferDesc{}
                .SetByteSize(std::max<size_t>(count, 1) * stride)
                .SetStructStride(stride)
                .SetInitialState(ResourceStates::Common)
                .SetCanHaveUAVs(uav)
                .SetDebugName(name));
        }

        ShaderHandle CompileShader(
            IDevice* device,
            const std::filesystem::path& filename,
            const char* entry,
            ShaderType type)
        {
            ShaderByteCode byteCode(ShaderCompileDesc{}
                .SetType(type)
                .SetMode(ShaderMode::SM_6_6)
                .SetFilename(filename.string())
                .SetEnterPoint(entry));
            if (!byteCode.IsValid()) return nullptr;
            return device->CreateShader(ShaderDesc{}
                .SetShaderType(type)
                .SetEntryName(entry)
                .SetDebugName(entry), byteCode.GetByteCode(), byteCode.GetByteCodeSize());
        }

        bool SettingsRequireHistoryReset(const Settings& lhs, const Settings& rhs)
        {
            return lhs.renderMode != rhs.renderMode ||
                lhs.initialCandidateCount != rhs.initialCandidateCount ||
                lhs.samplesPerPixel != rhs.samplesPerPixel ||
                lhs.referenceSamplesPerPixel != rhs.referenceSamplesPerPixel ||
                lhs.historyMCapMultiplier != rhs.historyMCapMultiplier ||
                lhs.spatialPassCount != rhs.spatialPassCount ||
                lhs.spatialNeighborCount != rhs.spatialNeighborCount ||
                lhs.spatialRadius != rhs.spatialRadius ||
                lhs.normalThresholdDegrees != rhs.normalThresholdDegrees ||
                lhs.relativeDepthThreshold != rhs.relativeDepthThreshold ||
                lhs.enableTemporalReuse != rhs.enableTemporalReuse ||
                lhs.enableSpatialReuse != rhs.enableSpatialReuse ||
                lhs.enableAnalyticLights != rhs.enableAnalyticLights ||
                lhs.enableEmissiveTriangles != rhs.enableEmissiveTriangles ||
                lhs.enableEnvironment != rhs.enableEnvironment ||
                lhs.freezeRandomSeed != rhs.freezeRandomSeed ||
                lhs.analyticDomainWeight != rhs.analyticDomainWeight ||
                lhs.emissiveDomainWeight != rhs.emissiveDomainWeight ||
                lhs.environmentDomainWeight != rhs.environmentDomainWeight ||
                lhs.alphaCutoff != rhs.alphaCutoff ||
                lhs.normalBias != rhs.normalBias ||
                lhs.maxRayDistance != rhs.maxRayDistance ||
                lhs.environmentIntensity != rhs.environmentIntensity ||
                lhs.environmentRotationDegrees != rhs.environmentRotationDegrees ||
                lhs.randomSeed != rhs.randomSeed;
        }
    }

    struct RenderPipeline::Implementation
    {
        IDevice* device = nullptr;
        SceneAdapter scene{};
        std::unique_ptr<CameraController> cameraController{};
        EnvironmentData environment{};
        BindingLayoutHandle commonLayout{};
        BindingLayoutHandle textureLayout{};
        BindingSetHandle textureSet{};
        SamplerHandle sampler{};
        ShaderLibraryHandle rayTracingLibrary{};
        RT::PipelineHandle rayTracingPipeline{};
        RT::ShaderTableHandle primaryTable{};
        RT::ShaderTableHandle visibilityTable{};
        RT::ShaderTableHandle referenceTable{};
        ComputePipelineHandle initialPipeline{};
        ComputePipelineHandle temporalPipeline{};
        ComputePipelineHandle spatialPipeline{};
        GraphicsPipelineHandle presentPipeline{};
        ShaderHandle presentVertexShader{};
        ShaderHandle presentPixelShader{};
        FramebufferHandle framebuffer{};
        BufferHandle frameConstants{};
        BufferHandle environmentPixels{};
        BufferHandle environmentAlias{};
        BufferHandle dummySrvBuffer{};
        BufferHandle dummyUavBuffer{};
        BufferHandle validationCounters{};
        std::array<BufferHandle, 2> surfaces{};
        std::vector<BufferHandle> reservoirSamples{};
        std::vector<BufferHandle> reservoirStats{};
        std::array<BufferHandle, 2> acceptance{};
        BufferHandle acceptanceAggregate{};
        BufferHandle hdr{};
        std::filesystem::path shaderDirectory{};
        std::filesystem::path assetsDirectory{};
        std::array<char, 1024> environmentPath{};
        std::vector<ITexture*> boundTextures{};
        std::vector<std::pair<FrameBindings, BindingSetHandle>> bindingSetCache{};
        Math::Matrix4 previousViewProjection = Math::Matrix4::Identity;
        Settings previousSettings{};
        std::string error{};
        std::string environmentStatus{};
        uint32_t width = 1;
        uint32_t height = 1;
        int32_t surfaceHistory = -1;
        std::vector<int32_t> reservoirHistoryIndices{};
        uint32_t reservoirLaneCount = 1;
        int32_t lastSurface = 0;
        int32_t lastReservoir = 0;
        bool initialized = false;
        bool dxrAvailable = false;
        bool historyValid = false;
        bool historyResetRequested = true;
        bool environmentDirty = true;
        bool settingsInitialized = false;

        bool Initialize(GraphicsRenderer& renderer, uint32_t samplesPerPixel);
        bool CreatePipelines(GraphicsRenderer& renderer);
        void CreateResolutionResources(
            GraphicsRenderer& renderer, uint32_t newWidth, uint32_t newHeight, uint32_t samplesPerPixel);
        void EnsureEnvironmentBuffers();
        bool EnsureTextureTable();
        BindingSetHandle CreateBindingSet(const FrameBindings& bindings);
        GpuFrameConstants BuildFrameConstants(
            GraphicsRenderer& renderer, const Settings& settings, uint64_t frameIndex,
            uint32_t spatialPass, uint32_t sampleIndex) const;
        bool Capture(GraphicsRenderer& renderer, ValidationSnapshot& snapshot, std::string& captureError);
    };

    bool RenderPipeline::Implementation::Initialize(
        GraphicsRenderer& renderer, uint32_t samplesPerPixel)
    {
        device = renderer.GetDevice();
        if (device == nullptr) {
            error = "ReSTIR DI 初始化失败：Device 为空。";
            return false;
        }
        dxrAvailable = device->QueryFeatureSupport(Feature::RayTracingPipeline);
        if (!dxrAvailable) {
            error = "当前设备不支持 DXR RayTracingPipeline。";
            return false;
        }

        const auto executableDirectory = ExecutableDirectory();
        const auto currentDirectory = std::filesystem::current_path();
        shaderDirectory = FindExisting({
            executableDirectory / "Shaders" / "RestirDITrace.hlsl",
            currentDirectory / "Samples" / "RayTracing" / "RestirDI" / "Shaders" / "RestirDITrace.hlsl"});
        if (!shaderDirectory.empty()) shaderDirectory = shaderDirectory.parent_path();
        assetsDirectory = FindExisting({
            executableDirectory / "Assets" / "Textures" / "daylight0.png",
            currentDirectory / "Samples" / "Assets" / "Textures" / "daylight0.png"});
        if (!assetsDirectory.empty()) assetsDirectory = assetsDirectory.parent_path().parent_path();
        if (shaderDirectory.empty() || assetsDirectory.empty()) {
            error = "找不到 ReSTIR DI Shader 或 Samples/Assets 目录。";
            return false;
        }
        if (!LoadDaylightEnvironment(assetsDirectory, environment, error)) return false;

        BindingLayoutDesc commonDescription{};
        commonDescription.SetVisibility(ShaderType::All)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0))
            .AddItem(BindingLayoutItem::RayTracingAccelStruct(0));
        for (uint32_t slot = 1; slot <= 19; ++slot) {
            commonDescription.AddItem(BindingLayoutItem::StructuredBuffer_SRV(slot));
        }
        for (uint32_t slot = 0; slot <= 5; ++slot) {
            commonDescription.AddItem(BindingLayoutItem::StructuredBuffer_UAV(slot));
        }
        commonDescription.AddItem(BindingLayoutItem::Sampler(0));
        commonLayout = device->CreateBindingLayout(commonDescription);

        textureLayout = device->CreateBindingLayout(BindingLayoutDesc{}
            .SetVisibility(ShaderType::All)
            .SetRegisterSpace(1)
            .AddItem(BindingLayoutItem::Texture_SRV(0).SetSize(kMaxMaterialTextures)));
        sampler = device->CreateSampler(SamplerDesc{}
            .SetAllFilters(true)
            .SetAllAddressModes(SamplerAddressMode::Wrap)
            .SetMaxAnisotropy(8.0f));
        frameConstants = device->CreateBuffer(BufferDesc{}
            .SetByteSize(sizeof(GpuFrameConstants))
            .SetIsConstantBuffer(true)
            .SetIsVolatile(true)
            .SetDebugName("ReSTIR DI Frame Constants"));
        dummySrvBuffer = CreateGpuBuffer(device, 32, sizeof(GpuFloat4), "ReSTIR DI Dummy SRV", false);
        dummyUavBuffer = CreateGpuBuffer(device, 32, sizeof(GpuFloat4), "ReSTIR DI Dummy UAV");
        validationCounters = CreateGpuBuffer(device, 16, sizeof(GpuUint4), "ReSTIR DI Validation Counters");
        EnsureEnvironmentBuffers();

        const auto& viewport = renderer.GetCamera().GetViewPort();
        CreateResolutionResources(renderer,
            std::max(static_cast<uint32_t>(viewport.Width()), 1u),
            std::max(static_cast<uint32_t>(viewport.Height()), 1u),
            samplesPerPixel);
        if (!CreatePipelines(renderer)) return false;
        initialized = true;
        error.clear();
        return true;
    }

    bool RenderPipeline::Implementation::CreatePipelines(GraphicsRenderer& renderer)
    {
        const auto traceFilename = shaderDirectory / "RestirDITrace.hlsl";
        ShaderByteCode libraryByteCode(ShaderCompileDesc{}
            .SetType(ShaderType::Library)
            .SetMode(ShaderMode::SM_6_6)
            .SetFilename(traceFilename.string()));
        if (!libraryByteCode.IsValid()) {
            error = "ReSTIR DI DXR Shader Library 编译失败。";
            return false;
        }
        rayTracingLibrary = device->CreateShaderLibrary(
            libraryByteCode.GetByteCode(), libraryByteCode.GetByteCodeSize());
        const auto primary = rayTracingLibrary->GetShader("PrimaryRayGen", ShaderType::RayGeneration);
        const auto visibility = rayTracingLibrary->GetShader("VisibilityRayGen", ShaderType::RayGeneration);
        const auto reference = rayTracingLibrary->GetShader("ReferenceRayGen", ShaderType::RayGeneration);
        const auto miss = rayTracingLibrary->GetShader("Miss", ShaderType::Miss);
        const auto closestHit = rayTracingLibrary->GetShader("ClosestHit", ShaderType::ClosestHit);
        const auto anyHit = rayTracingLibrary->GetShader("AlphaAnyHit", ShaderType::AnyHit);
        if (!primary || !visibility || !reference || !miss || !closestHit || !anyHit) {
            error = "ReSTIR DI DXR Shader 导出不完整。";
            return false;
        }

        RT::PipelineDesc rayTracingDescription{};
        rayTracingDescription
            .AddShader(RT::PipelineShaderDesc{}.SetExportName("PrimaryRayGen").SetShader(primary))
            .AddShader(RT::PipelineShaderDesc{}.SetExportName("VisibilityRayGen").SetShader(visibility))
            .AddShader(RT::PipelineShaderDesc{}.SetExportName("ReferenceRayGen").SetShader(reference))
            .AddShader(RT::PipelineShaderDesc{}.SetExportName("Miss").SetShader(miss))
            .AddHitGroup(RT::PipelineHitGroupDesc{}
                .SetExportName("SurfaceHitGroup")
                .SetClosestHitShader(closestHit)
                .SetAnyHitShader(anyHit))
            .AddBindingLayout(commonLayout)
            .AddBindingLayout(textureLayout)
            .SetMaxPayloadSize(sizeof(uint32_t) * 2)
            .SetMaxAttributeSize(sizeof(float) * 2)
            .SetMaxRecursionDepth(1);
        rayTracingPipeline = device->CreateRayTracingPipeline(rayTracingDescription);
        if (!rayTracingPipeline) {
            error = "创建 ReSTIR DI DXR Pipeline 失败。";
            return false;
        }
        auto createTable = [this](const char* rayGeneration, const char* name) {
            auto table = rayTracingPipeline->CreateShaderTable(RT::ShaderTableDesc{}.SetDebugName(name));
            table->SetGenerationShader(rayGeneration);
            table->AddMissShader("Miss");
            table->AddHitGroup("SurfaceHitGroup");
            return table;
        };
        primaryTable = createTable("PrimaryRayGen", "ReSTIR DI Primary Table");
        visibilityTable = createTable("VisibilityRayGen", "ReSTIR DI Visibility Table");
        referenceTable = createTable("ReferenceRayGen", "ReSTIR DI Reference Table");

        const auto computeFilename = shaderDirectory / "RestirDICompute.hlsl";
        auto createCompute = [&](const char* entry) {
            auto shader = CompileShader(device, computeFilename, entry, ShaderType::Compute);
            return shader ? device->CreateComputePipeline(ComputePipelineDesc{}
                .SetComputeShader(shader).AddBindingLayout(commonLayout, 0).AddBindingLayout(textureLayout, 1)) : nullptr;
        };
        initialPipeline = createCompute("InitialRISCS");
        temporalPipeline = createCompute("TemporalReuseCS");
        spatialPipeline = createCompute("SpatialReuseCS");
        const auto presentFilename = shaderDirectory / "RestirDIPresent.hlsl";
        presentVertexShader = CompileShader(device, presentFilename, "PresentVS", ShaderType::Vertex);
        presentPixelShader = CompileShader(device, presentFilename, "PresentPS", ShaderType::Pixel);
        if (!initialPipeline || !temporalPipeline || !spatialPipeline ||
            !presentVertexShader || !presentPixelShader) {
            error = "ReSTIR DI Compute 或 Present Shader 编译失败。";
            return false;
        }
        presentPipeline = device->CreateGraphicsPipeline(GraphicsPipelineDesc{}
            .SetVertexShader(presentVertexShader)
            .SetPixelShader(presentPixelShader)
            .SetRenderState(RenderState{}
                .SetDepthStencilState(DepthStencilState{}.DisableDepthTest().DisableDepthWrite())
                .SetRasterState(RasterState{}.SetCullNone()))
            .AddBindingLayout(commonLayout, 0)
            .AddBindingLayout(textureLayout, 1), framebuffer);
        if (!presentPipeline) {
            error = "创建 ReSTIR DI Present Pipeline 失败。";
            return false;
        }
        return true;
    }

    void RenderPipeline::Implementation::CreateResolutionResources(
        GraphicsRenderer& renderer,
        uint32_t newWidth,
        uint32_t newHeight,
        uint32_t samplesPerPixel)
    {
        bindingSetCache.clear();
        width = std::max(newWidth, 1u);
        height = std::max(newHeight, 1u);
        reservoirLaneCount = std::clamp(samplesPerPixel, 1u, 8u);
        const size_t pixelCount = size_t(width) * height;
        for (uint32_t index = 0; index < surfaces.size(); ++index) {
            surfaces[index] = CreateGpuBuffer(device, pixelCount, sizeof(GpuSurface),
                "ReSTIR DI Surface " + std::to_string(index));
        }
        reservoirSamples.resize(reservoirLaneCount + 2u);
        reservoirStats.resize(reservoirLaneCount + 2u);
        for (uint32_t index = 0; index < reservoirSamples.size(); ++index) {
            reservoirSamples[index] = CreateGpuBuffer(device, pixelCount, sizeof(GpuReservoirSample),
                "ReSTIR DI Reservoir Sample " + std::to_string(index));
            reservoirStats[index] = CreateGpuBuffer(device, pixelCount, sizeof(GpuReservoirStats),
                "ReSTIR DI Reservoir Stats " + std::to_string(index));
        }
        for (uint32_t index = 0; index < acceptance.size(); ++index) {
            acceptance[index] = CreateGpuBuffer(device, pixelCount, sizeof(GpuAcceptance),
                "ReSTIR DI Acceptance " + std::to_string(index));
        }
        acceptanceAggregate = CreateGpuBuffer(
            device, pixelCount, sizeof(GpuAcceptance), "ReSTIR DI Acceptance Aggregate");
        hdr = CreateGpuBuffer(device, pixelCount, sizeof(GpuFloat4), "ReSTIR DI HDR");
        framebuffer = device->CreateFramebuffer(FramebufferDesc{}.AddColorAttachment(renderer.GetColorTexture()));
        surfaceHistory = -1;
        reservoirHistoryIndices.assign(reservoirLaneCount, -1);
        lastSurface = 0;
        lastReservoir = 0;
        historyValid = false;
        historyResetRequested = true;
    }

    void RenderPipeline::Implementation::EnsureEnvironmentBuffers()
    {
        bindingSetCache.clear();
        environmentPixels = CreateGpuBuffer(device,
            environment.pixels.size(), sizeof(GpuFloat4), "ReSTIR DI Environment", false);
        environmentAlias = CreateGpuBuffer(device,
            environment.aliasTable.entries.size(), sizeof(GpuAliasEntry), "ReSTIR DI Environment Alias", false);
        environmentDirty = true;
    }

    bool RenderPipeline::Implementation::EnsureTextureTable()
    {
        const auto& textures = scene.GetTextures();
        std::vector<ITexture*> current{};
        current.reserve(textures.size());
        for (const auto& texture : textures) current.push_back(texture.Get());
        if (current == boundTextures && textureSet != nullptr) return true;
        if (current.empty() || current.size() > kMaxMaterialTextures) {
            error = std::format("ReSTIR DI 材质纹理数 {} 超出固定描述符表容量 {}。",
                current.size(), kMaxMaterialTextures);
            return false;
        }
        BindingSetDesc description{};
        for (uint32_t index = 0; index < kMaxMaterialTextures; ++index) {
            ITexture* texture = index < current.size() ? current[index] : current.front();
            description.AddItem(BindingSetItem::Texture_SRV(0, texture).SetArrayElement(index));
        }
        textureSet = device->CreateBindingSet(description, textureLayout);
        if (!textureSet) {
            error = "创建 ReSTIR DI 材质纹理描述符集失败。";
            return false;
        }
        boundTextures = std::move(current);
        return true;
    }

    BindingSetHandle RenderPipeline::Implementation::CreateBindingSet(const FrameBindings& input)
    {
        for (const auto& [bindings, bindingSet] : bindingSetCache) {
            if (bindings == input) return bindingSet;
        }
        auto srv = [this](IBuffer* buffer) { return buffer != nullptr ? buffer : dummySrvBuffer.Get(); };
        auto uav = [this](IBuffer* buffer) { return buffer != nullptr ? buffer : dummyUavBuffer.Get(); };
        if (scene.GetTLAS() == nullptr) return nullptr;
        BindingSetDesc description{};
        description
            .AddItem(BindingSetItem::ConstantBuffer(0, frameConstants))
            .AddItem(BindingSetItem::RayTracingAccelStruct(0, scene.GetTLAS()->GetDataBuffer()))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(1, srv(scene.GetVertexBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(2, srv(scene.GetIndexBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(3, srv(scene.GetGeometryBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(4, srv(scene.GetInstanceBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(5, srv(scene.GetMaterialBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(6, srv(scene.GetLightBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(7, srv(scene.GetLightAliasBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(8, srv(scene.GetEmissiveBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(9, srv(scene.GetEmissiveAliasBuffer())))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(10, srv(environmentPixels)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(11, srv(environmentAlias)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(12, srv(input.surfaceCurrent)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(13, srv(input.surfacePrevious)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(14, srv(input.reservoirCurrentSample)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(15, srv(input.reservoirCurrentStats)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(16, srv(input.reservoirHistorySample)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(17, srv(input.reservoirHistoryStats)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(18, srv(input.hdrInput)))
            .AddItem(BindingSetItem::StructuredBuffer_SRV(19, srv(input.acceptanceInput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(0, uav(input.surfaceOutput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(1, uav(input.reservoirSampleOutput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(2, uav(input.reservoirStatsOutput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(3, uav(input.acceptanceOutput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(4, uav(input.hdrOutput)))
            .AddItem(BindingSetItem::StructuredBuffer_UAV(5, validationCounters))
            .AddItem(BindingSetItem::Sampler(0, sampler));
        auto bindingSet = device->CreateBindingSet(description, commonLayout);
        if (bindingSet) bindingSetCache.emplace_back(input, bindingSet);
        return bindingSet;
    }

    GpuFrameConstants RenderPipeline::Implementation::BuildFrameConstants(
        GraphicsRenderer& renderer,
        const Settings& settings,
        uint64_t frameIndex,
        uint32_t spatialPass,
        uint32_t sampleIndex) const
    {
        GpuFrameConstants constants{};
        const auto viewProjection = renderer.GetCamera().GetViewProjMatrix();
        constants.viewProjection = ToGpuMatrix(viewProjection);
        constants.inverseViewProjection = ToGpuMatrix(Math::Matrix4::Inverse(viewProjection));
        constants.previousViewProjection = ToGpuMatrix(historyValid
            ? previousViewProjection : viewProjection);
        constants.cameraExposure = ToGpuFloat4(renderer.GetCamera().GetPosition(), settings.exposure);
        constants.reuseThresholds = {
            std::cos(settings.normalThresholdDegrees * std::numbers::pi_v<float> / 180.0f),
            settings.relativeDepthThreshold, settings.spatialRadius, settings.normalBias};

        const float analyticWeight = settings.enableAnalyticLights
            ? scene.GetAnalyticPower() * std::max(settings.analyticDomainWeight, 0.0f) : 0.0f;
        const float emissiveWeight = settings.enableEmissiveTriangles
            ? scene.GetEmissivePower() * std::max(settings.emissiveDomainWeight, 0.0f) : 0.0f;
        const float environmentWeight = settings.enableEnvironment
            ? environment.aliasTable.totalWeight * std::max(settings.environmentDomainWeight, 0.0f) *
                std::max(settings.environmentIntensity, 0.0f) : 0.0f;
        const float totalWeight = analyticWeight + emissiveWeight + environmentWeight;
        if (totalWeight > 0.0f) {
            constants.domainProbabilities = {
                analyticWeight / totalWeight, emissiveWeight / totalWeight,
                environmentWeight / totalWeight, settings.environmentIntensity};
        }
        constants.rayEnvironment = {
            settings.environmentRotationDegrees * std::numbers::pi_v<float> / 180.0f,
            settings.maxRayDistance, settings.alphaCutoff, historyValid ? 1.0f : 0.0f};
        constants.resolutionFrame = {
            width, height,
            settings.freezeRandomSeed ? 0u : static_cast<uint32_t>(frameIndex),
            settings.randomSeed};
        constants.sourceCounts = {
            scene.GetLightCount(), scene.GetEmissiveCount(),
            static_cast<uint32_t>(environment.pixels.size()), scene.GetLogicalInstanceCount()};
        constants.algorithm = {
            std::max(settings.initialCandidateCount, 1u),
            std::max(settings.initialCandidateCount * settings.historyMCapMultiplier, 1u),
            settings.spatialNeighborCount, spatialPass};
        constants.modes = {
            static_cast<uint32_t>(settings.renderMode), static_cast<uint32_t>(settings.debugView),
            settings.enableTemporalReuse ? 1u : 0u, settings.enableSpatialReuse ? 1u : 0u};
        constants.environmentInfo = {
            environment.width, environment.height,
            (settings.enableAnalyticLights ? 1u : 0u) |
                (settings.enableEmissiveTriangles ? 2u : 0u) |
                (settings.enableEnvironment ? 4u : 0u), 0u};
        constants.sampling = {
            std::clamp(settings.samplesPerPixel, 1u, 8u),
            std::clamp(settings.referenceSamplesPerPixel, 1u, 512u), sampleIndex, 0u};
        return constants;
    }

    RenderPipeline::RenderPipeline(bool enableUI)
        : m_Implementation(std::make_unique<Implementation>())
        , m_EnableUI(enableUI)
    {
    }

    RenderPipeline::~RenderPipeline() = default;

    void RenderPipeline::Render(GraphicsRenderer& renderer, float deltaTime)
    {
        auto& implementation = *m_Implementation;
        const uint32_t samplesPerPixel = std::clamp(m_Settings.samplesPerPixel, 1u, 8u);
        if (!implementation.initialized &&
            !implementation.Initialize(renderer, samplesPerPixel)) return;

        if (m_EnableUI && m_Settings.enableCameraControl && ImGui::GetCurrentContext() != nullptr) {
            if (implementation.cameraController == nullptr) {
                implementation.cameraController = std::make_unique<CameraController>();
                implementation.cameraController->InitCamera(&renderer.GetCamera());
            }
            implementation.cameraController->SetMoveSpeed(m_Settings.cameraMoveSpeed);
            implementation.cameraController->SetMouseSensitivity(
                m_Settings.cameraMouseSensitivity, m_Settings.cameraMouseSensitivity);
            implementation.cameraController->Update(deltaTime);
        }
        else if (!m_Settings.enableCameraControl) {
            implementation.cameraController.reset();
        }

        if (implementation.reservoirLaneCount != samplesPerPixel) {
            implementation.CreateResolutionResources(
                renderer, implementation.width, implementation.height, samplesPerPixel);
        }

        if (implementation.settingsInitialized &&
            SettingsRequireHistoryReset(m_Settings, implementation.previousSettings)) {
            implementation.historyResetRequested = true;
        }
        implementation.previousSettings = m_Settings;
        implementation.settingsInitialized = true;

        std::string syncError{};
        const SceneSyncResult syncResult = implementation.scene.Synchronize(
            implementation.device, m_Settings, syncError);
        if (!syncError.empty()) {
            implementation.error = std::move(syncError);
            return;
        }
        if (syncResult.topologyRebuilt || syncResult.transformsUpdated ||
            syncResult.lightDistributionUpdated || syncResult.materialDistributionUpdated) {
            implementation.bindingSetCache.clear();
        }
        implementation.historyResetRequested |= syncResult.historyResetRequired;
        if (!implementation.EnsureTextureTable()) return;
        if (implementation.historyResetRequested) {
            implementation.historyValid = false;
            implementation.surfaceHistory = -1;
            std::fill(implementation.reservoirHistoryIndices.begin(),
                implementation.reservoirHistoryIndices.end(), -1);
            implementation.historyResetRequested = false;
        }

        const int32_t currentSurface = implementation.surfaceHistory >= 0
            ? 1 - implementation.surfaceHistory : 0;
        std::vector<int32_t> freeReservoirs{};
        freeReservoirs.reserve(implementation.reservoirSamples.size());
        for (int32_t index = 0;
            index < static_cast<int32_t>(implementation.reservoirSamples.size()); ++index) {
            if (std::ranges::find(implementation.reservoirHistoryIndices, index) ==
                implementation.reservoirHistoryIndices.end()) {
                freeReservoirs.push_back(index);
            }
        }
        const uint32_t groupCountX = (implementation.width + 7u) / 8u;
        const uint32_t groupCountY = (implementation.height + 7u) / 8u;

        auto commandList = implementation.device->CreateCommandList(CommandListParameters{}
            .SetQueueType(CommandQueueType::Graphics)
            .SetDebugName("ReSTIR DI Frame"));
        commandList->Open();
        auto writeConstants = [&](uint32_t spatialPass, uint32_t sampleIndex) {
            const auto constants = implementation.BuildFrameConstants(
                renderer, m_Settings, m_RenderedFrameCount, spatialPass, sampleIndex);
            commandList->WriteBuffer(implementation.frameConstants, &constants, sizeof(constants));
        };
        writeConstants(0, 0);
        implementation.scene.RecordBuildAndUpload(commandList);
        if (implementation.environmentDirty) {
            commandList->WriteBuffer(implementation.environmentPixels,
                implementation.environment.pixels.data(),
                implementation.environment.pixels.size() * sizeof(GpuFloat4));
            commandList->WriteBuffer(implementation.environmentAlias,
                implementation.environment.aliasTable.entries.data(),
                implementation.environment.aliasTable.entries.size() * sizeof(GpuAliasEntry));
            implementation.environmentDirty = false;
        }

        auto primarySet = implementation.CreateBindingSet(FrameBindings{
            .surfaceOutput = implementation.surfaces[currentSurface]});
        commandList->SetRayTracingState(RT::State{}
            .SetShaderTable(implementation.primaryTable)
            .AddBindingSet(primarySet)
            .AddBindingSet(implementation.textureSet));
        commandList->DispatchRays({implementation.width, implementation.height, 1});

        int32_t finalReservoir = 0;
        if (m_Settings.renderMode != RenderMode::Reference) {
            for (uint32_t sampleIndex = 0;
                sampleIndex < implementation.reservoirLaneCount; ++sampleIndex) {
                assert(freeReservoirs.size() >= 2u);
                const int32_t workA = freeReservoirs.back();
                freeReservoirs.pop_back();
                const int32_t workB = freeReservoirs.back();
                freeReservoirs.pop_back();
                const int32_t oldHistory = implementation.reservoirHistoryIndices[sampleIndex];

                writeConstants(0, sampleIndex);
                auto initialSet = implementation.CreateBindingSet(FrameBindings{
                    .surfaceCurrent = implementation.surfaces[currentSurface],
                    .reservoirSampleOutput = implementation.reservoirSamples[workA],
                    .reservoirStatsOutput = implementation.reservoirStats[workA],
                    .acceptanceOutput = implementation.acceptance[0]});
                commandList->SetComputeState(ComputeState{}
                    .SetPipeline(implementation.initialPipeline)
                    .AddBindingSet(initialSet)
                    .AddBindingSet(implementation.textureSet));
                commandList->Dispatch(groupCountX, groupCountY, 1);

                int32_t currentReservoir = workA;
                uint32_t currentAcceptance = 0;
                if (m_Settings.renderMode == RenderMode::Restir &&
                    m_Settings.enableTemporalReuse && implementation.historyValid &&
                    implementation.surfaceHistory >= 0 && oldHistory >= 0) {
                    auto temporalSet = implementation.CreateBindingSet(FrameBindings{
                        .surfaceCurrent = implementation.surfaces[currentSurface],
                        .surfacePrevious = implementation.surfaces[implementation.surfaceHistory],
                        .reservoirCurrentSample = implementation.reservoirSamples[currentReservoir],
                        .reservoirCurrentStats = implementation.reservoirStats[currentReservoir],
                        .reservoirHistorySample = implementation.reservoirSamples[oldHistory],
                        .reservoirHistoryStats = implementation.reservoirStats[oldHistory],
                        .acceptanceInput = implementation.acceptance[currentAcceptance],
                        .reservoirSampleOutput = implementation.reservoirSamples[workB],
                        .reservoirStatsOutput = implementation.reservoirStats[workB],
                        .acceptanceOutput = implementation.acceptance[1]});
                    commandList->SetComputeState(ComputeState{}
                        .SetPipeline(implementation.temporalPipeline)
                        .AddBindingSet(temporalSet)
                        .AddBindingSet(implementation.textureSet));
                    commandList->Dispatch(groupCountX, groupCountY, 1);
                    currentReservoir = workB;
                    currentAcceptance = 1;
                }

                if (m_Settings.renderMode == RenderMode::Restir && m_Settings.enableSpatialReuse) {
                    const uint32_t passCount = std::min(m_Settings.spatialPassCount, 2u);
                    for (uint32_t pass = 0; pass < passCount; ++pass) {
                        const int32_t outputReservoir = currentReservoir == workA ? workB : workA;
                        const uint32_t outputAcceptance = 1u - currentAcceptance;
                        writeConstants(pass, sampleIndex);
                        auto spatialSet = implementation.CreateBindingSet(FrameBindings{
                            .surfaceCurrent = implementation.surfaces[currentSurface],
                            .reservoirCurrentSample = implementation.reservoirSamples[currentReservoir],
                            .reservoirCurrentStats = implementation.reservoirStats[currentReservoir],
                            .acceptanceInput = implementation.acceptance[currentAcceptance],
                            .reservoirSampleOutput = implementation.reservoirSamples[outputReservoir],
                            .reservoirStatsOutput = implementation.reservoirStats[outputReservoir],
                            .acceptanceOutput = implementation.acceptance[outputAcceptance]});
                        commandList->SetComputeState(ComputeState{}
                            .SetPipeline(implementation.spatialPipeline)
                            .AddBindingSet(spatialSet)
                            .AddBindingSet(implementation.textureSet));
                        commandList->Dispatch(groupCountX, groupCountY, 1);
                        currentReservoir = outputReservoir;
                        currentAcceptance = outputAcceptance;
                    }
                }

                writeConstants(0, sampleIndex);
                auto visibilitySet = implementation.CreateBindingSet(FrameBindings{
                    .surfaceCurrent = implementation.surfaces[currentSurface],
                    .reservoirCurrentSample = implementation.reservoirSamples[currentReservoir],
                    .reservoirCurrentStats = implementation.reservoirStats[currentReservoir],
                    .acceptanceInput = implementation.acceptance[currentAcceptance],
                    .acceptanceOutput = implementation.acceptanceAggregate,
                    .hdrOutput = implementation.hdr});
                commandList->SetBufferState(implementation.hdr, ResourceStates::UnorderedAccess);
                commandList->SetBufferState(
                    implementation.acceptanceAggregate, ResourceStates::UnorderedAccess);
                commandList->SetRayTracingState(RT::State{}
                    .SetShaderTable(implementation.visibilityTable)
                    .AddBindingSet(visibilitySet)
                    .AddBindingSet(implementation.textureSet));
                commandList->DispatchRays({implementation.width, implementation.height, 1});

                if (sampleIndex == 0u) finalReservoir = currentReservoir;
                implementation.reservoirHistoryIndices[sampleIndex] = currentReservoir;
                freeReservoirs.push_back(currentReservoir == workA ? workB : workA);
                if (oldHistory >= 0) freeReservoirs.push_back(oldHistory);
            }
        }
        else {
            writeConstants(0, 0);
            auto referenceSet = implementation.CreateBindingSet(FrameBindings{
                .surfaceCurrent = implementation.surfaces[currentSurface],
                .acceptanceOutput = implementation.acceptanceAggregate,
                .hdrOutput = implementation.hdr});
            commandList->SetRayTracingState(RT::State{}
                .SetShaderTable(implementation.referenceTable)
                .AddBindingSet(referenceSet)
                .AddBindingSet(implementation.textureSet));
            commandList->DispatchRays({implementation.width, implementation.height, 1});
            finalReservoir = !implementation.reservoirHistoryIndices.empty() &&
                implementation.reservoirHistoryIndices[0] >= 0
                ? implementation.reservoirHistoryIndices[0] : 0;
        }

        auto presentSet = implementation.CreateBindingSet(FrameBindings{.hdrInput = implementation.hdr});
        commandList->SetGraphicsState(GraphicsState{}
            .SetPipeline(implementation.presentPipeline)
            .SetFramebuffer(implementation.framebuffer)
            .SetViewport(ViewportState{}.AddViewportAndScissorRect(renderer.GetCamera().GetViewPort()))
            .AddBindingSet(presentSet, 0)
            .AddBindingSet(implementation.textureSet, 1));
        commandList->Draw(DrawArguments{}.SetVertexCount(3));
        commandList->SetTextureState(renderer.GetColorTexture(), AllSubresources, ResourceStates::ShaderResource);
        commandList->Close();
        implementation.device->ExecuteCommandList(commandList);

        implementation.lastSurface = currentSurface;
        implementation.lastReservoir = finalReservoir;
        implementation.surfaceHistory = currentSurface;
        implementation.historyValid = m_Settings.renderMode != RenderMode::Reference;
        implementation.previousViewProjection = renderer.GetCamera().GetViewProjMatrix();
        implementation.error.clear();
        ++m_RenderedFrameCount;
    }

    void RenderPipeline::RenderUI(GraphicsRenderer& renderer)
    {
        if (!m_EnableUI || ImGui::GetCurrentContext() == nullptr) return;
        ++m_UIFrameCount;
        auto& implementation = *m_Implementation;
        if (!ImGui::Begin("ReSTIR DI")) {
            ImGui::End();
            return;
        }

        ImGui::Text("DXR: %s", implementation.dxrAvailable ? "Available" : "Unavailable");
        ImGui::Text("Rendered frames: %llu", static_cast<unsigned long long>(m_RenderedFrameCount));
        if (!implementation.error.empty()) {
            ImGui::TextWrapped("Error: %s", implementation.error.c_str());
        }

        static constexpr const char* renderModes[] = {"ReSTIR", "Independent RIS", "Reference"};
        int renderMode = static_cast<int>(m_Settings.renderMode);
        if (ImGui::Combo("Render Mode", &renderMode, renderModes, std::size(renderModes))) {
            m_Settings.renderMode = static_cast<RenderMode>(renderMode);
        }
        static constexpr const char* debugViews[] = {
            "Final", "Surface", "Normal", "Albedo", "Source Type", "Source ID",
            "pHat", "Reservoir M", "Reservoir W", "Temporal Accept", "Spatial Accept", "Visibility"};
        int debugView = static_cast<int>(m_Settings.debugView);
        if (ImGui::Combo("Debug View", &debugView, debugViews, std::size(debugViews))) {
            m_Settings.debugView = static_cast<DebugView>(debugView);
        }

        if (ImGui::CollapsingHeader("Sampling", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (m_Settings.renderMode == RenderMode::Reference) {
                int referenceSPP = static_cast<int>(m_Settings.referenceSamplesPerPixel);
                if (ImGui::SliderInt("Reference SPP", &referenceSPP, 1, 512)) {
                    m_Settings.referenceSamplesPerPixel = static_cast<uint32_t>(referenceSPP);
                }
                ImGui::TextWrapped("Each Reference SPP traces one visibility ray.");
            }
            else {
                int samplesPerPixel = static_cast<int>(m_Settings.samplesPerPixel);
                if (ImGui::SliderInt("Samples per pixel", &samplesPerPixel, 1, 8)) {
                    m_Settings.samplesPerPixel = static_cast<uint32_t>(samplesPerPixel);
                }
                ImGui::TextWrapped(
                    "Each SPP owns an independent Reservoir history and runs the full reuse chain "
                    "plus one visibility ray.");
            }
        }

        if (ImGui::CollapsingHeader("Candidate domains", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Analytic lights", &m_Settings.enableAnalyticLights);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::DragFloat("Analytic weight", &m_Settings.analyticDomainWeight, 0.05f, 0.0f, 16.0f);
            ImGui::Checkbox("Emissive triangles", &m_Settings.enableEmissiveTriangles);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::DragFloat("Emissive weight", &m_Settings.emissiveDomainWeight, 0.05f, 0.0f, 16.0f);
            ImGui::Checkbox("Environment", &m_Settings.enableEnvironment);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::DragFloat("Environment weight", &m_Settings.environmentDomainWeight, 0.05f, 0.0f, 16.0f);
            ImGui::SliderInt("Initial candidates", reinterpret_cast<int*>(&m_Settings.initialCandidateCount), 1, 128);
        }

        if (ImGui::CollapsingHeader("Temporal and spatial reuse", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Temporal reuse", &m_Settings.enableTemporalReuse);
            ImGui::SliderInt("History M cap multiplier",
                reinterpret_cast<int*>(&m_Settings.historyMCapMultiplier), 1, 64);
            ImGui::Checkbox("Spatial reuse", &m_Settings.enableSpatialReuse);
            ImGui::SliderInt("Spatial passes", reinterpret_cast<int*>(&m_Settings.spatialPassCount), 0, 2);
            ImGui::SliderInt("Neighbors per pass", reinterpret_cast<int*>(&m_Settings.spatialNeighborCount), 1, 16);
            ImGui::SliderFloat("Radius (px)", &m_Settings.spatialRadius, 1.0f, 100.0f);
            ImGui::SliderFloat("Normal threshold (deg)", &m_Settings.normalThresholdDegrees, 0.0f, 90.0f);
            ImGui::SliderFloat("Relative depth threshold", &m_Settings.relativeDepthThreshold, 0.001f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Camera control", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable camera control", &m_Settings.enableCameraControl);
            ImGui::BeginDisabled(!m_Settings.enableCameraControl);
            ImGui::SliderFloat("Camera move speed", &m_Settings.cameraMoveSpeed, 0.1f, 50.0f);
            ImGui::SliderFloat("Mouse sensitivity", &m_Settings.cameraMouseSensitivity,
                0.0005f, 0.02f, "%.4f");
            ImGui::TextWrapped("Hold RMB and drag to look; use W/A/S/D to move.");
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Ray tracing and output", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Alpha cutoff", &m_Settings.alphaCutoff, 0.0f, 1.0f);
            ImGui::DragFloat("Normal bias", &m_Settings.normalBias, 0.0001f, 0.00001f, 0.1f, "%.5f");
            ImGui::DragFloat("Max ray distance", &m_Settings.maxRayDistance, 1.0f, 1.0f, 100000.0f);
            ImGui::SliderFloat("Environment intensity", &m_Settings.environmentIntensity, 0.0f, 16.0f);
            ImGui::SliderFloat("Environment rotation", &m_Settings.environmentRotationDegrees, -180.0f, 180.0f);
            ImGui::SliderFloat("Exposure (EV)", &m_Settings.exposure, -10.0f, 10.0f);
            ImGui::Checkbox("Freeze random seed", &m_Settings.freezeRandomSeed);
            ImGui::InputScalar("Random seed", ImGuiDataType_U32, &m_Settings.randomSeed);
        }

        if (ImGui::CollapsingHeader("Environment loading")) {
            ImGui::InputText("Radiance HDR", implementation.environmentPath.data(),
                implementation.environmentPath.size());
            if (ImGui::Button("Load HDR")) LoadEnvironmentHDR(implementation.environmentPath.data());
            ImGui::SameLine();
            if (ImGui::Button("Use daylight0..5")) LoadDefaultEnvironment();
            if (!implementation.environmentStatus.empty()) {
                ImGui::TextWrapped("%s", implementation.environmentStatus.c_str());
            }
        }

        if (ImGui::Button("Reset history")) ResetHistory();
        ImGui::SameLine();
        if (ImGui::Button("Generate validation scene")) {
            ImGui::OpenPopup("Confirm validation scene");
        }
        if (ImGui::BeginPopupModal("Confirm validation scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("This replaces the current in-memory scene and does not save it automatically.");
            if (ImGui::Button("Generate")) {
                std::string sceneError{};
                auto validation = CreateValidationScene(renderer.GetDevice(), sceneError);
                if (validation.scene != nullptr) {
                    DSMEngine::sm_GlobalContext.scene = std::move(validation.scene);
                    renderer.GetCamera().SetPosition({0.0f, 1.8f, -8.5f});
                    renderer.GetCamera().LookAt({0.0f, 1.2f, 2.8f}, {0.0f, 1.0f, 0.0f});
                    ResetHistory();
                }
                else {
                    implementation.error = std::move(sceneError);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    void RenderPipeline::OnResizeFrameBuffer(GraphicsRenderer&, uint32_t, uint32_t)
    {
    }

    void RenderPipeline::OnResizeRenderTexture(
        GraphicsRenderer& renderer, uint32_t newWidth, uint32_t newHeight)
    {
        if (m_Implementation->device != nullptr) {
            m_Implementation->CreateResolutionResources(
                renderer, newWidth, newHeight, m_Settings.samplesPerPixel);
        }
    }

    bool RenderPipeline::IsInitialized() const noexcept
    {
        return m_Implementation->initialized;
    }

    bool RenderPipeline::IsDXRAvailable() const noexcept
    {
        return m_Implementation->dxrAvailable;
    }

    const std::string& RenderPipeline::GetLastError() const noexcept
    {
        return m_Implementation->error;
    }

    void RenderPipeline::ResetHistory() noexcept
    {
        m_Implementation->historyResetRequested = true;
    }

    bool RenderPipeline::LoadEnvironmentHDR(const std::filesystem::path& filename)
    {
        EnvironmentData candidate{};
        std::string loadError{};
        if (!LoadRadianceEnvironment(filename, candidate, loadError)) {
            m_Implementation->environmentStatus = std::move(loadError);
            return false;
        }
        m_Implementation->environment = std::move(candidate);
        if (m_Implementation->device != nullptr) m_Implementation->EnsureEnvironmentBuffers();
        m_Implementation->environmentStatus = "已加载 " + filename.string();
        m_Implementation->historyResetRequested = true;
        return true;
    }

    bool RenderPipeline::LoadDefaultEnvironment()
    {
        EnvironmentData candidate{};
        std::string loadError{};
        if (m_Implementation->assetsDirectory.empty() ||
            !LoadDaylightEnvironment(m_Implementation->assetsDirectory, candidate, loadError)) {
            m_Implementation->environmentStatus = std::move(loadError);
            return false;
        }
        m_Implementation->environment = std::move(candidate);
        if (m_Implementation->device != nullptr) m_Implementation->EnsureEnvironmentBuffers();
        m_Implementation->environmentStatus = "已恢复 daylight0..5.png";
        m_Implementation->historyResetRequested = true;
        return true;
    }

    bool RenderPipeline::Implementation::Capture(
        GraphicsRenderer&, ValidationSnapshot& snapshot, std::string& captureError)
    {
        if (!initialized || lastSurface < 0 || lastReservoir < 0) {
            captureError = "ReSTIR DI 尚未产生可读回的帧。";
            return false;
        }
        const size_t pixelCount = size_t(width) * height;
        auto createReadback = [this](size_t byteSize, const char* name) {
            return device->CreateBuffer(BufferDesc{}
                .SetByteSize(byteSize)
                .SetInitialState(ResourceStates::CopyDest)
                .SetCpuAccess(CpuAccessMode::Read)
                .SetDebugName(name));
        };
        const auto hdrReadback = createReadback(pixelCount * sizeof(GpuFloat4), "ReSTIR DI HDR Readback");
        const auto surfaceReadback = createReadback(pixelCount * sizeof(GpuSurface), "ReSTIR DI Surface Readback");
        const auto sampleReadback = createReadback(pixelCount * sizeof(GpuReservoirSample), "ReSTIR DI Sample Readback");
        const auto statsReadback = createReadback(pixelCount * sizeof(GpuReservoirStats), "ReSTIR DI Stats Readback");
        const auto acceptanceReadback = createReadback(pixelCount * sizeof(GpuAcceptance), "ReSTIR DI Acceptance Readback");
        if (!hdrReadback || !surfaceReadback || !sampleReadback || !statsReadback || !acceptanceReadback) {
            captureError = "创建 ReSTIR DI Readback Buffer 失败。";
            return false;
        }

        auto commandList = device->CreateCommandList(CommandListParameters{}
            .SetQueueType(CommandQueueType::Graphics)
            .SetDebugName("ReSTIR DI Validation Readback"));
        commandList->Open();
        commandList->CopyBuffer(hdrReadback, 0, hdr, 0, pixelCount * sizeof(GpuFloat4));
        commandList->CopyBuffer(surfaceReadback, 0, surfaces[lastSurface], 0, pixelCount * sizeof(GpuSurface));
        commandList->CopyBuffer(sampleReadback, 0, reservoirSamples[lastReservoir], 0,
            pixelCount * sizeof(GpuReservoirSample));
        commandList->CopyBuffer(statsReadback, 0, reservoirStats[lastReservoir], 0,
            pixelCount * sizeof(GpuReservoirStats));
        commandList->CopyBuffer(acceptanceReadback, 0, acceptanceAggregate, 0,
            pixelCount * sizeof(GpuAcceptance));
        commandList->Close();
        device->ExecuteCommandList(commandList);
        if (!device->WaitForIdle()) {
            captureError = "等待 ReSTIR DI 验证读回失败。";
            return false;
        }

        snapshot.width = width;
        snapshot.height = height;
        snapshot.hdr.resize(pixelCount);
        snapshot.surfaces.resize(pixelCount);
        snapshot.reservoirSamples.resize(pixelCount);
        snapshot.reservoirStats.resize(pixelCount);
        snapshot.acceptance.resize(pixelCount);
        auto read = [this](IBuffer* buffer, void* destination, size_t byteSize) {
            const void* source = device->MapBuffer(buffer, CpuAccessMode::Read);
            if (source == nullptr) return false;
            std::memcpy(destination, source, byteSize);
            device->UnmapBuffer(buffer);
            return true;
        };
        const bool readSucceeded =
            read(hdrReadback, snapshot.hdr.data(), pixelCount * sizeof(GpuFloat4)) &&
            read(surfaceReadback, snapshot.surfaces.data(), pixelCount * sizeof(GpuSurface)) &&
            read(sampleReadback, snapshot.reservoirSamples.data(), pixelCount * sizeof(GpuReservoirSample)) &&
            read(statsReadback, snapshot.reservoirStats.data(), pixelCount * sizeof(GpuReservoirStats)) &&
            read(acceptanceReadback, snapshot.acceptance.data(), pixelCount * sizeof(GpuAcceptance));
        if (!readSucceeded) {
            captureError = "映射 ReSTIR DI 验证读回 Buffer 失败。";
            return false;
        }
        captureError.clear();
        return true;
    }

    bool RenderPipeline::CaptureValidation(
        GraphicsRenderer& renderer, ValidationSnapshot& snapshot, std::string& error)
    {
        return m_Implementation->Capture(renderer, snapshot, error);
    }

}
