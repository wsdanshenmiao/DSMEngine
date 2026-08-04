// DirectX Raytracing（DXR）端到端示例
// 演示 DSMEngine 的 RT 抽象层：构建 BLAS/TLAS、烘焙 ShaderTable、DispatchRays。
// 采用独立控制台程序，直接 CreateDevice，不依赖编辑器框架。
#include "Runtime/Graphics/D3D12.h"
#include "Runtime/Graphics/RayTracing.h"
#include "Runtime/Graphics/ResourceBindings.h"
#include "Runtime/Graphics/Buffer.h"
#include "Runtime/Graphics/Heap.h"
#include "Runtime/Graphics/Shader.h"
#include "Runtime/Render/ShaderCompiler.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace DSM;

// 错误回调：记录是否有 Error/Fatal 级别消息，用于功能验证
static bool g_HadError = false;

static bool WriteBmp(const std::filesystem::path& filename, const float* pixels, uint32_t width, uint32_t height)
{
    if (pixels == nullptr || width == 0 || height == 0) {
        return false;
    }

    const uint32_t rowSize = width * 3;
    const uint32_t rowPitch = (rowSize + 3u) & ~3u;
    const uint32_t imageSize = rowPitch * height;
    std::array<uint8_t, 54> header{};

    const auto writeU16 = [&header](size_t offset, uint16_t value) {
        header[offset] = static_cast<uint8_t>(value);
        header[offset + 1] = static_cast<uint8_t>(value >> 8);
    };
    const auto writeU32 = [&header](size_t offset, uint32_t value) {
        header[offset] = static_cast<uint8_t>(value);
        header[offset + 1] = static_cast<uint8_t>(value >> 8);
        header[offset + 2] = static_cast<uint8_t>(value >> 16);
        header[offset + 3] = static_cast<uint8_t>(value >> 24);
    };

    header[0] = 'B';
    header[1] = 'M';
    writeU32(2, static_cast<uint32_t>(header.size()) + imageSize);
    writeU32(10, static_cast<uint32_t>(header.size()));
    writeU32(14, 40);
    writeU32(18, width);
    writeU32(22, height);
    writeU16(26, 1);
    writeU16(28, 24);
    writeU32(34, imageSize);

    std::ofstream output(filename, std::ios::binary);
    if (!output) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    std::vector<uint8_t> row(rowPitch);
    for (uint32_t y = 0; y < height; ++y) {
        const float* sourceRow = pixels + size_t(height - 1 - y) * width * 4;
        for (uint32_t x = 0; x < width; ++x) {
            const float* color = sourceRow + x * 4;
            const size_t offset = size_t(x) * 3;
            row[offset] = static_cast<uint8_t>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f + 0.5f);
            row[offset + 1] = static_cast<uint8_t>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f + 0.5f);
            row[offset + 2] = static_cast<uint8_t>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        output.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    return static_cast<bool>(output);
}

class SampleMessageCallback : public IMessageCallback
{
public:
    void Message(MessageSeverity severity, const char* messageText) const override
    {
        if (severity == MessageSeverity::Error || severity == MessageSeverity::Fatal) {
            g_HadError = true;
            std::cerr << "[ERROR] " << messageText << std::endl;
        }
        else if (severity == MessageSeverity::Warning) {
            std::cerr << "[WARN] " << messageText << std::endl;
        }
    }
};

// 内嵌的 HLSL 着色器库源码（lib_6_5），运行时写出为文件后编译
static const char* k_HlslSource = R"HLSL(
struct RayPayload
{
    float3 color;
};

