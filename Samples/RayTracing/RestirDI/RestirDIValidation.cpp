#include "RestirDIValidation.h"

#include "RestirDIRenderPipeline.h"
#include "RestirDIValidationScene.h"
#include "Editor/DSMEditor.h"
#include "Editor/Project.h"
#include "Runtime/DSMEngine.h"
#include "Runtime/Framework/Component/MeshRenderer.h"
#include "Runtime/Framework/Component/TransformComponent.h"
#include "Runtime/Framework/Object/GameObject.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"

#include <d3d12.h>
#include <dxgidebug.h>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace DSM::RestirDI {
    namespace {
        using Microsoft::WRL::ComPtr;
        using Json = nlohmann::json;

        class ValidationMessageCallback final : public IMessageCallback
        {
        public:
            void Message(MessageSeverity severity, const char* messageText) const override
            {
                std::scoped_lock lock(m_Mutex);
                m_Messages.push_back({severity, messageText != nullptr ? messageText : ""});
            }

            bool HasWarningOrWorse() const
            {
                std::scoped_lock lock(m_Mutex);
                return std::ranges::any_of(m_Messages, [](const Entry& entry) {
                    return entry.severity == MessageSeverity::Warning ||
                        entry.severity == MessageSeverity::Error ||
                        entry.severity == MessageSeverity::Fatal;
                });
            }

            Json ToJson() const
            {
                std::scoped_lock lock(m_Mutex);
                Json output = Json::array();
                for (const auto& entry : m_Messages) {
                    output.push_back({
                        {"severity", static_cast<uint32_t>(entry.severity)},
                        {"message", entry.text}});
                }
                return output;
            }

        private:
            struct Entry
            {
                MessageSeverity severity{};
                std::string text{};
            };
            mutable std::mutex m_Mutex{};
            mutable std::vector<Entry> m_Messages{};
        };

        class NativeDebugQueues final
        {
        public:
            ~NativeDebugQueues()
            {
                m_DXGI.Reset();
                if (m_DXGIDebugModule != nullptr) {
                    FreeLibrary(m_DXGIDebugModule);
                }
            }

            bool Initialize(IDevice* device)
            {
                if (device == nullptr) return false;
                ID3D12Device* nativeDevice = device->GetNativeObject(ObjectTypes::D3D12_Device);
                if (nativeDevice == nullptr || FAILED(nativeDevice->QueryInterface(IID_PPV_ARGS(&m_D3D12)))) {
                    return false;
                }
                m_DXGIDebugModule = LoadLibraryExW(
                    L"dxgidebug.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (m_DXGIDebugModule == nullptr) return false;
                using GetDebugInterfaceFunction = HRESULT(WINAPI*)(REFIID, void**);
                const auto getDebugInterface = reinterpret_cast<GetDebugInterfaceFunction>(
                    GetProcAddress(m_DXGIDebugModule, "DXGIGetDebugInterface"));
                if (getDebugInterface == nullptr ||
                    FAILED(getDebugInterface(IID_PPV_ARGS(&m_DXGI)))) {
                    return false;
                }
                Clear();
                return true;
            }

            void Clear()
            {
                if (m_D3D12) m_D3D12->ClearStoredMessages();
                if (m_DXGI) m_DXGI->ClearStoredMessages(DXGI_DEBUG_ALL);
            }

            Json Collect(bool& hasWarningOrWorse) const
            {
                Json output{{"d3d12", Json::array()}, {"dxgi", Json::array()}};
                hasWarningOrWorse = false;
                if (m_D3D12) {
                    const uint64_t count = m_D3D12->GetNumStoredMessagesAllowedByRetrievalFilter();
                    for (uint64_t index = 0; index < count; ++index) {
                        SIZE_T byteSize = 0;
                        m_D3D12->GetMessage(index, nullptr, &byteSize);
                        std::vector<std::byte> storage(byteSize);
                        auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                        if (FAILED(m_D3D12->GetMessage(index, message, &byteSize))) continue;
                        output["d3d12"].push_back({
                            {"id", static_cast<uint32_t>(message->ID)},
                            {"severity", static_cast<uint32_t>(message->Severity)},
                            {"description", message->pDescription != nullptr ? message->pDescription : ""}});
                        hasWarningOrWorse |= message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING;
                    }
                }
                if (m_DXGI) {
                    const uint64_t count = m_DXGI->GetNumStoredMessagesAllowedByRetrievalFilters(DXGI_DEBUG_ALL);
                    for (uint64_t index = 0; index < count; ++index) {
                        SIZE_T byteSize = 0;
                        m_DXGI->GetMessage(DXGI_DEBUG_ALL, index, nullptr, &byteSize);
                        std::vector<std::byte> storage(byteSize);
                        auto* message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(storage.data());
                        if (FAILED(m_DXGI->GetMessage(DXGI_DEBUG_ALL, index, message, &byteSize))) continue;
                        output["dxgi"].push_back({
                            {"id", message->ID},
                            {"severity", static_cast<uint32_t>(message->Severity)},
                            {"description", message->pDescription != nullptr ? message->pDescription : ""}});
                        hasWarningOrWorse |= message->Severity <= DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING;
                    }
                }
                return output;
            }

        private:
            ComPtr<ID3D12InfoQueue> m_D3D12{};
            ComPtr<IDXGIInfoQueue> m_DXGI{};
            HMODULE m_DXGIDebugModule = nullptr;
        };

        bool WriteJson(const std::filesystem::path& filename, const Json& value)
        {
            std::ofstream output(filename);
            if (!output) return false;
            output << value.dump(2);
            return static_cast<bool>(output);
        }

        float ACES(float value)
        {
            constexpr float a = 2.51f;
            constexpr float b = 0.03f;
            constexpr float c = 2.43f;
            constexpr float d = 0.59f;
            constexpr float e = 0.14f;
            return std::clamp((value * (a * value + b)) /
                std::max(value * (c * value + d) + e, 1e-6f), 0.0f, 1.0f);
        }

        bool WriteBmp(
            const std::filesystem::path& filename,
            const ValidationSnapshot& snapshot,
            float exposure)
        {
            if (snapshot.hdr.empty() || snapshot.width == 0 || snapshot.height == 0) return false;
            const uint32_t rowBytes = snapshot.width * 3u;
            const uint32_t rowPitch = (rowBytes + 3u) & ~3u;
            const uint32_t imageBytes = rowPitch * snapshot.height;
            std::array<uint8_t, 54> header{};
            auto write16 = [&header](size_t offset, uint16_t value) {
                header[offset] = static_cast<uint8_t>(value);
                header[offset + 1] = static_cast<uint8_t>(value >> 8);
            };
            auto write32 = [&header](size_t offset, uint32_t value) {
                for (uint32_t byte = 0; byte < 4; ++byte) {
                    header[offset + byte] = static_cast<uint8_t>(value >> (byte * 8u));
                }
            };
            header[0] = 'B';
            header[1] = 'M';
            write32(2, static_cast<uint32_t>(header.size()) + imageBytes);
            write32(10, static_cast<uint32_t>(header.size()));
            write32(14, 40);
            write32(18, snapshot.width);
            write32(22, snapshot.height);
            write16(26, 1);
            write16(28, 24);
            write32(34, imageBytes);

            std::ofstream output(filename, std::ios::binary);
            if (!output) return false;
            output.write(reinterpret_cast<const char*>(header.data()), header.size());
            std::vector<uint8_t> row(rowPitch);
            const float exposureScale = std::exp2(exposure);
            for (uint32_t y = 0; y < snapshot.height; ++y) {
                const uint32_t sourceY = snapshot.height - 1u - y;
                std::ranges::fill(row, 0u);
                for (uint32_t x = 0; x < snapshot.width; ++x) {
                    const auto& hdr = snapshot.hdr[size_t(sourceY) * snapshot.width + x];
                    const float red = ACES(std::max(hdr.x * exposureScale, 0.0f));
                    const float green = ACES(std::max(hdr.y * exposureScale, 0.0f));
                    const float blue = ACES(std::max(hdr.z * exposureScale, 0.0f));
                    row[x * 3u] = static_cast<uint8_t>(blue * 255.0f + 0.5f);
                    row[x * 3u + 1u] = static_cast<uint8_t>(green * 255.0f + 0.5f);
                    row[x * 3u + 2u] = static_cast<uint8_t>(red * 255.0f + 0.5f);
                }
                output.write(reinterpret_cast<const char*>(row.data()), row.size());
            }
            return static_cast<bool>(output);
        }

        float PixelLuminance(const GpuFloat4& color)
        {
            return 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
        }

        Json CalculateMetrics(const ValidationSnapshot& snapshot, uint32_t samplesPerPixel = 1u)
        {
            uint64_t finitePixels = 0;
            uint64_t hitPixels = 0;
            uint64_t validReservoirs = 0;
            uint64_t temporalAccepted = 0;
            uint64_t temporalAcceptedSamples = 0;
            uint64_t spatialAccepted = 0;
            uint64_t spatialRejected = 0;
            uint64_t visibilitySamples = 0;
            uint32_t maxVisibilitySamples = 0;
            std::array<uint64_t, 4> sourceCounts{};
            double totalLuminance = 0.0;
            uint64_t overexposedPixels = 0;
            for (size_t index = 0; index < snapshot.hdr.size(); ++index) {
                const auto& color = snapshot.hdr[index];
                const bool finite = std::isfinite(color.x) && std::isfinite(color.y) &&
                    std::isfinite(color.z) && std::isfinite(color.w);
                finitePixels += finite;
                if (finite) totalLuminance += std::max(PixelLuminance(color), 0.0f);
                overexposedPixels += ACES(std::max({color.x, color.y, color.z, 0.0f})) >= 0.999f;
                const bool hit = (snapshot.surfaces[index].ids.w & 1u) != 0u;
                hitPixels += hit;
                const auto& sample = snapshot.reservoirSamples[index];
                const auto& stats = snapshot.reservoirStats[index];
                const bool valid = sample.sourceType > 0u && sample.sourceType < sourceCounts.size() &&
                    stats.M > 0.0f && stats.W > 0.0f && std::isfinite(stats.W);
                validReservoirs += hit && valid;
                if (valid) sourceCounts[sample.sourceType]++;
                temporalAccepted += hit && snapshot.acceptance[index].temporalAccepted != 0u;
                if (hit) temporalAcceptedSamples += snapshot.acceptance[index].temporalAccepted;
                spatialAccepted += snapshot.acceptance[index].spatialAccepted;
                spatialRejected += snapshot.acceptance[index].spatialRejected;
                visibilitySamples += snapshot.acceptance[index].visibility;
                maxVisibilitySamples = std::max(
                    maxVisibilitySamples, snapshot.acceptance[index].visibility);
            }
            const double pixelCount = std::max<size_t>(snapshot.hdr.size(), 1);
            const uint64_t temporalEligibleSamples = hitPixels *
                std::max<uint32_t>(samplesPerPixel, 1u);
            return {
                {"width", snapshot.width}, {"height", snapshot.height},
                {"finite_ratio", finitePixels / pixelCount},
                {"hit_pixels", hitPixels},
                {"valid_reservoir_ratio", hitPixels > 0 ? double(validReservoirs) / hitPixels : 1.0},
                {"temporal_eligible_samples", temporalEligibleSamples},
                {"temporal_accepted_samples", temporalAcceptedSamples},
                {"temporal_accept_ratio", temporalEligibleSamples > 0
                    ? double(temporalAcceptedSamples) / temporalEligibleSamples : 0.0},
                {"temporal_accept_ratio_all_hits", hitPixels > 0
                    ? double(temporalAccepted) / hitPixels : 0.0},
                {"spatial_accepted", spatialAccepted}, {"spatial_rejected", spatialRejected},
                {"visibility_samples", visibilitySamples},
                {"max_visibility_samples", maxVisibilitySamples},
                {"analytic_samples", sourceCounts[1]}, {"emissive_samples", sourceCounts[2]},
                {"environment_samples", sourceCounts[3]},
                {"mean_luminance", totalLuminance / pixelCount},
                {"overexposed_ratio", overexposedPixels / pixelCount}};
        }

        Json CompareWithReference(
            const ValidationSnapshot& restir,
            const ValidationSnapshot& reference)
        {
            if (restir.width != reference.width || restir.height != reference.height || restir.hdr.empty() ||
                restir.surfaces.size() != restir.hdr.size()) {
                return {{"mean_relative_error", 1.0}, {"block_nrmse", 1.0},
                    {"pixel_nrmse", 1.0}, {"tonemapped_rmse", 1.0}};
            }
            double restirMean = 0.0;
            double referenceMean = 0.0;
            double pixelSquaredError = 0.0;
            double pixelSquaredReference = 0.0;
            double tonemappedSquaredError = 0.0;
            uint64_t comparedPixels = 0;
            for (size_t index = 0; index < restir.hdr.size(); ++index) {
                const double restirLuminance = std::max(PixelLuminance(restir.hdr[index]), 0.0f);
                const double referenceLuminance = std::max(PixelLuminance(reference.hdr[index]), 0.0f);
                restirMean += restirLuminance;
                referenceMean += referenceLuminance;
                if ((restir.surfaces[index].ids.w & 1u) != 0u) {
                    const double difference = restirLuminance - referenceLuminance;
                    pixelSquaredError += difference * difference;
                    pixelSquaredReference += referenceLuminance * referenceLuminance;
                    const double tonemappedDifference = ACES(static_cast<float>(restirLuminance)) -
                        ACES(static_cast<float>(referenceLuminance));
                    tonemappedSquaredError += tonemappedDifference * tonemappedDifference;
                    ++comparedPixels;
                }
            }
            restirMean /= restir.hdr.size();
            referenceMean /= reference.hdr.size();
            const double meanRelativeError = std::abs(restirMean - referenceMean) /
                std::max(referenceMean, 1e-6);

            double squaredError = 0.0;
            double squaredReference = 0.0;
            uint64_t blockCount = 0;
            for (uint32_t blockY = 0; blockY < restir.height; blockY += 16u) {
                for (uint32_t blockX = 0; blockX < restir.width; blockX += 16u) {
                    double restirBlock = 0.0;
                    double referenceBlock = 0.0;
                    uint32_t count = 0;
                    for (uint32_t y = blockY; y < std::min(blockY + 16u, restir.height); ++y) {
                        for (uint32_t x = blockX; x < std::min(blockX + 16u, restir.width); ++x) {
                            const size_t index = size_t(y) * restir.width + x;
                            restirBlock += std::max(PixelLuminance(restir.hdr[index]), 0.0f);
                            referenceBlock += std::max(PixelLuminance(reference.hdr[index]), 0.0f);
                            ++count;
                        }
                    }
                    restirBlock /= std::max(count, 1u);
                    referenceBlock /= std::max(count, 1u);
                    squaredError += (restirBlock - referenceBlock) * (restirBlock - referenceBlock);
                    squaredReference += referenceBlock * referenceBlock;
                    ++blockCount;
                }
            }
            const double blockNrmse = std::sqrt(squaredError / std::max<double>(blockCount, 1.0)) /
                std::max(std::sqrt(squaredReference / std::max<double>(blockCount, 1.0)), 1e-6);
            const double pixelNrmse = std::sqrt(pixelSquaredError /
                std::max<double>(comparedPixels, 1.0)) /
                std::max(std::sqrt(pixelSquaredReference /
                    std::max<double>(comparedPixels, 1.0)), 1e-6);
            const double tonemappedRmse = std::sqrt(tonemappedSquaredError /
                std::max<double>(comparedPixels, 1.0));
            return {{"restir_mean_luminance", restirMean},
                {"reference_mean_luminance", referenceMean},
                {"mean_relative_error", meanRelativeError}, {"block_nrmse", blockNrmse},
                {"pixel_nrmse", pixelNrmse}, {"tonemapped_rmse", tonemappedRmse},
                {"compared_pixels", comparedPixels}};
        }

        bool CaptureImage(
            RenderPipeline& pipeline,
            GraphicsRenderer& renderer,
            const std::filesystem::path& output,
            ValidationSnapshot& snapshot,
            std::string& error,
            float exposure = 0.0f)
        {
            return pipeline.CaptureValidation(renderer, snapshot, error) &&
                WriteBmp(output, snapshot, exposure);
        }

        void RunFrames(DSMEngine& engine, uint32_t frameCount)
        {
            for (uint32_t frame = 0; frame < frameCount && engine.IsRunning(); ++frame) {
                engine.Update();
            }
        }

        bool PrepareOutput(const std::filesystem::path& output)
        {
            std::error_code error{};
            std::filesystem::create_directories(output, error);
            return !error;
        }

        bool WriteHdrFixture(const std::filesystem::path& filename)
        {
            std::ofstream output(filename, std::ios::binary);
            if (!output) return false;
            output << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n";
            const std::array<uint8_t, 16> pixels = {
                128, 96, 64, 129, 64, 128, 96, 129,
                96, 64, 128, 129, 128, 128, 128, 129};
            output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
            return static_cast<bool>(output);
        }

        std::pair<int32_t, int32_t> ProjectPixel(
            GraphicsRenderer& renderer,
            Math::Vector3 worldPosition,
            uint32_t width,
            uint32_t height)
        {
            const Math::Vector4 clip = Math::Vector4(worldPosition, 1.0f) *
                renderer.GetCamera().GetViewProjMatrix();
            const float inverseW = 1.0f / std::max(std::abs(float(clip.Get(3))), 1e-6f);
            const float ndcX = float(clip.Get(0)) * inverseW;
            const float ndcY = float(clip.Get(1)) * inverseW;
            return {
                static_cast<int32_t>((ndcX * 0.5f + 0.5f) * width),
                static_cast<int32_t>((0.5f - ndcY * 0.5f) * height)};
        }

        bool NeighborhoodHasStableID(
            const ValidationSnapshot& snapshot,
            std::pair<int32_t, int32_t> pixel,
            uint32_t stableID,
            uint32_t radius = 1)
        {
            for (int32_t y = pixel.second - static_cast<int32_t>(radius);
                y <= pixel.second + static_cast<int32_t>(radius); ++y) {
                for (int32_t x = pixel.first - static_cast<int32_t>(radius);
                    x <= pixel.first + static_cast<int32_t>(radius); ++x) {
                    if (x < 0 || y < 0 || x >= static_cast<int32_t>(snapshot.width) ||
                        y >= static_cast<int32_t>(snapshot.height)) continue;
                    if (snapshot.surfaces[size_t(y) * snapshot.width + x].ids.x == stableID) return true;
                }
            }
            return false;
        }

        bool NeighborhoodHasVisibleReceiver(
            const ValidationSnapshot& snapshot,
            std::pair<int32_t, int32_t> pixel,
            uint32_t receiverID)
        {
            for (int32_t y = pixel.second - 2; y <= pixel.second + 2; ++y) {
                for (int32_t x = pixel.first - 2; x <= pixel.first + 2; ++x) {
                    if (x < 0 || y < 0 || x >= static_cast<int32_t>(snapshot.width) ||
                        y >= static_cast<int32_t>(snapshot.height)) continue;
                    const size_t index = size_t(y) * snapshot.width + x;
                    if (snapshot.surfaces[index].ids.x == receiverID &&
                        snapshot.acceptance[index].visibility != 0u) return true;
                }
            }
            return false;
        }

        Json SurfaceIDMetrics(const ValidationSnapshot& snapshot, uint32_t stableID)
        {
            uint64_t count = 0;
            uint64_t visibleCount = 0;
            int32_t minX = static_cast<int32_t>(snapshot.width);
            int32_t minY = static_cast<int32_t>(snapshot.height);
            int32_t maxX = -1;
            int32_t maxY = -1;
            GpuFloat4 minimumPosition{
                std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(), 0.0f};
            GpuFloat4 maximumPosition{
                std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(), 0.0f};
            for (uint32_t y = 0; y < snapshot.height; ++y) {
                for (uint32_t x = 0; x < snapshot.width; ++x) {
                    const size_t index = size_t(y) * snapshot.width + x;
                    if (snapshot.surfaces[index].ids.x != stableID) continue;
                    ++count;
                    visibleCount += snapshot.acceptance[index].visibility != 0u;
                    minX = std::min(minX, static_cast<int32_t>(x));
                    minY = std::min(minY, static_cast<int32_t>(y));
                    maxX = std::max(maxX, static_cast<int32_t>(x));
                    maxY = std::max(maxY, static_cast<int32_t>(y));
                    const auto& position = snapshot.surfaces[index].positionDepth;
                    minimumPosition.x = std::min(minimumPosition.x, position.x);
                    minimumPosition.y = std::min(minimumPosition.y, position.y);
                    minimumPosition.z = std::min(minimumPosition.z, position.z);
                    maximumPosition.x = std::max(maximumPosition.x, position.x);
                    maximumPosition.y = std::max(maximumPosition.y, position.y);
                    maximumPosition.z = std::max(maximumPosition.z, position.z);
                }
            }
            return {{"pixel_count", count}, {"visible_count", visibleCount},
                {"bounds", {minX, minY, maxX, maxY}},
                {"minimum_position", {minimumPosition.x, minimumPosition.y, minimumPosition.z}},
                {"maximum_position", {maximumPosition.x, maximumPosition.y, maximumPosition.z}}};
        }

        Json SceneSurfaceMetrics(Scene& scene, const ValidationSnapshot& snapshot)
        {
            Json output = Json::array();
            auto view = scene.GetObjectsWithComponents<MeshRenderer>();
            std::vector<ObjectID> ids(view.begin(), view.end());
            std::ranges::sort(ids, {}, [](ObjectID id) {
                return static_cast<uint32_t>(entt::to_integral(id));
            });
            for (ObjectID id : ids) {
                const auto object = scene.GetObjectByID(id).lock();
                if (object == nullptr) continue;
                const uint32_t stableID = static_cast<uint32_t>(entt::to_integral(id));
                Json item = SurfaceIDMetrics(snapshot, stableID);
                item["stable_id"] = stableID;
                item["name"] = object->GetName();
                output.push_back(std::move(item));
            }
            return output;
        }

        bool MetricsPassed(const Json& metrics)
        {
            return metrics["finite_ratio"].get<double>() == 1.0 &&
                metrics["valid_reservoir_ratio"].get<double>() >= 0.90 &&
                metrics["temporal_accept_ratio"].get<double>() >= 0.50 &&
                metrics["spatial_accepted"].get<uint64_t>() > 0 &&
                metrics["spatial_rejected"].get<uint64_t>() > 0 &&
                metrics["overexposed_ratio"].get<double>() <= 0.05;
        }
    }

    int RunRenderValidation(const ValidationOptions& options)
    {
        if (!PrepareOutput(options.outputDirectory)) return 3;
        Json status{{"mode", "render"}, {"passed", false}, {"exit_code", 3},
            {"stage", "output-prepared"}};
        WriteJson(options.outputDirectory / "status.raw.json", status);
        ValidationMessageCallback callback{};
        DSMEngine engine{};
        EngineParameters parameters{};
        parameters.enableDebugLayer = true;
        parameters.graphicsMessageCallback = &callback;
        status["stage"] = "engine-starting";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        engine.StartEngine(parameters);
        status["stage"] = "engine-started";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        auto renderer = DSMEngine::sm_GlobalContext.renderer;
        if (renderer == nullptr || renderer->GetDevice() == nullptr) {
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 3;
        }

        NativeDebugQueues debugQueues{};
        if (!debugQueues.Initialize(renderer->GetDevice())) {
            status["exit_code"] = 2;
            status["reason"] = "D3D12 或 DXGI Debug InfoQueue 不可用";
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 2;
        }
        debugQueues.Clear();
        status["stage"] = "debug-queues-ready";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        renderer->ResizeRenderTexture(320, 180);
        renderer->GetCamera().SetPosition({0.0f, 1.8f, -8.5f});
        renderer->GetCamera().LookAt({0.0f, 1.2f, 2.8f}, {0.0f, 1.0f, 0.0f});

        std::string sceneError{};
        status["stage"] = "scene-creating";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        ValidationSceneState validationScene = CreateValidationScene(renderer->GetDevice(), sceneError);
        if (validationScene.scene == nullptr) {
            status["reason"] = sceneError;
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 3;
        }
        status["stage"] = "scene-created";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        DSMEngine::sm_GlobalContext.scene = validationScene.scene;
        auto pipeline = std::make_unique<RenderPipeline>(false);
        RenderPipeline* pipelinePtr = pipeline.get();
        engine.SetRenderPipeline(std::move(pipeline));

        status["stage"] = "first-frame-starting";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        RunFrames(engine, 1);
        status["stage"] = "first-frame-completed";
        WriteJson(options.outputDirectory / "status.raw.json", status);
        if (!pipelinePtr->IsInitialized()) {
            const int exitCode = pipelinePtr->IsDXRAvailable() ? 3 : 2;
            status["exit_code"] = exitCode;
            status["reason"] = pipelinePtr->GetLastError();
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return exitCode;
        }

        const auto hdrFixture = options.outputDirectory / "valid-fixture.hdr";
        const bool hdrFixtureWritten = WriteHdrFixture(hdrFixture);
        const bool validHdrLoaded = hdrFixtureWritten && pipelinePtr->LoadEnvironmentHDR(hdrFixture);
        const bool invalidHdrRejected = !pipelinePtr->LoadEnvironmentHDR(
            options.outputDirectory / "invalid-fixture.hdr");
        const bool daylightRestored = pipelinePtr->LoadDefaultEnvironment();

        std::string captureError{};
        ValidationSnapshot analytic{};
        ValidationSnapshot emissive{};
        ValidationSnapshot environment{};
        ValidationSnapshot sppTwo{};
        ValidationSnapshot sppFour{};
        ValidationSnapshot sppEight{};
        ValidationSnapshot restir{};
        ValidationSnapshot reference{};
        ValidationSnapshot sourceDebug{};
        ValidationSnapshot motion{};
        ValidationSnapshot noShadow{};
        auto& settings = pipelinePtr->GetSettings();

        settings.enableAnalyticLights = true;
        settings.enableEmissiveTriangles = false;
        settings.enableEnvironment = false;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 8);
        bool artifactsOk = CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "analytic.bmp", analytic, captureError);

        settings.enableAnalyticLights = false;
        settings.enableEmissiveTriangles = true;
        settings.enableEnvironment = false;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 8);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "emissive.bmp", emissive, captureError);

        settings.enableAnalyticLights = false;
        settings.enableEmissiveTriangles = false;
        settings.enableEnvironment = true;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 8);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "environment.bmp", environment, captureError);

        settings.enableAnalyticLights = true;
        settings.enableEmissiveTriangles = true;
        settings.enableEnvironment = true;
        settings.renderMode = RenderMode::Restir;
        settings.debugView = DebugView::Final;

        settings.samplesPerPixel = 8;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 32);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "spp-8.bmp", sppEight, captureError);

        settings.samplesPerPixel = 4;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 32);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "spp-4.bmp", sppFour, captureError);

        settings.samplesPerPixel = 2;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 32);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "spp-2.bmp", sppTwo, captureError);

        settings.samplesPerPixel = 1;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 64);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "spp-1.bmp", restir, captureError);
        artifactsOk &= WriteBmp(options.outputDirectory / "restir.bmp", restir, settings.exposure);
        artifactsOk &= WriteBmp(options.outputDirectory / "alpha.bmp", restir, settings.exposure);

        settings.debugView = DebugView::SourceType;
        RunFrames(engine, 1);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "source-debug.bmp", sourceDebug, captureError);
        settings.debugView = DebugView::Final;

        settings.renderMode = RenderMode::Reference;
        settings.referenceSamplesPerPixel = 512;
        constexpr uint32_t referenceFrameCount = 8u;
        bool referenceCaptured = true;
        for (uint32_t referenceFrameIndex = 0;
            referenceFrameIndex < referenceFrameCount; ++referenceFrameIndex) {
            RunFrames(engine, 1);
            ValidationSnapshot referenceFrame{};
            if (!pipelinePtr->CaptureValidation(*renderer, referenceFrame, captureError)) {
                referenceCaptured = false;
                break;
            }
            if (referenceFrameIndex == 0u) {
                reference = std::move(referenceFrame);
            }
            else {
                for (size_t pixelIndex = 0; pixelIndex < reference.hdr.size(); ++pixelIndex) {
                    reference.hdr[pixelIndex].x += referenceFrame.hdr[pixelIndex].x;
                    reference.hdr[pixelIndex].y += referenceFrame.hdr[pixelIndex].y;
                    reference.hdr[pixelIndex].z += referenceFrame.hdr[pixelIndex].z;
                    reference.hdr[pixelIndex].w += referenceFrame.hdr[pixelIndex].w;
                }
            }
        }
        if (referenceCaptured) {
            for (auto& pixel : reference.hdr) {
                pixel.x /= referenceFrameCount;
                pixel.y /= referenceFrameCount;
                pixel.z /= referenceFrameCount;
                pixel.w /= referenceFrameCount;
            }
            referenceCaptured = WriteBmp(
                options.outputDirectory / "reference.bmp", reference, settings.exposure);
        }
        artifactsOk &= referenceCaptured;

        settings.renderMode = RenderMode::Restir;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 16);
        if (const auto moving = validationScene.scene->GetObjectByID(validationScene.movingObject).lock()) {
            moving->GetComponent<TransformComponent>()->Translate({0.65f, 0.0f, 0.0f});
        }
        RunFrames(engine, 1);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "motion.bmp", motion, captureError);

        for (ObjectID lightID : validationScene.analyticLightObjects) {
            if (const auto object = validationScene.scene->GetObjectByID(lightID).lock()) {
                object->SetEnabled(false);
            }
        }
        if (const auto object = validationScene.scene->GetObjectByID(
            validationScene.shadowProbeLight).lock()) {
            object->SetEnabled(true);
        }
        if (const auto alphaObject = validationScene.scene->GetObjectByID(validationScene.alphaObject).lock()) {
            alphaObject->SetEnabled(false);
        }
        settings.enableAnalyticLights = true;
        settings.enableEmissiveTriangles = false;
        settings.enableEnvironment = false;
        settings.renderMode = RenderMode::IndependentRIS;
        pipelinePtr->ResetHistory();
        RunFrames(engine, 8);
        artifactsOk &= CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "no-shadow.bmp", noShadow, captureError);

        renderer->ResizeRenderTexture(480, 270);
        RunFrames(engine, 2);
        ValidationSnapshot resizedUp{};
        artifactsOk &= pipelinePtr->CaptureValidation(*renderer, resizedUp, captureError);
        renderer->ResizeRenderTexture(320, 180);
        RunFrames(engine, 2);
        ValidationSnapshot resizedDown{};
        artifactsOk &= pipelinePtr->CaptureValidation(*renderer, resizedDown, captureError);

        Json metrics = CalculateMetrics(restir);
        metrics["analytic_mode"] = CalculateMetrics(analytic);
        metrics["emissive_mode"] = CalculateMetrics(emissive);
        metrics["environment_mode"] = CalculateMetrics(environment);
        metrics["spp_1"] = CalculateMetrics(restir, 1u);
        metrics["spp_1"]["requested_samples_per_pixel"] = 1;
        metrics["spp_2"] = CalculateMetrics(sppTwo, 2u);
        metrics["spp_2"]["requested_samples_per_pixel"] = 2;
        metrics["spp_4"] = CalculateMetrics(sppFour, 4u);
        metrics["spp_4"]["requested_samples_per_pixel"] = 4;
        metrics["spp_8"] = CalculateMetrics(sppEight, 8u);
        metrics["spp_8"]["requested_samples_per_pixel"] = 8;
        metrics["motion"] = CalculateMetrics(motion);
        metrics["reference_comparison"] = CompareWithReference(restir, reference);
        metrics["reference_frame_count"] = referenceFrameCount;
        metrics["reference_effective_samples_per_pixel"] =
            referenceFrameCount * settings.referenceSamplesPerPixel;
        metrics["spp_quality"] = {
            {"spp_1", CompareWithReference(restir, reference)},
            {"spp_2", CompareWithReference(sppTwo, reference)},
            {"spp_4", CompareWithReference(sppFour, reference)},
            {"spp_8", CompareWithReference(sppEight, reference)}};
        metrics["resize_up_valid"] = resizedUp.width == 480u && resizedUp.height == 270u;
        metrics["resize_down_valid"] = resizedDown.width == 320u && resizedDown.height == 180u;
        metrics["valid_hdr_loaded"] = validHdrLoaded;
        metrics["invalid_hdr_rejected"] = invalidHdrRejected;
        metrics["daylight_restored"] = daylightRestored;
        metrics["scene_surfaces"] = SceneSurfaceMetrics(*validationScene.scene, restir);

        const uint32_t alphaStableID = static_cast<uint32_t>(entt::to_integral(validationScene.alphaObject));
        const auto opaquePixel = ProjectPixel(*renderer, {-1.575f, 1.9125f, 0.5f}, restir.width, restir.height);
        const auto transparentPixel = ProjectPixel(*renderer, {-0.675f, 1.9125f, 0.5f}, restir.width, restir.height);
        const bool alphaOpaqueHit = NeighborhoodHasStableID(restir, opaquePixel, alphaStableID, 2);
        const bool alphaTransparentPassed = !NeighborhoodHasStableID(restir, transparentPixel, alphaStableID, 1);
        metrics["alpha_opaque_hit"] = alphaOpaqueHit;
        metrics["alpha_transparent_passed"] = alphaTransparentPassed;
        metrics["alpha_surface"] = SurfaceIDMetrics(restir, alphaStableID);
        metrics["alpha_opaque_probe"] = {opaquePixel.first, opaquePixel.second};
        metrics["alpha_transparent_probe"] = {transparentPixel.first, transparentPixel.second};

        const uint32_t noShadowStableID = static_cast<uint32_t>(entt::to_integral(validationScene.noShadowObject));
        const uint32_t receiverStableID = static_cast<uint32_t>(entt::to_integral(validationScene.noShadowReceiver));
        const bool noShadowPrimaryVisible = std::ranges::any_of(noShadow.surfaces,
            [noShadowStableID](const GpuSurface& surface) { return surface.ids.x == noShadowStableID; });
        const auto receiverPixel = ProjectPixel(*renderer, {-1.2f, 0.0f, 6.1f}, noShadow.width, noShadow.height);
        const bool noShadowVisibilityPassed = NeighborhoodHasVisibleReceiver(
            noShadow, receiverPixel, receiverStableID);
        metrics["no_shadow_primary_visible"] = noShadowPrimaryVisible;
        metrics["no_shadow_visibility_passed"] = noShadowVisibilityPassed;
        metrics["no_shadow_surface"] = SurfaceIDMetrics(noShadow, noShadowStableID);
        metrics["no_shadow_receiver_surface"] = SurfaceIDMetrics(noShadow, receiverStableID);
        metrics["no_shadow_receiver_probe"] = {receiverPixel.first, receiverPixel.second};
        WriteJson(options.outputDirectory / "metrics.json", metrics);

        const bool sourceModesPassed =
            metrics["analytic_mode"]["mean_luminance"].get<double>() > 0.0 &&
            metrics["analytic_mode"]["analytic_samples"].get<uint64_t>() > 0 &&
            metrics["emissive_mode"]["mean_luminance"].get<double>() > 0.0 &&
            metrics["emissive_mode"]["emissive_samples"].get<uint64_t>() > 0 &&
            metrics["environment_mode"]["mean_luminance"].get<double>() > 0.0 &&
            metrics["environment_mode"]["environment_samples"].get<uint64_t>() > 0;
        const bool comparisonPassed =
            metrics["reference_comparison"]["mean_relative_error"].get<double>() <= 0.10 &&
            metrics["reference_comparison"]["block_nrmse"].get<double>() <= 0.20;
        const bool resizePassed = metrics["resize_up_valid"].get<bool>() &&
            metrics["resize_down_valid"].get<bool>();
        const bool environmentLoadPassed = validHdrLoaded && invalidHdrRejected && daylightRestored;
        const bool alphaPassed = alphaOpaqueHit && alphaTransparentPassed;
        const bool shadowMaskPassed = noShadowPrimaryVisible && noShadowVisibilityPassed;
        const bool motionPassed =
            metrics["motion"]["temporal_accept_ratio"].get<double>() > 0.0 &&
            metrics["motion"]["spatial_rejected"].get<uint64_t>() > 0;
        const bool sppPassed =
            metrics["spp_1"]["finite_ratio"].get<double>() == 1.0 &&
            metrics["spp_2"]["finite_ratio"].get<double>() == 1.0 &&
            metrics["spp_4"]["finite_ratio"].get<double>() == 1.0 &&
            metrics["spp_8"]["finite_ratio"].get<double>() == 1.0 &&
            metrics["spp_1"]["max_visibility_samples"].get<uint32_t>() >= 1u &&
            metrics["spp_2"]["max_visibility_samples"].get<uint32_t>() >= 2u &&
            metrics["spp_4"]["max_visibility_samples"].get<uint32_t>() >= 4u &&
            metrics["spp_8"]["max_visibility_samples"].get<uint32_t>() >= 8u;
        const double sppOneError = metrics["spp_quality"]["spp_1"]["tonemapped_rmse"].get<double>();
        const double sppTwoError = metrics["spp_quality"]["spp_2"]["tonemapped_rmse"].get<double>();
        const double sppFourError = metrics["spp_quality"]["spp_4"]["tonemapped_rmse"].get<double>();
        const double sppEightError = metrics["spp_quality"]["spp_8"]["tonemapped_rmse"].get<double>();
        const double sppOnePixelError = metrics["spp_quality"]["spp_1"]["pixel_nrmse"].get<double>();
        const double sppTwoPixelError = metrics["spp_quality"]["spp_2"]["pixel_nrmse"].get<double>();
        const double sppFourPixelError = metrics["spp_quality"]["spp_4"]["pixel_nrmse"].get<double>();
        const double sppEightPixelError = metrics["spp_quality"]["spp_8"]["pixel_nrmse"].get<double>();
        static constexpr std::array<const char*, 4> sppMetricNames = {
            "spp_1", "spp_2", "spp_4", "spp_8"};
        const bool sppEnergyPassed = std::ranges::all_of(
            sppMetricNames, [&](const char* name) {
                return metrics["spp_quality"][name]["mean_relative_error"].get<double>() <= 0.10;
            });
        const bool sppQualityPassed =
            sppTwoError <= sppOneError * 1.02 &&
            sppFourError <= sppTwoError * 1.02 &&
            sppEightError <= sppFourError * 1.02 &&
            sppEightError <= sppOneError * 0.85 &&
            sppTwoPixelError <= sppOnePixelError * 1.02 &&
            sppFourPixelError <= sppTwoPixelError * 1.02 &&
            sppEightPixelError <= sppFourPixelError * 1.02 &&
            sppEightPixelError <= sppOnePixelError * 0.75 && sppEnergyPassed;
        const bool numericPassed = MetricsPassed(metrics) && sourceModesPassed &&
            comparisonPassed && resizePassed && motionPassed && environmentLoadPassed &&
            alphaPassed && shadowMaskPassed && sppPassed && sppQualityPassed;

        bool nativeWarnings = false;
        const Json nativeMessages = debugQueues.Collect(nativeWarnings);
        WriteJson(options.outputDirectory / "d3d12-dxgi-messages.json", nativeMessages);
        WriteJson(options.outputDirectory / "runtime-messages.json", callback.ToJson());
        const bool debugPassed = !nativeWarnings && !callback.HasWarningOrWorse();

        int exitCode = 0;
        if (!artifactsOk) exitCode = 3;
        else if (!debugPassed) exitCode = 5;
        else if (!numericPassed) exitCode = 4;
        status = {
            {"mode", "render"}, {"passed", exitCode == 0}, {"exit_code", exitCode},
            {"artifacts_ok", artifactsOk}, {"numeric_passed", numericPassed},
            {"spp_passed", sppPassed},
            {"spp_quality_passed", sppQualityPassed},
            {"debug_layer_passed", debugPassed}, {"capture_error", captureError}};
        WriteJson(options.outputDirectory / "status.raw.json", status);
        engine.ShutDownEngine();
        return exitCode;
    }

    int RunEditorValidation(const ValidationOptions& options)
    {
        if (!PrepareOutput(options.outputDirectory)) return 3;
        Json status{{"mode", "editor"}, {"passed", false}, {"exit_code", 6}};
        ValidationMessageCallback callback{};
        DSMEngine engine{};
        EngineParameters parameters{};
        parameters.enableDebugLayer = true;
        parameters.graphicsMessageCallback = &callback;
        engine.StartEngine(parameters);
        auto renderer = DSMEngine::sm_GlobalContext.renderer;
        if (renderer == nullptr || renderer->GetDevice() == nullptr) {
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 6;
        }

        NativeDebugQueues debugQueues{};
        if (!debugQueues.Initialize(renderer->GetDevice())) {
            status["exit_code"] = 2;
            status["reason"] = "D3D12 或 DXGI Debug InfoQueue 不可用";
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 2;
        }
        debugQueues.Clear();
        renderer->GetCamera().SetPosition({0.0f, 1.8f, -8.5f});
        renderer->GetCamera().LookAt({0.0f, 1.2f, 2.8f}, {0.0f, 1.0f, 0.0f});
        std::string sceneError{};
        auto validationScene = CreateValidationScene(renderer->GetDevice(), sceneError);
        if (validationScene.scene == nullptr) {
            status["reason"] = sceneError;
            WriteJson(options.outputDirectory / "status.raw.json", status);
            engine.ShutDownEngine();
            return 6;
        }
        DSMEngine::sm_GlobalContext.scene = validationScene.scene;

        auto& project = Project::GetInstance();
        const auto projectPath = options.outputDirectory / "RestirDIEditorValidation.dsmproj";
        const auto scenePath = options.outputDirectory / "RestirDIEditorValidation.dsmscene";
        std::filesystem::create_directories(options.outputDirectory / Project::s_AssetsFolderName);
        std::filesystem::create_directories(options.outputDirectory / Project::s_LibraryFolderName);
        project.SetProjectName("RestirDIEditorValidation");
        project.SetFilePath(projectPath.string());
        project.SetSceneFilePath(scenePath.string());
        validationScene.scene->SetSceneFilePath(scenePath.string());
        validationScene.scene->SetDirty(false);

        auto pipeline = std::make_unique<RenderPipeline>(true);
        RenderPipeline* pipelinePtr = pipeline.get();
        engine.SetRenderPipeline(std::move(pipeline));
        bool warningBeforeEditor = false;
        debugQueues.Collect(warningBeforeEditor);
        DSMEditor editor{};
        editor.StartEditor(&engine);
        // 验证使用独立布局，避免用户的多视口窗口位置污染自动化结果。
        const auto editorIniFile = options.outputDirectory / "validation-imgui.ini";
        {
            std::ofstream ini(editorIniFile);
            ini << "[Window][DockSpace Demo]\nPos=0,32\nSize=1600,992\nCollapsed=0\n\n"
                << "[Window][Viewport]\nPos=280,32\nSize=1000,728\nCollapsed=0\n\n"
                << "[Window][ReSTIR DI]\nPos=1280,32\nSize=320,728\nCollapsed=0\n\n"
                << "[Window][Hierarchy]\nPos=0,32\nSize=280,488\nCollapsed=0\n\n"
                << "[Window][Properties]\nPos=0,520\nSize=280,504\nCollapsed=0\n\n"
                << "[Window][Assets]\nPos=280,760\nSize=1320,264\nCollapsed=0\n";
        }
        const std::string editorIniPath = editorIniFile.string();
        ImGui::GetIO().IniFilename = editorIniPath.c_str();
        bool warningAfterEditorStart = false;
        debugQueues.Collect(warningAfterEditorStart);
        const auto initialCameraPosition = renderer->GetCamera().GetPosition();
        const auto initialCameraRotation = renderer->GetCamera().GetRotation();
        bool cameraMoved = false;
        uint32_t executedFrames = 0;
        uint32_t firstWarningFrame = std::numeric_limits<uint32_t>::max();
        for (; executedFrames < options.editorFrameCount; ++executedFrames) {
            if (executedFrames == 4u) {
                ImGui::GetIO().AddKeyEvent(ImGuiKey_W, true);
            }
            if (executedFrames == 12u) {
                ImGui::GetIO().AddKeyEvent(ImGuiKey_W, false);
            }
            if (!editor.RunFrame()) break;
            if (executedFrames == 20u) {
                cameraMoved = (renderer->GetCamera().GetPosition() -
                    initialCameraPosition).SqrMagnitude() > 1e-6f;
                pipelinePtr->GetSettings().enableCameraControl = false;
                renderer->GetCamera().SetPosition(initialCameraPosition);
                renderer->GetCamera().SetRotation(initialCameraRotation);
                pipelinePtr->ResetHistory();
            }
            bool frameWarning = false;
            debugQueues.Collect(frameWarning);
            if (frameWarning && firstWarningFrame == std::numeric_limits<uint32_t>::max()) {
                firstWarningFrame = executedFrames;
            }
        }
        ValidationSnapshot snapshot{};
        std::string captureError{};
        const bool captured = CaptureImage(*pipelinePtr, *renderer,
            options.outputDirectory / "editor.bmp", snapshot, captureError);
        const bool editorPassed = executedFrames == options.editorFrameCount &&
            pipelinePtr->IsInitialized() &&
            pipelinePtr->GetRenderedFrameCount() >= options.editorFrameCount &&
            pipelinePtr->GetUIFrameCount() >= options.editorFrameCount && captured &&
            snapshot.width > 0 && snapshot.height > 0 && cameraMoved;

        editor.ShutDownEditor();
        bool nativeWarnings = false;
        const Json nativeMessages = debugQueues.Collect(nativeWarnings);
        WriteJson(options.outputDirectory / "d3d12-dxgi-messages.json", nativeMessages);
        WriteJson(options.outputDirectory / "runtime-messages.json", callback.ToJson());
        const bool debugPassed = !nativeWarnings && !callback.HasWarningOrWorse();
        const int exitCode = !debugPassed ? 5 : (editorPassed ? 0 : 6);
        status = {
            {"mode", "editor"}, {"passed", exitCode == 0}, {"exit_code", exitCode},
            {"requested_frames", options.editorFrameCount}, {"executed_frames", executedFrames},
            {"rendered_frames", pipelinePtr->GetRenderedFrameCount()},
            {"ui_frames", pipelinePtr->GetUIFrameCount()}, {"captured", captured},
            {"camera_moved", cameraMoved},
            {"warning_before_editor", warningBeforeEditor},
            {"warning_after_editor_start", warningAfterEditorStart},
            {"first_warning_frame", firstWarningFrame == std::numeric_limits<uint32_t>::max()
                ? -1 : static_cast<int64_t>(firstWarningFrame)},
            {"debug_layer_passed", debugPassed}, {"capture_error", captureError}};
        WriteJson(options.outputDirectory / "status.raw.json", status);
        engine.ShutDownEngine();
        return exitCode;
    }

}
