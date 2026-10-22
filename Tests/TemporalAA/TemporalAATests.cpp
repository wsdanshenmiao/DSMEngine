#include "Runtime/DSMEngine.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"
#include "Shaders/Common/ResourceData.h"
#include "Runtime/Render/Renderer/CommonPass/TaaPass.h"
#include "Runtime/Render/Renderer/CommonPass/MotionVectorPass.h"
// 同时编译延迟管线，防止共享投影接口只在前向入口中可见。
#include "Runtime/Render/Renderer/DeferredRenderer/DeferredRenderPipeline.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace DSM {
namespace {
    using Microsoft::WRL::ComPtr;

    struct Pixel { float r, g, b, a; };
    struct Velocity { float x, y; };

    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    class Messages final : public IMessageCallback
    {
    public:
        void Message(MessageSeverity severity, const char* text) const override
        {
            if (severity == MessageSeverity::Warning || severity == MessageSeverity::Error ||
                severity == MessageSeverity::Fatal) {
                failed = true;
                std::fprintf(stderr, "GPU: %s\n", text);
            }
        }
        mutable bool failed = false;
    };

    bool CheckDebugMessages(IDevice* device)
    {
        ID3D12Device* nativeDevice = device->GetNativeObject(ObjectTypes::D3D12_Device);
        ComPtr<ID3D12InfoQueue> queue;
        Require(SUCCEEDED(nativeDevice->QueryInterface(IID_PPV_ARGS(&queue))), "D3D12 调试消息队列不可用");
        bool passed = true;
        for (UINT64 index = 0; index < queue->GetNumStoredMessages(); ++index) {
            SIZE_T bytes = 0;
            queue->GetMessage(index, nullptr, &bytes);
            std::vector<std::byte> storage(bytes);
            auto message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            Require(SUCCEEDED(queue->GetMessage(index, message, &bytes)), "读取 D3D12 调试消息失败");
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                std::fprintf(stderr, "D3D12 [%u]: %s\n", unsigned(message->ID), message->pDescription);
                passed = false;
            }
        }
        std::printf("d3d12-debug-layer: %s\n", passed ? "PASS" : "FAIL");
        return passed;
    }

    std::vector<Pixel> ReadTexture(IDevice* device, ITexture* texture)
    {
        ID3D12Device* nativeDevice = device->GetNativeObject(ObjectTypes::D3D12_Device);
        ID3D12Resource* resource = texture->GetNativeObject(ObjectTypes::D3D12_Resource);
        const auto desc = resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 bytes = 0;
        nativeDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = bytes;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> readback;
        Require(SUCCEEDED(nativeDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
            &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback))), "创建读回失败");

        auto command = device->CreateCommandList();
        command->Open();
        command->SetTextureState(texture, AllSubresources, ResourceStates::CopySource);
        command->CommitBarriers();
        ID3D12GraphicsCommandList* nativeList = command->GetNativeObject(ObjectTypes::D3D12_GraphicsCommandList);
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = resource;
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        nativeList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        command->Close();
        device->ExecuteCommandList(command);
        device->WaitForIdle();

        void* mapped = nullptr;
        const D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
        Require(SUCCEEDED(readback->Map(0, &range, &mapped)), "映射读回失败");
        std::vector<Pixel> pixels(desc.Width * desc.Height);
        for (uint32_t y = 0; y < desc.Height; ++y) {
            const auto row = static_cast<const std::byte*>(mapped) + footprint.Offset + y * footprint.Footprint.RowPitch;
            if (texture->GetDesc().format == Format::RG16_FLOAT) {
                const auto values = reinterpret_cast<const uint16_t*>(row);
                for (uint32_t x = 0; x < desc.Width; ++x) {
                    pixels[y * desc.Width + x] = {
                        DirectX::PackedVector::XMConvertHalfToFloat(values[x * 2]),
                        DirectX::PackedVector::XMConvertHalfToFloat(values[x * 2 + 1]), 0, 1};
                }
            }
            else std::memcpy(pixels.data() + y * desc.Width, row, desc.Width * sizeof(Pixel));
        }
        const D3D12_RANGE written{0, 0};
        readback->Unmap(0, &written);
        return pixels;
    }

    class Fixture
    {
    public:
        explicit Fixture(GraphicsRenderer& renderer) : renderer(renderer), device(renderer.GetDevice())
        {
            RenderResource::Create(device);
            Resize(48, 32);
            pass = std::make_unique<TaaPass>(renderer);
        }

        ~Fixture()
        {
            device->WaitForIdle();
            pass.reset();
            RenderResource::Destroy();
        }

        void Resize(uint32_t w, uint32_t h)
        {
            device->WaitForIdle();
            width = w;
            height = h;
            renderer.ResizeRenderTexture(w, h);
            auto& resources = RenderResource::GetInstance();
            auto desc = TextureDesc().SetWidth(w).SetHeight(h).SetInitialState(ResourceStates::ShaderResource);
            color = device->CreateTexture(TextureDesc(desc).SetFormat(Format::RGBA32_FLOAT).SetIsRenderTarget(true));
            motion = device->CreateTexture(TextureDesc(desc).SetFormat(Format::RG32_FLOAT));
            depth = device->CreateTexture(TextureDesc(desc).SetFormat(Format::R32_FLOAT));
            resources.SetCommonTexture(CommonTextureSlot::Color, color);
            resources.SetCommonTexture(CommonTextureSlot::MotionVector, motion);
            resources.SetCommonTexture(CommonTextureSlot::Depth, depth);
            resources.GetFramebuffer() = device->CreateFramebuffer(FramebufferDesc().AddColorAttachment(color));
            if (pass) pass->OnResize(renderer, w, h);
            previousJitter = {};
        }

        // 信号的真值位于固定输出网格；输入颜色和速度则模拟实际的抖动光栅化。
        double Frame(float shiftX, float shiftY, float moveX, float moveY, bool invalidHistory = false)
        {
            auto jitter = TaaPass::GetJitterOffset(renderer.GetFrameIndex());
            const float jx = jitter.Get(0), jy = -float(jitter.Get(1));
            const Velocity velocity = invalidHistory ? Velocity{2, 2} : Velocity{
                (moveX + jx - float(previousJitter.Get(0))) / width,
                (moveY + jy - float(previousJitter.Get(1))) / height};
            std::vector<Pixel> input(width * height);
            std::vector<Velocity> velocities(input.size(), velocity);
            std::vector<float> depths(input.size(), 0.5f);
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) input[y * width + x] = Signal(x - jx - shiftX, y - jy - shiftY);
            }
            auto upload = device->CreateCommandList();
            upload->Open();
            upload->WriteTexture(color, 0, 0, input.data(), width * sizeof(Pixel));
            upload->WriteTexture(motion, 0, 0, velocities.data(), width * sizeof(Velocity));
            upload->WriteTexture(depth, 0, 0, depths.data(), width * sizeof(float));
            upload->Close();
            device->ExecuteCommandList(upload);
            pass->Render(renderer, 1.0f / 60.0f);
            const auto output = ReadTexture(device, color);
            double error = 0;
            for (uint32_t y = 4; y + 4 < height; ++y) {
                for (uint32_t x = 4; x + 4 < width; ++x) {
                    const auto expected = Signal(x - shiftX, y - shiftY);
                    const auto actual = output[y * width + x];
                    if (!std::isfinite(actual.r) || !std::isfinite(actual.g) || !std::isfinite(actual.b)) {
                        error = std::numeric_limits<double>::infinity();
                    }
                    error = std::max({error, double(std::abs(actual.r - expected.r)),
                        double(std::abs(actual.g - expected.g)), double(std::abs(actual.b - expected.b))});
                }
            }
            previousJitter = {jx, jy};
            renderer.Render(0);
            device->RunGarbageCollection();
            return error;
        }

        static Pixel Signal(float x, float y)
        {
            return {0.2f + x * 0.004f, 0.25f + y * 0.004f, 0.4f, 1.0f};
        }

        GraphicsRenderer& renderer;
        IDevice* device;
        uint32_t width{}, height{};
        TextureHandle color, motion, depth;
        std::unique_ptr<TaaPass> pass;
        Math::Vector2 previousJitter{};
    };

    bool TestSkyMotion(GraphicsRenderer& renderer)
    {
        auto device = renderer.GetDevice();
        auto& resources = RenderResource::GetInstance();
        RenderResource::Create(device);
        renderer.ResizeRenderTexture(48, 32);
        resources.OnResize(renderer, 48, 32);
        renderer.GetCamera().SetPosition(0, 0, 0);
        renderer.GetCamera().SetRotation(0, 0, 0);
        renderer.GetCamera().ReverseZ(false);
        bool passed = true;
        {
            MotionVectorPass pass(renderer);
            Math::Vector2 previousJitter{};
            Math::Matrix4 previousViewProj = Math::Matrix4::Identity;
            for (int frame = 0; frame < 5; ++frame) {
                // 天空方向忽略平移，但必须保留旋转；后两帧覆盖 reversed-Z。
                renderer.GetCamera().SetPosition(frame * 0.25f, frame * -0.125f, frame * 0.5f);
                if (frame == 3) {
                    renderer.GetCamera().ReverseZ(true);
                    // 深度约定变化时也更新快速清除元数据，不能用声明为 1 的资源清除为 0。
                    auto depthDesc = resources.GetCommonTexture(CommonTextureSlot::Depth)->GetDesc();
                    resources.SetCommonTexture(CommonTextureSlot::Depth,
                        device->CreateTexture(depthDesc.SetClearValue(Color{0.f, 0.f, 0.f, 0.f})));
                    pass.OnResize(renderer, 48, 32);
                }
                if (frame == 4) renderer.GetCamera().SetRotation(-0.05f, 0.1f, 0.f);
                const float clearDepth = renderer.GetCamera().IsReversedZ() ? 0.f : 1.f;
                auto clear = device->CreateCommandList();
                clear->Open();
                clear->ClearDepthStencilTexture(resources.GetCommonTexture(CommonTextureSlot::Depth),
                    AllSubresources, true, clearDepth, false, 0);
                clear->Close();
                device->ExecuteCommandList(clear);
                pass.Render(renderer, 0);
                const auto jitter = TaaPass::GetJitterUV(renderer.GetCamera(), renderer.GetFrameIndex());
                const auto expected = jitter - previousJitter;
                const auto currentViewProj = renderer.GetCamera().GetViewMatrix() *
                    TaaPass::GetJitteredProjection(renderer.GetCamera(), renderer.GetFrameIndex());
                const auto output = ReadTexture(device, resources.GetCommonTexture(CommonTextureSlot::MotionVector));
                if (frame > 0) {
                    double maxErrorPixels = 0;
                    bool finiteAndQuantized = true;
                    for (size_t index = 0; index < output.size(); ++index) {
                        const auto pixel = output[index];
                        auto expectedVelocity = expected;
                        if (frame == 4) {
                            const float u = (float(index % 48) + 0.5f) / 48;
                            const float v = (float(index / 48) + 0.5f) / 32;
                            auto world = Math::Vector4{u * 2 - 1, 1 - v * 2, clearDepth, 1} *
                                Math::Matrix4::Inverse(currentViewProj);
                            world /= float(world.Get(3));
                            const auto direction = Math::Vector3{world} - renderer.GetCamera().GetPosition();
                            const auto previous = Math::Vector4{direction, 0} * previousViewProj;
                            expectedVelocity = {u - (0.5f + 0.5f * float(previous.Get(0) / previous.Get(3))),
                                v - (0.5f - 0.5f * float(previous.Get(1) / previous.Get(3)))};
                        }
                        if (!std::isfinite(pixel.r) || !std::isfinite(pixel.g)) {
                            maxErrorPixels = std::numeric_limits<double>::infinity();
                            finiteAndQuantized = false;
                        }
                        // FLOAT32 写入 FLOAT16 RT 按 Direct3D 规则向零取整，允许一个半精度 ULP。
                        const auto tolerance = [](float value) {
                            int exponent = 0;
                            std::frexp(std::abs(value), &exponent);
                            const float ulp = value == 0 ? 0.f : std::ldexp(1.f, exponent - 11);
                            return std::max(ulp, std::ldexp(1.f, -24)) + 1e-6f;
                        };
                        finiteAndQuantized &= std::abs(pixel.r - float(expectedVelocity.Get(0))) <=
                            tolerance(expectedVelocity.Get(0));
                        finiteAndQuantized &= std::abs(pixel.g - float(expectedVelocity.Get(1))) <=
                            tolerance(expectedVelocity.Get(1));
                        const auto pixelError = std::max(
                            double(std::abs(pixel.r - float(expectedVelocity.Get(0))) * 48),
                            double(std::abs(pixel.g - float(expectedVelocity.Get(1))) * 32));
                        maxErrorPixels = std::max(maxErrorPixels, pixelError);
                    }
                    const bool ok = finiteAndQuantized;
                    std::printf("sky-motion-%d: max_error_pixels=%.9f %s\n", frame,
                        maxErrorPixels, ok ? "PASS" : "FAIL");
                    passed &= ok;
                }
                previousJitter = jitter;
                previousViewProj = currentViewProj;
                renderer.Render(0);
            }
        }
        device->WaitForIdle();
        RenderResource::Destroy();
        return passed;
    }
    bool TestResolve(GraphicsRenderer& renderer)
    {
        bool passed = true;
        Fixture fixture(renderer);
        const auto originalSettings = TaaPass::GetSettings();
        const auto jitter0 = TaaPass::GetJitterOffset(0);
        const auto jitter1 = TaaPass::GetJitterOffset(1);
        Require(std::abs(float(jitter0.Get(0) - jitter1.Get(0))) +
            std::abs(float(jitter0.Get(1) - jitter1.Get(1))) > 0.f,
            "回归测试必须使用变化的非零 jitter，不能通过关闭 jitter 绕过");
        const auto check = [&](const char* name, double error) {
            const bool ok = std::isfinite(error) && error < 0.00005;
            std::printf("%s: max_error=%.9f %s\n", name, error, ok ? "PASS" : "FAIL");
            passed &= ok;
        };

        check("first-frame", fixture.Frame(0, 0, 0, 0));
        double staticError = 0;
        for (int frame = 0; frame < 39; ++frame) staticError = std::max(staticError, fixture.Frame(0, 0, 0, 0));
        check("static-nonzero-jitter", staticError);
        double movingError = 0;
        for (int frame = 1; frame <= 8; ++frame) {
            movingError = std::max(movingError, fixture.Frame(frame * 0.25f, frame * -0.125f, 0.25f, -0.125f));
        }
        check("real-motion", movingError);
        // 关闭速度拒绝，确保用例确实在验证越界拒绝，而非碰巧由大速度清零历史权重。
        auto noVelocityRejection = originalSettings;
        noVelocityRejection.varianceClip = 0.f;
        fixture.pass->SetSettings(noVelocityRejection);
        check("out-of-bounds-history", fixture.Frame(0, 0, 0, 0, true));
        fixture.pass->SetSettings(originalSettings);
        fixture.Resize(37, 23);
        check("resize-first-frame", fixture.Frame(0, 0, 0, 0));
        double optionsError = 0;
        for (unsigned option = 0; option < 16; ++option) {
            auto settings = originalSettings;
            settings.enableColorClip = (option & 1) != 0;
            settings.useYCoCg = (option & 2) != 0;
            settings.useClosestFragment = (option & 4) != 0;
            fixture.renderer.GetCamera().ReverseZ((option & 8) != 0);
            fixture.pass->SetSettings(settings);
            fixture.pass->OnResize(fixture.renderer, fixture.width, fixture.height);
            for (int frame = 0; frame < 12; ++frame) {
                optionsError = std::max(optionsError, fixture.Frame(0, 0, 0, 0));
            }
        }
        check("all-shader-options", optionsError);
        auto singleSampleSettings = originalSettings;
        singleSampleSettings.sampleCount = 1;
        fixture.pass->SetSettings(singleSampleSettings);
        fixture.pass->OnResize(fixture.renderer, fixture.width, fixture.height);
        check("single-sample-first-frame", fixture.Frame(0, 0, 0, 0));
        check("single-sample-next-frame", fixture.Frame(0, 0, 0, 0));
        fixture.pass->SetSettings(originalSettings);
        return passed;
    }
}
}

int main()
{
    using namespace DSM;
    Messages messages;
    DSMEngine engine;
    EngineParameters parameters{};
    parameters.enableDebugLayer = true;
    parameters.graphicsMessageCallback = &messages;
    if (!engine.StartEngine(parameters)) return 2;
    bool passed = true;
    try {
        passed &= TestResolve(*DSMEngine::sm_GlobalContext.renderer);
        passed &= TestSkyMotion(*DSMEngine::sm_GlobalContext.renderer);
        passed &= CheckDebugMessages(DSMEngine::sm_GlobalContext.renderer->GetDevice());
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "TemporalAATests: %s\n", error.what());
        passed = false;
    }
    passed &= !messages.failed;
    engine.ShutDownEngine();
    std::printf("TemporalAATests: %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