RaytracingAccelerationStructure g_AccelStruct : register(t0);
RWStructuredBuffer<float4> g_Output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dims = DispatchRaysDimensions().xy;

    float2 uv = (float2(pixel) + 0.5f) / float2(dims);
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    // 简单透视相机：位于原点后方，朝 +Z 观察
    float3 origin = float3(0.0f, 0.0f, -2.0f);
    float3 dir = normalize(float3(ndc, 1.0f));

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001f;
    ray.TMax = 1000.0f;

    RayPayload payload = { float3(0.0f, 0.0f, 0.0f) };
    TraceRay(g_AccelStruct, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    g_Output[pixel.y * dims.x + pixel.x] = float4(payload.color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    // 背景色（未命中三角形）
    payload.color = float3(0.1f, 0.3f, 0.6f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // 用重心坐标作为命中着色，便于肉眼区分命中区域
    float3 bary = float3(
        1.0f - attr.barycentrics.x - attr.barycentrics.y,
        attr.barycentrics.x,
        attr.barycentrics.y);
    payload.color = bary;
}
)HLSL";

int main()
{
    SampleMessageCallback callback;
    D3D12::DeviceDesc deviceDesc{};
    deviceDesc.errorCB = &callback;
    deviceDesc.enableDebugLayer = false;

    D3D12::DeviceHandle device = D3D12::CreateDevice(deviceDesc);
    if (!device) {
        std::cerr << "创建 D3D12 设备失败。\n";
        return 1;
    }

    // 特性门禁：不支持 DXR 时优雅退出
    if (!device->QueryFeatureSupport(Feature::RayTracingPipeline)) {
        std::cout << "当前设备不支持 DXR（RayTracingPipeline），示例跳过。\n";
        return 0;
    }
    std::cout << "DXR 支持已确认，开始构建光线追踪示例...\n";

    const uint32_t width = 512;
    const uint32_t height = 512;

    // 将内嵌 HLSL 写出为文件后编译为着色器库
    const char* hlslPath = "RayTracingGen.hlsl";
    {
        std::ofstream ofs(hlslPath);
        ofs << k_HlslSource;
    }
    ShaderByteCode shaderLibByteCode(ShaderCompileDesc{}
        .SetType(ShaderType::Library)
        .SetMode(ShaderMode::SM_6_5)
        .SetFilename(hlslPath));
    if (!shaderLibByteCode.IsValid()) {
        std::cerr << "着色器库编译失败。\n";
        return 1;
    }
    ShaderLibraryHandle shaderLib = device->CreateShaderLibrary(
        shaderLibByteCode.GetByteCode(), shaderLibByteCode.GetByteCodeSize());
    if (!shaderLib) {
        std::cerr << "创建着色器库失败。\n";
        return 1;
    }
    ShaderHandle rayGenerationShader = shaderLib->GetShader("RayGen", ShaderType::RayGeneration);
    ShaderHandle missShader = shaderLib->GetShader("Miss", ShaderType::Miss);
    ShaderHandle closestHitShader = shaderLib->GetShader("ClosestHit", ShaderType::ClosestHit);

    // 三角形顶点（位于 z=0 平面）
    struct Vertex { float x, y, z; };
    Vertex vertices[3] = {
        { 0.0f,  0.5f, 0.0f },
        { 0.5f, -0.5f, 0.0f },
        {-0.5f, -0.5f, 0.0f },
    };

    BufferDesc vtxDesc{};
    vtxDesc.byteSize = sizeof(vertices);
    vtxDesc.initialState = ResourceStates::Common;
    vtxDesc.debugName = "TriangleVertices";
    RefPtr<IBuffer> vtxBuffer = device->CreateBuffer(vtxDesc);
    if (!vtxBuffer) {
        std::cerr << "创建顶点缓冲失败。\n";
        return 1;
    }

    // 输出缓冲（RWStructuredBuffer<float4>）
    BufferDesc outDesc{};
    outDesc.byteSize = uint64_t(width) * height * sizeof(float) * 4;
    outDesc.structStride = sizeof(float) * 4;
    outDesc.canHaveUAVs = true;
    outDesc.initialState = ResourceStates::Common;
    outDesc.debugName = "RayTracingOutput";
    RefPtr<IBuffer> outputBuffer = device->CreateBuffer(outDesc);
    if (!outputBuffer) {
        std::cerr << "创建输出缓冲失败。\n";
        return 1;
    }

    // Readback Buffer：接收 GPU 输出，供 CPU 导出为 BMP 图片。
    BufferDesc readbackDesc{};
    readbackDesc.byteSize = outDesc.byteSize;
    readbackDesc.initialState = ResourceStates::CopyDest;
    readbackDesc.cpuAccess = CpuAccessMode::Read;
    readbackDesc.debugName = "RayTracingReadback";
    BufferHandle readbackBuffer = device->CreateBuffer(readbackDesc);
    if (!readbackBuffer) {
        std::cerr << "创建光追结果读回缓冲失败。\n";
        return 1;
    }

    // 全局根签名布局：t0 = TLAS（SRV），u0 = 输出缓冲（UAV）
    BindingLayoutDesc layoutDesc{};
    layoutDesc.visibility = ShaderType::RayGeneration;
    layoutDesc.AddItem(BindingLayoutItem::RayTracingAccelStruct(0));
    layoutDesc.AddItem(BindingLayoutItem::StructuredBuffer_UAV(0));
    BindingLayoutHandle layout = device->CreateBindingLayout(layoutDesc);
    if (!layout) {
        std::cerr << "创建绑定布局失败。\n";
        return 1;
    }

    // 1) 构建 BLAS
    RT::AccelStructDesc blasDesc{};
    RT::GeometryTriangles triangles{};
    triangles.SetVertexBuffer(vtxBuffer.Get())
        .SetVertexStride(sizeof(Vertex))
        .SetVertexCount(3)
        .SetVertexFormat(Format::RGB32_FLOAT);
    RT::GeometryDesc geometry{};
    geometry.SetTriangles(triangles).SetFlags(RT::GeometryFlags::Opaque);
    blasDesc.SetIsTopLevel(false)
        .SetIsVirtual(true)
        .AddBottomLevelGeometry(geometry)
        .SetDebugName("TriangleBLAS");

    RT::AccelStructHandle blas = device->CreateAccelStruct(blasDesc);
    if (!blas) {
        std::cerr << "创建 BLAS 失败。\n";
        return 1;
    }
    {
        MemoryRequirements memReq = device->GetAccelStructMemoryRequirements(blas.Get());
        HeapDesc heapDesc{};
        heapDesc.capacity = memReq.size;
        heapDesc.type = HeapType::Default;
        heapDesc.debugName = "BLASHeap";
        HeapHandle heap = device->CreateHeap(heapDesc);
        if (!heap || !device->BindAccelStructMemory(blas.Get(), heap.Get(), 0)) {
            std::cerr << "绑定 BLAS 显存失败。\n";
            return 1;
        }
    }

    // 2) 构建 TLAS（单实例引用 BLAS）
    RT::AccelStructDesc tlasDesc{};
    tlasDesc.SetIsTopLevel(true)
        .SetTopLevelMaxInstances(1)
        .SetIsVirtual(true)
        .SetDebugName("SceneTLAS");

    RT::AccelStructHandle tlas = device->CreateAccelStruct(tlasDesc);
    if (!tlas) {
        std::cerr << "创建 TLAS 失败。\n";
        return 1;
    }
    {
        MemoryRequirements memReq = device->GetAccelStructMemoryRequirements(tlas.Get());
        HeapDesc heapDesc{};
        heapDesc.capacity = memReq.size;
        heapDesc.type = HeapType::Default;
        heapDesc.debugName = "TLASHeap";
        HeapHandle heap = device->CreateHeap(heapDesc);
        if (!heap || !device->BindAccelStructMemory(tlas.Get(), heap.Get(), 0)) {
            std::cerr << "绑定 TLAS 显存失败。\n";
            return 1;
        }
    }

    RT::InstanceDesc instance{};
    instance.SetBottomLevelAS(blas.Get())
        .SetInstanceMask(0xFF); // 单位变换（默认 identity）

    // 3) 创建光线追踪管线
    RT::PipelineDesc pipeDesc{};
    pipeDesc.shaders.push_back(RT::PipelineShaderDesc{}
        .SetExportName("RayGen").SetShader(rayGenerationShader.Get()));
    pipeDesc.shaders.push_back(RT::PipelineShaderDesc{}
        .SetExportName("Miss").SetShader(missShader.Get()));
    pipeDesc.shaders.push_back(RT::PipelineShaderDesc{}
        .SetExportName("ClosestHit").SetShader(closestHitShader.Get()));
    pipeDesc.hitGroups.push_back(RT::PipelineHitGroupDesc{}
        .SetExportName("MyHitGroup").SetClosestHitShader("ClosestHit"));
    pipeDesc.maxRecursionDepth = 1;
    pipeDesc.maxPayloadSize = sizeof(float) * 3;   // RayPayload.color
    pipeDesc.maxAttributeSize = sizeof(float) * 2; // barycentrics
    pipeDesc.globalBindingLayout = layout;

    RT::PipelineHandle pipeline = device->CreateRayTracingPipeline(pipeDesc);
    if (!pipeline) {
        std::cerr << "创建光线追踪管线失败。\n";
        return 1;
    }

    // 4) 创建着色器表
    RT::ShaderTableHandle shaderTable = pipeline->CreateShaderTable();
    if (!shaderTable) {
        std::cerr << "创建着色器表失败。\n";
        return 1;
    }
    shaderTable->SetGenerationShader("RayGen");
    shaderTable->AddMissShader("Miss");
    shaderTable->AddHitGroup("MyHitGroup");

    // 5) 创建绑定集合（TLAS + 输出缓冲）
    BindingSetDesc setDesc{};
    setDesc.bindings.push_back(BindingSetItem::RayTracingAccelStruct(0, tlas->GetDataBuffer()));
    setDesc.bindings.push_back(BindingSetItem::StructuredBuffer_UAV(0, outputBuffer.Get()));
    BindingSetHandle bindingSet = device->CreateBindingSet(setDesc, layout.Get());
    if (!bindingSet) {
        std::cerr << "创建绑定集合失败。\n";
        return 1;
    }

    // 6) 录制命令：上传顶点 -> 构建 BLAS/TLAS -> 派发光线
    auto cmdList = device->CreateCommandList(
        CommandListParameters().SetDebugName("DXR").SetQueueType(CommandQueueType::Graphics));
    cmdList->Open();

    cmdList->WriteBuffer(vtxBuffer.Get(), vertices, sizeof(vertices));
    // 顶点缓冲在写入后需处于构建输入状态
    cmdList->SetBufferState(vtxBuffer.Get(), ResourceStates::AccelStructBuildInput);
    cmdList->CommitBarriers();

    cmdList->BuildBottomLevelAccelStruct(blas.Get(), blasDesc.buildFlags);
    const std::array instances{ instance };
    cmdList->BuildTopLevelAccelStruct(tlas.Get(), instances, tlasDesc.buildFlags);

    RT::State rtState{};
    rtState.shaderTable = shaderTable.Get();
    rtState.bindingSets.push_back(bindingSet.Get());
    cmdList->SetRayTracingState(rtState);
    cmdList->DispatchRays({ width, height, 1 });
    cmdList->CopyBuffer(readbackBuffer.Get(), 0, outputBuffer.Get(), 0, outDesc.byteSize);

    cmdList->Close();
    device->ExecuteCommandList(cmdList.Get());

    bool idleOk = device->WaitForIdle();
    if (!idleOk) {
        std::cerr << "GPU 等待失败（可能设备移除）。\n";
        return 1;
    }

    if (g_HadError) {
        std::cerr << "DXR 示例执行过程中出现错误。\n";
        return 1;
    }

    const std::filesystem::path imagePath = std::filesystem::absolute("RayTracingOutput.bmp");
    const float* pixels = static_cast<const float*>(device->MapBuffer(readbackBuffer.Get(), CpuAccessMode::Read));
    if (pixels == nullptr) {
        std::cerr << "映射光追结果读回缓冲失败。\n";
        return 1;
    }

    const bool imageSaved = WriteBmp(imagePath, pixels, width, height);
    device->UnmapBuffer(readbackBuffer.Get());
    if (!imageSaved) {
        std::cerr << "保存光追结果图片失败：" << imagePath.string() << '\n';
        return 1;
    }

    std::cout << "DXR 示例执行成功：已导出光追图片 " << imagePath.string() << '\n';
    return 0;
}
